#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <numbers>
#include <print>
#include <thread>
#include <vector>

#include <Math/Mat.h>
#include <SDL3/SDL.h>

#include <Renderer/Camera.hpp>
#include <Renderer/Fragment.hpp>
#include <Renderer/MeshLoader.hpp>
#include <Renderer/PrimitiveAssembly.hpp>
#include <Renderer/Renderer.hpp>
#include <Renderer/Shaders.hpp>
#include <Renderer/TerrainBatcher.hpp>
#include <Renderer/TerrainGenerator.hpp>
#include <Renderer/Texture.hpp>
#include <Renderer/Transform.hpp>
#include <Renderer/Types.hpp>
#include <Utils/RenderProfiler.hpp>
#include <Utils/Utils.hpp>
#include <Utils/Window.hpp>
#include <Utils/Events/Events.hpp>

#include <Renderer/Passes/ClearPass.hpp>
#include <Renderer/Passes/GeometryPass.hpp>
#include <Renderer/Passes/TextPass.hpp>
#include <Renderer/Particles.hpp>
#include <Renderer/ParticleSystem.hpp>
#include <Renderer/Passes/ParticlePass.hpp>
#include <Renderer/Passes/SkyboxPass.hpp>
#include <Renderer/Passes/OutlinePass.hpp>

using namespace Pumpkin;

namespace {
    constexpr std::uint64_t kDefaultTargetFps = 120;
    constexpr std::uint64_t kUncappedFps      = 9'999'999;

    void LoadTextureOrWarn(Texture& texture, const char* path) {
        if (!LoadTexture(texture, path)) {
            std::println("Impossible to load texture: {}", path);
        }
    }

    TerrainConfig MakeTerrainConfig() {
        return TerrainConfig{
            .cellsX = 256,
            .cellsZ = 256,
            .sizeX = 50.0f,
            .sizeZ = 50.0f,
            .minHeight = -2.0f,
            .maxHeight = 5.0f,
            .baseHeight = 1.5f,
            .noiseScale = 0.18f,
            .noiseOctaves = 4,
            .persistence = 0.5f,
            .lacunarity = 2.0f,
            .seed = 41,
            .waterLevel = 0.20f,
            .sandLevel = 0.35f,
            .grassLevel = 0.35f,
            .rockLevel = 0.45f,
            .snowLevel = 0.72f
        };
    }

    Particles MakeSnowParticles() {
        Particles particles{};
        particles.
            SetPosition(0.0f, 3.0f, 0.0f).
            SetDirection(0.0f, -1.0f, 0.0f).
            SetMaxParticles(2000).
            SetMinDuration(0.5f).
            SetMaxDuration(3.0f).
            SetSpeedMin(0.1f).
            SetSpeedMax(0.4f).
            SetGravity(0.0f, -0.62f, 0.0f).
            SetInitialVelocity(0.0f, 0.0f, 0.0f).
            SetColorMin({ 255, 210, 210, 255 }).
            SetColorMax({ 255, 255, 255, 255 }).
            SetSpawnRadius(20);
        return particles;
    }

    Particles MakeFountainParticles() {
        Particles particles{};
        particles.
            SetPosition(3.0f, 7.0f, 0.0f).
            SetDirection(-1.0f, 1.0f, 0.3f).
            SetMaxParticles(500).
            SetMinDuration(0.5f).
            SetMaxDuration(3.0f).
            SetSpeedMin(0.7f).
            SetSpeedMax(1.3f).
            SetGravity(0.0f, -9.81f, 0.0f).
            SetInitialVelocity(0.0f, 2.0f, 0.0f).
            SetColorMin({ 255, 0, 255, 255 }).
            SetColorMax({ 255, 255, 255, 255 });
        return particles;
    }
}

// ---------------------------------------------------------------------------

