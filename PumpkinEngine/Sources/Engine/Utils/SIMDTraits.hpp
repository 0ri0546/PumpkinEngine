#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cmath>

#include <immintrin.h>


template <typename T>
concept float_num = (std::floating_point<T> || std::same_as<unsigned int, T> || std::same_as<int, T>) && !std::same_as<long double, T>;

// Bit widths: 256 (AVX/AVX2), 128 (SSE/SSE2, AVX2 gather) and 0 (plain scalar, width 1).
// Everything below is AVX2 at most (no AVX-512, no FMA).
//
// Common interface of every specialization:
//   load / store / set1 / add / sub / mul / div
//   neg(a)                      -a, flips the sign bit (keeps -0.0 / NaN behaviour of unary minus)
//   gather(base, idx)           lane k = base[idx[k]]  (idx points to `width` int32 indices)
//   reduce_add(a)               horizontal sum of all lanes
//   all_equal(a, b)             true when every lane of a == b
//
// Only on the two 4-lane specializations (float/128 and double/256):
//   set4(a, b, c, d)            lane 0 = a ... lane 3 = d
//   permute<I0, I1, I2, I3>(a)  lane k = a[Ik]
//   blend<Mask>(a, b)           lane k = b[k] when bit k of Mask is set, a[k] otherwise

template <float_num T, std::size_t N>
struct simd_traits;

template <float_num T>
struct std::formatter<simd_traits<T, 128>>;

template <float_num T>
struct std::formatter<simd_traits<T, 256>>;

template <>
struct simd_traits<double, 128>
{
    using type = __m128d;
    static constexpr std::size_t width = 2;

    static type load(const double* data)
    {
        return _mm_loadu_pd(data);
    }

    static void store(double* data, type vec)
    {
        return _mm_storeu_pd(data, vec);
    }

    static double* from(type data)
    {
        return reinterpret_cast<double*>(&data);
    }

    static type set1(double scalar)
    {
        return _mm_set1_pd(scalar);
    }

    static type add(type a, type b)
    {
        return _mm_add_pd(a, b);
    }

    static type sub(type a, type b)
    {
        return _mm_sub_pd(a, b);
    }

    static type mul(type a, type b)
    {
        return _mm_mul_pd(a, b);
    }

    static type div(type a, type b)
    {
        return _mm_div_pd(a, b);
    }

    static type sqrt(type a)
    {
        return _mm_sqrt_pd(a);
    }

    static type neg(type a)
    {
        return _mm_xor_pd(a, _mm_set1_pd(-0.0));
    }

    static type gather(const double* base, const std::int32_t* idx)
    {
        // only 2 indices (8 bytes) may be read here
        return _mm_i32gather_pd(base, _mm_loadl_epi64(reinterpret_cast<const __m128i*>(idx)), 8);
    }

    static double reduce_add(type a)
    {
        return _mm_cvtsd_f64(_mm_add_sd(a, _mm_unpackhi_pd(a, a)));
    }

    static bool all_equal(type a, type b)
    {
        return _mm_movemask_pd(_mm_cmpeq_pd(a, b)) == 0x3;
    }

    static type compareEqual(type a, type b)
    {
        return _mm_cmpeq_pd(a, b);
    }

    static type select(type a, type b, type mask)
    {
        return _mm_or_pd(
            _mm_and_pd(mask, b),
            _mm_andnot_pd(mask, a)
        );
    }
};

template <>
struct simd_traits<float, 128>
{
    using type = __m128;
    static constexpr std::size_t width = 4;

    static type load(const float* data)
    {
        return _mm_loadu_ps(data);
    }

    static void store(float* data, type vec)
    {
        return _mm_storeu_ps(data, vec);
    }

    static float* from(type data)
    {
        return reinterpret_cast<float*>(&data);
    }

    static type set1(float scalar)
    {
        return _mm_set_ps1(scalar);
    }

    static type add(type a, type b)
    {
        return _mm_add_ps(a, b);
    }

    static type sub(type a, type b)
    {
        return _mm_sub_ps(a, b);
    }

    static type mul(type a, type b)
    {
        return _mm_mul_ps(a, b);
    }

