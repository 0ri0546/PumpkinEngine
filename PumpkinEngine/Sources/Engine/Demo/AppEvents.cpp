module;

#include <SDL_FixedIncludes.h>

module Pumpkin.Demo.AppEvents;

import Pumpkin.Renderer;
import Pumpkin.Utils;
import Pumpkin.UI;
import Pumpkin.Math.Vec;
import std;

namespace Pumpkin::Demo {
    /// SDL invalidates the window surface on every resize: always re-fetch it through this helper.
    /// The framebuffer is written as 32-bit RGBA, hence the format override.
    SDL_Surface* AcquireWindowSurface(SDL_Window* window) {
        SDL_Surface* surface = SDL_GetWindowSurface(window);
        if (surface != nullptr) {
            surface->format = SDL_PIXELFORMAT_RGBA32;
        }
        return surface;
    }

    namespace {
        void OnWindowResized(AppEventContext& app) {
            app.surface = AcquireWindowSurface(app.window);
            if (app.surface == nullptr) {
                std::println(stderr, "Could not get the resized window surface! SDL Error: {}", SDL_GetError());
                app.isRunning = false;   // the main loop cannot render without a surface
                return;
            }

            // Use the surface size, not the event payload: this is what the framebuffer actually maps to.
            ResizeBuffers(app.renderer, app.fb, app.db, app.surface->w, app.surface->h);

            app.ui.UpdateLayout(app.fb.width, app.fb.height);

            app.camera.aspectRatio =
                static_cast<float>(app.fb.width) / static_cast<float>(app.fb.height);

            app.toonMask.resize(static_cast<std::size_t>(app.fb.width) * app.fb.height);

            app.textPass.ResizeText(app.windowDims[0], app.windowDims[1], app.renderContext);
        }
    }

    void ConnectEvents(EventDispatcher& dispatcher, AppEventContext& app) {
        dispatcher.On(SDL_EVENT_QUIT, [&app](const SDL_Event&) {
            app.isRunning = false;
        });

        dispatcher.On(SDL_EVENT_WINDOW_RESIZED, [&app](const SDL_Event&) {
            OnWindowResized(app);
        });

        // UI input. These handlers do not consume the event, so a click on a button also reaches the
        // camera controller (same behaviour as before). Once UIRenderSystem reports whether it handled
        // the click, return that value here and the camera will ignore clicks on the UI.
        dispatcher.On(SDL_EVENT_MOUSE_MOTION, [&app](const SDL_Event& event) {
            app.ui.HandleMouseMove(
                static_cast<int>(event.motion.x),
                static_cast<int>(event.motion.y));
        });

        dispatcher.On(SDL_EVENT_MOUSE_BUTTON_DOWN, [&app](const SDL_Event& event) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                app.ui.HandleMouseButtonDown(
                    static_cast<int>(event.button.x),
                    static_cast<int>(event.button.y));
            }
        });

        dispatcher.On(SDL_EVENT_MOUSE_BUTTON_UP, [&app](const SDL_Event& event) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                app.ui.HandleMouseButtonUp(
                    static_cast<int>(event.button.x),
                    static_cast<int>(event.button.y));
            }
        });
    }
}
