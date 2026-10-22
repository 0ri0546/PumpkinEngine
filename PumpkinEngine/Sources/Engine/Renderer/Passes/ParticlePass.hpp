#pragma once

#include <Renderer/RenderPass.hpp>
#include <Renderer/ParticleSystem.hpp>
#include <type_traits>

namespace Pumpkin {

    template <typename T>
    concept IsParticleSystem = std::is_same_v<T, ParticleSystem>;

    class ParticlePass final : public RenderPass {
    public:
        template <IsParticleSystem... Parts>
        explicit ParticlePass(Parts&... systems) {
            (m_systems.push_back(std::forward<Parts&>(systems)), ...);
        }

        [[nodiscard]]
        std::string_view Name() const override {
            return "Particles";
        }

        void Execute(RenderContext& ctx) override;

    private:
        std::vector<ParticleSystem> m_systems;
    };

}