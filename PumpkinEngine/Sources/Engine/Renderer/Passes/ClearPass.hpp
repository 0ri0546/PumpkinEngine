#pragma once

#include <Renderer/RenderPass.hpp>

namespace Pumpkin {
    class ClearPass final : public RenderPass {
    public:
        explicit ClearPass(const uint32_t clear = 0x00000000u) { m_ClearColor = clear; }
        [[nodiscard]] std::string_view Name() const override { return "Clear"; }
        void Execute(RenderContext& ctx) override;

    private:
        uint32_t m_ClearColor { 0x00000000u };
    };
}
