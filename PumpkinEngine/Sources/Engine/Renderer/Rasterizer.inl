#pragma once

#include <Renderer/Binning.hpp>
#include <Renderer/RasterizerDetail.hpp>
#include <Utils/StageTimer.hpp>
#include <Utils/ThreadPool.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <span>
#include <vector>

namespace Pumpkin {
    namespace Detail {
        // Reused every frame so we don't allocate again (shared by all shader types).
        inline BinData& FrameBins() {
            static BinData bins;
            return bins;
        }
        
        using TexelSIMD = __m256i;

        // 8 texels at once: wrap, UV -> pixel, clamp, gather. Returns 8 packed pixels in the texture's layout.
        [[nodiscard]] inline TexelSIMD SampleTexture8(const std::uint32_t* texels, int width, int height,
            V::type u, V::type v) {
            u = V::sub(u, _mm256_floor_ps(u));
            v = V::sub(v, _mm256_floor_ps(v));

            TexelSIMD x = _mm256_cvttps_epi32(V::mul(u, V::set1(static_cast<float>(width))));
            TexelSIMD y = _mm256_cvttps_epi32(V::mul(V::sub(V::set1(1.0f), v), V::set1(static_cast<float>(height))));

            const TexelSIMD zero = _mm256_setzero_si256();
            x = _mm256_min_epi32(_mm256_max_epi32(x, zero), _mm256_set1_epi32(width - 1));
            y = _mm256_min_epi32(_mm256_max_epi32(y, zero), _mm256_set1_epi32(height - 1));

            const TexelSIMD index = _mm256_add_epi32(_mm256_mullo_epi32(y, _mm256_set1_epi32(width)), x);
            return _mm256_i32gather_epi32(reinterpret_cast<const int*>(texels), index, 4);
        }

        // Four filtered channels (0..255 floats) for 8 pixels.
        struct Texel8 { V::type r, g, b, a; };

        template <int Shift>
        [[nodiscard]] inline V::type BilerpChannel(TexelSIMD t00, TexelSIMD t10, TexelSIMD t01, TexelSIMD t11,
            V::type tx, V::type ty) {
            const auto lerp = [](V::type a, V::type b, V::type t) {
                return V::add(a, V::mul(V::sub(b, a), t));
            };
            const V::type top    = lerp(Channel<Shift>(t00), Channel<Shift>(t10), tx);
            const V::type bottom = lerp(Channel<Shift>(t01), Channel<Shift>(t11), tx);
            return lerp(top, bottom, ty);
        }

