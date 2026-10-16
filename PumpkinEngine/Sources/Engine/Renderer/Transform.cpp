#include <Renderer/Transform.hpp>

#include <Utils/SIMDTraits.hpp>
#include <Utils/ThreadPool.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace Pumpkin {
    using V = simd_traits<float, 256>;
    static constexpr std::size_t kLanes = V::width;
    static constexpr std::size_t kVertexChunk = 16384; // vertices per pool job

    static_assert(sizeof(Vec4f) == 4 * sizeof(float));

    // Handles vertices [begin, end). Returns false if one of them has w <= 0.
    [[nodiscard]] static bool TransformChunk(const Vec4f* src, Vec4f* dst, std::size_t begin, std::size_t end,
        const Mat3x4& M, float width, float height) {
        constexpr std::int32_t stride = 4; // floats between two positions
        std::int32_t idx[kLanes];
        for (std::int32_t k = 0; k < static_cast<std::int32_t>(kLanes); ++k) idx[k] = k * stride;

        const V::type m00 = V::set1(M.m[0][0]), m01 = V::set1(M.m[0][1]), m02 = V::set1(M.m[0][2]), t0 = V::set1(M.t[0]);
        const V::type m10 = V::set1(M.m[1][0]), m11 = V::set1(M.m[1][1]), m12 = V::set1(M.m[1][2]), t1 = V::set1(M.t[1]);
        const V::type m20 = V::set1(M.m[2][0]), m21 = V::set1(M.m[2][1]), m22 = V::set1(M.m[2][2]), t2 = V::set1(M.t[2]);
        const V::type one = V::set1(1.0f), half = V::set1(0.5f), zero = V::set1(0.0f);
        const V::type vW = V::set1(width), vH = V::set1(height);

        const auto row = [](V::type a, V::type b, V::type c, V::type x, V::type y, V::type z, V::type t) {
            return V::add(V::add(V::add(V::mul(a, x), V::mul(b, y)), V::mul(c, z)), t);
        };

        alignas(32) float sx[kLanes], sy[kLanes], sz[kLanes], sw[kLanes];
        int bad = 0; // bit set = some lane had w <= 0

        std::size_t i = begin;
        for (; i + kLanes <= end; i += kLanes) {
            const float* base = reinterpret_cast<const float*>(src + i);
            const V::type x = V::gather(base + 0, idx);
            const V::type y = V::gather(base + 1, idx);
            const V::type z = V::gather(base + 2, idx);
            const V::type w = V::gather(base + 3, idx); // w is not rotated

            bad |= V::movemask(V::cmp_le(w, zero));

            // rotation (clip space)
            const V::type cx = row(m00, m01, m02, x, y, z, t0);
            const V::type cy = row(m10, m11, m12, x, y, z, t1);
            const V::type cz = row(m20, m21, m22, x, y, z, t2);

            // perspective divide
            const V::type invW = V::div(one, w);
            const V::type ndcX = V::mul(cx, invW);
            const V::type ndcY = V::mul(cy, invW);
            const V::type ndcZ = V::mul(cz, invW);

            // viewport. Y is flipped: NDC is up, pixels are down.
            V::store(sx, V::mul(V::add(V::mul(ndcX, half), half), vW));
            V::store(sy, V::mul(V::sub(half, V::mul(ndcY, half)), vH));
            V::store(sz, ndcZ);
            V::store(sw, invW);

            // AVX2 has no scatter: write the 8 results one by one.
            for (std::size_t k = 0; k < kLanes; ++k)
                dst[i + k] = Vec4f{ sx[k], sy[k], sz[k], sw[k] };
        }

        for (; i < end; ++i) {
            const float x = src[i][0], y = src[i][1], z = src[i][2], w = src[i][3];
            if (w <= 0.0f) { bad = 1; continue; }

            const float cx = M.m[0][0] * x + M.m[0][1] * y + M.m[0][2] * z + M.t[0];
            const float cy = M.m[1][0] * x + M.m[1][1] * y + M.m[1][2] * z + M.t[1];
            const float cz = M.m[2][0] * x + M.m[2][1] * y + M.m[2][2] * z + M.t[2];

            const float invW = 1.0f / w;
            dst[i] = Vec4f{
                (cx * invW * 0.5f + 0.5f) * width,
                (0.5f - cy * invW * 0.5f) * height,
                cz * invW,
                invW
            };
        }
        return bad == 0;
    }

    Mat3x4 BuildTransform(float angleX, float angleY, float angleZ,
        float pivotX, float pivotY, float pivotZ, float zScale) {
        const float cx = std::cos(angleX), sx = std::sin(angleX);
        const float cy = std::cos(angleY), sy = std::sin(angleY);
        const float cz = std::cos(angleZ), sz = std::sin(angleZ);

        // The three rotations applied to one vector.
        const auto rotate = [&](float x, float y, float z) {
            const float y1 = y * cx - z * sx;
            const float z1 = y * sx + z * cx;
            const float x2 = x * cy + z1 * sy;
            const float z2 = -x * sy + z1 * cy;
            return std::array<float, 3>{ x2 * cz - y1 * sz, x2 * sz + y1 * cz, z2 };
        };

        // Column j of the matrix = the rotation of basis vector j.
        const std::array<std::array<float, 3>, 3> col{ rotate(1, 0, 0), rotate(0, 1, 0), rotate(0, 0, 1) };

        Mat3x4 r{};
        const float pivot[3] = { pivotX, pivotY, pivotZ };
        const float rowScale[3] = { 1.0f, 1.0f, zScale };
        for (int row = 0; row < 3; ++row) {
            for (int c = 0; c < 3; ++c) r.m[row][c] = col[c][row] * rowScale[row];
            // move back - rotate the pivot, so that the pivot stays where it is
            r.t[row] = pivot[row] - (r.m[row][0] * pivotX + r.m[row][1] * pivotY + r.m[row][2] * pivotZ);
        }
        return r;
    }

    bool TransformToScreen(std::span<const Vec4f> src, std::vector<Vec4f>& out,
        const Mat3x4& M, int width, int height) {
        out.resize(src.size()); // no-op after the first frame

        const auto w = static_cast<float>(width);
        const auto h = static_cast<float>(height);

        const std::size_t n = src.size();
        const std::size_t chunkCount = (n + kVertexChunk - 1) / kVertexChunk;

        std::atomic<bool> ok{ true };
        ParallelFor(GetPool(), chunkCount, [&](std::size_t c) {
            const std::size_t begin = c * kVertexChunk;
            if (!TransformChunk(src.data(), out.data(), begin, std::min(begin + kVertexChunk, n), M, w, h))
                ok.store(false, std::memory_order_relaxed);
        });
        return ok.load();
    }
}
