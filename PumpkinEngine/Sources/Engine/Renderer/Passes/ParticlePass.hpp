#pragma once

#include <Renderer/RenderPass.hpp>
#include <Renderer/ParticleSystem.hpp>

namespace Pumpkin {

    class ParticlePass final : public RenderPass {
    public:
        explicit ParticlePass(ParticleSystem& system)
            : m_system(system) {
        }

        [[nodiscard]]
        std::string_view Name() const override {
            return "Particles";
        }

        void Execute(RenderContext& ctx) override;

    private:
        ParticleSystem& m_system;
    };

}