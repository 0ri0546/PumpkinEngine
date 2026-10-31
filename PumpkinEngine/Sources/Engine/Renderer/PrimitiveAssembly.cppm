/// @file PrimitiveAssembly.cppm
/// @brief Module Pumpkin.Renderer.PrimitiveAssembly: stage 2 of the pipeline, builds rasterizer-ready triangles.

export module Pumpkin.Renderer.PrimitiveAssembly;

import Pumpkin.Renderer.Types;

import std;

export namespace Pumpkin {
    /// @brief Number of primitives per chunk of a PrimitiveBatch.
    ///
    /// A primitive id encodes its location: `id = chunk * kSetupChunk + indexInChunk` (see kChunkShift).
    inline constexpr std::size_t kSetupChunk = 512;
    /// @brief `log2(kSetupChunk)`: `id >> kChunkShift` is the chunk number.
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

    /// @brief Parameters of AssemblePrimitives().
    struct AssemblyDesc {
        /// @brief Faces to discard.
        CullMode culling = CullMode::Back;
        /// @brief Target width in pixels (must be > 0); bounding boxes are clamped to it.
        std::uint16_t width = 0;  // screen size, used to clamp the bounding boxes
        /// @brief Target height in pixels (must be > 0); bounding boxes are clamped to it.
        std::uint16_t height = 0;
    };

    /// @brief Primitives are stored in chunks (built in parallel), so nothing is copied afterwards.
    struct PrimitiveBatch {
        std::vector<std::vector<Primitive>> chunks;

        /// @brief Looks up a primitive by id.
        /// @param id Primitive id (`chunk << kChunkShift | indexInChunk`).
        /// @return Reference to the primitive. @pre The id was produced by this batch.
        [[nodiscard]] const Primitive& operator[](std::uint32_t id) const {
            return chunks[id >> kChunkShift][id & (kSetupChunk - 1)];
        }

        /// @brief Total number of primitives over all chunks.
        /// @complexity O(number of chunks).
        [[nodiscard]] std::size_t Size() const {
            std::size_t n = 0;
            for (const auto& c : chunks) n += c.size();
            return n;
        }
    };

    /// @brief Builds the primitives from screen-space positions and indices.
    ///
    /// Culls (back faces, flat or offscreen triangles) and sets up the edge, depth and 1/w planes.
    /// `colors` is only used to tell opaque from transparent triangles; it can be empty (all opaque).
    /// If the input is invalid (indices not a multiple of 3, too few colors), the batch is left empty.
    ///
    /// Pixel centres are at `+0.5`; the bounding box is the set of pixels whose centre can lie in the triangle
    /// (`x1` / `y1` exclusive). Front faces have a positive signed area computed in screen space. A triangle is
    /// flagged transparent when any of its three vertex colours has alpha < 255 (only if @p colors is non-empty).
    ///
    /// @param screen  Screen-space vertices (see ToScreen); `position[3]` holds `1 / w`.
    /// @param indices Triangle list, size must be a multiple of 3.
    /// @param colors  Per-vertex colours (same size as @p screen) or empty.
    /// @param desc    Culling mode and target size.
    /// @param out     Receives the primitives in chunks of kSetupChunk (chunk storage is reused between calls).
    /// @post On invalid input (indices not a multiple of 3, too few colours, empty target) @p out has no chunks.
    /// @thread_safety Chunks are built in parallel on the engine thread pool; call from one thread at a time per @p out.
    void AssemblePrimitives(std::span<const Vertex> screen, std::span<const std::uint32_t> indices,
        std::span<const Color> colors, const AssemblyDesc& desc, PrimitiveBatch& out);
}
