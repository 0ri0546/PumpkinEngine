#pragma once

#include <Renderer/RenderPass.hpp>

namespace Pumpkin {
    class SkyboxPass final : public RenderPass {
    public:
        explicit SkyboxPass(const Texture& texture)
            : m_texture(texture) {
        }

        [[nodiscard]] std::string_view Name() const override {
            return "Skybox";
        }

        void Execute(RenderContext& ctx) override;

    private:
        const Texture& m_texture;
    };
}