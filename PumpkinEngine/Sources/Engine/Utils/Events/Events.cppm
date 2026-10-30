module;

#include <DLL/Export.hpp>
#include <SDL_FixedIncludes.h>

export module Pumpkin.Utils.Events;

import Pumpkin.Renderer.Camera;
import Pumpkin.Renderer.Renderer;
import Pumpkin.Renderer.Types;
import Pumpkin.Renderer.Passes.Text;
import Pumpkin.UI.Systems.Render;
import std;

export namespace Pumpkin {

    bool MouseClick(void* p, SDL_Event* event, Camera* camera);

    void PUMPKIN_API UpdateCameraMovement(
        Camera& camera,
        float deltaTime
    );

    void PUMPKIN_API HandleCameraEvent(
        Camera& camera,
        const SDL_Event& event
    );

    template <
        typename RendererType,
        typename FrameBufferType,
        typename DepthBufferType,
        typename UIRendererType,
        typename TextPassType,
        typename RenderContextType,
        typename ToonType,
        typename WindowDimsType
    >
    void HandleEvents(
        bool& isRunning,
        SDL_Window* window,
        SDL_Surface*& surface,
        RendererType& renderer,
        FrameBufferType& fb,
        DepthBufferType& db,
        Camera& camera,
        UIRendererType& uiRenderer,
        TextPassType& textPass,
        RenderContextType& ctx,
        ToonType& toon,
        WindowDimsType& windowDims
    ) {
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

                ResizeBuffers(
                    renderer,
                    *fb,
                    *db,
                    event.window.data1,
                    event.window.data2
                );

                uiRenderer.UpdateLayout(fb->width, fb->height);

                camera.aspectRatio =
                    static_cast<float>(fb->width) /
                    static_cast<float>(fb->height);

                toon.resize(
                    static_cast<std::size_t>(fb->width) * fb->height
                );

                textPass.ResizeText(
                    windowDims[0],
                    windowDims[1],
                    ctx
                );
                break;

            case SDL_EVENT_MOUSE_MOTION:
                uiRenderer.HandleMouseMove(
                    static_cast<int>(event.motion.x),
                    static_cast<int>(event.motion.y)
                );
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    uiRenderer.HandleMouseButtonDown(
                        static_cast<int>(event.button.x),
                        static_cast<int>(event.button.y)
                    );
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    uiRenderer.HandleMouseButtonUp(
                        static_cast<int>(event.button.x),
                        static_cast<int>(event.button.y)
                    );
                }
                break;

            default:
                break;
            }

            HandleCameraEvent(camera, event);
        }
    }
}