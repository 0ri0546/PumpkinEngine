
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

                    std::uint32_t& dst =
                        fb.pixels[static_cast<std::size_t>(y) * fb.width + x];

                    const float dstR = ((dst >> 24) & 0xFF) / 255.0f;
                    const float dstG = ((dst >> 16) & 0xFF) / 255.0f;
                    const float dstB = ((dst >> 8) & 0xFF) / 255.0f;
                    const float dstA = (dst & 0xFF) / 255.0f;

                    const float outA = srcA + dstA * (1.0f - srcA);

                    float outR = 0.0f;
                    float outG = 0.0f;
                    float outB = 0.0f;

                    if (outA > 0.0f)
                    {
                        outR = (srcR * srcA + dstR * dstA * (1.0f - srcA)) / outA;
                        outG = (srcG * srcA + dstG * dstA * (1.0f - srcA)) / outA;
                        outB = (srcB * srcA + dstB * dstA * (1.0f - srcA)) / outA;
                    }

                    const auto toByte = [](float value)
                    {
                        return static_cast<std::uint32_t>(
                            std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f
                        );
                    };

                    dst =
                        (toByte(outR) << 24) |
                        (toByte(outG) << 16) |
                        (toByte(outB) << 8) |
                        toByte(outA);
                }
            }
        }

        std::span<const UIImageDraw> m_images;
    };
}
