#include <SDL_FixedIncludes.h>

import Pumpkin.Renderer.Camera;
import Pumpkin.Renderer.Texture;
import Pumpkin.Renderer.MeshLoader;
import Pumpkin.Renderer.Particles;
import Pumpkin.Renderer.ParticleSystem;
import Pumpkin.Renderer.Passes.Outline;
import Pumpkin.Renderer.Passes.Clear;
import Pumpkin.Renderer.Passes.Geometry;
import Pumpkin.Renderer.Passes.Particle;
import Pumpkin.Renderer.Passes.Skybox;
import Pumpkin.Renderer.Passes.Text;
import Pumpkin.Renderer.Passes.Image;
import Pumpkin.Renderer.Passes.Button;
import Pumpkin.Renderer.Renderer;
import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.TerrainBatcher;
import Pumpkin.Renderer.TerrainGenerator;
import Pumpkin.Renderer.Types;
import Pumpkin.Utils.Events;
import Pumpkin.Utils.RenderProfiler;
import Pumpkin.Utils.Utils;
import Pumpkin.Utils.Window;
import Pumpkin.UI.YAML;
import Pumpkin.UI.Canvas;
import Pumpkin.UI.Loader;
import Pumpkin.UI.Systems.Render;

import Pumpkin.Math.Mat;
import Pumpkin.Math.Quaternion;
import Pumpkin.Math.Vec;

