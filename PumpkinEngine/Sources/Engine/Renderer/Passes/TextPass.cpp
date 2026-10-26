module;

#include <SDL_FixedIncludes.h>

module Pumpkin.Renderer.Passes.Text;

import Pumpkin.Utils.ThreadPool;

import std;

namespace Pumpkin {
    void TextPass::DrawText(const FrameBuffer& fb, const Text& text) {
        if (!fb.pixels || !text.text || !text.surface)
            return;

        if (!text.surface)
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

        if (startX >= endX || startY >= endY) {
            SDL_UnlockSurface(text.surface);
            return;
        }

        const auto* pixels = static_cast<const std::uint32_t*>(text.surface->pixels);
        const int srcStride = text.surface->pitch / sizeof(std::uint32_t);

        // Format lookups done once, not per pixel
        const SDL_PixelFormatDetails* srcFormat = SDL_GetPixelFormatDetails(text.surface->format);
        const SDL_PixelFormatDetails* dstFormat = SDL_GetPixelFormatDetails(SDL_PixelFormat::SDL_PIXELFORMAT_ABGR8888);

        // Blend a range of rows
        const auto drawRows = [&](int rowBegin, int rowEnd) {
            for (int py = rowBegin; py < rowEnd; ++py) {
                const std::uint32_t* srcRow = pixels + (py - text.y) * srcStride;
                Uint32* dstRow = fb.pixels + py * fb.width;

                for (int px = startX; px < endX; ++px) {
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(srcRow[px - text.x], srcFormat, nullptr, &r, &g, &b, &a);

                    if (a == 0)
                        continue;

                    Uint32& dst = dstRow[px];

                    Uint8 dr, dg, db, da;
                    SDL_GetRGBA(dst, dstFormat, nullptr, &dr, &dg, &db, &da);

                    const float alpha = a / 255.0f;
                    const float inv = 1.0f - alpha;

                    const auto outR = static_cast<Uint8>(r * alpha + dr * inv);
                    const auto outG = static_cast<Uint8>(g * alpha + dg * inv);
                    const auto outB = static_cast<Uint8>(b * alpha + db * inv);
                    const auto outA = static_cast<Uint8>(a + da * inv);

                    dst = SDL_MapRGBA(dstFormat, nullptr, outR, outG, outB, outA);
                }
            }
        };

        constexpr int kChunkRows = 16;
        const int rows = endY - startY;
        const int chunks = (rows + kChunkRows - 1) / kChunkRows;

        if (chunks <= 1) {
            drawRows(startY, endY);
        } else {
            ParallelFor(GetPool(), static_cast<std::size_t>(chunks), [&](std::size_t c) {
                const int begin = startY + static_cast<int>(c) * kChunkRows;
                drawRows(begin, std::min(endY, begin + kChunkRows));
            });
        }

        SDL_UnlockSurface(text.surface);
    }
}
