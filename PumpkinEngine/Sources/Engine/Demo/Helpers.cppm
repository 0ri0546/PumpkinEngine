/// @file Helpers.cppm
/// @brief Module Pumpkin.Demo.Helpers: small free functions used across the demo.

export module Pumpkin.Demo.Helpers;

import Pumpkin.Renderer;
import Pumpkin.Math.Quaternion;
import std;

export namespace Pumpkin::Demo {
    /// @brief Degrees to radians.
    constexpr float ToRadians(float degrees) {
        return degrees * (std::numbers::pi_v<float> / 180.0f);
    }

    /// @brief Worker threads for the ECS scheduler.
    ///
    /// The rasterizer has its own thread pool: this stays small to avoid oversubscription.
    [[nodiscard]] std::size_t EcsWorkerCount();

    /// @brief Human-readable name of a shader, for logs and the window title.
    /// @return A string literal; "other" for shaders the demo does not list.
    [[nodiscard]] const char* ShaderName(ShaderId id);

    /// @brief Rotation of @p yaw radians around world Y applied after @p pitch radians around X.
    ///
    /// Same convention as the original VikingRoom setup: pitch = -90 degrees lays the model down.
    [[nodiscard]] Quaternionf MakeYawPitch(float yaw, float pitch);

    /// @brief Loads a texture and prints a warning when it fails.
    /// @param texture Receives the image.
    /// @param path    File to load.
    void LoadTextureOrWarn(Texture& texture, const char* path);
}