        // Bilinear: 4 neighbouring texels blended by the fractional position. Repeat wrapping.
        [[nodiscard]] inline Texel8 SampleTexture8Bilinear(const std::uint32_t* texels, int width, int height,
            V::type u, V::type v) {
            u = V::sub(u, _mm256_floor_ps(u));
            v = V::sub(v, _mm256_floor_ps(v));

            // Texel is centered at 0.5
            // Shifting by half a texel
            const V::type half = V::set1(0.5f);
            const V::type fx = V::sub(V::mul(u, V::set1(static_cast<float>(width))), half);
            const V::type fy = V::sub(V::mul(V::sub(V::set1(1.0f), v), V::set1(static_cast<float>(height))), half);
            const V::type flx = _mm256_floor_ps(fx);
            const V::type fly = _mm256_floor_ps(fy);
            const V::type tx = V::sub(fx, flx);
            const V::type ty = V::sub(fy, fly);

            const TexelSIMD zero = _mm256_setzero_si256();
            const TexelSIMD one = _mm256_set1_epi32(1);
            const TexelSIMD wMax = _mm256_set1_epi32(width - 1);
            const TexelSIMD hMax = _mm256_set1_epi32(height - 1);

            // x0 is in [-1, width-1], x1 in [0, width]
            // Wrapping the two edge cases.
            TexelSIMD x0 = _mm256_cvttps_epi32(flx);
            TexelSIMD y0 = _mm256_cvttps_epi32(fly);
            TexelSIMD x1 = _mm256_add_epi32(x0, one);
            TexelSIMD y1 = _mm256_add_epi32(y0, one);

            x0 = _mm256_blendv_epi8(x0, wMax, _mm256_cmpgt_epi32(zero, x0));
            y0 = _mm256_blendv_epi8(y0, hMax, _mm256_cmpgt_epi32(zero, y0));
            x1 = _mm256_andnot_si256(_mm256_cmpgt_epi32(x1, wMax), x1);
            y1 = _mm256_andnot_si256(_mm256_cmpgt_epi32(y1, hMax), y1);

            const TexelSIMD stride = _mm256_set1_epi32(width);
            const TexelSIMD row0 = _mm256_mullo_epi32(y0, stride);
            const TexelSIMD row1 = _mm256_mullo_epi32(y1, stride);

            // The four nearest texels, see:
            // https://github.com/rodrigogq/3d-cpu-engine/tree/main/chapter-12-clipping-filtering#128--bilinear-filtering
            const int* base = reinterpret_cast<const int*>(texels);
            const TexelSIMD t00 = _mm256_i32gather_epi32(base, _mm256_add_epi32(row0, x0), 4);
            const TexelSIMD t10 = _mm256_i32gather_epi32(base, _mm256_add_epi32(row0, x1), 4);
            const TexelSIMD t01 = _mm256_i32gather_epi32(base, _mm256_add_epi32(row1, x0), 4);
            const TexelSIMD t11 = _mm256_i32gather_epi32(base, _mm256_add_epi32(row1, x1), 4);

            return {
                BilerpChannel<24>(t00, t10, t01, t11, tx, ty),
                BilerpChannel<16>(t00, t10, t01, t11, tx, ty),
                BilerpChannel<8>(t00, t10, t01, t11, tx, ty),
                BilerpChannel<0>(t00, t10, t01, t11, tx, ty)
            };
        }

        // Scalar version, same maths, for the 0..7 pixel tail.
        struct TexelF { float r, g, b, a; };

        [[nodiscard]] inline TexelF SampleTextureBilinear(const Texture& tex, float u, float v) {
            const int w = tex.width;
            const int h = tex.height;

            u -= std::floor(u);
            v -= std::floor(v);

            const float fx = u * static_cast<float>(w) - 0.5f;
            const float fy = (1.0f - v) * static_cast<float>(h) - 0.5f;
            const float flx = std::floor(fx), fly = std::floor(fy);
            const float tx = fx - flx, ty = fy - fly;

            int x0 = static_cast<int>(flx), y0 = static_cast<int>(fly);
            int x1 = x0 + 1, y1 = y0 + 1;
            if (x0 < 0) x0 = w - 1;
            if (y0 < 0) y0 = h - 1;
            if (x1 >= w) x1 = 0;
            if (y1 >= h) y1 = 0;

            const std::uint32_t* p = tex.pixels.data();
            const std::uint32_t t00 = p[y0 * w + x0], t10 = p[y0 * w + x1];
            const std::uint32_t t01 = p[y1 * w + x0], t11 = p[y1 * w + x1];

            const auto ch = [&](const int shift) {
                const auto f = [&](const std::uint32_t t) { return static_cast<float>((t >> shift) & 0xFF); };
                const float top    = f(t00) + (f(t10) - f(t00)) * tx;
                const float bottom = f(t01) + (f(t11) - f(t01)) * tx;
                return top + (bottom - top) * ty;
            };
            return { ch(24), ch(16), ch(8), ch(0) };
        }
    }

