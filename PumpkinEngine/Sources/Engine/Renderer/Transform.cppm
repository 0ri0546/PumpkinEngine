export module Pumpkin.Renderer.Transform;

import Pumpkin.Renderer.Shaders;
import Pumpkin.Renderer.Types;

import std;

export namespace Pumpkin {
    /// @brief Perspective divide + viewport for every position, in parallel.
    /// @return false if a vertex is behind the camera (w <= 0).
    [[nodiscard]] bool ToScreen(std::span<const Vertex> clip, std::vector<Vertex>& out, int width, int height);

    [[nodiscard]] bool ClipNearPlane(
        std::span<const Vertex> inputVertices,
        std::span<const std::uint32_t> inputIndices,
        std::span<const Color> inputColors,
        ClipInfo& clip,
        std::vector<Vertex>& outputVertices,
        std::vector<std::uint32_t>& outputIndices,
        std::vector<Color>& outputColors
    );
}
