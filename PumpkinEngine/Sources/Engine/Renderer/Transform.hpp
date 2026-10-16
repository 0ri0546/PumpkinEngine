#pragma once

#include <Math/Mat.h>
#include <Renderer/Types.hpp>

#include <span>
#include <vector>

/// Temporary file for rotations of meshes

namespace Pumpkin {
    /// @brief Rotation + offset: out = m * p + t.
    struct Mat3x4 {
        float m[3][3];
        float t[3];
    };

    /// @brief Folds the rotations (X, then Y, then Z, around the pivot) and the z scale into one Mat3x4.
    [[nodiscard]] Mat3x4 BuildTransform(float angleX, float angleY, float angleZ,
        float pivotX, float pivotY, float pivotZ, float zScale);

    /// @brief Rotation + perspective divide + viewport for every position, in parallel.
    /// @return false if a vertex is behind the camera (w <= 0).
    [[nodiscard]] bool TransformToScreen(std::span<const Vec4f> src, std::vector<Vec4f>& out,
        const Mat3x4& M, int width, int height);
}
