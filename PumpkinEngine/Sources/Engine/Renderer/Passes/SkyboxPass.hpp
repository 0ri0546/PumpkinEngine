
#pragma once

#include <vector>

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

        std::vector<float> m_rayX;
        std::vector<float> m_rayY;

        int m_cachedWidth = 0;
        int m_cachedHeight = 0;
        float m_cachedFov = -1.0f;
    };
}
