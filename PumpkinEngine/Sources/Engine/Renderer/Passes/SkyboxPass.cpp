
#include "SkyboxPass.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <Renderer/Texture.hpp>
#include <Utils/ThreadPool.hpp>
#include <Utils/Utils.hpp>

namespace Pumpkin {
    namespace {
        constexpr float Pi = std::numbers::pi_v<float>;
        constexpr float HalfPi = Pi * 0.5f;

        float FastAtan2(float y, float x) {
            if (x == 0.0f) {
                if (y > 0.0f) return HalfPi;
                if (y < 0.0f) return -HalfPi;
                return 0.0f;
            }

            const float absY = std::abs(y) + 1e-10f;
            float angle;

            if (x >= 0.0f) {
                const float r = (x - absY) / (x + absY);
                angle = Pi * 0.25f - Pi * 0.25f * r;
            }
            else {
                const float r = (x + absY) / (absY - x);
                angle = Pi * 0.75f - Pi * 0.25f * r;
            }

            return y < 0.0f ? -angle : angle;
        }

        float FastAsin(float x) {
            x = std::clamp(x, -1.0f, 1.0f);

            const float absX = std::abs(x);
            const float result =
                HalfPi - std::sqrt(1.0f - absX) *
                (1.5707288f
                    - 0.2121144f * absX
                    + 0.0742610f * absX * absX
                    - 0.0187293f * absX * absX * absX);

            return std::copysign(result, x);
        }
    }


    void SkyboxPass::Execute(RenderContext& ctx) {
        const int width = ctx.fb.width;
        const int height = ctx.fb.height;

        if (width <= 0 || height <= 0 ||
            !ctx.fb.pixels ||
            m_texture.width <= 0 ||
            m_texture.height <= 0 ||
            m_texture.pixels.empty()) {
            return;
        }

        const float fov = ctx.camera.FOVY;

        if (m_cachedWidth != width ||
            m_cachedHeight != height ||
            m_cachedFov != fov) {

            const float aspect = static_cast<float>(width) / height;
            const float tanHalfFov = std::tan(fov * Pi / 360.0f);

            m_rayX.resize(width);
            m_rayY.resize(height);

            for (int x = 0; x < width; ++x) {
                m_rayX[x] =
                    (2.0f * (x + 0.5f) / width - 1.0f)
                    * aspect * tanHalfFov;
            }

            for (int y = 0; y < height; ++y) {
                m_rayY[y] =
                    (1.0f - 2.0f * (y + 0.5f) / height)
                    * tanHalfFov;
            }

            m_cachedWidth = width;
            m_cachedHeight = height;
            m_cachedFov = fov;
        }

        const Vec3f front = ctx.camera.front.Normalized();
        const Vec3f right = ctx.camera.right.Normalized();
        const Vec3f up = ctx.camera.up.Normalized();

        constexpr size_t RowsPerChunk = 8;
        const size_t chunkCount =
            (static_cast<size_t>(height) + RowsPerChunk - 1)
            / RowsPerChunk;

        const auto renderChunk = [&](size_t chunk) {
            const int startY = static_cast<int>(chunk * RowsPerChunk);
            const int endY = std::min(
                height,
                startY + static_cast<int>(RowsPerChunk)
            );

            for (int y = startY; y < endY; ++y) {
                const float rayY = m_rayY[y];
                const Vec3f rowRay = front + up * rayY;

                uint32_t* dst =
                    ctx.fb.pixels + static_cast<size_t>(y) * width;

                for (int x = 0; x < width; ++x) {
                    const Vec3f ray = rowRay + right * m_rayX[x];

                    const float lengthSquared =
                        ray[0] * ray[0] +
                        ray[1] * ray[1] +
                        ray[2] * ray[2];

                    if (lengthSquared <= 1e-12f)
                        continue;

                    const float invLength =
                        1.0f / std::sqrt(lengthSquared);

                    const float u =
                        FastAtan2(ray[2], ray[0])
                        / (2.0f * Pi) + 0.5f;

                    const float v =
                        FastAsin(ray[1] * invLength)
                        / Pi + 0.5f;

                    const uint32_t texel =
                        SampleTexture(m_texture, u, v);

                    const uint32_t r = (texel >> 24) & 0xFF;
                    const uint32_t g = (texel >> 16) & 0xFF;
                    const uint32_t b = (texel >> 8) & 0xFF;
                    const uint32_t a = texel & 0xFF;

                    dst[x] = ToRGBA(
                        static_cast<float>(r),
                        static_cast<float>(g),
                        static_cast<float>(b),
                        static_cast<float>(a)
                    );
                }
            }
            };

        if (chunkCount <= 1)
            renderChunk(0);
        else
            ParallelFor(GetPool(), chunkCount, renderChunk);
    }

}
