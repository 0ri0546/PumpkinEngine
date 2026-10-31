/// @file Shaders.cppm
/// @brief Module Pumpkin.Renderer.Shaders: vertex stage (model -> clip space with outcodes), clip scratch buffers and the fragment-stage entry point.

export module Pumpkin.Renderer.Shaders;

import Pumpkin.Renderer.Camera;
import Pumpkin.Renderer.PrimitiveAssembly;
import Pumpkin.Renderer.Fragment;
import Pumpkin.Renderer.Rasterizer;
import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Texture;
import Pumpkin.Renderer.Types;

import Pumpkin.Math.Mat;
import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {
    /// @brief Frustum summary of a whole mesh produced by VertexShader().
    ///
    /// `andCode` holds the outcode bits (6 low bits) set for *every* vertex: non-zero means the whole mesh
    /// lies outside one frustum plane and can be skipped. `orCode` holds the bits set for *any* vertex; if it
    /// contains kNearBit, the mesh needs near-plane clipping.
    struct ClipFlags { std::uint32_t andCode = 0x3F; std::uint32_t orCode = 0; };

    /// @brief Outcode bit meaning "this vertex is behind the near plane (z < -w)"; only ever set in orCode / per-vertex codes, never in andCode.
    inline constexpr std::uint32_t kNearBit = 1u << 6;

    /// @brief Classifies a clip-space point against the six frustum planes.
    ///
    /// Bits: 1 / 2 = x < -w / x > w, 4 / 8 = y < -w / y > w, 16 / 32 = z < -w / z > w, 64 (kNearBit) = z < -w again, as the
    /// "needs near clipping" flag. A point inside the frustum returns 0.
    ///
    /// @param p Clip-space position `(x, y, z, w)`.
    /// @return Bit mask of violated planes.
    [[nodiscard]] inline std::uint32_t Outcode(const Vec4f& p) {
        const float w = p[3];
        return (p[0] < -w ?  1u : 0u) | (p[0] > w ?  2u : 0u)
             | (p[1] < -w ?  4u : 0u) | (p[1] > w ?  8u : 0u)
             | (p[2] < -w ? 16u : 0u) | (p[2] > w ? 32u : 0u)
             | (p[2] < -w ? kNearBit : 0u);   // "needs near clip"
    }

    /// @brief Fixed-type scratch buffer that is never zero-filled on growth.
    ///
    /// Only for data that is always written before it is read (it uses `std::make_unique_for_overwrite`).
    ///
    /// @tparam T Element type (trivially constructible).
    /// @warning Reserve() discards the previous contents when it has to grow.
    /// @thread_safety Not synchronised; distinct elements may be written from different threads.
    template <class T>
    class ScratchArray {
    public:
        /// @brief Ensures capacity for @p n elements.
        /// @param n Required number of elements. Does nothing if already large enough.
        /// @warning When growing, a new uninitialised buffer replaces the old one and existing values are lost.
        void Reserve(std::size_t n) {
            if (n <= capacity_) return;
            data_ = std::make_unique_for_overwrite<T[]>(n);
            capacity_ = n;
        }
        /// @brief Unchecked element access. @param i Index, must be below the reserved capacity. @return Reference to the element.
        T& operator[](std::size_t i) { return data_[i]; }
        /// @brief Unchecked read-only element access. @param i Index, must be below the reserved capacity. @return Const reference to the element.
        const T& operator[](std::size_t i) const { return data_[i]; }
        /// @brief Raw pointer to the storage (nullptr before the first Reserve()).
        T* data() { return data_.get(); }
        /// @brief Read-only raw pointer to the storage (nullptr before the first Reserve()).
        [[nodiscard]] const T* data() const { return data_.get(); }

    private:
        std::unique_ptr<T[]> data_;
        std::size_t capacity_ = 0;
    };

    /// @brief Per-vertex clipping data written by VertexShader() and read by ClipNearPlane().
    ///
    /// Both arrays are indexed by vertex and sized to the mesh by VertexShader().
    struct ClipInfo {
        ScratchArray<std::uint8_t> codes; // frustum outcode per vertex
        ScratchArray<float> dist;         // z + w, signed distance to the near plane
    };

    /// @brief Vertex stage: transforms every vertex to clip space and classifies it against the frustum.
    ///
    /// For each vertex: optional VertexEffect displacement (model space), then `position = projection * view * model * position`,
    /// normal transformed by `view * model` and re-normalised (view space), then the frustum outcode and the signed
    /// near-plane distance (`z + w`) are stored in @p info. Large meshes are processed in parallel chunks of 4096 vertices.
    ///
    /// @param vertices          Model-space vertices.
    /// @param out               Receives the clip-space vertices (resized to `vertices.size()`).
    /// @param info              Receives per-vertex outcodes and near-plane distances (buffers grown as needed).
    /// @param model             Model-to-world matrix.
    /// @param camera            Camera providing view matrix, FOVY, near and far planes.
    /// @param aspectRatio       Width / height of the target.
    /// @param animatedWater     Whether or not the water has a shader effect.
    /// @param animatedWaterWave Whether or not the water has an animated displacement.
    /// @return Mesh-wide AND / OR of the vertex outcodes.
    /// @note The projection is always perspective, and normals use the view-model matrix directly (no inverse-transpose),
    ///       which is exact only for rotation + uniform scale.
    /// @thread_safety Parallel internally via the engine thread pool; one call at a time per @p info / @p out.
    ClipFlags VertexShader(
        std::span<const Vertex> vertices,
        std::vector<Vertex>& out,
        ClipInfo& info,
        const Mat4f& model,
        const Camera& camera,
        float aspectRatio,
        bool animatedWater = false,
        bool animatedWaterWave = false);

    /// @brief Fragment stage entry point: rasterizes @p batch with @p shader.
    ///
    /// Forwards to a Rasterizer with bilinear filtering enabled.
    ///
    /// @tparam Shader Fragment shader type (see FragmentShader_T).
    /// @param batch  Assembled primitives.
    /// @param ctx    Frame data.
    /// @param shader Shader invoked per pixel.
    /// @param item   Draw item (material).
    /// @warning The Rasterizer is a function-local `static` per Shader type, configured once from the
    ///          `ctx.camera.type` seen on the first call: later changes of the camera type are ignored.
    /// @see Rasterizer::Rasterize
    template <FragmentShader_T Shader>
    void FragmentShader(
        PrimitiveBatch& batch,
        RenderContext& ctx,
        Shader& shader,
        const DrawItem& item
    ) {
        static const Rasterizer rasterizer{
            .Projection = ctx.camera.type,
            .BilinearFiltering = true
        };
        rasterizer.Rasterize(batch, ctx, shader, item);
    }
}
