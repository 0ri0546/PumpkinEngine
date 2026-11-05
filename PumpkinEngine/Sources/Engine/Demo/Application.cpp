module;

#include <SDL_FixedIncludes.h>

module Pumpkin.Demo.Application;

import Pumpkin.Demo.AppEvents;
import Pumpkin.Demo.Assets;
import Pumpkin.Demo.Components;
import Pumpkin.Demo.Config;
import Pumpkin.Demo.Diagnostics;
import Pumpkin.Demo.Helpers;
import Pumpkin.Demo.ParticlePresets;
import Pumpkin.Demo.SceneExtraction;
import Pumpkin.Demo.Systems;
import Pumpkin.Demo.WorldSetup;

import Pumpkin.ECS;
import Pumpkin.Renderer;
import Pumpkin.Utils;
import Pumpkin.UI;

import Pumpkin.Math.Mat;
import Pumpkin.Math.Quaternion;
import Pumpkin.Math.Vec;

import std;

using namespace Pumpkin;
using namespace Pumpkin::ECS;
using namespace Pumpkin::Demo::Comp;

namespace Pumpkin::Demo {
    int RunApplication() {
        const auto fb = std::make_unique<FrameBuffer>();
        const auto db = std::make_unique<DepthBuffer>();

        std::uint64_t targetFps = kDefaultTargetFps;
        Camera camera;
        auto previousTime = std::chrono::steady_clock::now();

        // --- Window and renderer -------------------------------------------------
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

        constexpr SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE;

        SDL_Window* window =
            SDL_CreateWindow("Pumpkin Engine", fb->width, fb->height, windowFlags);

        if (!window) {
            std::println(stderr, "Could not create window! SDL Error: {}", SDL_GetError());
            return 0;
        }

        SetMultiResWindowIcon(window);

        SDL_Surface* surface = AcquireWindowSurface(window);
        if (surface == nullptr) {
            std::println(stderr, "Could not get the window surface! SDL Error: {}", SDL_GetError());
            SDL_DestroyWindow(window);
            return 1;
        }

        // --- Assets, terrain -----------------------------------------------------
        // Declaration order matters for destruction (reverse order):
        //   scheduler -> world -> terrain -> materials -> assets.
        // Components only hold non-owning pointers to the objects declared before them.
        const TerrainConfig terrainConfig = MakeTerrainConfig();

        Assets assets;
        LoadAssets(assets, terrainConfig);
        const Materials materials = MakeMaterials(assets);

        TerrainBatcher terrain{ GenerateTerrainChunks(terrainConfig, 16) };

        // --- ECS: world, entities, systems ----------------------------------------
        World world;
        const SceneHandles handles = PopulateWorld(world, assets, materials, terrain, terrainConfig);

        Scheduler scheduler{ world, EcsWorkerCount() };
        RegisterSystems(scheduler);

        std::println("[ECS] {} worker thread(s); system graph:\n{}",
                     scheduler.ThreadCount(), scheduler.DescribeGraph());

        // --- Particles -------------------------------------------------------------
        auto particleSystem  = std::make_shared<ParticleSystem>(MakeSnowParticles());
        auto particleSystem2 = std::make_shared<ParticleSystem>(MakeFountainParticles());

        // --- UI ----------------------------------------------------------------------
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

        // --- Render passes -----------------------------------------------------------
        renderer.AddPass<ClearPass>(ToRGBA(0, 0, 0, 0));
        renderer.AddPass<SkyboxPass>(assets.skybox);
        renderer.AddPass<ParticlePass>(particleSystem, particleSystem2);
        renderer.AddPass<GeometryPass>();
        renderer.AddPass<OutlinePass>(0.03f);
        renderer.AddPass<ButtonPass>(uiRenderer.Buttons());
        renderer.AddPass<ImagePass>(uiRenderer.Images());
        auto& textPass = renderer.AddPass<TextPass>(uiRenderer.Texts());

        // --- Main loop -----------------------------------------------------------------
        const auto frameTime = std::chrono::nanoseconds(1'000'000'000 / targetFps);

        auto nextFrame = std::chrono::steady_clock::now();
        std::uint64_t lastFpsUpdate = SDL_GetTicks();
        std::uint32_t frames = 0;
        std::uint32_t secondsElapsed = 0;
        bool isRunning = true;

        std::vector<std::uint8_t> toon(static_cast<std::size_t>(fb->width) * fb->height);

        // Rebuilt every frame from the ECS. GeometryPass sorts it in place, which is fine.
        std::vector<DrawItem> scene;
        scene.reserve(kReservedEntities);

        Vec2f windowDims{};
        {
            SDL_DisplayID* ids = SDL_GetDisplays(nullptr);
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(*ids);
            windowDims = { static_cast<float>(mode->w), static_cast<float>(mode->h) };
            SDL_free(ids);
        }

        RenderSettings settings{};

        // The render context is created once: the resize handler needs it between frames.
        // Only `deltaTime` and `scene` change from one frame to the next.
        RenderContext ctx{
            .fb = *fb,
            .db = *db,
            .camera = camera,
            .deltaTime = 0.0f,
            .scene = {},
            .toonMask = toon,
            .settings = settings,
        };

        // Events. Declaration order matters (reverse destruction): the dispatcher must outlive the
        // camera controller, and `appEvents` must outlive the dispatcher's handlers.
        AppEventContext appEvents{
            .isRunning = isRunning,
            .window = window,
            .surface = surface,
            .renderer = renderer,
            .fb = *fb,
            .db = *db,
            .camera = camera,
            .ui = uiRenderer,
            .textPass = textPass,
            .renderContext = ctx,
            .toonMask = toon,
            .windowDims = windowDims
        };

        EventDispatcher dispatcher;
        ConnectEvents(dispatcher, appEvents);          // UI first: it sees events before the camera

        CameraController cameraController{ camera };   // declared after the dispatcher: destroyed before it
        cameraController.Attach(dispatcher);

        uiRenderer.AddCallback("PAUSE (ECS)", [&settings] { settings.paused   = !settings.paused;   });
        uiRenderer.AddCallback("SKYBOX",      [&settings] { settings.skybox   = !settings.skybox;   });
        uiRenderer.AddCallback("PARTICLES",   [&settings] { settings.particle = !settings.particle; });
        uiRenderer.AddCallback("GEOMETRY",    [&settings] { settings.geometry = !settings.geometry; });
        uiRenderer.AddCallback("OUTLINE",     [&settings] { settings.outline  = !settings.outline;  });
        uiRenderer.AddCallback("IMAGE",       [&settings] { settings.image    = !settings.image;    });
        uiRenderer.AddCallback("BILINEAR",    [&settings] { settings.bilinear = !settings.bilinear; });

        while (isRunning) {
            nextFrame += frameTime;

            const auto currentTime = std::chrono::steady_clock::now();
            const std::chrono::duration<float> elapsedTime = currentTime - previousTime;

            const float deltaTime = elapsedTime.count();
            previousTime = currentTime;

            // The pointers of last frame's items may dangle (pools can reallocate when commands
            // are applied): event handlers must not see them. ExtractScene() refills the list below.
            scene.clear();
            ctx.scene = {};
            ctx.deltaTime = deltaTime;

            dispatcher.PumpEvents();

            if (WindowNotFocused(window)) {
                std::this_thread::sleep_until(nextFrame);
                continue;
            }

            cameraController.Update(deltaTime);

            // 1. Simulation: every system, in parallel where Access allows, then the command
            //    buffers (spawn / destroy / add / remove) are applied single-threaded.
            //    A throwing system is reported but must not kill the frame.
            try {
                if (!settings.paused)
                    scheduler.Run(std::min(deltaTime, kMaxEcsStep));
            } catch (const std::exception& error) {
                std::println(stderr, "[ECS] system failure: {}", error.what());
            }

            // 2. Terrain culling, using the terrain's own model matrix from the ECS.
            fb->pixels = static_cast<std::uint32_t*>(surface->pixels);

            const float fovRadians = ToRadians(camera.FOVY);
            const Mat4f view = camera.GetViewMatrix();
            const Mat4f projection = Mat4f::Perspective(
                fovRadians,
                camera.aspectRatio,
                camera.nearFrame,
                camera.farFrame
            );

            const Mat4f& terrainModel = world.Get<ModelMatrix>(handles.terrainInside).value;
            terrain.Update(projection * (view * terrainModel));

            // 3. ECS -> renderer. The vector may have reallocated: refresh the span.
            ExtractScene(world, scene);
            ctx.scene = scene;

            // 4. Render.
            renderer.Render(ctx);
            SDL_UpdateWindowSurface(window);

            // 5. Once per second: FPS in the title, ECS report, direct world edit.
            ++frames;
            const std::uint64_t now = SDL_GetTicks();

            if (now - lastFpsUpdate >= 1000) {
                SDL_SetWindowTitle(
                    window,
                    std::format("Pumpkin Engine | FPS: {} | Avg: {:.3f}ms | Cow shader: {} | Entities: {}",
                                frames,
                                1000.0f / static_cast<float>(frames),
                                CowShaderName(world, handles.cow),
                                scene.size()).c_str()
                );

                ++secondsElapsed;
                ReportWorld(world, handles);
                if (secondsElapsed % kBobToggleSeconds == 0) {
                    ToggleCowBob(world, handles.cow);
                }

                frames = 0;
                lastFpsUpdate = now;
            }

            Prof::EndFrame();
            std::this_thread::sleep_until(nextFrame);
        }

        // Destroy the window while SDL is still initialized.
        // (Scheduler, World, terrain, materials and assets are destroyed after this, in that order.)
        SDL_DestroyWindow(window);
        return 0;
    }
}
