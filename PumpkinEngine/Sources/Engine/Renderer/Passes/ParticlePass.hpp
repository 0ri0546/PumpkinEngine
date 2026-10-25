#pragma once

#include <Renderer/RenderPass.hpp>
#include <Renderer/ParticleSystem.hpp>
#include <type_traits>

#include <memory>
#include <vector>

namespace Pumpkin
{
    class ParticlePass final : public RenderPass
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
