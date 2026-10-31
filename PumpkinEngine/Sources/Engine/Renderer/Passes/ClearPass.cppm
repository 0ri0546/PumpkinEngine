/// @file ClearPass.cppm
/// @brief Module Pumpkin.Renderer.Passes.Clear: resets the colour and depth buffers.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Clear;

import Pumpkin.Renderer.RenderPass;

import std;

export namespace Pumpkin {
    /// @brief First pass of a frame: fills the frame buffer with a colour and the depth buffer with 1.0 (far).
    /// @note Does not touch RenderContext::toonMask.
    class PUMPKIN_API ClearPass final : public RenderPass {
    public:
        /// @brief Creates the pass.
        /// @param clear Colour written to every pixel, in frame buffer layout `0xAARRGGBB` (default 0, transparent black).
        explicit ClearPass(const std::uint32_t clear = 0x00000000u) { m_ClearColor = clear; }
        /// @brief Profiler name. @return "Clear".
        [[nodiscard]] std::string_view Name() const override { return "Clear"; }
        /// @brief Clears colour and depth, in parallel chunks of 64K pixels.
        /// @param ctx Frame data.
        /// @pre `ctx.fb.pixels` is valid and `ctx.db.pixels` holds at least `width * height` floats (not checked).
        /// @thread_safety Parallel internally via the engine thread pool.
        void Execute(RenderContext& ctx) override;

    private:
        std::uint32_t m_ClearColor { 0x00000000u };
    };
}
