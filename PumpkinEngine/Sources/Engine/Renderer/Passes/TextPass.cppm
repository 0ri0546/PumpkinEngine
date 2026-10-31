/// @file TextPass.cppm
/// @brief Module Pumpkin.Renderer.Passes.Text: alpha-blends pre-rendered Text surfaces onto the frame buffer.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Text;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Types;
import std;

export namespace Pumpkin {
    /// @brief Draws a list of Text objects (their `surface`) in list order.
    /// @note The span is referenced, not copied: the Text array must outlive the pass.
    class PUMPKIN_API TextPass final : public RenderPass {
    public:
        /// @brief Creates the pass. @param texts Non-owning list of texts to draw each frame.
        explicit TextPass(std::span<Text> texts)
            : m_texts(texts)
        {
        }

        /// @brief Profiler name. @return "Text".
        [[nodiscard]]
        std::string_view Name() const override
        {
            return "Text";
        }

        /// @brief Draws every text. @param ctx Frame data.
        void Execute(RenderContext& ctx) override
        {
            for (const Text& text : m_texts)
            {
                DrawText(ctx.fb, text);
            }
        }

        /// @brief Resizes every text.
        /// @param referenceWidth  Base canvas width
        /// @param referenceHeight Base canvas height
        /// @param ctx Frame data.
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
        /// @brief Blends one text surface at its position, clipped to the frame buffer; row bands are drawn in parallel.
        /// @param fb   Target.
        /// @param text Text to draw; skipped if its surface or string is null or it lies fully off-screen.
        void DrawText(const FrameBuffer&, const Text&);

        std::span<Text> m_texts;
    };
}