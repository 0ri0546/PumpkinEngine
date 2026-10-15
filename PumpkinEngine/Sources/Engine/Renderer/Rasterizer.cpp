#include "Rasterizer.hpp"

#include <Renderer/Binning.hpp>
#include <Renderer/Types.hpp>
#include <Utils/Utils.hpp>
#include <Utils/ThreadPool.hpp>
#include <Utils/SIMDTraits.hpp>
#include <Utils/StageTimer.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <vector>

#include <Math/Mat.h>

namespace Pumpkin {
    // Helpers used only by this file are `static`.

    using V = simd_traits<float, 256>;
    static constexpr int kLanes = static_cast<int>(V::width);
    static constexpr float kFrontFaceSign = 1.0f;

    alignas(32) static constexpr float kRamp[kLanes] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    [[nodiscard]] static V::type Clamp01(V::type x) {
        return V::min(V::max(x, V::set1(0.0f)), V::set1(1.0f));
    }

    // 8 floats in [0, 255] per channel -> 8 BGRA8 pixels (truncates, like ToByte).
    [[nodiscard]] static __m256i PackBGRA(V::type r, V::type g, V::type b, V::type a) {
        const __m256i ri = _mm256_cvttps_epi32(r);
        const __m256i gi = _mm256_cvttps_epi32(g);
        const __m256i bi = _mm256_cvttps_epi32(b);
        const __m256i ai = _mm256_cvttps_epi32(a);
        return _mm256_or_si256(
            _mm256_or_si256(_mm256_slli_epi32(ai, 24), _mm256_slli_epi32(ri, 16)),
            _mm256_or_si256(_mm256_slli_epi32(gi, 8), bi));
    }

    // One channel of 8 packed pixels as floats [0, 255].
    template <int Shift>
    [[nodiscard]] static V::type Channel(__m256i pixels) {
        const __m256i c = _mm256_and_si256(_mm256_srli_epi32(pixels, Shift), _mm256_set1_epi32(255));
        return _mm256_cvtepi32_ps(c);
    }