    template <FragmentShader_T S>
    void Rasterizer::Rasterize(const PrimitiveBatch& batch, const FrameBuffer& fb, DepthBuffer& db,
        const S& shader, const Texture& texture) const {
        if (fb.width == 0 || fb.height == 0) return;

        using Clock = StageTimer::Clock;

        const int w = fb.width;
        const int h = fb.height;
        const int tilesX = (w + kTileSize - 1) / kTileSize;
        const int tilesY = (h + kTileSize - 1) / kTileSize;
        const auto tileCount = static_cast<std::uint32_t>(tilesX * tilesY);

        const auto t0 = Clock::now();
        BinData& bins = Detail::FrameBins();
        BinPrimitives(batch, tilesX, tilesY, bins);
        const auto t1 = Clock::now();

        // Each thread grabs the next free tile until none are left.
        std::atomic<std::uint32_t> nextTile{ 0 };
        float* depth = db.pixels.data();

        const std::function<void()> worker = [&] {
            thread_local std::vector<SortItem> scratch;

            for (;;) {
                const std::uint32_t index = nextTile.fetch_add(1, std::memory_order_relaxed);
                if (index >= tileCount) return;

                const int x0 = static_cast<int>(index % tilesX) * kTileSize;
                const int y0 = static_cast<int>(index / tilesX) * kTileSize;
                const Tile tile{ x0, y0, std::min(x0 + kTileSize, w), std::min(y0 + kTileSize, h) };

                ClearTile(tile, depth, fb.pixels, fb.width);

                // This tile's list is only touched by this thread: sort it in place.
                SortItem* begin = bins.items.data() + bins.tileBegin[index];
                SortItem* split = bins.items.data() + bins.tileSplit[index];
                SortItem* end   = bins.items.data() + bins.tileBegin[index + 1];
                SortItems(std::span<SortItem>{ begin, split }, scratch); // opaque, front to back
                SortItems(std::span<SortItem>{ split, end }, scratch);   // transparent, back to front

                for (const SortItem* it = begin; it != end; ++it) {
                    const Primitive& p = batch[it->index];
                    if (p.transparent) DrawInTile<true>(p, it->index, tile, depth, fb.pixels, fb.width, shader, texture);
                    else               DrawInTile<false>(p, it->index, tile, depth, fb.pixels, fb.width, shader, texture);
                }
            }
        };

        GetPool().Run(worker);
        const auto t2 = Clock::now();

        gTimer.binMs  += StageTimer::Ms(t0, t1);
        gTimer.drawMs += StageTimer::Ms(t1, t2);
        gTimer.EndFrame(batch.Size());
    }

