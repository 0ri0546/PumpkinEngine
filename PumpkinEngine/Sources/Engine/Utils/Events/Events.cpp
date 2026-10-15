#include "Events.hpp"
#include <print>

namespace Pumpkin {

    bool mouseClick(void* p, SDL_Event* event, Camera* camera) {
        if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event->button.button == SDL_BUTTON_LEFT)
                std::println("mouse left");

            if (event->button.button == SDL_BUTTON_RIGHT)
                std::println("mouse right");

            if (event->button.button == SDL_BUTTON_MIDDLE)
                std::println("mouse middle");

            return true;
        }

        if (event->type == SDL_EVENT_MOUSE_MOTION) {
            camera->rotate(
                static_cast<float>(event->motion.xrel),
                static_cast<float>(event->motion.yrel)
            );

            return true;
        }

        return false;
    }

    void updateCameraMovement(Camera& camera, float deltaTime) {
        const bool* state = SDL_GetKeyboardState(nullptr);

        float forward = 0.0f;
        float right = 0.0f;

        if (state[SDL_SCANCODE_W]) forward += 1.0f;
        if (state[SDL_SCANCODE_S]) forward -= 1.0f;
        if (state[SDL_SCANCODE_D]) right += 1.0f;
        if (state[SDL_SCANCODE_A]) right -= 1.0f;

        camera.move(forward, right, deltaTime);
    }

    void handleCameraEvent(Camera& camera, const SDL_Event& event) {
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            camera.rotate(
                static_cast<float>(event.motion.xrel),
                static_cast<float>(event.motion.yrel)
            );
        }
    }
}