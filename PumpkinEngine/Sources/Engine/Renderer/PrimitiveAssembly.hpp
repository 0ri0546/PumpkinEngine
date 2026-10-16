#pragma once

#include <Renderer/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Pumpkin {
    // A primitive id is chunk * kSetupChunk + index in the chunk.
    inline constexpr std::size_t kSetupChunk = 512;
    inline constexpr std::uint32_t kChunkShift = 9;
    static_assert(kSetupChunk == (1u << kChunkShift));

    /// @brief One assembled screen-space triangle: everything the rasterizer needs.
    struct Primitive {
        int x0, y0, x1, y1;        // pixel bounding box, clamped to the screen (x1 and y1 excluded)
        Plane w0, w1, w2;          // screen-space barycentric weights, inside = all >= 0
        Plane z;                   // depth
        Plane k;                   // 1 / w, interpolated (for perspective-correct attributes)
        Plane u, v;                // UV
        float kv[3];               // 1 / w of each vertex
        std::uint32_t i0, i1, i2;  // vertex indices, handed to the shader
        float zKey;                // depth sort key
        bool transparent;
    };

    struct AssemblyDesc {
        CullMode culling = CullMode::Back;
        int width = 0;  // screen size, used to clamp the bounding boxes
        int height = 0;
    };

    /// @brief Primitives are stored in chunks (built in parallel), so nothing is copied afterwards.
    struct PrimitiveBatch {
        std::vector<std::vector<Primitive>> chunks;

        [[nodiscard]] const Primitive& operator[](std::uint32_t id) const {
            return chunks[id >> kChunkShift][id & (kSetupChunk - 1)];
        }

        [[nodiscard]] std::size_t Size() const {
            std::size_t n = 0;
            for (const auto& c : chunks) n += c.size();
            return n;
        }
    };

    /// @brief Stage 2: builds the primitives from screen-space positions and indices.
    ///
    /// Culls (back faces, flat or offscreen triangles) and sets up the edge, depth and 1/w planes.
    /// `colors` is only used to tell opaque from transparent triangles; it can be empty (all opaque).
    /// If the input is invalid (indices not a multiple of 3, too few colors), the batch is left empty.
    void AssemblePrimitives(std::span<const Vec4f> screen, std::span<const std::uint32_t> indices,
        std::span<const Color> colors, std::span<const Vec2f> tex, const AssemblyDesc& desc, PrimitiveBatch& out);
}
