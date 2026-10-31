/// @file RenderPass.cppm
/// @brief Module Pumpkin.Renderer.RenderPass: the pass interface and the per-frame data passes operate on.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.RenderPass;

import Pumpkin.Renderer.Camera;
import Pumpkin.Renderer.Types;

import Pumpkin.Math.Mat;

import std;

export namespace Pumpkin {
    /// @brief One object to draw: a mesh, its model matrix and its material (all non-owning, must be non-null).
    /// @note GeometryPass reorders the scene by distance, so do not rely on item positions across frames.
    struct PUMPKIN_API DrawItem {
        /// @brief Geometry to draw.
        const Mesh*     mesh;
        /// @brief Model-to-world matrix.
        const Mat4f*    model;
		/// @brief Surface description and pipeline state.
		const Material* material;
    };

    /// @brief Everything a RenderPass needs for one frame (references only, owned by the caller).
    ///
    /// @thread_safety Passes run sequentially on the render thread; they parallelise internally through the engine thread pool.
    struct PUMPKIN_API RenderContext {
        /// @brief Colour target (`0xAARRGGBB`).
        FrameBuffer&  fb;
        /// @brief Depth target, same size as @c fb.
        DepthBuffer&  db;
        /// @brief Camera of the frame.
        const Camera& camera;
        /// @brief Seconds since the previous frame (used by ParticlePass).
        float         deltaTime{};
        /// @brief Objects to draw. Mutable: GeometryPass sorts it in place.
        std::span<DrawItem> scene;
        /// @brief One byte per pixel, set by the rasterizer where a Toon-shaded fragment was written (consumed by OutlinePass). Must hold at least `width * height` entries.
        std::vector<std::uint8_t>& toonMask; // for tooning, only saves tooned materials data
    };

    /// @brief Interface of one rendering stage (clear, geometry, outline, UI...).
    ///
    /// Passes are owned by a Renderer and executed in insertion order, each wrapped in a profiler scope named after Name().
    class PUMPKIN_API RenderPass {
    public:
        /// @brief Virtual destructor (passes are owned through `std::unique_ptr<RenderPass>`).
        virtual ~RenderPass() = default;
        /// @brief Display name used for profiling. @return A string with static storage duration.
        [[nodiscard]] virtual std::string_view Name() const = 0;
        /// @brief Notifies the pass that the target size changed; override in passes that own size-dependent buffers.
        /// @param w New width in pixels.
        /// @param h New height in pixels.
        virtual void OnResize(int /*w*/, int /*h*/) {}   // for passes that own buffers
        /// @brief Runs the pass for the current frame.
        /// @param ctx Frame data; the pass reads/writes the frame buffer, depth buffer and scene as documented by each pass.
        virtual void Execute(RenderContext& ctx) = 0;
    };
}