    // Writes `value` only in the lanes where `mask` is set.
    static void StoreMasked(uint32_t* dst, __m256i value, V::type mask) {
        const __m256i old = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst));
        const __m256i mixed = _mm256_blendv_epi8(old, value, _mm256_castps_si256(mask));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), mixed);
    }

    // Same tests as your scalar ternary chain, 8 lanes at once.
    [[nodiscard]] static V::type DepthMask(DepthTestMode mode, V::type z, V::type d) {
        const V::type diff = V::sub(z, d);
        const V::type absDiff = V::max(diff, V::neg(diff));
        const V::type eps = V::set1(1e-6f);

        switch (mode) {
        case DepthTestMode::None:         return V::compareEqual(z, z); // all lanes true
        case DepthTestMode::Less:         return V::cmp_lt(z, d);
        case DepthTestMode::LessEqual:    return V::cmp_le(z, d);
        case DepthTestMode::Greater:      return V::cmp_gt(z, d);
        case DepthTestMode::GreaterEqual: return V::cmp_ge(z, d);
        case DepthTestMode::Equal:        return V::cmp_lt(absDiff, eps);
        case DepthTestMode::NotEqual:     return V::cmp_ge(absDiff, eps);
        }
        return V::cmp_lt(z, z); // all lanes false
    }

    [[nodiscard]] static V::type BlendFactor(BlendMode mode, V::type dst, V::type src) {
        switch (mode) {
        case BlendMode::Alpha:    return V::add(dst, V::mul(V::sub(src, dst), src));
        case BlendMode::Additive: return V::min(V::add(dst, src), V::set1(1.0f));
        case BlendMode::Multiply: return V::mul(dst, src);
        case BlendMode::None:     return src;
        }
        return src;
    }

    // Keeps only the x values where `base + dx * x >= 0`.
    // `lo` and `hi` are the current span. Returns false if it becomes empty.
    [[nodiscard]] static bool ClipSpan(float base, float dx, float& lo, float& hi) {
        if (dx > 0.0f)       lo = std::max(lo, std::ceil(-base / dx));
        else if (dx < 0.0f)  hi = std::min(hi, std::floor(-base / dx) + 1.0f);
        else if (base < 0.0f) return false;
        return lo < hi;
    }

    // float -> byte (truncates). Goes through int32 because it is faster.
    [[nodiscard]] static uint32_t ToByte(float value) {
        return static_cast<uint32_t>(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] static uint32_t PackColor(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 255u) {
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    // Returns w0 * a + w1 * b + w2 * c for start, dx and dy.
    static Plane Mix(const Plane& w0, const Plane& w1, const Plane& w2,
        float a, float b, float c) {
        return {
            w0.start * a + w1.start * b + w2.start * c,
            w0.dx * a + w1.dx * b + w2.dx * c,
            w0.dy * a + w1.dy * b + w2.dy * c
        };
    }

    /// @brief Clears and draws a whole frame.
    void Rasterizer::RasterizeFrame(
        std::span<const Vec4f> v,
        std::span<const uint32_t> indices,
        std::span<const Color> colors,
        const FrameBuffer& fb,
        DepthBuffer& db
    ) {
        if (fb.width == 0 || fb.height == 0) return;

        using Clock = StageTimer::Clock;

        const int w = fb.width;
        const int h = fb.height;
        const int tilesX = (w + kTileSize - 1) / kTileSize;
        const int tilesY = (h + kTileSize - 1) / kTileSize;
        const auto tileCount = static_cast<uint32_t>(tilesX * tilesY);

        // Reused every frame so we don't allocate again.
        static std::vector<std::vector<Triangle>> chunks;
        static BinData bins;

        const auto t0 = Clock::now();
        const bool validInput = indices.size() % 3 == 0 && colors.size() >= v.size();
        if (validInput) BuildTriangles(v, indices, colors, w, h, chunks);
        else            chunks.clear(); // bad input: just clear the screen
        const auto t1 = Clock::now();

        BinTriangles(chunks, tilesX, tilesY, bins);
        const auto t2 = Clock::now();

        // Each thread grabs the next free tile until none are left.
        std::atomic<uint32_t> nextTile{ 0 };
        float* depth = db.pixels.data();

        const std::function<void()> worker = [&] {
            thread_local std::vector<SortItem> scratch;

            for (;;) {
                const uint32_t index = nextTile.fetch_add(1, std::memory_order_relaxed);
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
                    const Triangle& t = chunks[it->index >> kChunkShift][it->index & (kSetupChunk - 1)];
                    if (t.transparent) DrawInTile<true>(t, tile, depth, fb.pixels, fb.width);
                    else               DrawInTile<false>(t, tile, depth, fb.pixels, fb.width);
                }
            }
        };

        GetPool().Run(worker);
        const auto t3 = Clock::now();

        size_t triangleCount = 0;
        for (const auto& c : chunks) triangleCount += c.size();

        gTimer.buildMs += StageTimer::Ms(t0, t1);
        gTimer.binMs   += StageTimer::Ms(t1, t2);
        gTimer.drawMs  += StageTimer::Ms(t2, t3);
        gTimer.EndFrame(triangleCount);
    }

    void Rasterizer::BuildTriangle(std::span<const Vec4f> v, std::span<const uint32_t> indices,
        std::span<const Color> colors, size_t tri,
        int width, int height, std::vector<Triangle>& dst) const {
        const size_t i = tri * 3 + 2;
        const uint32_t ia = indices[i - 2], ib = indices[i - 1], ic = indices[i];

        const auto& pa = v[ia];
        const auto& pb = v[ib];
        const auto& pc = v[ic];

        const float ax = pa[0], ay = pa[1], az = pa[2], ak = pa[3];
        const float bx = pb[0], by = pb[1], bz = pb[2], bk = pb[3];
        const float cx = pc[0], cy = pc[1], cz = pc[2], ck = pc[3];

        // Twice the signed area. Zero means a flat triangle.
        const float area = (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
        if (area == 0.0f) return;
        if (Culling == CullMode::Back && area * kFrontFaceSign < 0.0f) return;
        if (Culling == CullMode::Front && area * kFrontFaceSign > 0.0f) return;
        const float invArea = 1.0f / area;

        // Bounding box, clamped to the screen (in float, before the int cast).
        const float fx0 = std::max(0.0f, std::floor(std::min({ ax, bx, cx })));
        const float fy0 = std::max(0.0f, std::floor(std::min({ ay, by, cy })));
        const float fx1 = std::min(static_cast<float>(width), std::ceil(std::max({ ax, bx, cx })) + 1.0f);
        const float fy1 = std::min(static_cast<float>(height), std::ceil(std::max({ ay, by, cy })) + 1.0f);
        if (fx0 >= fx1 || fy0 >= fy1) return; // fully offscreen

        Triangle t{};
        t.x0 = static_cast<int>(fx0);
        t.y0 = static_cast<int>(fy0);
        t.x1 = static_cast<int>(fx1);
        t.y1 = static_cast<int>(fy1);

        // Center of the first pixel.
        const float sx = static_cast<float>(t.x0) + 0.5f;
        const float sy = static_cast<float>(t.y0) + 0.5f;

        // Weight of the vertex opposite to the edge p -> q.
        const auto weightPlane = [&](float px, float py, float qx, float qy) {
            return Plane{
                ((sx - px) * (qy - py) - (sy - py) * (qx - px)) * invArea,
                (qy - py) * invArea,
                -(qx - px) * invArea
            };
        };
        t.w0 = weightPlane(bx, by, cx, cy);
        t.w1 = weightPlane(cx, cy, ax, ay);
        t.w2 = weightPlane(ax, ay, bx, by);

        const Color& c0 = colors[ia];
        const Color& c1 = colors[ib];
        const Color& c2 = colors[ic];

        t.transparent = c0.a < 255 || c1.a < 255 || c2.a < 255;
        t.zKey = az + bz + cz;

        t.z = Mix(t.w0, t.w1, t.w2, az, bz, cz);

        // Perspective-correct color = (color * k) / k.
        // Both parts are linear on screen, so we divide once per pixel.
        t.k = Mix(t.w0, t.w1, t.w2, ak, bk, ck);
        t.r = Mix(t.w0, t.w1, t.w2, ak * c0.r, bk * c1.r, ck * c2.r);
        t.g = Mix(t.w0, t.w1, t.w2, ak * c0.g, bk * c1.g, ck * c2.g);
        t.b = Mix(t.w0, t.w1, t.w2, ak * c0.b, bk * c1.b, ck * c2.b);
        t.a = Mix(t.w0, t.w1, t.w2, ak * c0.a, bk * c1.a, ck * c2.a);

        dst.push_back(t);
    }

    void Rasterizer::BuildTriangles(std::span<const Vec4f> v, std::span<const uint32_t> indices,
        std::span<const Color> colors, int width, int height,
        std::vector<std::vector<Triangle>>& chunks) const {
        const size_t triCount = indices.size() / 3;
        const size_t chunkCount = (triCount + kSetupChunk - 1) / kSetupChunk;
        chunks.resize(chunkCount);

        ParallelFor(GetPool(), chunkCount, [&](size_t c) {
            auto& dst = chunks[c];
            dst.clear(); // keeps memory, no new allocation
            const size_t begin = c * kSetupChunk;
            const size_t end = std::min(begin + kSetupChunk, triCount);
            for (size_t t = begin; t < end; ++t)
                BuildTriangle(v, indices, colors, t, width, height, dst);
        });
    }

    /// @brief Clears the color and depth of one tile.
    void Rasterizer::ClearTile(const Tile& tile, float* depth, uint32_t* pixels, uint32_t width) const {
        const auto count = static_cast<size_t>(tile.x1 - tile.x0);
        for (int y = tile.y0; y < tile.y1; ++y) {
            const size_t offset = static_cast<size_t>(y) * width + tile.x0;
            std::ranges::fill(std::span{ depth + offset, count }, std::numeric_limits<float>::max());
            std::ranges::fill(std::span{ pixels + offset, count }, ClearColor);
        }
    }

    template <bool Transparent>
    void Rasterizer::DrawInTile(const Triangle& t, const Tile& tile, float* depth, uint32_t* pixels, uint32_t width) const {
        constexpr float nearPlane = 0.01f;
        constexpr float farPlane = 100.0f;

        const int xa = std::max(t.x0, tile.x0), xb = std::min(t.x1, tile.x1);
        const int ya = std::max(t.y0, tile.y0), yb = std::min(t.y1, tile.y1);

        const V::type ramp = V::load(kRamp);
        const V::type one = V::set1(1.0f);
        const V::type inv255 = V::set1(1.0f / 255.0f);

        // Start value of an attribute for each lane: start + dx * (0..7).
        const auto lanes = [&](const Plane& p, float start) {
            return V::add(V::set1(start), V::mul(V::set1(p.dx), ramp));
        };
        const auto step = [](const Plane& p) { return V::set1(p.dx * kLanes); };

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

            float* depthRow = depth + static_cast<size_t>(y) * width;
            uint32_t* pixelRow = pixels + static_cast<size_t>(y) * width;

            int x = t.x0 + static_cast<int>(lo);
            const int end = t.x0 + static_cast<int>(hi);

            // ---- SIMD part: 8 pixels per iteration ----
            if (x + kLanes <= end) {
                const auto l = static_cast<float>(x - t.x0);
                V::type vz = lanes(t.z, t.z.At(row, l));
                V::type vk = lanes(t.k, t.k.At(row, l));
                V::type vr = lanes(t.r, t.r.At(row, l));
                V::type vg = lanes(t.g, t.g.At(row, l));
                V::type vb = lanes(t.b, t.b.At(row, l));
                V::type va = V::set1(0.0f);
                if constexpr (Transparent) va = lanes(t.a, t.a.At(row, l));

                const V::type sz = step(t.z), sk = step(t.k), sr_ = step(t.r), sg_ = step(t.g), sb_ = step(t.b);
                const V::type sa = Transparent ? step(t.a) : V::set1(0.0f);

                for (; x + kLanes <= end; x += kLanes) {
                    const V::type oldDepth = V::load(depthRow + x);
                    const V::type mask = DepthMask(DepthTest, vz, oldDepth);

                    // No lane passes: skip the division and the stores (SIMD early-out).
                    if (V::movemask(mask) != 0) {
                        const V::type inv = V::div(one, vk);
                        const V::type sr = V::mul(vr, inv);
                        const V::type sg = V::mul(vg, inv);
                        const V::type sb = V::mul(vb, inv);

                        if constexpr (!Transparent) {
                            V::store(depthRow + x, V::select(oldDepth, vz, mask));
                            StoreMasked(pixelRow + x, PackBGRA(sr, sg, sb, V::set1(255.0f)), mask);
                        }
                        else {
                            const __m256i old = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pixelRow + x));

                            const V::type dst = V::mul(Channel<24>(old), inv255);
                            const V::type src = Clamp01(V::mul(V::mul(va, inv), inv255));
                            const V::type out = Clamp01(BlendFactor(AlphaBlend, dst, src));
                            const V::type half = V::set1(0.5f);

                            // lerp(d, s, out) + 0.5
                            const auto mix = [&](V::type d, V::type s) {
                                return V::add(V::add(d, V::mul(V::sub(s, d), out)), half);
                            };

                            const __m256i packed = PackBGRA(
                                mix(Channel<16>(old), sr),
                                mix(Channel<8>(old), sg),
                                mix(Channel<0>(old), sb),
                                va);
                            StoreMasked(pixelRow + x, packed, mask);
                        }
                    }

                    vz = V::add(vz, sz); vk = V::add(vk, sk);
                    vr = V::add(vr, sr_); vg = V::add(vg, sg_); vb = V::add(vb, sb_);
                    if constexpr (Transparent) va = V::add(va, sa);
                }
            }

            // ---- Scalar tail: the 0..7 pixels left ----
            const auto tailLo = static_cast<float>(x - t.x0);
            float z = t.z.At(row, tailLo);
            float k = t.k.At(row, tailLo);
            float r = t.r.At(row, tailLo);
            float g = t.g.At(row, tailLo);
            float b = t.b.At(row, tailLo);
            float a = Transparent ? t.a.At(row, tailLo) : 0.0f;

            for (; x < end; ++x) {
                static const auto AboutEqual = [](float a, float b) { return std::abs(a - b) < 1e-6f; };
                if (DepthTest == DepthTestMode::None ? true :
                    DepthTest == DepthTestMode::Less ? z < depthRow[x] :
                    DepthTest == DepthTestMode::LessEqual ? z <= depthRow[x] :
                    DepthTest == DepthTestMode::Greater ? z > depthRow[x] :
                    DepthTest == DepthTestMode::GreaterEqual ? z >= depthRow[x] :
                    DepthTest == DepthTestMode::Equal ? AboutEqual(z, depthRow[x]) :
                    DepthTest == DepthTestMode::NotEqual ? !AboutEqual(z, depthRow[x]) : false) {
                    const float inv = 1.0f / k;
                    const float sr = r * inv, sg = g * inv, sb = b * inv; // source color

                    if constexpr (!Transparent) {
                        depthRow[x] = z; // only opaque triangles write depth
                        pixelRow[x] = PackColor(ToByte(sr), ToByte(sg), ToByte(sb));
                    }
                    else {
                        const uint32_t pixel = pixelRow[x];

                        const float dst = static_cast<float>((pixel >> 24) & 0xFF) / 255.0f;
                        const float src = std::clamp(a * inv / 255.0f, 0.0f, 1.0f);

                        float out = 0.0f;

                        switch (AlphaBlend)
                        {
                        case BlendMode::Alpha:
                            out = dst + (src - dst) * src;
                            break;

                        case BlendMode::Additive:
                            out = std::min(dst + src, 1.0f);
                            break;

                        case BlendMode::Multiply:
                            out = dst * src;
                            break;

                        case BlendMode::None:
                            out = src;
                            break;
                        }

                        out = std::clamp(out, 0.0f, 1.0f);

                        const auto dr = static_cast<float>((pixel >> 16) & 0xFF);
                        const auto dg = static_cast<float>((pixel >> 8) & 0xFF);
                        const auto db = static_cast<float>(pixel & 0xFF);
                        pixelRow[x] = PackColor(
                            ToByte(std::lerp(dr, sr, out) + 0.5f),
                            ToByte(std::lerp(dg, sg, out) + 0.5f),
                            ToByte(std::lerp(db, sb, out) + 0.5f),
                            static_cast<uint32_t>(a));
                    }
                }

                // Step to the next pixel.
                z += t.z.dx; k += t.k.dx;
                r += t.r.dx; g += t.g.dx; b += t.b.dx;
                if constexpr (Transparent) a += t.a.dx;
            }
        }
    }
}
