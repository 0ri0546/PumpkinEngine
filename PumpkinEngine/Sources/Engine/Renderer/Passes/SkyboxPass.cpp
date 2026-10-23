#include "SkyboxPass.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <Renderer/Texture.hpp>
#include <Utils/ThreadPool.hpp>
#include <Utils/Utils.hpp>

namespace Pumpkin {

    void SkyboxPass::Execute(RenderContext& ctx) {
        const int width = ctx.fb.width;
        const int height = ctx.fb.height;

        if (width <= 0 || height <= 0 ||
            ctx.fb.pixels == nullptr ||
            m_texture.width <= 0 ||
            m_texture.height <= 0 ||
            m_texture.pixels.empty()) {
            return;
        }

        const float aspect =
            static_cast<float>(width) / static_cast<float>(height);

        const float tanHalfFov = std::tan(
            ctx.camera.FOVY * std::numbers::pi_v<float> / 360.0f
        );

        const Vec3f front = ctx.camera.front.Normalized();
        const Vec3f right = ctx.camera.right.Normalized();
        const Vec3f up = ctx.camera.up.Normalized();

        constexpr size_t kChunkSize = 64 * 1024;

        const size_t pixelCount =
            static_cast<size_t>(width) * height;

        const size_t chunkCount =
            (pixelCount + kChunkSize - 1) / kChunkSize;

        const auto renderChunk = [&](size_t chunk) {
            const size_t begin = chunk * kChunkSize;
            const size_t end = std::min(
                pixelCount,
                begin + kChunkSize
            );

            for (size_t index = begin; index < end; ++index) {
                const int x = static_cast<int>(index % width);
                const int y = static_cast<int>(index / width);

                const float ndcX =
                    2.0f * (static_cast<float>(x) + 0.5f) / width - 1.0f;

                const float ndcY =
                    1.0f - 2.0f * (static_cast<float>(y) + 0.5f) / height;

                Vec3f direction =
                    front
                    + right * (ndcX * aspect * tanHalfFov)
                    + up * (ndcY * tanHalfFov);

                direction = direction.Normalized();

                const float u =
                    std::atan2(direction[2], direction[0])
                    / (2.0f * std::numbers::pi_v<float>)
                    + 0.5f;

                const float v =
                    std::asin(std::clamp(direction[1], -1.0f, 1.0f))
                    / std::numbers::pi_v<float>
                    +0.5f;

                const uint32_t texel =
                    SampleTexture(m_texture, u, v);

                const float r = static_cast<float>((texel >> 24) & 0xFF);
                const float g = static_cast<float>((texel >> 16) & 0xFF);
                const float b = static_cast<float>((texel >> 8) & 0xFF);
                const float a = static_cast<float>(texel & 0xFF);

                ctx.fb.pixels[index] = ToRGBA(r, g, b, a);
            }
            };

        if (chunkCount <= 1) {
            renderChunk(0);
        }
        else {
            ParallelFor(GetPool(), chunkCount, renderChunk);
        }
    }
}