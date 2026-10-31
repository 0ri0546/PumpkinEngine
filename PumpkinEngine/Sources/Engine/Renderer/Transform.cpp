/// @file Transform.cpp
/// @brief Implementation of Pumpkin.Renderer.Transform: AVX2 screen conversion and parallel near-plane clipping.

module Pumpkin.Renderer.Transform;

import Pumpkin.Utils.SIMDTraits;
import Pumpkin.Utils.ThreadPool;

import Pumpkin.Math.Vec;

import std;

namespace Pumpkin {
    /// @brief 256-bit float SIMD traits (8 lanes).
    using V = simd_traits<float, 256>;
    /// @brief Vertices converted per SIMD step.
    static constexpr std::size_t kLanes = V::width;
    /// @brief Vertices per parallel work unit (ToScreen, ClipNearPlane).
    static constexpr std::size_t kVertexChunk = 16384;

    /// @brief Converts vertices `[begin, end)` to screen space (8 at a time with gathers, then a scalar tail).
    /// @param src    Clip-space vertices. @param dst Output vertices. @param begin First index. @param end One past the last index.
    /// @param width  Target width. @param height Target height.
    /// @return false if any vertex had `w <= 0`.
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


    bool ClipNearPlane(
        std::span<const Vertex> inputVertices,
        std::span<const std::uint32_t> inputIndices,
        std::span<const Color> inputColors,
        ClipInfo& info,
        std::vector<Vertex>& outputVertices,
        std::vector<std::uint32_t>& outputIndices,
        std::vector<Color>& outputColors)
    {
        constexpr std::size_t kTriChunk = 4096;

        const std::size_t n = std::min(inputVertices.size(), inputColors.size());
        const std::size_t triCount = inputIndices.size() / 3;
        if (n == 0 || triCount == 0) {
            outputVertices.clear();
            outputIndices.clear();
            outputColors.clear();
            return false;
        }

        // Scratch reused between calls
        static ScratchArray<std::uint8_t> used;   // 1 = vertex survives clipping
        static ScratchArray<std::uint32_t> remap; // input vertex -> output vertex
        static ScratchArray<std::uint8_t> cls;    // per triangle outside mask, 7 = dropped
        static std::vector<std::uint32_t> usedCount, cutCount, indexCount; // per chunk

        const float* dist = info.dist.data();           // the rest of the function keeps using dist[...]
        const std::uint8_t* codes = info.codes.data();

        const std::size_t vertexChunks = (n + kVertexChunk - 1) / kVertexChunk;
        const std::size_t triChunks = (triCount + kTriChunk - 1) / kTriChunk;

        used.Reserve(n);
        remap.Reserve(n);
        cls.Reserve(triCount);
        usedCount.resize(vertexChunks);
        cutCount.resize(triChunks);
        indexCount.resize(triChunks);

        const auto forEachChunk = [](std::size_t chunks, auto&& fn) {
            if (chunks <= 1) {
                for (std::size_t c = 0; c < chunks; ++c) fn(c);
                return;
            }
            Pumpkin::ParallelFor(GetPool(), chunks, fn);
        };

        std::memset(used.data(), 0, n);

        // Output sizes for each triangle class (index = bit mask of outside vertices)
        // 0: visible, 1/2/4: one outside (quad = 2 triangles), 3/5/6: two outside, 7: behind
        // These are every possible configurations
        constexpr std::uint32_t kCuts[8]    = { 0, 2, 2, 2, 2, 2, 2, 0 };
        constexpr std::uint32_t kIndices[8] = { 3, 6, 6, 3, 6, 3, 3, 0 };

        // Classifies every triangle, counts its output, marks the vertices that survive
        forEachChunk(triChunks, [&](std::size_t chunk) {
            const std::size_t begin = chunk * kTriChunk;
            const std::size_t end = std::min(triCount, begin + kTriChunk);
            std::uint32_t cuts = 0, indices = 0;

            const auto mark = [&](std::uint32_t id) {
                // avoids race conditions
                std::atomic_ref<std::uint8_t> flag(used[id]);
                if (flag.load(std::memory_order_relaxed) == 0)
                    flag.store(1, std::memory_order_relaxed);
            };

            const std::uint32_t* idx = inputIndices.data() + begin * 3;
            for (std::size_t t = begin; t < end; ++t, idx += 3) {
                const std::uint32_t v0 = idx[0], v1 = idx[1], v2 = idx[2];
                if (v0 >= n || v1 >= n || v2 >= n) { cls[t] = 7; continue; }
                if (codes[v0] & codes[v1] & codes[v2]) { cls[t] = 7; continue; }

                const unsigned outside =
                    (dist[v0] < 0.0f ? 1u : 0u) |
                    (dist[v1] < 0.0f ? 2u : 0u) |
                    (dist[v2] < 0.0f ? 4u : 0u);

                cls[t] = static_cast<std::uint8_t>(outside);
                cuts += kCuts[outside];
                indices += kIndices[outside];

                if (outside != 7) {
                    if (!(outside & 1u)) mark(v0);
                    if (!(outside & 2u)) mark(v1);
                    if (!(outside & 4u)) mark(v2);
                }
            }
            cutCount[chunk] = cuts;
            indexCount[chunk] = indices;
        });

        // Prefix sums over the chunks: where each chunk starts writing.
        std::uint32_t cutTotal = 0;
        std::size_t indexTotal = 0;
        for (std::size_t c = 0; c < triChunks; ++c) {
            const std::uint32_t cuts = cutCount[c];
            const std::uint32_t indices = indexCount[c];
            cutCount[c] = cutTotal;
            indexCount[c] = static_cast<std::uint32_t>(indexTotal);
            cutTotal += cuts;
            indexTotal += indices;
        }

        if (indexTotal == 0) {
            outputVertices.clear();
            outputIndices.clear();
            outputColors.clear();
            return false;
        }

        // Counts the surviving vertices per chunk
        forEachChunk(vertexChunks, [&](std::size_t chunk) {
            const std::size_t begin = chunk * kVertexChunk;
            const std::size_t end = std::min(n, begin + kVertexChunk);
            std::uint32_t count = 0;
            for (std::size_t i = begin; i < end; ++i) count += used[i];
            usedCount[chunk] = count;
        });

        std::uint32_t usedTotal = 0;
        for (std::size_t c = 0; c < vertexChunks; ++c) {
            const std::uint32_t count = usedCount[c];
            usedCount[c] = usedTotal;
            usedTotal += count;
        }

        outputVertices.resize(usedTotal + cutTotal);
        outputColors.resize(usedTotal + cutTotal);
        outputIndices.resize(indexTotal);

        // Copies an input vertex into the output the first time it is used, then reuses it
        forEachChunk(vertexChunks, [&](std::size_t chunk) {
            const std::size_t begin = chunk * kVertexChunk;
            const std::size_t end = std::min(n, begin + kVertexChunk);
            std::uint32_t slot = usedCount[chunk];
            for (std::size_t i = begin; i < end; ++i) {
                if (!used[i]) continue;
                remap[i] = slot;
                outputVertices[slot] = inputVertices[i];
                outputColors[slot] = inputColors[i];
                ++slot;
            }
        });

        // Builds the triangles. Each chunk writes into its own slice of the output
        forEachChunk(triChunks, [&](std::size_t chunk) {
            const std::size_t begin = chunk * kTriChunk;
            const std::size_t end = std::min(triCount, begin + kTriChunk);

            std::uint32_t nextVertex = usedTotal + cutCount[chunk];
            std::size_t nextIndex = indexCount[chunk];

            const auto outputIndex = [&](std::uint32_t id) -> std::uint32_t {
                return remap[id];
            };

            // Point where the edge in -> out crosses the plane. Always computed from the inside
            // vertex toward the outside one, so two triangles sharing an edge get bit-identical
            // results (no cracks). din >= 0 > dout, so the division is safe and t is in [0, 1].
            const auto cut = [&](std::uint32_t in, std::uint32_t out) -> std::uint32_t {
                const float din = dist[in];
                const float t = din / (din - dist[out]);
                const Vertex& a = inputVertices[in];
                const Vertex& b = inputVertices[out];
                const Color& ca = inputColors[in];
                const Color& cb = inputColors[out];

                Vertex v;
                v.position = a.position + (b.position - a.position) * t;
                v.texcoord = a.texcoord + (b.texcoord - a.texcoord) * t;
                v.normal = (a.normal + (b.normal - a.normal) * t).Normalized();

                const auto lerp = [t](std::uint8_t x, std::uint8_t y) {
                    return static_cast<std::uint8_t>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
                };

                const std::uint32_t slot = nextVertex++;
                outputVertices[slot] = v;
                outputColors[slot] = Color{ lerp(ca.r, cb.r), lerp(ca.g, cb.g), lerp(ca.b, cb.b), lerp(ca.a, cb.a) };
                return slot;
            };

            // Pushes back a triangle into output indices
            const auto pushTriangle = [&](const std::uint32_t a, const std::uint32_t b, const std::uint32_t c) {
                outputIndices[nextIndex++] = a;
                outputIndices[nextIndex++] = b;
                outputIndices[nextIndex++] = c;
            };

            // One vertex outside: the visible part is a quad, splits it into two triangles
            // o is the outside vertex, a and b follow it in winding order
            const auto clipOne = [&](const std::uint32_t o, const std::uint32_t a, const std::uint32_t b) {
                const std::uint32_t p = cut(a, o);
                const std::uint32_t q = cut(b, o);
                const std::uint32_t ia = outputIndex(a);
                const std::uint32_t ib = outputIndex(b);
                pushTriangle(p, ia, ib);
                pushTriangle(p, ib, q);
            };

            // Two vertices outside: a single smaller triangle, i is the inside vertex
            const auto clipTwo = [&](std::uint32_t i, std::uint32_t o1, std::uint32_t o2) {
                const std::uint32_t ii = outputIndex(i);
                const std::uint32_t p = cut(i, o1);
                const std::uint32_t q = cut(i, o2);
                pushTriangle(ii, p, q);
            };

            const std::uint32_t* idx = inputIndices.data() + begin * 3;
            for (std::size_t t = begin; t < end; ++t, idx += 3) {
                const std::uint32_t v0 = idx[0], v1 = idx[1], v2 = idx[2];

                switch (cls[t]) {
                    case 0: pushTriangle(outputIndex(v0), outputIndex(v1), outputIndex(v2)); break; // fully visible: triangle
                    case 7:                                                                         // fully behind / default: dropped
                    default: break;

                    // Rotations keep the winding order of the original triangle.
                    case 1: clipOne(v0, v1, v2); break;
                    case 2: clipOne(v1, v2, v0); break;
                    case 4: clipOne(v2, v0, v1); break;
                    case 6: clipTwo(v0, v1, v2); break;
                    case 5: clipTwo(v1, v2, v0); break;
                    case 3: clipTwo(v2, v0, v1); break;
                }
            }
        });

        return true;
    }

}
