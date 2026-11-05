/// @file AppEvents.cppm
/// @brief Module Pumpkin.Demo.AppEvents: window and input events wiring on top of Pumpkin.Utils.Events.
///
/// Events.cppm only provides the generic EventDispatcher and the CameraController. What each event
/// *means* for this application is decided here, one subscription per event type, in registration order.

module;

#include <SDL_FixedIncludes.h>

export module Pumpkin.Demo.AppEvents;

import Pumpkin.Renderer;
import Pumpkin.Utils;
import Pumpkin.UI;
import Pumpkin.Math.Vec;
import std;

export namespace Pumpkin::Demo {
    /// @brief Re-fetches the window surface (SDL invalidates it on every resize).
    ///
    /// The framebuffer is written as 32-bit RGBA, hence the format override.
    /// @return The surface, or nullptr on failure (see SDL_GetError()).
    [[nodiscard]] SDL_Surface* AcquireWindowSurface(SDL_Window* window);

    /// Everything the event handlers read or modify. Plain references: it must outlive the dispatcher.
    struct AppEventContext {
        bool&                         isRunning;
        SDL_Window*                   window;
        SDL_Surface*&                 surface;
        Renderer&                     renderer;
        FrameBuffer&                  fb;
        DepthBuffer&                  db;
        Camera&                       camera;
        UI::UIRenderSystem&           ui;
        TextPass&                     textPass;
        RenderContext&                renderContext;
        std::vector<std::uint8_t>&    toonMask;
        const Vec2f&                  windowDims;
    };

    /// @brief Subscribes the application's reactions (quit, resize, UI mouse input) to @p dispatcher.
    ///
    /// @param dispatcher Dispatcher to subscribe to.
    /// @param app        Everything the handlers touch; it must outlive the dispatcher.
    void ConnectEvents(EventDispatcher& dispatcher, AppEventContext& app);
}
