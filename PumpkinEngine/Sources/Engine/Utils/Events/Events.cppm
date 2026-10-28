module;

#include <DLL/Export.hpp>
#include <SDL_FixedIncludes.h>

export module Pumpkin.Utils.Events;

import Pumpkin.Renderer.Camera;

export namespace Pumpkin {
    bool MouseClick(void* p, SDL_Event* event, Camera* camera);
    void PUMPKIN_API UpdateCameraMovement(Camera& camera, float deltaTime);
    void PUMPKIN_API HandleCameraEvent(Camera& camera, const SDL_Event& event);
}