    template <bool Transparent, FragmentShader_T S>
    void Rasterizer::DrawInTile(const Primitive& t, std::uint32_t primitiveId, const Tile& tile,
        float* depth, std::uint32_t* pixels, std::uint32_t width, const S& shader, const Texture& texture) const {
        using namespace Detail;

        constexpr float nearPlane = 0.01f;
        constexpr float farPlane = 100.0f;

        const int xa = std::max(t.x0, tile.x0), xb = std::min(t.x1, tile.x1);
        const int ya = std::max(t.y0, tile.y0), yb = std::min(t.y1, tile.y1);

        const V::type ramp = V::load(kRamp);
        const V::type one = V::set1(1.0f);
        const V::type inv255 = V::set1(1.0f / 255.0f);
        const V::type kv0 = V::set1(t.kv[0]), kv1 = V::set1(t.kv[1]), kv2 = V::set1(t.kv[2]);

        // Start value of an attribute for each lane: start + dx * (0..7).
        const auto lanes = [&](const Plane& p, float start) {
            return V::add(V::set1(start), V::mul(V::set1(p.dx), ramp));
        };
        const auto step = [](const Plane& p) { return V::set1(p.dx * kLanes); };

        FragmentInput frag;
        frag.i0 = t.i0;
        frag.i1 = t.i1;
        frag.i2 = t.i2;
        frag.primitiveId = primitiveId;
        const bool hasTexture = texture.width > 0 && texture.height > 0 && !texture.pixels.empty();

        for (int y = ya; y < yb; ++y) {
            const auto row = static_cast<float>(y - t.y0);

            auto lo = static_cast<float>(xa - t.x0);
            auto hi = static_cast<float>(xb - t.x0);
            if (!ClipSpan(t.w0.At(row), t.w0.dx, lo, hi)) continue;
            if (!ClipSpan(t.w1.At(row), t.w1.dx, lo, hi)) continue;
            if (!ClipSpan(t.w2.At(row), t.w2.dx, lo, hi)) continue;

            // near / far planes: z is linear on the row
            if (Projection == CameraType::Perspective) {
                if (!ClipSpan(t.z.At(row) - nearPlane, t.z.dx, lo, hi)) continue;
                if (!ClipSpan(farPlane - t.z.At(row), -t.z.dx, lo, hi)) continue;
            }

            float* depthRow = depth + static_cast<std::size_t>(y) * width;
            std::uint32_t* pixelRow = pixels + static_cast<std::size_t>(y) * width;

            int x = t.x0 + static_cast<int>(lo);
            const int end = t.x0 + static_cast<int>(hi);
            frag.y = y;

            // ---- SIMD part: 8 pixels per iteration ----
            if (x + kLanes <= end) {
                const auto l = static_cast<float>(x - t.x0);
                V::type vz = lanes(t.z, t.z.At(row, l));
                V::type vk = lanes(t.k, t.k.At(row, l));
                V::type vu = lanes(t.u, t.u.At(row, l));
                V::type vv = lanes(t.v, t.v.At(row, l));
                V::type vw0 = lanes(t.w0, t.w0.At(row, l));
                V::type vw1 = lanes(t.w1, t.w1.At(row, l));
                V::type vw2 = lanes(t.w2, t.w2.At(row, l));
                const V::type sz = step(t.z), sk = step(t.k);
                const V::type su = step(t.u), sv = step(t.v);
                const V::type s0 = step(t.w0), s1 = step(t.w1), s2 = step(t.w2);

                alignas(32) float b0[kLanes], b1[kLanes], b2[kLanes], zs[kLanes];
                alignas(32) float outR[kLanes] = {}, outG[kLanes] = {}, outB[kLanes] = {}, outA[kLanes] = {};

                for (; x + kLanes <= end; x += kLanes) {
                    const V::type oldDepth = V::load(depthRow + x);
                    const V::type mask = DepthMask(DepthTest, vz, oldDepth);
                    const auto bits = V::movemask(mask);

                    // No lane passes: skip the division, the shader and the stores.
                    if (bits != 0) {
                        // Perspective-correct weights: w_i * (1 / w_i) / (sum of them).
                        const V::type inv = V::div(one, vk);
                        V::store(b0, V::mul(V::mul(vw0, kv0), inv));
                        V::store(b1, V::mul(V::mul(vw1, kv1), inv));
                        V::store(b2, V::mul(V::mul(vw2, kv2), inv));
                        V::store(zs, vz);

                        // The shader is scalar: call it for the lanes that passed.
                        for (int lane = 0; lane < kLanes; ++lane) {
                            if (!(bits & (1 << lane))) continue;
                            frag.x = x + lane;
                            frag.depth = zs[lane];
                            frag.b0 = b0[lane];
                            frag.b1 = b1[lane];
                            frag.b2 = b2[lane];
                            const PixelOutput c = shader(frag);
                            outR[lane] = c.r;
                            outG[lane] = c.g;
                            outB[lane] = c.b;
                            outA[lane] = c.a;
                        }

                        V::type sr = V::load(outR);
                        V::type sg = V::load(outG);
                        V::type sb = V::load(outB);
                        V::type sa = V::load(outA);

                        if (hasTexture) {
                            if (BilinearFiltering) {
                                const Texel8 texel = SampleTexture8Bilinear(texture.pixels.data(), texture.width, texture.height, V::mul(vu, inv), V::mul(vv, inv));
                                sr = V::mul(sr, V::mul(texel.r, inv255));
                                sg = V::mul(sg, V::mul(texel.g, inv255));
                                sb = V::mul(sb, V::mul(texel.b, inv255));
                                sa = V::mul(sa, V::mul(texel.a, inv255));
                            }
                            else {
                                const TexelSIMD texel = SampleTexture8(texture.pixels.data(), texture.width, texture.height, V::mul(vu, inv), V::mul(vv, inv));
                                sr = V::mul(sr, V::mul(Channel<24>(texel), inv255));
                                sg = V::mul(sg, V::mul(Channel<16>(texel), inv255));
                                sb = V::mul(sb, V::mul(Channel<8>(texel), inv255));
                                sa = V::mul(sa, V::mul(Channel<0>(texel), inv255));
                            }
                        }

                        if constexpr (!Transparent) {
                            V::store(depthRow + x, V::select(oldDepth, vz, mask)); // only opaque writes depth
                            StoreMasked(pixelRow + x, PackBGRA(sr, sg, sb, V::set1(255.0f)), mask);
                        }
                        else {
                            const TexelSIMD old = _mm256_loadu_si256(reinterpret_cast<const TexelSIMD*>(pixelRow + x));

                            const V::type a = Clamp01(V::mul(sa, inv255));
                            const V::type half = V::set1(0.5f);
                            const auto mix = [&](V::type d, V::type s) {
                                return V::add(Blend(AlphaBlend, d, s, a), half);
                                };

                            const TexelSIMD packed = PackBGRA(
                                mix(Channel<16>(old), sr),
                                mix(Channel<8>(old), sg),
                                mix(Channel<0>(old), sb),
                                mix(Channel<24>(old), sa));
                            StoreMasked(pixelRow + x, packed, mask);
                        }
                    }

                    vz = V::add(vz, sz); vk = V::add(vk, sk);
                    vu = V::add(vu, su); vv = V::add(vv, sv);
                    vw0 = V::add(vw0, s0); vw1 = V::add(vw1, s1); vw2 = V::add(vw2, s2);
                }
            }

            // ---- Scalar tail: the 0..7 pixels left ----
            const auto tailLo = static_cast<float>(x - t.x0);
            float z = t.z.At(row, tailLo);
            float k = t.k.At(row, tailLo);
            float u = t.u.At(row, tailLo);
            float v = t.v.At(row, tailLo);
            float w0 = t.w0.At(row, tailLo);
            float w1 = t.w1.At(row, tailLo);
            float w2 = t.w2.At(row, tailLo);

            for (; x < end; ++x) {
                if (DepthPass(DepthTest, z, depthRow[x])) {
                    const float inv = 1.0f / k;
                    frag.x = x;
                    frag.depth = z;
                    frag.b0 = w0 * t.kv[0] * inv;
                    frag.b1 = w1 * t.kv[1] * inv;
                    frag.b2 = w2 * t.kv[2] * inv;
                    const PixelOutput c = shader(frag);

                    float sr = c.r, sg = c.g, sb = c.b, sa = c.a;
                    if (hasTexture) {
                        if (BilinearFiltering) {
                            const TexelF texel = SampleTextureBilinear(texture, u * inv, v * inv);
                            sr *= texel.r / 255.0f;
                            sg *= texel.g / 255.0f;
                            sb *= texel.b / 255.0f;
                            sa *= texel.a / 255.0f;
                        }
                        else {
                            const std::uint32_t texel = SampleTexture(texture, u * inv, v * inv);
                            sr *= static_cast<float>((texel >> 24) & 0xFF) / 255.0f;
                            sg *= static_cast<float>((texel >> 16) & 0xFF) / 255.0f;
                            sb *= static_cast<float>((texel >> 8) & 0xFF) / 255.0f;
                            sa *= static_cast<float>(texel & 0xFF) / 255.0f;
                        }
                    }

                    if constexpr (!Transparent) {
                        depthRow[x] = z;
                        pixelRow[x] = PackColor(ToByte(sr), ToByte(sg), ToByte(sb));
                    } else {
                        const std::uint32_t pixel = pixelRow[x];
                        const float a = std::clamp(sa / 255.0f, 0.0f, 1.0f);
                        const auto da = static_cast<float>((pixel >> 24) & 0xFF);
                        const auto dr = static_cast<float>((pixel >> 16) & 0xFF);
                        const auto dg = static_cast<float>((pixel >> 8) & 0xFF);
                        const auto dB = static_cast<float>(pixel & 0xFF);
                        pixelRow[x] = PackColor(
                            ToByte(Blend(AlphaBlend, dr, sr, a) + 0.5f),
                            ToByte(Blend(AlphaBlend, dg, sg, a) + 0.5f),
                            ToByte(Blend(AlphaBlend, dB, sb, a) + 0.5f),
                            ToByte(Blend(AlphaBlend, da, sa, a) + 0.5f)
                        );
                    }
                }

                // Step to the next pixel.
                z += t.z.dx; k += t.k.dx;
                u += t.u.dx; v += t.v.dx;
                w0 += t.w0.dx; w1 += t.w1.dx; w2 += t.w2.dx;
            }
        }
    }
}
