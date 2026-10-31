/// @file ParticlePass.cppm
/// @brief Module Pumpkin.Renderer.Passes.Particle: updates and draws ParticleSystem emitters as single pixels.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Particle;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.ParticleSystem;

import std;

export namespace Pumpkin
{
    /// @brief Simulates and renders one or more particle systems.
    ///
    /// Each particle projects to one pixel which is depth-tested against (and writes to) the depth buffer; no blending is done and
    /// particles outside the view volume are skipped.
    class PUMPKIN_API ParticlePass final : public RenderPass
    {
    public:
        /// @brief Creates the pass and shares ownership of the systems.
        /// @tparam Parts Types convertible to `std::shared_ptr<ParticleSystem>`.
        /// @param systems Emitters to update and draw each frame.
        template<typename... Parts>
        explicit ParticlePass(std::shared_ptr<Parts>... systems)
        {
            (m_systems.emplace_back(std::move(systems)), ...);
        }

        /// @brief Profiler name. @return "Particles".
        [[nodiscard]] std::string_view Name() const override
        {
            return "Particles";
        }

        /// @brief Advances every system by `ctx.deltaTime` and draws its live particles.
        /// @param ctx Frame data.
        /// @note Side effect: the simulation is stepped here, so particles only advance while the pass runs.
        /// @thread_safety Single-threaded.
        void Execute(RenderContext& ctx) override;

    private:
        std::vector<std::shared_ptr<ParticleSystem>> m_systems;
    };
}
