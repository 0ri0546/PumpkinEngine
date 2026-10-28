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

    private:
        void DrawText(const FrameBuffer&, const Text&);

        std::span<Text> m_texts;
    };
}