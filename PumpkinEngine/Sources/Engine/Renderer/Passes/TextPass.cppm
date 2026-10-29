module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Text;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Types;
import std;

export namespace Pumpkin {
    class PUMPKIN_API TextPass final : public RenderPass {
    public:
        explicit TextPass(std::span<Text> texts)
            : m_texts(texts)
        {
        }

        [[nodiscard]]
        std::string_view Name() const override
        {
            return "Text";
        }

        void Execute(RenderContext& ctx) override
        {
            for (const Text& text : m_texts)
            {
                DrawText(ctx.fb, text);
            }
        }

        void ResizeText(
            float referenceWidth,
            float referenceHeight,
            RenderContext& ctx)
        {
            const float scaleX =
                static_cast<float>(ctx.fb.width) / referenceWidth;

            const float scaleY =
                static_cast<float>(ctx.fb.height) / referenceHeight;

            const float scale = std::min(scaleX, scaleY);

            for (Text& text : m_texts)
            {
                text.SetSize(20.0f * scale);
            }
        }

    private:
        void DrawText(const FrameBuffer&, const Text&);

        std::span<Text> m_texts;
    };
}