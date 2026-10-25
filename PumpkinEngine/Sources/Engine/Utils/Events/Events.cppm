module;

#include <SDL3/SDL.h>

export module Pumpkin.Utils.Events;

import Pumpkin.Renderer.Camera;

export namespace Pumpkin {
    bool MouseClick(void* p, SDL_Event* event, Camera* camera);
    void UpdateCameraMovement(Camera& camera, float deltaTime);
    void HandleCameraEvent(Camera& camera, const SDL_Event& event);
}
