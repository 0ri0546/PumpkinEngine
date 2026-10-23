
#include "TextPass.hpp"

#include <algorithm>
#include <SDL3/SDL.h>

namespace Pumpkin {
    void TextPass::DrawText(const FrameBuffer& fb, const Text& text) {
        if (!fb.pixels || !text.text || !text.surface)
            return;

        if (!SDL_LockSurface(text.surface))
            return;

        const int startX = std::max(0, text.x);
        const int startY = std::max(0, text.y);
        const int endX = std::min(
            static_cast<int>(fb.width),
            text.x + text.surface->w
        );
        const int endY = std::min(
            static_cast<int>(fb.height),
            text.y + text.surface->h
        );

        const auto* pixels =
            static_cast<const uint32_t*>(text.surface->pixels);

        const auto* srcFormat =
            SDL_GetPixelFormatDetails(text.surface->format);

        for (int py = startY; py < endY; ++py) {
            for (int px = startX; px < endX; ++px) {
                const int sx = px - text.x;
                const int sy = py - text.y;

                const uint32_t pixel =
                    pixels[sy * (text.surface->pitch / sizeof(uint32_t)) + sx];

                Uint8 r, g, b, a;
                SDL_GetRGBA(pixel, srcFormat, nullptr, &r, &g, &b, &a);

                if (a == 0)
                    continue;

                uint32_t& dst = fb.pixels[py * fb.width + px];

                // Framebuffer : 0xAARRGGBB
                const Uint8 dr = (dst >> 16) & 0xFF;
                const Uint8 dg = (dst >> 8) & 0xFF;
                const Uint8 db = dst & 0xFF;
                const Uint8 da = (dst >> 24) & 0xFF;

                const unsigned invAlpha = 255 - a;

                const Uint8 outR = static_cast<Uint8>(
                    (r * a + dr * invAlpha + 127) / 255
                    );
                const Uint8 outG = static_cast<Uint8>(
                    (g * a + dg * invAlpha + 127) / 255
                    );
                const Uint8 outB = static_cast<Uint8>(
                    (b * a + db * invAlpha + 127) / 255
                    );
                const Uint8 outA = static_cast<Uint8>(
                    a + (da * invAlpha + 127) / 255
                    );

                dst =
                    (static_cast<uint32_t>(outA) << 24) |
                    (static_cast<uint32_t>(outR) << 16) |
                    (static_cast<uint32_t>(outG) << 8) |
                    static_cast<uint32_t>(outB);
            }
        }

        SDL_UnlockSurface(text.surface);
    }
}
