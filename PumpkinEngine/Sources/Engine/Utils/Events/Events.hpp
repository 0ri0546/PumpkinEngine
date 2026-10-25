#pragma once

#include <SDL3/SDL.h>

#include <Renderer/Camera.hpp>

namespace Pumpkin {
    bool MouseClick(void* p, SDL_Event* event, Camera* camera);
    void UpdateCameraMovement(Camera& camera, float deltaTime);
    void HandleCameraEvent(Camera& camera, const SDL_Event& event);
}
