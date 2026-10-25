export module Pumpkin.Renderer.Passes.Clear;

import Pumpkin.Renderer.RenderPass;

import std;

export namespace Pumpkin {
    class ClearPass final : public RenderPass {
    public:
        explicit ClearPass(const std::uint32_t clear = 0x00000000u) { m_ClearColor = clear; }
        [[nodiscard]] std::string_view Name() const override { return "Clear"; }
        void Execute(RenderContext& ctx) override;

    private:
        std::uint32_t m_ClearColor { 0x00000000u };
    };
}
