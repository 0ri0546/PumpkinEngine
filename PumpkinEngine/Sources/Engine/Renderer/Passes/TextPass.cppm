export module Pumpkin.Renderer.Passes.Text;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Types;

export namespace Pumpkin {
    class TextPass final : public RenderPass {
    public:
        explicit TextPass(Text& text) : m_text(text) {}
        [[nodiscard]] std::string_view Name() const override { return "Text"; }
        void Execute(RenderContext& ctx) override { DrawText(ctx.fb, m_text); }
    private:
        void DrawText(const FrameBuffer&, const Text&);

        Text& m_text;
    };
}
