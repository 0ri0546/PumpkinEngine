#pragma once

#include <Math/Mat.h>
#include <Math/Vec.h>
#include <cmath>

namespace Pumpkin {

    struct Camera {
        Vec3f pos{ 0.0f, 0.0f, 5.0f };
        Vec3f front{ 0.0f, 0.0f, -1.0f };
        Vec3f up{ 0.0f, 1.0f, 0.0f };
        Vec3f right{ 1.0f, 0.0f, 0.0f };
        const Vec3f worldUp{ 0.0f, 1.0f, 0.0f };

        float yaw = -90.0f;
        float pitch = 0.0f;

        float FOVY = 90.0f;
        float aspectRatio = 16.0f / 9.0f;
        float nearFrame = 0.1f;
        float farFrame = 1000.0f;

        float speed = 5.0f;
        float sensitivity = 0.1f;

        void updateVectors() {
            float radYaw = yaw * (3.14159265f / 180.0f);
            float radPitch = pitch * (3.14159265f / 180.0f);

            Vec3f newFront;
            newFront[0] = std::cos(radYaw) * std::cos(radPitch);
            newFront[1] = std::sin(radPitch);
            newFront[2] = std::sin(radYaw) * std::cos(radPitch);

            front = newFront.Normalized();
            right = Cross(front, worldUp).Normalized();
            up = Cross(front, right).Normalized();
        }

        void rotate(float deltaX, float deltaY) {
            yaw += deltaX * sensitivity;
            pitch -= deltaY * sensitivity;

            if (pitch > 89.0f)
                pitch = 89.0f;

            if (pitch < -89.0f)
                pitch = -89.0f;

            updateVectors();
        }

        void move(float forward, float rightMovement, float deltaTime) {
            const float velocity = speed * deltaTime;

            pos += front * (forward * velocity);
            pos += right * (rightMovement * velocity);
        }

        Mat4f getViewMatrix() const {
            Mat4f mat{};
            return mat.LookAt(pos, pos + front, up);
        }
    };

}