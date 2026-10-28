module;

#include <SDL_FixedIncludes.h>
export module Pumpkin.Renderer.Passes.Image;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Types;
import Pumpkin.UI.Components;
import std;

export namespace Pumpkin
{
    struct UIImageDraw
    {
        const Texture* texture = nullptr;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::array<std::uint8_t, 4> color{255, 255, 255, 255};
    };

    class ImagePass final : public RenderPass
    {
    public:
        explicit ImagePass(std::span<const UIImageDraw> images)
            : m_images(images)
        {
        }

        [[nodiscard]]
        std::string_view Name() const override
        {
            return "UI Images";
        }

        void Execute(RenderContext& ctx) override
        {
            if (!ctx.fb.pixels)
                return;

            for (const UIImageDraw& image : m_images)
                DrawImage(ctx.fb, image);
        }

    private:
        static void DrawImage(const FrameBuffer& fb, const UIImageDraw& image)
        {
            if (!image.texture ||
                image.texture->width <= 0 ||
                image.texture->height <= 0 ||
                image.texture->pixels.empty() ||
                image.width <= 0 ||
                image.height <= 0)
                return;

            const int x0 = std::max(0, image.x);
            const int y0 = std::max(0, image.y);
            const int x1 = std::min(
                static_cast<int>(fb.width),
                image.x + image.width
            );
            const int y1 = std::min(
                static_cast<int>(fb.height),
                image.y + image.height
            );

            if (x0 >= x1 || y0 >= y1)
                return;

            const Texture& texture = *image.texture;

            const float tintR = image.color[0] / 255.0f;
            const float tintG = image.color[1] / 255.0f;
            const float tintB = image.color[2] / 255.0f;
            const float tintA = image.color[3] / 255.0f;

            for (int y = y0; y < y1; ++y)
            {
                const int srcY = std::min(
                    ((y - image.y) * texture.height) / image.height,
                    texture.height - 1
                );

                for (int x = x0; x < x1; ++x)
                {
                    const int srcX = std::min(
                        ((x - image.x) * texture.width) / image.width,
                        texture.width - 1
                    );

                    const std::size_t index =
                        static_cast<std::size_t>(srcY) * texture.width + srcX;

                    const std::uint32_t pixel = texture.pixels[index];

                    const float srcR = ((pixel >> 24) & 0xFF) / 255.0f * tintR;
                    const float srcG = ((pixel >> 16) & 0xFF) / 255.0f * tintG;
                    const float srcB = ((pixel >> 8) & 0xFF) / 255.0f * tintB;
                    const float srcA = (pixel & 0xFF) / 255.0f * tintA;

                    if (srcA <= 0.0f)
                        continue;

                    std::uint32_t& dst = fb.pixels[static_cast<std::size_t>(y) * fb.width + x];

                    const SDL_PixelFormatDetails* dstFormat =
                        SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ABGR8888);

                    Uint8 dr, dg, db, da;
                    SDL_GetRGBA(dst, dstFormat, nullptr, &dr, &dg, &db, &da);

                    const float alpha = srcA;
                    const float invAlpha = 1.0f - alpha;

                    const auto outR = static_cast<Uint8>(
                        srcR * 255.0f * alpha + dr * invAlpha
                    );
                    const auto outG = static_cast<Uint8>(
                        srcG * 255.0f * alpha + dg * invAlpha
                    );
                    const auto outB = static_cast<Uint8>(
                        srcB * 255.0f * alpha + db * invAlpha
                    );
                    const auto outA = static_cast<Uint8>(
                        alpha * 255.0f + da * invAlpha
                    );

                    dst = SDL_MapRGBA(
                        dstFormat,
                        nullptr,
                        outB,
                        outG,
                        outR,
                        outA
                    );
                }
            }
        }

        std::span<const UIImageDraw> m_images;
    };
}
