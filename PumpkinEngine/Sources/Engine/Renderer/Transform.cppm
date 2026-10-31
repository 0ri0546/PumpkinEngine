/// @file Transform.cppm
/// @brief Module Pumpkin.Renderer.Transform: clip-space to screen-space conversion and near-plane clipping.

export module Pumpkin.Renderer.Transform;

import Pumpkin.Renderer.Shaders;
import Pumpkin.Renderer.Types;

import std;

export namespace Pumpkin {
    /// @brief Perspective divide + viewport for every position, in parallel.
    /// @return false if a vertex is behind the camera (w <= 0).
    ///
    /// Output position is `(pixelX, pixelY, depth01, 1 / w)`: Y is flipped (pixels grow downward) and NDC depth
    /// [-1, 1] is mapped to [0, 1]. Other vertex attributes are copied. Uses AVX2 gathers (8 vertices at a time) with a
    /// scalar tail, in parallel chunks of 16384 vertices.
    ///
    /// @param clip   Clip-space vertices (from VertexShader, after optional near clipping).
    /// @param out    Receives the screen-space vertices (resized to `clip.size()`); cleared on failure.
    /// @param width  Target width in pixels.
    /// @param height Target height in pixels.
    /// @note Requires a CPU with AVX2.
    /// @thread_safety Parallel internally via the engine thread pool.
    [[nodiscard]] bool ToScreen(std::span<const Vertex> clip, std::vector<Vertex>& out, int width, int height);

    /// @brief Clips triangles against the near plane (`z + w >= 0`) and builds compact output buffers.
    ///
    /// Triangles whose three vertices are outside the same frustum plane are dropped. Fully visible triangles are copied;
    /// triangles with one vertex outside become two triangles, with two outside become one smaller triangle. New vertices are
    /// interpolated from the inside vertex toward the outside one (so shared edges are bit-identical and leave no cracks):
    /// position, texcoord and colour are linearly interpolated, the normal is re-normalised. Winding order is preserved.
    /// The output holds the surviving input vertices first (in input order), then the newly created ones.
    ///
    /// @param inputVertices Clip-space vertices.
    /// @param inputIndices  Triangle list over @p inputVertices.
    /// @param inputColors   Per-vertex colours; only `min(inputVertices.size(), inputColors.size())` vertices are considered,
    ///                      triangles referencing a vertex beyond that are dropped.
    /// @param clip          Outcodes and near-plane distances filled by VertexShader() for the same vertices.
    /// @param outputVertices Receives the clipped vertices.
    /// @param outputIndices  Receives the clipped triangle list.
    /// @param outputColors   Receives one colour per output vertex.
    /// @return false (with all outputs cleared) if nothing remains visible.
    /// @warning Not reentrant: scratch arrays are function-local statics shared by all callers.
    /// @thread_safety Parallel internally via the engine thread pool; call from one thread at a time.
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
