module;

#include <SDL_FixedIncludes.h>

module Pumpkin.Utils.Events;

import std;

namespace Pumpkin {

    bool MouseClick(void* p, SDL_Event* event, Camera* camera) {
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event->button.button == SDL_BUTTON_LEFT)
                std::println("mouse left");

            if (event->button.button == SDL_BUTTON_RIGHT)
                std::println("mouse right");

            if (event->button.button == SDL_BUTTON_MIDDLE)
                std::println("mouse middle");

            
            return true;
        }

        return false;
    }


    void UpdateCameraMovement(Camera& camera, float deltaTime) {
        const bool* state = SDL_GetKeyboardState(nullptr);

        float forward = 0.0f;
        float right = 0.0f;

        if (state[SDL_Scancode::SDL_SCANCODE_W]) forward += 1.0f;
        if (state[SDL_Scancode::SDL_SCANCODE_S]) forward -= 1.0f;
        if (state[SDL_Scancode::SDL_SCANCODE_D]) right += 1.0f;
        if (state[SDL_Scancode::SDL_SCANCODE_A]) right -= 1.0f;

        camera.Move(forward, right, deltaTime);
    }

    void HandleCameraEvent(Camera& camera, const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_MOTION &&
            (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK)) {
            camera.Rotate(
                static_cast<float>(event.motion.xrel),
                static_cast<float>(event.motion.yrel)
            );
        }
    }
}
