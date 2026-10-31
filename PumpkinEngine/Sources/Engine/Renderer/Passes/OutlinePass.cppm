/// @file OutlinePass.cppm
/// @brief Module Pumpkin.Renderer.Passes.Outline: depth-based ink outline for toon-shaded pixels.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Outline;

import Pumpkin.Renderer.RenderPass;

import std;

export namespace Pumpkin {
    /// @brief Post-process that paints black outlines where the linearised depth changes abruptly around a toon pixel.
    ///
    /// For each interior pixel flagged in RenderContext::toonMask, the linear view-space depth of its four neighbours is compared:
    /// `|right - left| + |down - up| > threshold * depth` turns the pixel opaque black. Pixels next to the background
    /// (depth >= 1) are left alone.
    ///
    /// @thread_safety Parallel over rows via the engine thread pool.
    class PUMPKIN_API OutlinePass final : public RenderPass {
    public:
        /// @brief Creates the pass.
        /// @param threshold Relative depth jump that triggers an outline (smaller = more outlines).
        /// @param toonOnly  When true, the pass does nothing unless the scene contains a ShaderId::Toon material.
        OutlinePass(float threshold = 0.02f, bool toonOnly = true) : threshold(threshold), toonOnly(toonOnly) {}

        /// @brief Profiler name. @return "Outline".
        [[nodiscard]] std::string_view Name() const override {
            return "Outline";
        }

        /// @brief Draws the outlines into the frame buffer.
        /// @param ctx Frame data. Skipped when the target is smaller than 3x3, the camera planes are invalid (`near <= 0` or `far <= near`),
        ///            or the depth buffer / toon mask are smaller than the frame buffer.
        void Execute(RenderContext& ctx) override;

        /// @brief Relative depth discontinuity above which an outline is drawn.
        float threshold;
        /// @brief Skip the pass when no toon material is in the scene.
        bool toonOnly;
    };
}
