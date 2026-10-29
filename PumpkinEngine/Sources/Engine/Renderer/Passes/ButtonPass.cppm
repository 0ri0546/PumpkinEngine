
module;

#include <DLL/Export.hpp>
#include <SDL_FixedIncludes.h>

export module Pumpkin.Renderer.Passes.Button;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Types;
import Pumpkin.UI.Components;
import std;

export namespace Pumpkin
{
    class PUMPKIN_API ButtonPass final : public RenderPass
    {
    public:
        explicit ButtonPass(std::span<const UI::UIButtonDraw> buttons)
            : m_buttons(buttons)
        {
        }

        [[nodiscard]]
        std::string_view Name() const override
        {
            return "UI Buttons";
        }

        void Execute(RenderContext& ctx) override
        {
            if (!ctx.fb.pixels)
                return;

            for (const UI::UIButtonDraw& button : m_buttons)
                DrawButton(ctx.fb, button);
        }

    private:
        static void DrawButton(
            const FrameBuffer& fb,
            const UI::UIButtonDraw& button)
        {
            if (button.width <= 0 || button.height <= 0)
                return;

            const float scaledWidth = button.width * button.scale;
            const float scaledHeight = button.height * button.scale;

            const int left = static_cast<int>(
                std::lround(button.x + (button.width - scaledWidth) * 0.5f)
            );
            const int top = static_cast<int>(
                std::lround(button.y + (button.height - scaledHeight) * 0.5f)
            );

            const int width = static_cast<int>(std::lround(scaledWidth));
            const int height = static_cast<int>(std::lround(scaledHeight));

            const int x0 = std::max(0, left);
            const int y0 = std::max(0, top);
            const int x1 = std::min(static_cast<int>(fb.width), left + width);
            const int y1 = std::min(static_cast<int>(fb.height), top + height);

            if (x0 >= x1 || y0 >= y1)
                return;

            const float srcR = button.color[0];
            const float srcG = button.color[1];
            const float srcB = button.color[2];
            const float alpha = button.color[3] / 255.0f;

            if (alpha <= 0.0f)
                return;

            const SDL_PixelFormatDetails* format =
                SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ABGR8888);

            for (int y = y0; y < y1; ++y)
            {
                for (int x = x0; x < x1; ++x)
                {
                    std::uint32_t& dst =
                        fb.pixels[
                            static_cast<std::size_t>(y) * fb.width + x
                        ];

                    Uint8 dr, dg, db, da;
                    SDL_GetRGBA(dst, format, nullptr, &dr, &dg, &db, &da);

                    const float invAlpha = 1.0f - alpha;

                    const auto outR = static_cast<Uint8>(
                        srcR * alpha + dr * invAlpha
                    );

                    const auto outG = static_cast<Uint8>(
                        srcG * alpha + dg * invAlpha
                    );

                    const auto outB = static_cast<Uint8>(
                        srcB * alpha + db * invAlpha
                    );

                    const auto outA = static_cast<Uint8>(
                        button.color[3] + da * invAlpha
                    );

                    dst = SDL_MapRGBA(
                        format,
                        nullptr,
                        outB,
                        outG,
                        outR,
                        outA
                    );
                }
            }
        }

        std::span<const UI::UIButtonDraw> m_buttons;
    };
}
