#pragma once

#include <Renderer/Types.hpp>
#include <Utils/SIMDTraits.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Pumpkin::Detail {
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
}