int main() {
    const auto fb = std::make_unique<FrameBuffer>();
    const auto db = std::make_unique<DepthBuffer>();

    std::uint64_t targetFps = kDefaultTargetFps;

    Camera camera;

    auto previousTime = std::chrono::steady_clock::now();

    // SDL, renderer and window

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL could not initialize! SDL Error: {}", SDL_GetError());
        return 1;
    }

    if (!TTF_Init()) {
        std::println(stderr, "SDL_ttf couldn't initialize! SDL Error: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    Renderer renderer{};
    {
        SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);
        ResizeBuffers(renderer, *fb, *db, mode->w / 2, mode->h / 2);
        camera.aspectRatio = static_cast<float>(fb->width) / static_cast<float>(fb->height);
        if (mode->refresh_rate != 0.0f) {
            targetFps = kUncappedFps;//static_cast<std::uint64_t>(mode->refresh_rate);
        }
        SDL_free(ids);
    }

    constexpr SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_TRANSPARENT;
    SDL_Window* window = SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, windowFlags);
    if (!window) {
        std::println("Could not create window! SDL Error: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SetMultiResWindowIcon(window);

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    surface->format = SDL_PIXELFORMAT_RGBA32;

    // Meshes, textures and materials

    constexpr Color white{ .r = 255, .g = 255, .b = 255, .a = 255 };

    Mesh cowObj    = LoadMesh("Assets/Models/Spot.obj", white);
    Mesh vkRoomObj = LoadMesh("Assets/Models/VikingRoom.obj", white);

    Mat4f cowModel    = Mat4f::Translate({ 0.0f, 7.0f, 0.0f });
    Mat4f vkRoomModel = Mat4f::Translate({ -3.0f, 7.0f, -3.0f });
    vkRoomModel.SetRotation(
        Quaternionf{ -std::sin(std::numbers::pi_v<float> / 2.0f), 0.0f, 0.0f, 1.0f }.Normalized());

    Texture cowAlbedo, vkRoomAlbedo, vkRoomNormal, vkRoomORM, skyboxTexture;
    LoadTextureOrWarn(cowAlbedo,     "Assets/Textures/SpotTexture.png");
    LoadTextureOrWarn(vkRoomAlbedo,  "Assets/Textures/VikingRoom/Albedo.png");
    LoadTextureOrWarn(vkRoomNormal,  "Assets/Textures/VikingRoom/Normal.png");
    LoadTextureOrWarn(vkRoomORM,     "Assets/Textures/VikingRoom/ORM.png");
    LoadTextureOrWarn(skyboxTexture, "Assets/Textures/Skybox.png");

    Material cowMaterial{
        .baseColor = white,
        .metallic = 0.7f,
        .roughness = 0.3f,
        .ao = 0.5f,
        .albedo = &cowAlbedo,
        .shader = ShaderId::Toon
    };
    Material vkRoomMaterial{
        .baseColor = white,
        .albedo = &vkRoomAlbedo,
        .normal = &vkRoomNormal,
        .orm = &vkRoomORM,
        .shader = ShaderId::PBR
    };

    // Terrain and water

    const TerrainConfig terrainConfig = MakeTerrainConfig();
    TerrainBatcher terrain{ GenerateTerrainChunks(terrainConfig, 16) };
    Mesh waterMesh = GenerateWater(terrainConfig);

    Mat4f terrainModel = Mat4f::Identity();
    Mat4f waterModel   = Mat4f::Identity();

    Material terrainMaterial{
        .baseColor = { 255, 255, 255, 255 },
        .shader = ShaderId::PBR
    };
    Material waterMaterial{
        .baseColor = { 255, 255, 255, 255 },
        .shader = ShaderId::VertexColor
    };

    // Text and particles

    constexpr float fontSize = 14.0f;
    TTF_Font* font = TTF_OpenFont("Assets/Fonts/Arial.ttf", fontSize);
    if (font == nullptr) {
        SDL_Log("Couldn't load %g pt font from %s: %s",
            fontSize, "Assets/Fonts/Arial.ttf", SDL_GetError());
    } else {
        std::println("It works bebou");
    }

    Text text(font, "hello world bebou", 50.0f, SDL_Color{ 255, 0, 0, 255 }, 20, 20);

    auto particleSystem  = std::make_shared<ParticleSystem>(MakeSnowParticles());
    auto particleSystem2 = std::make_shared<ParticleSystem>(MakeFountainParticles());

    // Render passes and scene

    renderer.AddPass<ClearPass>(ToRGBA(0, 0, 0, 0));
    renderer.AddPass<SkyboxPass>(skyboxTexture);
    renderer.AddPass<ParticlePass>(particleSystem, particleSystem2);
    renderer.AddPass<GeometryPass>();
    renderer.AddPass<OutlinePass>(0.03f);
    renderer.AddPass<TextPass>(text);

    std::array scene{
        DrawItem{ .mesh = &waterMesh,          .model = &waterModel,   .material = &waterMaterial },
        DrawItem{ .mesh = &terrain.Inside(),   .model = &terrainModel, .material = &terrainMaterial },
        DrawItem{ .mesh = &terrain.Crossing(), .model = &terrainModel, .material = &terrainMaterial },
        DrawItem{ .mesh = &cowObj,             .model = &cowModel,     .material = &cowMaterial },
        DrawItem{ .mesh = &vkRoomObj,          .model = &vkRoomModel,  .material = &vkRoomMaterial },
    };

    // Main loop

    const auto frameTime = std::chrono::nanoseconds(1'000'000'000 / targetFps);
    auto nextFrame = std::chrono::steady_clock::now();

    std::uint64_t lastFpsUpdate = SDL_GetTicks();
    std::uint32_t frames = 0;
    bool isRunning = true;

    std::vector<std::uint8_t> toon(static_cast<std::size_t>(fb->width) * fb->height);

    while (isRunning) {
        nextFrame += frameTime;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                isRunning = false;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                SDL_UpdateWindowSurface(window);
                surface = SDL_GetWindowSurface(window);
                surface->format = SDL_PIXELFORMAT_RGBA32;
                ResizeBuffers(renderer, *fb, *db, event.window.data1, event.window.data2);
                camera.aspectRatio = static_cast<float>(fb->width) / static_cast<float>(fb->height);
                toon.resize(static_cast<std::size_t>(fb->width) * fb->height);
                break;

            default:
                break;
            }

            HandleCameraEvent(camera, event);
        }

        if (WindowNotFocused(window)) {
            std::this_thread::sleep_until(nextFrame);
            continue;
        }

        const auto currentTime = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsedTime = currentTime - previousTime;
        const float deltaTime = elapsedTime.count();
        previousTime = currentTime;

        UpdateCameraMovement(camera, deltaTime);

        fb->pixels = static_cast<std::uint32_t*>(surface->pixels);

        // Same matrices as VertexShader, so the chunk culling matches what is drawn.
        const float fovRadians = camera.FOVY * (std::numbers::pi_v<float> / 180.0f);
        const Mat4f view       = camera.getViewMatrix();
        const Mat4f projection = Mat4f::Perspective(fovRadians, camera.aspectRatio, camera.nearFrame, camera.farFrame);
        const Mat4f viewModel  = view * terrainModel;
        terrain.Update(projection * viewModel);

        RenderContext ctx{ .fb = *fb, .db = *db, .camera = camera, .deltaTime = deltaTime, .scene = scene, .toonMask = toon };
        renderer.Render(ctx);

        SDL_UpdateWindowSurface(window);

        // FPS counter in the window title, refreshed once per second.
        ++frames;
        const std::uint64_t now = SDL_GetTicks();
        if (now - lastFpsUpdate >= 1000) {
            SDL_SetWindowTitle(window, std::format("Pumpkin Engine | FPS: {}", frames).c_str());
            frames = 0;
            lastFpsUpdate = now;
        }

        Prof::EndFrame();
        std::this_thread::sleep_until(nextFrame);
    }

    // Shutdown

    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