import std;

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
            .seed = 0,
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
    // Pumpkin::UI::YAML::TestParser();
    GetEngineTime();
    Pumpkin::UI::TestUILoader();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println(stderr, "SDL could not initialize! SDL Error: {}", SDL_GetError());
        return 1;
    }

    if (!TTF_Init()) {
        std::println(stderr, "SDL_ttf couldn't initialize! SDL Error: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    {
        const auto fb = std::make_unique<FrameBuffer>();
        const auto db = std::make_unique<DepthBuffer>();

        std::uint64_t targetFps = kDefaultTargetFps;
        Camera camera;
        auto previousTime = std::chrono::steady_clock::now();

        // Window and renderer
        Renderer renderer{};

        {
            SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);

            ResizeBuffers(renderer, *fb, *db, mode->w / 2, mode->h / 2);
            camera.aspectRatio =
                static_cast<float>(fb->width) / static_cast<float>(fb->height);

            if (mode->refresh_rate != 0.0f) {
                targetFps = kUncappedFps;
            }

            SDL_free(ids);
        }

        constexpr SDL_WindowFlags windowFlags =
            SDL_WINDOW_RESIZABLE;

        SDL_Window* window =
            SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, windowFlags);

        if (!window) {
            std::println(stderr, "Could not create window! SDL Error: {}", SDL_GetError());
        } else {
            SetMultiResWindowIcon(window);

            SDL_Surface* surface = SDL_GetWindowSurface(window);
            surface->format = SDL_PIXELFORMAT_RGBA32;

            // Meshes, textures and materials
            constexpr Color white{ .r = 255, .g = 255, .b = 255, .a = 255 };

            Mesh cowObj = LoadMesh("Assets/Models/Spot.obj", white);
            Mesh vkRoomObj = LoadMesh("Assets/Models/VikingRoom.obj", white);

            Mat4f cowModel = Mat4f::Translate({ 0.0f, 7.0f, 0.0f });
            Mat4f vkRoomModel = Mat4f::Translate({ -3.0f, 7.0f, -3.0f });

            vkRoomModel.SetRotation(
                Quaternionf{
                    -std::sin(std::numbers::pi_v<float> / 2.0f),
                    0.0f,
                    0.0f,
                    1.0f
                }.Normalized()
            );

            Texture cowAlbedo, vkRoomAlbedo, vkRoomNormal, vkRoomORM, skyboxTexture;

            LoadTextureOrWarn(cowAlbedo, "Assets/Textures/SpotTexture.png");
            LoadTextureOrWarn(vkRoomAlbedo, "Assets/Textures/VikingRoom/Albedo.png");
            LoadTextureOrWarn(vkRoomNormal, "Assets/Textures/VikingRoom/Normal.png");
            LoadTextureOrWarn(vkRoomORM, "Assets/Textures/VikingRoom/ORM.png");
            LoadTextureOrWarn(skyboxTexture, "Assets/Textures/Skybox.png");

            Material cowMaterial{
                .baseColor = white,
                .metallic = 0.7f,
                .roughness = 0.3f,
                .ao = 0.5f,
                .albedo = &cowAlbedo,
                .shader = ShaderId::Lambert
            };

            Material vkRoomMaterial{
                .baseColor = white,
                .albedo = &vkRoomAlbedo,
                .normal = &vkRoomNormal,
                .orm = &vkRoomORM,
                .shader = ShaderId::PBR,
                .animatedWaterWave = true
            };

            // Terrain and water
            const TerrainConfig terrainConfig = MakeTerrainConfig();
            TerrainBatcher terrain{ GenerateTerrainChunks(terrainConfig, 16) };
            Mesh waterMesh = GenerateWater(terrainConfig);

            Mat4f terrainModel = Mat4f::Identity();
            Mat4f waterModel = Mat4f::Identity();

            Material terrainMaterial{
                .shader = ShaderId::Lambert,
                .animatedWaterOndulation = false
            };
            Material waterMaterial{
                .baseColor = Color{ 35, 145, 220, 127 },
                .shader = ShaderId::Water,
                .animatedWaterOndulation = false,
                .animatedWaterWave = true
            };

            // Particles
            auto particleSystem =
                std::make_shared<ParticleSystem>(MakeSnowParticles());

            auto particleSystem2 =
                std::make_shared<ParticleSystem>(MakeFountainParticles());

            // UI
            std::ifstream uiFile("Assets/UI/MainMenu.yaml");


            if (!uiFile) {
                std::println(stderr, "Impossible d'ouvrir MainMenu.yaml");
                SDL_DestroyWindow(window);
                return 1;
            }

            const std::string uiSource{
                std::istreambuf_iterator<char>(uiFile),
                std::istreambuf_iterator<char>()
            };
            std::cout << "YAML loaded:\n" << uiSource << '\n';
            auto uiRoot = Pumpkin::UI::YAML::Parse(uiSource);
            auto loadedUI = Pumpkin::UI::LoadUI(uiRoot);

            // Declare UI before renderer passes are added.
            Pumpkin::UI::UIRenderSystem uiRenderer(loadedUI);
            uiRenderer.UpdateLayout(fb->width, fb->height);

            // Render passes
            renderer.AddPass<ClearPass>(ToRGBA(0, 0, 0, 0));
            renderer.AddPass<SkyboxPass>(skyboxTexture);
            renderer.AddPass<ParticlePass>(particleSystem, particleSystem2);
            renderer.AddPass<GeometryPass>();
            renderer.AddPass<OutlinePass>(0.03f);
            renderer.AddPass<ButtonPass>(uiRenderer.Buttons());
            renderer.AddPass<ImagePass>(uiRenderer.Images());
            auto& textPass = renderer.AddPass<TextPass>(uiRenderer.Texts());

            std::array scene{
                DrawItem{
                    .mesh = &waterMesh,
                    .model = &waterModel,
                    .material = &waterMaterial
                },
                DrawItem{
                    .mesh = &terrain.Inside(),
                    .model = &terrainModel,
                    .material = &terrainMaterial
                },
                DrawItem{
                    .mesh = &terrain.Crossing(),
                    .model = &terrainModel,
                    .material = &terrainMaterial
                },
                DrawItem{
                    .mesh = &cowObj,
                    .model = &cowModel,
                    .material = &cowMaterial
                },
                DrawItem{
                    .mesh = &vkRoomObj,
                    .model = &vkRoomModel,
                    .material = &vkRoomMaterial
                }
            };

            // Main loop
            const auto frameTime =
                std::chrono::nanoseconds(1'000'000'000 / targetFps);

            auto nextFrame = std::chrono::steady_clock::now();
            std::uint64_t lastFpsUpdate = SDL_GetTicks();
            std::uint32_t frames = 0;
            bool isRunning = true;

            std::vector<std::uint8_t> toon(
                static_cast<std::size_t>(fb->width) * fb->height
            );

            Vec2f windowDims{};
            {
                SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
                const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);
                windowDims = { static_cast<float>(mode->w), static_cast<float>(mode->h) };
                SDL_free(ids);
            }

            while (isRunning) {
                nextFrame += frameTime;

                const auto currentTime = std::chrono::steady_clock::now();
                const std::chrono::duration<float> elapsedTime =
                    currentTime - previousTime;

                const float deltaTime = elapsedTime.count();
                previousTime = currentTime;

                RenderContext ctx{
                    .fb = *fb,
                    .db = *db,
                    .camera = camera,
                    .deltaTime = deltaTime,
                    .scene = scene,
                    .toonMask = toon
                };

                HandleEvents(
                    isRunning,
                    window,
                    surface,
                    renderer,
                    fb,
                    db,
                    camera,
                    uiRenderer,
                    textPass,
                    ctx,
                    toon,
                    windowDims
                );

                if (WindowNotFocused(window)) {
                    std::this_thread::sleep_until(nextFrame);
                    continue;
                }

                UpdateCameraMovement(camera, deltaTime);

                fb->pixels = static_cast<std::uint32_t*>(surface->pixels);

                const float fovRadians =
                    camera.FOVY * (std::numbers::pi_v<float> / 180.0f);

                const Mat4f view = camera.GetViewMatrix();

                const Mat4f projection = Mat4f::Perspective(
                    fovRadians,
                    camera.aspectRatio,
                    camera.nearFrame,
                    camera.farFrame
                );

                const Mat4f viewModel = view * terrainModel;
                terrain.Update(projection * viewModel);

                renderer.Render(ctx);
                SDL_UpdateWindowSurface(window);

                ++frames;
                const std::uint64_t now = SDL_GetTicks();

                if (now - lastFpsUpdate >= 1000) {
                    SDL_SetWindowTitle(
                        window,
                        std::format("Pumpkin Engine | FPS: {} | Avg: {:.3f}ms", frames, 1000.0f / static_cast<float>(frames)).c_str()
                    );

                    frames = 0;
                    lastFpsUpdate = now;
                }

                Prof::EndFrame();
                std::this_thread::sleep_until(nextFrame);
            }

            // Destroy the window while SDL is still initialized.
            SDL_DestroyWindow(window);
        }
    } // Renderer, UI, fonts and scene objects are destroyed here.

    TTF_Quit();
    SDL_Quit();

    return 0;
}
