#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <random>
#include <thread>
#include <unordered_map>
#include <vector>
#include <cstring>

#include <Math/Mat.h>
#include <SDL3/SDL.h>

#include <Renderer/Camera.hpp>
#include <Renderer/Fragment.hpp>
#include <Renderer/PrimitiveAssembly.hpp>
#include <Renderer/Renderer.hpp>
#include <Renderer/Shaders.hpp>
#include <Renderer/Texture.hpp>
#include <Renderer/Transform.hpp>
#include <Renderer/Types.hpp>
#include <Utils/ObjParser.hpp>
#include <Utils/RenderProfiler.hpp>
#include <Utils/Utils.hpp>
#include <Utils/Events/Events.hpp>

#include <Renderer/Passes/ClearPass.hpp>
#include <Renderer/Passes/GeometryPass.hpp>
#include <Renderer/Passes/TextPass.hpp>


using namespace Pumpkin;

constexpr uint64_t kDefaultTargetFps = 120;
constexpr uint64_t kUncappedFps      = 9'999'999;
constexpr float    kDefaultVertexW   = 2.0f;
constexpr uint32_t kInvalidIndex     = static_cast<uint32_t>(-1);


static void GenerateNormals(Mesh& mesh)
{
    for (auto& vertex : mesh.vertices)
        vertex.normal = Vec3f{};

    for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const uint32_t i0 = mesh.indices[i];
        const uint32_t i1 = mesh.indices[i + 1];
        const uint32_t i2 = mesh.indices[i + 2];

        const Vec3f p0{
            mesh.vertices[i0].position[0],
            mesh.vertices[i0].position[1],
            mesh.vertices[i0].position[2]
        };

        const Vec3f p1{
            mesh.vertices[i1].position[0],
            mesh.vertices[i1].position[1],
            mesh.vertices[i1].position[2]
        };

        const Vec3f p2{
            mesh.vertices[i2].position[0],
            mesh.vertices[i2].position[1],
            mesh.vertices[i2].position[2]
        };

        const Vec3f normal = Cross(p1 - p0, p2 - p0);

        mesh.vertices[i0].normal += normal;
        mesh.vertices[i1].normal += normal;
        mesh.vertices[i2].normal += normal;
    }

    for (auto& vertex : mesh.vertices)
        vertex.normal = vertex.normal.Normalized();
}


// ---------------------------------------------------------------------------
// Mesh loading / drawing
// ---------------------------------------------------------------------------

static Mesh LoadMesh(
    const std::filesystem::path& filepath,
    Color col = Color{ .r = 255, .g = 255, .b = 255, .a = 255 },
    bool deduplicate = true) {
    if (filepath.extension().string() != ".obj") return Mesh{};

    // triangle test
	static constexpr bool triangleTest = false;
	if constexpr (triangleTest) {
		Mesh mesh{};
		mesh.vertices = {
			{
				{ 0.0f, 1.0f, 0.5f, 1.0f },
				{},
				{}
			},
			{
				{ -1.0f, -1.0f, 0.5f, 1.0f },
				{},
				{}
			},
			{
				{ 1.0f, -1.0f, 0.5f, 1.0f },
				{},
				{}
			}
		};
		mesh.colors = { {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255} };
		mesh.indices = { 0, 1, 2 };
		return mesh;
	}

    const auto [attribs, objIndices] = LoadObj(filepath);

    size_t validNormals = 0;

    for (const auto& idx : objIndices)
    {
        if (idx.normal_index != kInvalidIndex)
            ++validNormals;
    }

    std::cout << "OBJ normals: " << attribs.normals.size() / 3
        << " | Face references with normals: " << validNormals
        << " / " << objIndices.size() << '\n';

    Mesh mesh;
    mesh.vertices.reserve(objIndices.size());
    mesh.indices.reserve(objIndices.size());

    std::unordered_map<IndexKey, uint32_t> seen;
    if (deduplicate) seen.reserve(objIndices.size());

    for (const auto& idx : objIndices) {
        const size_t pos = static_cast<size_t>(idx.vertex_index) * 3;
        if (pos + 2 >= attribs.vertices.size()) {
            continue;
        }

        const auto vertexIndex = static_cast<uint32_t>(mesh.vertices.size());

        if (deduplicate) {
            const IndexKey key{
                idx.vertex_index,
                idx.texcoord_index,
                idx.normal_index
            };

            // Duplicate: reuse the old vertex, add no new one
            if (const auto it = seen.find(key); it != seen.end()) {
                mesh.indices.push_back(it->second);
                continue;
            }
            seen.emplace(key, vertexIndex);
        }

        Vec2f uv{};
        if (idx.texcoord_index != kInvalidIndex) {
            const size_t i = static_cast<size_t>(idx.texcoord_index) * 2;
            if (i + 1 < attribs.texcoords.size()) {
                uv = { attribs.texcoords[i], attribs.texcoords[i + 1] };
            }
        }

        Vec3f normal{};
        if (idx.normal_index != kInvalidIndex) {
            const size_t i = static_cast<size_t>(idx.normal_index) * 3;
            if (i + 2 < attribs.normals.size()) {
                normal = { attribs.normals[i], attribs.normals[i + 1], attribs.normals[i + 2] };
            }
        }

        // New vertex: its index is its position in mesh.vertices
        mesh.indices.push_back(vertexIndex);
        mesh.vertices.emplace_back(
            Vec4f{
                attribs.vertices[pos],
                attribs.vertices[pos + 1] - kDefaultVertexW / 2.0f,
                attribs.vertices[pos + 2],
                kDefaultVertexW
            },
            uv,
            normal
        );
    }

    if (!mesh.vertices.empty()) {
        Vec3f minBounds{
            mesh.vertices[0].position[0],
            mesh.vertices[0].position[1],
            mesh.vertices[0].position[2]
        };

        Vec3f maxBounds = minBounds;

        for (const auto& vertex : mesh.vertices) {
            for (int axis = 0; axis < 3; ++axis) {
                minBounds[axis] = std::min(minBounds[axis], vertex.position[axis]);
                maxBounds[axis] = std::max(maxBounds[axis], vertex.position[axis]);
            }
        }

        Vec3f center = (minBounds + maxBounds) * 0.5f;
        Vec3f size = maxBounds - minBounds;

        float maxSize = std::max({ size[0], size[1], size[2] });

        if (maxSize > 0.0f) {
            float scale = 2.0f / maxSize;

            for (auto& vertex : mesh.vertices) {
                for (int axis = 0; axis < 3; ++axis) {
                    vertex.position[axis] =
                        (vertex.position[axis] - center[axis]) * scale;
                }
            }
        }
    }

    mesh.colors = GenerateRGBA(mesh.vertices.size(), col.r, col.g, col.b, col.a);
    GenerateNormals(mesh);
    return mesh;
}

