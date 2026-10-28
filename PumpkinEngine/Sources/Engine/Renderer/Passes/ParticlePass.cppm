module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Particle;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.ParticleSystem;

import std;

export namespace Pumpkin
{
    class PUMPKIN_API ParticlePass final : public RenderPass
    {
    public:
        template<typename... Parts>
        explicit ParticlePass(std::shared_ptr<Parts>... systems)
        {
            (m_systems.emplace_back(std::move(systems)), ...);
        }

        [[nodiscard]] std::string_view Name() const override
        {
            return "Particles";
        }

        void Execute(RenderContext& ctx) override;

    private:
        std::vector<std::shared_ptr<ParticleSystem>> m_systems;
    };
}
