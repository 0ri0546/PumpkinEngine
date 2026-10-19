#include "TextPass.hpp"

#include <SDL3/SDL.h>

namespace Pumpkin {
    void TextPass::DrawText(const FrameBuffer& fb, const Text& text) {
        if (!fb.pixels || !text.text)
            return;

        if (!TTF_SetFontSize(text.font, text.fontSize))
            return;

        if (!text.surface)
            return;

        if (!SDL_LockSurface(text.surface)) {
            return;
        }

        const int startX = std::max(0, text.x);
        const int startY = std::max(0, text.y);
        const int endX = std::min(static_cast<int>(fb.width), text.x + text.surface->w);
        const int endY = std::min(static_cast<int>(fb.height), text.y + text.surface->h);

        const auto* pixels = static_cast<const uint32_t*>(text.surface->pixels);

        for (int py = startY; py < endY; ++py) {
            for (int px = startX; px < endX; ++px) {
                const int sx = px - text.x;
                const int sy = py - text.y;

                Uint32 pixel = pixels[sy * (text.surface->pitch / sizeof(uint32_t)) + sx];

                Uint8 r, g, b, a;
                SDL_GetRGBA(
                    pixel,
                    SDL_GetPixelFormatDetails(text.surface->format),
                    nullptr,
                    &r, &g, &b, &a
                );

                if (a == 0)
                    continue;

                Uint32& dst = fb.pixels[py * fb.width + px];

                const Uint8 dr = (dst >> 24) & 0xFF;
                const Uint8 dg = (dst >> 16) & 0xFF;
                const Uint8 db = (dst >> 8) & 0xFF;
                const Uint8 da = dst & 0xFF;

                const float alpha = a / 255.0f;

                const auto outR = static_cast<Uint8>(r * alpha + dr * (1.0f - alpha));
                const auto outG = static_cast<Uint8>(g * alpha + dg * (1.0f - alpha));
                const auto outB = static_cast<Uint8>(b * alpha + db * (1.0f - alpha));
                const auto outA = static_cast<Uint8>(a + da * (1.0f - alpha));

                const auto* dstFormat = SDL_GetPixelFormatDetails(
                    SDL_PIXELFORMAT_ABGR8888
                );

                dst = SDL_MapRGBA(
                    dstFormat,
                    nullptr,
                    outR, outG, outB, outA
                );
            }
        }

        SDL_UnlockSurface(text.surface);
    }
}
