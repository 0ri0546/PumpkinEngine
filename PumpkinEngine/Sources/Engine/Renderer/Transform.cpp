#include "Transform.hpp"

#include <Utils/SIMDTraits.hpp>
#include <Utils/ThreadPool.hpp>

namespace Pumpkin {
    using V = simd_traits<float, 256>;
    static constexpr std::size_t kLanes = V::width;
    static constexpr std::size_t kVertexChunk = 16384;

    // Handles vertices [begin, end). Returns false if one of them has w <= 0.
    [[nodiscard]] static bool ScreenChunk(const Vertex* src, Vertex* dst,
        std::size_t begin, std::size_t end, float width, float height) {
        constexpr std::int32_t stride = 9; // floats between two positions
        std::int32_t idx[kLanes];
        for (std::int32_t k = 0; k < static_cast<std::int32_t>(kLanes); ++k) idx[k] = k * stride;

        const V::type one = V::set1(1.0f), half = V::set1(0.5f), zero = V::set1(0.0f);
        const V::type vW = V::set1(width), vH = V::set1(height);

        alignas(32) float sx[kLanes], sy[kLanes], sz[kLanes], sw[kLanes];
        int bad = 0; // bit set = some lane had w <= 0

        std::size_t i = begin;
        for (; i + kLanes <= end; i += kLanes) {
            const float* base = reinterpret_cast<const float*>(src + i);
            const V::type x = V::gather(base + 0, idx);
            const V::type y = V::gather(base + 1, idx);
            const V::type z = V::gather(base + 2, idx);
            const V::type w = V::gather(base + 3, idx);

            bad |= V::movemask(V::cmp_le(w, zero));

            // perspective divide
            const V::type invW = V::div(one, w);
            const V::type ndcX = V::mul(x, invW);
            const V::type ndcY = V::mul(y, invW);
            const V::type ndcZ = V::mul(z, invW);

            // viewport. Y is flipped: NDC is up, pixels are down. Z goes [-1, 1] -> [0, 1].
            V::store(sx, V::mul(V::add(V::mul(ndcX, half), half), vW));
            V::store(sy, V::mul(V::sub(half, V::mul(ndcY, half)), vH));
            V::store(sz, V::add(V::mul(ndcZ, half), half));
            V::store(sw, invW);

            // No scatter on AVX2: copy texcoord/normal, then overwrite the position.
            for (std::size_t k = 0; k < kLanes; ++k) {
                dst[i + k] = src[i + k];
                dst[i + k].position = Vec4f{ sx[k], sy[k], sz[k], sw[k] };
            }
        }

        // scalar tail
        for (; i < end; ++i) {
            const float w = src[i].position[3];
            if (w <= 0.0f) { bad = 1; continue; }

            const float invW = 1.0f / w;
            const float ndcX = src[i].position[0] * invW;
            const float ndcY = src[i].position[1] * invW;
            const float ndcZ = src[i].position[2] * invW * 0.5f + 0.5f;

            dst[i] = src[i];
            dst[i].position = Vec4f{
                (ndcX * 0.5f + 0.5f) * width,
                (0.5f - ndcY * 0.5f) * height,
                ndcZ,
                invW
            };
        }
        return bad == 0;
    }

    // Returns false (and leaves `out` empty) if any vertex is behind the camera.
    bool ToScreen(std::span<const Vertex> clip, std::vector<Vertex>& out, int width, int height) {
        out.resize(clip.size()); // no-op after the first frame

        const auto w = static_cast<float>(width);
        const auto h = static_cast<float>(height);
        const std::size_t n = clip.size();
        const std::size_t chunkCount = (n + kVertexChunk - 1) / kVertexChunk;

        std::atomic<bool> ok{ true };
        ParallelFor(GetPool(), chunkCount, [&](std::size_t c) {
            const std::size_t begin = c * kVertexChunk;
            if (!ScreenChunk(clip.data(), out.data(), begin, std::min(begin + kVertexChunk, n), w, h))
                ok.store(false, std::memory_order_relaxed);
        });

        if (!ok.load()) { out.clear(); return false; }
        return true;
    }
}
