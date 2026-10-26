module Pumpkin.Renderer.Passes.Skybox;

import Pumpkin.Renderer.Texture;
import Pumpkin.Renderer.Types;
import Pumpkin.Utils.ThreadPool;
import Pumpkin.Utils.Utils;

import Pumpkin.Math.Vec;

import std;

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
        const int tw = m_texture.width;
        const int th = m_texture.height;

        if (width <= 0 || height <= 0 || !ctx.fb.pixels ||
            tw <= 0 || th <= 0 || m_texture.pixels.empty()) {
            return;
        }

        const float fov = ctx.camera.FOVY;
        if (m_cachedWidth != width || m_cachedHeight != height || m_cachedFov != fov) {
            const float aspect = static_cast<float>(width) / height;
            const float tanHalfFov = std::tan(fov * Pi / 360.0f);

            m_rayX.resize(width + 1);
            m_rayY.resize(height);

            for (int x = 0; x <= width; ++x)
                m_rayX[x] = (2.0f * (x + 0.5f) / width - 1.0f) * aspect * tanHalfFov;
            for (int y = 0; y < height; ++y)
                m_rayY[y] = (1.0f - 2.0f * (y + 0.5f) / height) * tanHalfFov;

            m_cachedWidth = width;
            m_cachedHeight = height;
            m_cachedFov = fov;
        }

        // Pre-packed texture, rebuilt if modified
        const void* src = m_texture.pixels.data();
        if (m_packedSrc != src || m_packedW != tw || m_packedH != th) {
            m_packed.resize(static_cast<std::size_t>(tw) * th);
            std::uint32_t* out = m_packed.data();

            const auto packRows = [&](std::size_t y) {
                const float v = (static_cast<float>(y) + 0.5f) / th;
                for (int x = 0; x < tw; ++x) {
                    const Color c = SampleTexture(m_texture, (x + 0.5f) / tw, v);
                    out[y * tw + x] = ToRGBA(
                        static_cast<float>(c.r), static_cast<float>(c.g),
                        static_cast<float>(c.b), static_cast<float>(c.a));
                }
            };
            ParallelFor(GetPool(), static_cast<std::size_t>(th), packRows);

            m_packedSrc = src;
            m_packedW = tw;
            m_packedH = th;
        }

        const std::uint32_t* const tex = m_packed.data();
        const auto ftw = static_cast<float>(tw);
        const auto fth = static_cast<float>(th);

        const Vec3f front = ctx.camera.front.Normalized();
        const Vec3f right = ctx.camera.right.Normalized();
        const Vec3f up = ctx.camera.up.Normalized();

        const float fx = front[0], fy = front[1], fz = front[2];
        const float rx_ = right[0], ry_ = right[1], rz_ = right[2];
        const float ux = up[0], uy = up[1], uz = up[2];

        constexpr float InvTwoPi = 1.0f / (2.0f * Pi);
        constexpr float InvPi = 1.0f / Pi;
        constexpr int Step = 8;               // pixels between two exact nodes
        constexpr float MaxSegmentDu = 0.1f;  // poles get exact calculation

        constexpr std::size_t RowsPerChunk = 8;
        const std::size_t chunkCount = (static_cast<std::size_t>(height) + RowsPerChunk - 1) / RowsPerChunk;

        const auto renderChunk = [&](std::size_t chunk) {
            const int startY = static_cast<int>(chunk * RowsPerChunk);
            const int endY = std::min(height, startY + static_cast<int>(RowsPerChunk));

            for (int y = startY; y < endY; ++y) {
                const float ry = m_rayY[y];
                const float bx = fx + ux * ry;
                const float by = fy + uy * ry;
                const float bz = fz + uz * ry;

                std::uint32_t* dst = ctx.fb.pixels + static_cast<std::size_t>(y) * width;

                // Exact uvs for the x column
                const auto exact = [&](int x, float& u, float& v) {
                    const float rx = m_rayX[x];
                    const float dx = bx + rx_ * rx;
                    const float dy = by + ry_ * rx;
                    const float dz = bz + rz_ * rx;
                    const float len2 = std::max(dx * dx + dy * dy + dz * dz, 1e-12f);
                    u = FastAtan2(dz, dx) * InvTwoPi + 0.5f;
                    v = FastAsin(dy / std::sqrt(len2)) * InvPi + 0.5f;
                };

                const auto put = [&](int x, float u, float v) {
                    int ix = static_cast<int>((u + 1.0f) * ftw);
                    if (ix >= 2 * tw) ix -= 2 * tw;
                    if (ix >= tw) ix -= tw;
                    const int iy = std::clamp(static_cast<int>(v * fth), 0, th - 1);
                    dst[x] = tex[static_cast<std::size_t>(iy) * tw + ix];
                };

                float u0, v0;
                exact(0, u0, v0);

                for (int x0 = 0; x0 < width; x0 += Step) {
                    const int x1 = std::min(x0 + Step, width);
                    const int span = x1 - x0;

                    float u1, v1;
                    exact(x1, u1, v1);

                    float u1w = u1;
                    const float d = u1 - u0;
                    if (d > 0.5f) u1w -= 1.0f;
                    else if (d < -0.5f) u1w += 1.0f;

                    if (std::abs(u1w - u0) > MaxSegmentDu) {
                        for (int x = x0; x < x1; ++x) {
                            float u, v;
                            exact(x, u, v);
                            put(x, u, v);
                        }
                    }
                    else {
                        const float inv = 1.0f / span;
                        const float du = (u1w - u0) * inv;
                        const float dv = (v1 - v0) * inv;
                        float u = u0, v = v0;
                        for (int x = x0; x < x1; ++x) {
                            put(x, u, v);
                            u += du;
                            v += dv;
                        }
                    }

                    u0 = u1;
                    v0 = v1;
                }
            }
        };

        if (chunkCount <= 1)
            renderChunk(0);
        else
            ParallelFor(GetPool(), chunkCount, renderChunk);
    }

}
