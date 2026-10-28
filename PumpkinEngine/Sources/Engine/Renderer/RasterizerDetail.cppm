module;

#include <immintrin.h>

export module Pumpkin.Renderer.RasterizerDetail;

import Pumpkin.Renderer.Types;
import Pumpkin.Renderer.Passes.Binning;
import Pumpkin.Utils.SIMDTraits;

import std;

export namespace Pumpkin::Detail {
    using V = simd_traits<float, 256>;
    inline constexpr int kLanes = static_cast<int>(V::width);

    alignas(32) inline constexpr float kRamp[kLanes] = { 0, 1, 2, 3, 4, 5, 6, 7 };

    // ---- 8 pixels at a time ----

    [[nodiscard]] inline V::type Clamp01(V::type x) {
        return V::min(V::max(x, V::set1(0.0f)), V::set1(1.0f));
    }

    // 8 floats in [0, 255] per channel -> 8 BGRA8 pixels (truncates, like ToByte).
    [[nodiscard]] inline __m256i PackBGRA(V::type r, V::type g, V::type b, V::type a) {
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
    [[nodiscard]] inline V::type Channel(__m256i pixels) {
        const __m256i c = _mm256_and_si256(_mm256_srli_epi32(pixels, Shift), _mm256_set1_epi32(255));
        return _mm256_cvtepi32_ps(c);
    }

    // Writes `value` only in the lanes where `mask` is set.
    inline void StoreMasked(std::uint32_t* dst, __m256i value, V::type mask) {
        const __m256i old = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(dst));
        const __m256i mixed = _mm256_blendv_epi8(old, value, _mm256_castps_si256(mask));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst), mixed);
    }

    [[nodiscard]] inline V::type DepthMask(DepthTestMode mode, V::type z, V::type d) {
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

    // ---- one pixel at a time ----

    // Keeps only the x values where `base + dx * x >= 0`.
    // `lo` and `hi` are the current span. Returns false if it becomes empty.
    [[nodiscard]] inline bool ClipSpan(float base, float dx, float& lo, float& hi) {
        if (dx > 0.0f)       lo = std::max(lo, std::ceil(-base / dx));
        else if (dx < 0.0f)  hi = std::min(hi, std::floor(-base / dx) + 1.0f);
        else if (base < 0.0f) return false;
        return lo < hi;
    }

    // float -> byte (truncates). Goes through int32 because it is faster.
    [[nodiscard]] inline std::uint32_t ToByte(float value) {
        return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
    }

    [[nodiscard]] inline std::uint32_t PackColor(std::uint32_t r, std::uint32_t g, std::uint32_t b,
        std::uint32_t a = 255u) {
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    [[nodiscard]] inline bool DepthPass(DepthTestMode mode, float z, float d) {
        const auto aboutEqual = [](float a, float b) { return std::abs(a - b) < 1e-6f; };
        switch (mode) {
        case DepthTestMode::None:         return true;
        case DepthTestMode::Less:         return z < d;
        case DepthTestMode::LessEqual:    return z <= d;
        case DepthTestMode::Greater:      return z > d;
        case DepthTestMode::GreaterEqual: return z >= d;
        case DepthTestMode::Equal:        return aboutEqual(z, d);
        case DepthTestMode::NotEqual:     return !aboutEqual(z, d);
        }
        return false;
    }

    // 8 pixels: blends one colour channel (all values in [0, 255]); a is alpha in [0, 1].
    [[nodiscard]] inline V::type Blend(BlendMode mode, V::type d, V::type s, V::type a) {
        switch (mode) {
        case BlendMode::Alpha:    return V::add(d, V::mul(V::sub(s, d), a));
        case BlendMode::Additive: return V::min(V::add(d, V::mul(s, a)), V::set1(255.0f));
        case BlendMode::Multiply: {
            const V::type m = V::mul(V::mul(d, s), V::set1(1.0f / 255.0f));
            return V::add(d, V::mul(V::sub(m, d), a));
        }
        case BlendMode::None:     return s;
        }
        return s;
    }

    // One pixel, same formulas.
    [[nodiscard]] inline float Blend(BlendMode mode, float d, float s, float a) {
        switch (mode) {
        case BlendMode::Alpha:    return d + (s - d) * a;
        case BlendMode::Additive: return std::min(d + s * a, 255.0f);
        case BlendMode::Multiply: return d + (d * s / 255.0f - d) * a;
        case BlendMode::None:     return s;
        }
        return s;
    }

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

    // The four neighbouring texels of 8 pixels, plus the fractional position between them.
    struct BilinearTaps { TexelSIMD t00, t10, t01, t11; V::type tx, ty; };

    struct BilinearCoords { TexelSIMD i00, i10, i01, i11; V::type tx, ty; };

    [[nodiscard]] inline BilinearCoords MakeBilinearCoords(int width, int height, V::type u, V::type v) {
        u = V::sub(u, _mm256_floor_ps(u));
        v = V::sub(v, _mm256_floor_ps(v));

        // Texel is centered at 0.5
        // Shifting by half a texel
        const V::type half = V::set1(0.5f);
        const V::type fx = V::sub(V::mul(u, V::set1(static_cast<float>(width))), half);
        const V::type fy = V::sub(V::mul(V::sub(V::set1(1.0f), v), V::set1(static_cast<float>(height))), half);
        const V::type flx = _mm256_floor_ps(fx);
        const V::type fly = _mm256_floor_ps(fy);

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

        return {
            _mm256_add_epi32(row0, x0), // (x0, y0)
            _mm256_add_epi32(row0, x1), // (x1, y0)
            _mm256_add_epi32(row1, x0), // (x0, y1)
            _mm256_add_epi32(row1, x1), // (x1, y1)
            V::sub(fx, flx), V::sub(fy, fly)
        };
    }

    // Only the four gathers. Several textures with the same size can share one BilinearCoords.
    [[nodiscard]] inline BilinearTaps GatherTaps(const std::uint32_t* texels, const BilinearCoords& c) {
        const int* base = reinterpret_cast<const int*>(texels);
        return {
            _mm256_i32gather_epi32(base, c.i00, 4),
            _mm256_i32gather_epi32(base, c.i10, 4),
            _mm256_i32gather_epi32(base, c.i01, 4),
            _mm256_i32gather_epi32(base, c.i11, 4),
            c.tx, c.ty
        };
    }

    [[nodiscard]] inline BilinearTaps GatherBilinear8(const std::uint32_t* texels, int width, int height,
        V::type u, V::type v) {
        return GatherTaps(texels, MakeBilinearCoords(width, height, u, v));
    }

    // Fixed-point bilinear on the packed bytes.
    // Two channels share one 32-bit lane (16 bits each), weights are 0..256 and sum to exactly 256,
    // so a byte times a weight never overflows 16 bits. Result: 0..255 floats like the float path,
    // but fractions are quantised to 1/256 and the output is truncated to an integer.
    [[nodiscard]] inline Texel8 BilerpAllChannelsFixed(const BilinearTaps& k) {
        const __m256i mask = _mm256_set1_epi32(0x00FF00FF);
        const __m256i c256 = _mm256_set1_epi32(256);

        const __m256i fx = _mm256_cvttps_epi32(V::mul(k.tx, V::set1(256.0f))); // 0..255
        const __m256i fy = _mm256_cvttps_epi32(V::mul(k.ty, V::set1(256.0f)));
        const __m256i ix = _mm256_sub_epi32(c256, fx);
        const __m256i iy = _mm256_sub_epi32(c256, fy);

        const __m256i w00 = _mm256_srli_epi32(_mm256_mullo_epi32(ix, iy), 8);
        const __m256i w10 = _mm256_srli_epi32(_mm256_mullo_epi32(fx, iy), 8);
        const __m256i w01 = _mm256_srli_epi32(_mm256_mullo_epi32(ix, fy), 8);
        const __m256i w11 = _mm256_sub_epi32(_mm256_sub_epi32(_mm256_sub_epi32(c256, w00), w10), w01);

        // Put the weight in both 16-bit halves of each lane.
        const auto dup = [](__m256i w) { return _mm256_or_si256(w, _mm256_slli_epi32(w, 16)); };
        const __m256i d00 = dup(w00), d10 = dup(w10), d01 = dup(w01), d11 = dup(w11);

        // lo holds the bytes at shifts 0 (low half) and 16 (high half);
        // hi holds the bytes at shifts 8 (low half) and 24 (high half).
        const auto accumulate = [&](__m256i t, __m256i w, __m256i& lo, __m256i& hi, bool first) {
            const __m256i l = _mm256_mullo_epi16(_mm256_and_si256(t, mask), w);
            const __m256i h = _mm256_mullo_epi16(_mm256_and_si256(_mm256_srli_epi32(t, 8), mask), w);
            lo = first ? l : _mm256_add_epi16(lo, l);
            hi = first ? h : _mm256_add_epi16(hi, h);
        };

        __m256i lo, hi;
        accumulate(k.t00, d00, lo, hi, true);
        accumulate(k.t10, d10, lo, hi, false);
        accumulate(k.t01, d01, lo, hi, false);
        accumulate(k.t11, d11, lo, hi, false);

        lo = _mm256_srli_epi16(lo, 8);
        hi = _mm256_srli_epi16(hi, 8);

        const __m256i low16 = _mm256_set1_epi32(0xFFFF);
        return {
            _mm256_cvtepi32_ps(_mm256_srli_epi32(hi, 16)),    // shift 24
            _mm256_cvtepi32_ps(_mm256_srli_epi32(lo, 16)),    // shift 16
            _mm256_cvtepi32_ps(_mm256_and_si256(hi, low16)),  // shift 8
            _mm256_cvtepi32_ps(_mm256_and_si256(lo, low16))   // shift 0
        };
    }

    // Scalar versions, same maths, for the 0..7 pixel tail.
    struct TexelF { float r, g, b, a; };
    struct BilinearTapsF { std::uint32_t t00, t10, t01, t11; float tx, ty; };

    [[nodiscard]] inline BilinearTapsF GatherBilinear(const Texture& tex, float u, float v) {
        const int w = tex.width;
        const int h = tex.height;

        u -= std::floor(u);
        v -= std::floor(v);

        const float fx = u * static_cast<float>(w) - 0.5f;
        const float fy = (1.0f - v) * static_cast<float>(h) - 0.5f;
        const float flx = std::floor(fx), fly = std::floor(fy);

        int x0 = static_cast<int>(flx), y0 = static_cast<int>(fly);
        int x1 = x0 + 1, y1 = y0 + 1;
        if (x0 < 0) x0 = w - 1;
        if (y0 < 0) y0 = h - 1;
        if (x1 >= w) x1 = 0;
        if (y1 >= h) y1 = 0;

        const std::uint32_t* p = tex.pixels.data();
        return { p[y0 * w + x0], p[y0 * w + x1], p[y1 * w + x0], p[y1 * w + x1], fx - flx, fy - fly };
    }

    template <int Shift>
    [[nodiscard]] inline float BilerpChannelF(const BilinearTapsF& k) {
        const auto f = [](const std::uint32_t t) { return static_cast<float>((t >> Shift) & 0xFF); };
        const float top    = f(k.t00) + (f(k.t10) - f(k.t00)) * k.tx;
        const float bottom = f(k.t01) + (f(k.t11) - f(k.t01)) * k.tx;
        return top + (bottom - top) * k.ty;
    }

    [[nodiscard]] inline TexelF SampleTextureBilinear(const Texture& tex, float u, float v) {
        const BilinearTapsF k = GatherBilinear(tex, u, v);
        return { BilerpChannelF<24>(k), BilerpChannelF<16>(k), BilerpChannelF<8>(k), BilerpChannelF<0>(k) };
    }

    template <int Shift>
    [[nodiscard]] inline float SampleTextureBilinearChannel(const Texture& tex, float u, float v) {
        return BilerpChannelF<Shift>(GatherBilinear(tex, u, v));
    }

    [[nodiscard]] inline V::type SqrtMaxEps(V::type len2) {
        return _mm256_sqrt_ps(_mm256_max_ps(len2, V::set1(1e-8f)));
    }

    // Loads 8 packed BGRA pixels (unaligned).
    [[nodiscard]] inline TexelSIMD LoadPixels(const std::uint32_t* src) {
        return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src));
    }
}