    static type div(type a, type b)
    {
        return _mm_div_ps(a, b);
    }

    static type sqrt(type a)
    {
        return _mm_sqrt_ps(a);
    }

    static type neg(type a)
    {
        return _mm_xor_ps(a, _mm_set1_ps(-0.0f));
    }

    static type set4(float a, float b, float c, float d)
    {
        return _mm_set_ps(d, c, b, a);
    }

    template <int I0, int I1, int I2, int I3>
    static type permute(type a)
    {
        return _mm_shuffle_ps(a, a, _MM_SHUFFLE(I3, I2, I1, I0));
    }

    template <int Mask>
    static type blend(type a, type b)
    {
        return _mm_blend_ps(a, b, Mask);
    }

    static type gather(const float* base, const std::int32_t* idx)
    {
        return _mm_i32gather_ps(base, _mm_loadu_si128(reinterpret_cast<const __m128i*>(idx)), 4);
    }

    static float reduce_add(type a)
    {
        type t = _mm_add_ps(a, _mm_movehl_ps(a, a));
        t = _mm_add_ss(t, _mm_shuffle_ps(t, t, 0x55));
        return _mm_cvtss_f32(t);
    }

    static bool all_equal(type a, type b)
    {
        return _mm_movemask_ps(_mm_cmpeq_ps(a, b)) == 0xF;
    }

    static type compareEqual(type a, type b)
    {
        return _mm_cmpeq_ps(a, b);
    }

    static type select(type a, type b, type mask)
    {
        return _mm_or_ps(
            _mm_and_ps(mask, b),
            _mm_andnot_ps(mask, a)
        );
    }
};

template <>
struct simd_traits<float, 256>
{
    using type = __m256;
    static constexpr std::size_t width = 8;

    static type load(const float* data)
    {
        return _mm256_loadu_ps(data);
    }

    static void store(float* data, type vec)
    {
        return _mm256_storeu_ps(data, vec);
    }

    static float* from(type data)
    {
        return reinterpret_cast<float*>(&data);
    }

    static type set1(float scalar)
    {
        return _mm256_set1_ps(scalar);
    }

    static type add(type a, type b)
    {
        return _mm256_add_ps(a, b);
    }

    static type sub(type a, type b)
    {
        return _mm256_sub_ps(a, b);
    }

    static type mul(type a, type b)
    {
        return _mm256_mul_ps(a, b);
    }

    static type div(type a, type b)
    {
        return _mm256_div_ps(a, b);
    }

    static type sqrt(type a)
    {
        return _mm256_sqrt_ps(a);
    }

    static type neg(type a)
    {
        return _mm256_xor_ps(a, _mm256_set1_ps(-0.0f));
    }

    static type gather(const float* base, const std::int32_t* idx)
    {
        return _mm256_i32gather_ps(base, _mm256_loadu_si256(reinterpret_cast<const __m256i*>(idx)), 4);
    }

    static float reduce_add(type a)
    {
        const __m128 sum = _mm_add_ps(_mm256_castps256_ps128(a), _mm256_extractf128_ps(a, 1));
        return simd_traits<float, 128>::reduce_add(sum);
    }

    static bool all_equal(type a, type b)
    {
        return _mm256_movemask_ps(_mm256_cmp_ps(a, b, _CMP_EQ_OQ)) == 0xFF;
    }

    static type compareEqual(type a, type b)
    {
        return _mm256_cmp_ps(a, b, _CMP_EQ_OQ);
    }

    static type cmp_lt(type a, type b) 
    { 
        return _mm256_cmp_ps(a, b, _CMP_LT_OQ); 
    }
    
    static type cmp_le(type a, type b) 
    { 
        return _mm256_cmp_ps(a, b, _CMP_LE_OQ); 
    }
    
    static type cmp_gt(type a, type b) 
    { 
        return _mm256_cmp_ps(a, b, _CMP_GT_OQ); 
    }
    
    static type cmp_ge(type a, type b) 
    { 
        return _mm256_cmp_ps(a, b, _CMP_GE_OQ);
    }

    static type min(type a, type b)    
    { 
        return _mm256_min_ps(a, b); 
    }
    
