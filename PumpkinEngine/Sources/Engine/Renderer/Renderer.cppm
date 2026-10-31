/// @file Renderer.cppm
/// @brief Module Pumpkin.Renderer.Renderer: ordered list of render passes executed every frame.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Renderer;

import std;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Utils.RenderProfiler;

export namespace Pumpkin {
    /// @brief Owns an ordered list of RenderPass objects and runs them each frame.
    ///
    /// Typical order: Clear, Skybox, Geometry, Particles, Outline, Images, Text.
    ///
    /// @thread_safety Not thread-safe: configure and render from the render thread only.
    class PUMPKIN_API Renderer {
    public:
        /// @brief Creates a renderer without any pass.
        Renderer() = default;
        /// @brief Non-copyable (owns the passes).
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        /// @brief Movable.
        Renderer(Renderer&&) noexcept = default;
        Renderer& operator=(Renderer&&) noexcept = default;

        /// @brief Appends a pass constructed in place.
        /// @tparam T    Concrete pass type, derived from RenderPass.
        /// @tparam Args Constructor argument types.
        /// @param args  Arguments forwarded to T's constructor.
        /// @return Reference to the new pass; it stays valid until the Renderer is destroyed.
        /// @note Passes execute in the order they were added.
        template <std::derived_from<RenderPass> T, class... Args>
        T& AddPass(Args&&... args) {
            auto& p = m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            return static_cast<T&>(*p);
        }

        /// @brief Forwards a size change to every pass (RenderPass::OnResize).
        /// @param w New width in pixels.
        /// @param h New height in pixels.
        void Resize(int w, int h) { for (auto& p : m_passes) p->OnResize(w, h); }

        /// @brief Executes every pass, in order, each inside a profiler scope named after the pass.
        /// @param ctx Frame data shared by all passes.
        void Render(RenderContext& ctx) {
            for (auto& p : m_passes) {
                Prof::Scope scope(p->Name());
                p->Execute(ctx);
            }
        }
    private:
        std::vector<std::unique_ptr<RenderPass>> m_passes;
    };
}
