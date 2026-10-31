/// @file Camera.cppm
/// @brief Module Pumpkin.Renderer.Camera: free-fly camera (yaw/pitch) producing the view matrix.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Camera;

import Pumpkin.Renderer.Types;

import Pumpkin.Math.Mat;
import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {

    /// @brief Free-fly camera described by a position and yaw/pitch angles.
    ///
    /// `front`, `right` and `up` are derived from yaw/pitch by UpdateVectors(); change `yaw`/`pitch`
    /// directly only if you then call it (Rotate() does so itself). Right-handed, +Y up, yaw -90 looks down -Z.
    ///
    /// @note `worldUp` is const, which makes Camera copy-constructible but not copy-assignable.
    /// @thread_safety Plain data; not synchronised.
    struct PUMPKIN_API Camera {
        /// @brief World-space position.
        Vec3f pos{ 0.0f, 7.0f, 4.0f };
        /// @brief Unit view direction (derived).
        Vec3f front{ 0.0f, 0.0f, -1.0f };
        /// @brief Unit up vector of the camera (derived).
        Vec3f up{ 0.0f, 1.0f, 0.0f };
        /// @brief Unit right vector of the camera (derived).
        Vec3f right{ 1.0f, 0.0f, 0.0f };

        /// @brief World up axis used to build the camera basis.
        const Vec3f worldUp{ 0.0f, 1.0f, 0.0f };
        /// @brief Projection type.
        /// @note VertexShader always builds a perspective projection; this value only configures the Rasterizer (see FragmentShader()).
        CameraType type = CameraType::Perspective;

        /// @brief Horizontal angle in degrees (-90 looks down -Z).
        float yaw = -90.0f;
        /// @brief Vertical angle in degrees, clamped to [-89, 89] by Rotate().
        float pitch = 0.0f;

        /// @brief Width / height hint. @note The built-in passes derive the aspect ratio from the frame buffer instead.
        float aspectRatio = 16.0f / 9.0f;
        /// @brief Vertical field of view in degrees.
        float FOVY = 60.0f;
        /// @brief Distance of the near plane (> 0).
        float nearFrame = 0.1f;
        /// @brief Distance of the far plane (> nearFrame).
        float farFrame = 100.0f;

        /// @brief Movement speed in world units per second (used by Move()).
        float speed = 5.0f;
        /// @brief Degrees of rotation per unit of mouse delta (used by Rotate()).
        float sensitivity = 0.1f;

        /// @brief Recomputes `front`, `right` and `up` from `yaw` and `pitch`.
        /// @post The three vectors are normalised and mutually orthogonal.
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

        /// @brief Turns the camera from a mouse-style delta.
        /// @param deltaX Horizontal delta; adds `deltaX * sensitivity` degrees to yaw.
        /// @param deltaY Vertical delta; *subtracts* `deltaY * sensitivity` degrees from pitch (screen Y points down).
        /// @post Pitch is clamped to [-89, 89] and the basis vectors are updated.
        void Rotate(float deltaX, float deltaY) {
            yaw += deltaX * sensitivity;
            pitch -= deltaY * sensitivity;

            if (pitch > 89.0f)
                pitch = 89.0f;

            if (pitch < -89.0f)
                pitch = -89.0f;

            UpdateVectors();
        }

        /// @brief Moves the camera along its own axes.
        /// @param forward       Amount along `front` (negative moves backwards); includes the vertical component when pitched.
        /// @param rightMovement Amount along `right` (negative strafes left).
        /// @param deltaTime     Frame time in seconds; the displacement is `amount * speed * deltaTime`.
        void Move(float forward, float rightMovement, float deltaTime) {
            const float velocity = speed * deltaTime;

            pos += front * (forward * velocity);
            pos += right * (rightMovement * velocity);
        }

        /// @brief Builds the world-to-view matrix (look-at from `pos` toward `pos + front`).
        /// @return View matrix using the current `front` and `up`.
        [[nodiscard]] Mat4f GetViewMatrix() const {
            return Mat4f::LookAt(pos, pos + front, up);
        }
    };

}