    static type max(type a, type b)    
    { 
        return _mm256_max_ps(a, b); 
    }

    static int  movemask(type a)       
    { 
        return _mm256_movemask_ps(a); 
    }


    static type select(type a, type b, type mask) 
    { 
        return _mm256_blendv_ps(a, b, mask); 
    }
};

template <>
struct simd_traits<double, 256>
{
    using type = __m256d;
    static constexpr std::size_t width = 4;

    static type load(const double* data)
    {
        return _mm256_loadu_pd(data);
    }

    static void store(double* data, type vec)
    {
        return _mm256_storeu_pd(data, vec);
    }

    static double* from(type data)
    {
        return reinterpret_cast<double*>(&data);
    }

    static type set1(double scalar)
    {
        return _mm256_set1_pd(scalar);
    }

    static type add(type a, type b)
    {
        return _mm256_add_pd(a, b);
    }

    static type sub(type a, type b)
    {
        return _mm256_sub_pd(a, b);
    }

    static type mul(type a, type b)
    {
        return _mm256_mul_pd(a, b);
    }

    static type div(type a, type b)
    {
        return _mm256_div_pd(a, b);
    }

    static type sqrt(type a)
    {
        return _mm256_sqrt_pd(a);
    }

    static type neg(type a)
    {
        return _mm256_xor_pd(a, _mm256_set1_pd(-0.0));
    }

    static type set4(double a, double b, double c, double d)
    {
        return _mm256_set_pd(d, c, b, a);
    }

    template <int I0, int I1, int I2, int I3>
    static type permute(type a)
    {
        return _mm256_permute4x64_pd(a, _MM_SHUFFLE(I3, I2, I1, I0));
    }

    template <int Mask>
    static type blend(type a, type b)
    {
        return _mm256_blend_pd(a, b, Mask);
    }

    static type gather(const double* base, const std::int32_t* idx)
    {
        return _mm256_i32gather_pd(base, _mm_loadu_si128(reinterpret_cast<const __m128i*>(idx)), 8);
    }

    static double reduce_add(type a)
    {
        const __m128d sum = _mm_add_pd(_mm256_castpd256_pd128(a), _mm256_extractf128_pd(a, 1));
        return simd_traits<double, 128>::reduce_add(sum);
    }

    static bool all_equal(type a, type b)
    {
        return _mm256_movemask_pd(_mm256_cmp_pd(a, b, _CMP_EQ_OQ)) == 0xF;
    }

    static type compareEqual(type a, type b)
    {
        return _mm256_cmp_pd(a, b, _CMP_EQ_OQ);
    }

    static type select(type a, type b, type mask)
    {
        return _mm256_or_pd(
            _mm256_and_pd(mask, b),
            _mm256_andnot_pd(mask, a)
        );
    }
};

// Bit width 0 = scalar "batch" of one element: lets the same generic code handle the tail of a loop.
template <float_num T>
struct simd_traits<T, 0>
{
    using type = T;
    static constexpr std::size_t width = 1;

    static constexpr type load(const T* data)
    {
        return *data;
    }

    static constexpr void store(T* data, type vec)
    {
        *data = vec;
    }

    static constexpr type set1(T scalar)
    {
        return scalar;
    }

    static constexpr type add(type a, type b)
    {
        return a + b;
    }

    static constexpr type sub(type a, type b)
    {
        return a - b;
    }

    static constexpr type mul(type a, type b)
    {
        return a * b;
    }

    static constexpr type div(type a, type b)
    {
        return a / b;
    }

    static constexpr type sqrt(type a)
    {
        return std::sqrt(a);
    }

    static constexpr type neg(type a)
    {
        return -a;
    }

    static constexpr type gather(const T* base, const std::int32_t* idx)
    {
        return base[idx[0]];
    }

    static constexpr T reduce_add(type a)
    {
        return a;
    }

    static constexpr bool all_equal(type a, type b)
    {
        return a == b;
    }

    static constexpr bool compareEqual(type a, type b)
    {
        return a == b;
    }

    static constexpr type select(type a, type b, bool mask)
    {
        return mask ? b : a;
    }
};
