/// @file Application.cppm
/// @brief Module Pumpkin.Demo.Application: window, UI, render passes and the main loop.
///
/// Frame flow:
/// @code
///   events (EventDispatcher) -> camera (CameraController) -> scheduler.Run(dt)
///                                                             ECS systems, parallel, then
///                                                             deferred commands applied
///                                                               |
///                                                               v
///                                                             terrain.Update(mvp)   frustum culling of terrain chunks
///                                                               |
///                                                               v
///                                                             ExtractScene(world)   World::View -> std::vector<DrawItem>
///                                                               |
///                                                               v
///                                                             renderer.Render(ctx)  Clear, Skybox, Particles, Geometry,
///                                                                                   Outline, Buttons, Images, Text
/// @endcode

module;

#include <DLL/Export.hpp>

export module Pumpkin.Demo.Application;

export namespace Pumpkin::Demo {
    /// @brief Creates the window, loads everything and runs the main loop until the window is closed.
    ///
    /// SDL and SDL_ttf must already be initialised; the caller shuts them down afterwards. Everything created
    /// here (renderer, UI, ECS world, assets) is destroyed before this function returns.
    ///
    /// @return 0 on success, 1 if a required resource (window surface, UI file) could not be opened.
    int PUMPKIN_API RunApplication();
}