// ---------------------------------------------------------------------------
// Window / buffers
// ---------------------------------------------------------------------------

static void SetMultiResWindowIcon(SDL_Window* window) {
    SDL_Surface* baseIcon = SDL_LoadPNG("Assets/Icons/Pumpkin@32.png");
    SDL_Surface* icon16   = SDL_LoadPNG("Assets/Icons/Pumpkin@16.png");
    SDL_Surface* icon64   = SDL_LoadPNG("Assets/Icons/Pumpkin@64.png");

    if (!baseIcon || !icon16 || !icon64) {
        SDL_Log("Couldn't load icons : %s", SDL_GetError());
        // SDL_DestroySurface(nullptr) is a safe no-op; release whatever did load.
        SDL_DestroySurface(baseIcon);
        SDL_DestroySurface(icon16);
        SDL_DestroySurface(icon64);
        return;
    }

    // The alternate images are ref-counted by SDL, so our handles can go right away.
    SDL_AddSurfaceAlternateImage(baseIcon, icon16);
    SDL_DestroySurface(icon16);
    SDL_AddSurfaceAlternateImage(baseIcon, icon64);
    SDL_DestroySurface(icon64);

    if (!SDL_SetWindowIcon(window, baseIcon)) {
        SDL_Log("Error while setting the window icon : %s", SDL_GetError());
    }

    SDL_DestroySurface(baseIcon);
}

static void ResizeBuffers(Renderer& renderer, FrameBuffer& fb, DepthBuffer& db, int width, int height) {
    renderer.Resize(width, height);
    fb.width  = width;
    fb.height = height;
    db.width  = width;
    db.height = height;
    db.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height), 1.0f);
}

static bool WindowNotFocused(SDL_Window* window) {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    return !(flags & SDL_WINDOW_INPUT_FOCUS) || (flags & SDL_WINDOW_MINIMIZED);
}

// ---------------------------------------------------------------------------

