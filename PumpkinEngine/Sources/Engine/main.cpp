#include <Math/Mat.h>
#include <Renderer/Rasterizer.hpp>
#include <Renderer/Shaders.hpp>
#include <Renderer/Transform.hpp>
#include <Renderer/Types.hpp>
#include <Utils/ObjParser.hpp>
#include <Utils/Utils.hpp>

#include <SDL3/SDL.h>

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

using namespace Pumpkin;

constexpr uint64_t kDefaultTargetFps = 120;
constexpr uint64_t kUncappedFps      = 9'999'999;
constexpr float    kDefaultVertexW   = 110.0f;
constexpr uint32_t kInvalidIndex     = static_cast<uint32_t>(-1);

// ---------------------------------------------------------------------------
// Mesh loading / drawing
// ---------------------------------------------------------------------------

static Mesh LoadMesh(const std::filesystem::path& filepath, bool deduplicate = true) {
    if (filepath.extension().string() != ".obj") return Mesh{};

    // triangle test when debugging
    static constexpr bool triangle = false;
    if constexpr (triangle) {
        Mesh triangleMesh{};
        triangleMesh.vertices = {
            {{ 0.0f, 1.0f, 0.5f, 1.0f },{},{}},
            {{ -1.0f, -1.0f, 0.5f, 1.0f },{},{}},
            {{ 1.0f, -1.0f, 0.5f, 1.0f },{},{}}
        };
        triangleMesh.colors = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}};
        triangleMesh.indices = {0, 1, 2};
        return triangleMesh;
    }

    const auto [attribs, objIndices] = LoadObj(filepath);

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

    mesh.colors = GenerateRGBA(mesh.vertices.size(), 255, 127, 0, 107);
    return mesh;
}

// Pipeline: input -> clip -> screen -> pixel shader
// (the pixel shader calls the rasterizer, then shades during rasterization)
static void DrawMesh(Mesh& mesh, const FrameBuffer& fb, DepthBuffer& db) {
    static const std::vector<Vertex> originalVertices = mesh.vertices;
    static const std::vector<Vec4f> originalPositions = [&] {
        std::vector<Vec4f> p;
        p.reserve(originalVertices.size());
        for (const auto& v : originalVertices) p.push_back(v.position);
        return p;
    }();
    static constexpr float speed = 1.0f;

    const auto t = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();

    // Toggle
    bool rotateX = false;
    bool rotateY = true;
    bool rotateZ = false;

    // Rotation center. Your cube spans z 0..1, so its center is z = 0.5.
    constexpr float pivotX = 0.0f, pivotY = -1.0f, pivotZ = 0.5f;
    constexpr float zScale = 0.5f / 0.8660254f;

    const float angle = t * speed;

    // Angle is 0 for a disabled axis, which makes that rotation a no-op.
    const float angleX = rotateX ? angle : 0.0f;
    const float angleY = rotateY ? angle : 0.0f;
    const float angleZ = rotateZ ? angle : 0.0f;

    const Mat3x4 M = BuildTransform(angleX, angleY, angleZ, pivotX, pivotY, pivotZ, zScale);

    // no-op at the moment, cameras + mvp coming later
    //const std::vector<Vertex> clip = VertexShader(mesh.vertices);

    static std::vector<Vec4f> screen;
    if (!TransformToScreen(originalPositions, screen, M, fb.width, fb.height)) return;

    static Rasterizer rasterizer{
        .ClearColor = 0x00000000u,
        .Culling = CullMode::Back,
        .DepthTest = DepthTestMode::Less,
        .AlphaBlend = BlendMode::Alpha,
        .Projection = CameraType::Orthographic
    };
    rasterizer.RasterizeFrame(screen, mesh.indices, mesh.colors, fb, db);
    // Pixel shader incoming
}

// ---------------------------------------------------------------------------
// Window / buffers
// ---------------------------------------------------------------------------

static void SetMultiResWindowIcon(SDL_Window* window) {
    SDL_Surface* baseIcon = SDL_LoadPNG("Assets/Pumpkin@32.png");
    SDL_Surface* icon16   = SDL_LoadPNG("Assets/Pumpkin@16.png");
    SDL_Surface* icon64   = SDL_LoadPNG("Assets/Pumpkin@64.png");

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

static void ResizeBuffers(FrameBuffer& fb, DepthBuffer& db, int width, int height) {
    fb.width  = width;
    fb.height = height;
    db.width  = width;
    db.height = height;
    db.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height), 1.0f);
}

// ---------------------------------------------------------------------------

int main() {
    const auto fb = std::make_unique<FrameBuffer>();
    const auto db = std::make_unique<DepthBuffer>();
    uint64_t targetFps = kDefaultTargetFps;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::println("SDL could not initialize! SDL Error: {}", SDL_GetError());
        return 1;
    }

    {
        SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);
        ResizeBuffers(*fb, *db, mode->h / 2, mode->h / 2);
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

    Mesh pumpkin = LoadMesh("Assets/Pumpkin.obj");

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
                ResizeBuffers(*fb, *db, event.window.data1, event.window.data2);
                break;

            default:
                break;
            }
        }

        fb->pixels = static_cast<uint32_t*>(surface->pixels);

        DrawMesh(pumpkin, *fb, *db);
        SDL_UpdateWindowSurface(window);

        // FPS counter in the window title, refreshed once per second.
        ++frames;
        const uint64_t now = SDL_GetTicks();
        if (now - lastFpsUpdate >= 1000) {
            SDL_SetWindowTitle(window, std::format("Pumpkin Engine | FPS: {}", frames).c_str());
            frames = 0;
            lastFpsUpdate = now;
        }

        std::this_thread::sleep_until(nextFrame);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
