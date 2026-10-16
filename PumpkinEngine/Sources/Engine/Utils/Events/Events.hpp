#pragma once

#include <SDL3/SDL.h>
#include "Renderer/Camera.hpp"

namespace Pumpkin {

    bool mouseClick(void* p, SDL_Event* event, Camera* camera);
    void updateCameraMovement(Camera& camera, float deltaTime);

    void handleCameraEvent(Camera& camera, const SDL_Event& event);
}