int main() {
    const auto fb = std::make_unique<FrameBuffer>();
    const auto db = std::make_unique<DepthBuffer>();

    uint64_t targetFps = kDefaultTargetFps;

    Camera camera;

    auto previousTime = std::chrono::steady_clock::now();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println("SDL could not initialize! SDL Error: {}", SDL_GetError());
        return 1;
    }

    if (!TTF_Init()) {
        std::println(stderr, "SDL_ttf couldn't initialize! SDL Error: {}", SDL_GetError());
        return 1;
    }

    Renderer renderer{};
    {
        SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);
        ResizeBuffers(renderer, *fb, *db, mode->w / 2, mode->h / 2);
        if (mode->refresh_rate != 0.0f) {
            targetFps = kUncappedFps;//static_cast<std::int32_t>(mode->refresh_rate);
        }
        SDL_free(ids);
    }

    constexpr SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_TRANSPARENT;
    SDL_Window* window = SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, flags);
    if (!window) {
        std::println("Could not create window! SDL Error: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SetMultiResWindowIcon(window);

    SDL_Surface* surface = SDL_GetWindowSurface(window);
    surface->format = SDL_PIXELFORMAT_RGBA32;

    // Objects creation
    Mesh cowObj    = LoadMesh("Assets/Models/Spot.obj"), vkRoomObj = LoadMesh("Assets/Models/VikingRoom.obj");
    Mat4f cowModel = Mat4f::Identity(),                  vkRoomModel = Mat4f::Identity();
    vkRoomModel = vkRoomModel.Translate({ 1.0f, 0.0f, 1.0f });
    vkRoomModel.SetRotation(Quaternionf{ -std::sin(std::numbers::pi_v<float> / 2.0f), std::sin(std::numbers::pi_v<float>), 0.0f, 1.0f }.Normalized());
    Texture cowTexture;
    if (!LoadTexture(cowTexture, "Assets/Textures/SpotTexture.png")) {
        std::println("Impossible to load texture.");
    }
    Texture vkRoomAlbedo;
    if (!LoadTexture(vkRoomAlbedo, "Assets/Textures/VikingRoom/Albedo.png")) {
        std::println("Impossible to load texture.");
    }
    Texture vkRoomMetalness;
    if (!LoadTexture(vkRoomMetalness, "Assets/Textures/VikingRoom/Metallic.png")) {
        std::println("Impossible to load texture.");
    }
    Texture vkRoomRoughness;
    if (!LoadTexture(vkRoomRoughness, "Assets/Textures/VikingRoom/Roughness.png")) {
        std::println("Impossible to load texture.");
    }
    Texture vkRoomAmbient;
    if (!LoadTexture(vkRoomAmbient, "Assets/Textures/VikingRoom/AO.png")) {
        std::println("Impossible to load texture.");
    }

    // Font loading / text creation
    TTF_Font* font = nullptr;
    constexpr float fontSize = 14.0f;

    font = TTF_OpenFont("Assets/Fonts/Arial.ttf", fontSize);
    if (font == nullptr) {
        SDL_Log("Couldn't load %g pt font from %s: %s",
            fontSize, "Assets/Fonts/Arial.ttf", SDL_GetError());
    } else {
        std::println("It works bebou");
    }

    Text text(font, "hello world bebou", 50.0f, SDL_Color{ 255, 0, 0, 255 }, 20, 20);
	text.
        SetColor(SDL_Color{ 255, 0, 255, 255 }).
        SetSize(50.0f).
        Build();

    // Creating render passes and scene
    renderer.AddPass<ClearPass>(ToRGBA(0, 0, 0, 0));
    renderer.AddPass<GeometryPass>();
    renderer.AddPass<TextPass>(text);

    std::array scene{
        DrawItem { 
            .mesh = &cowObj,
            .model = &cowModel,
            .albedo = &cowTexture,
            .ambient = 0.5f,
            .metalness = 0.7f,
            .roughness = 0.3f
        },
        DrawItem { 
            .mesh = &vkRoomObj,
            .model = &vkRoomModel,
            .albedo = &vkRoomAlbedo,
            .metalMap = &vkRoomMetalness,
            .roughMap = &vkRoomRoughness,
            .ambientMap = &vkRoomAmbient
        } // nothing to test
    };

    const auto frameTime = std::chrono::nanoseconds(1'000'000'000 / targetFps);
    auto nextFrame = std::chrono::steady_clock::now();

    uint64_t lastFpsUpdate = SDL_GetTicks();
    uint32_t frames = 0;
    bool isRunning = true;

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
                break;

            default:
                break;
            }

            Pumpkin::handleCameraEvent(camera, event);
        }

        if (WindowNotFocused(window)) {
            std::this_thread::sleep_until(nextFrame);
            continue;
        }

        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<float> elapsedTime = currentTime - previousTime;
        float deltaTime = elapsedTime.count();
        previousTime = currentTime;

        Pumpkin::updateCameraMovement(camera, deltaTime);

        fb->pixels = static_cast<uint32_t*>(surface->pixels);
        RenderContext ctx{ *fb, *db, camera, deltaTime, scene };
        renderer.Render(ctx);

        SDL_UpdateWindowSurface(window);

        // FPS counter in the window title, refreshed once per second.
        ++frames;
        const uint64_t now = SDL_GetTicks();
        if (now - lastFpsUpdate >= 1000) {
            SDL_SetWindowTitle(window, std::format("Pumpkin Engine | FPS: {}", frames).c_str());
            frames = 0;
            lastFpsUpdate = now;
        }

        Prof::EndFrame();
        std::this_thread::sleep_until(nextFrame);
    }

    TTF_CloseFont(font);
    TTF_Quit();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
