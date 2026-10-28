module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Camera;

import Pumpkin.Renderer.Types;

import Pumpkin.Math.Mat;
import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {

    struct PUMPKIN_API Camera {
        Vec3f pos{ 0.0f, 7.0f, 4.0f };
        Vec3f front{ 0.0f, 0.0f, -1.0f };
        Vec3f up{ 0.0f, 1.0f, 0.0f };
        Vec3f right{ 1.0f, 0.0f, 0.0f };

        const Vec3f worldUp{ 0.0f, 1.0f, 0.0f };
        CameraType type = CameraType::Perspective;

        float yaw = -90.0f;
        float pitch = 0.0f;

        float aspectRatio = 16.0f / 9.0f;
        float FOVY = 60.0f;
        float nearFrame = 0.1f;
        float farFrame = 100.0f;

        float speed = 5.0f;
        float sensitivity = 0.1f;

        void UpdateVectors() {
            const float radYaw = yaw * (std::numbers::pi_v<float> / 180.0f);
            const float radPitch = pitch * (std::numbers::pi_v<float> / 180.0f);

            Vec3f newFront;
            newFront[0] = std::cos(radYaw) * std::cos(radPitch);
            newFront[1] = std::sin(radPitch);
            newFront[2] = std::sin(radYaw) * std::cos(radPitch);

            front = newFront.Normalized();
            right = Cross(front, worldUp).Normalized();
            up = Cross(right, front).Normalized();
        }

        void Rotate(float deltaX, float deltaY) {
            yaw += deltaX * sensitivity;
            pitch -= deltaY * sensitivity;

            if (pitch > 89.0f)
                pitch = 89.0f;

            if (pitch < -89.0f)
                pitch = -89.0f;

            UpdateVectors();
        }

        void Move(float forward, float rightMovement, float deltaTime) {
            const float velocity = speed * deltaTime;

            pos += front * (forward * velocity);
            pos += right * (rightMovement * velocity);
        }

        [[nodiscard]] Mat4f GetViewMatrix() const {
            return Mat4f::LookAt(pos, pos + front, up);
        }
    };

}
