#include <Renderer/PrimitiveAssembly.hpp>

#include <Utils/ThreadPool.hpp>

#include <algorithm>
#include <cmath>

namespace Pumpkin {
    static constexpr float kFrontFaceSign = 1.0f;

    // Returns w0 * a + w1 * b + w2 * c for start, dx and dy.
    static Plane Mix(const Plane& w0, const Plane& w1, const Plane& w2,
        float a, float b, float c) {
        return {
            w0.start * a + w1.start * b + w2.start * c,
            w0.dx * a + w1.dx * b + w2.dx * c,
            w0.dy * a + w1.dy * b + w2.dy * c
        };
    }

    // Culls one triangle. If it stays, appends its primitive to dst.
    static void AssembleTriangle(std::span<const Vertex> v, std::span<const std::uint32_t> indices,
        std::span<const Color> colors, std::size_t tri, const AssemblyDesc& desc,
        std::vector<Primitive>& dst) {
        const std::size_t i = tri * 3 + 2;
        const std::uint32_t ia = indices[i - 2], ib = indices[i - 1], ic = indices[i];

        const auto& pa = v[ia];
        const auto& pb = v[ib];
        const auto& pc = v[ic];

        const float ax = pa.position[0], ay = pa.position[1], az = pa.position[2], ak = pa.position[3];
        const float bx = pb.position[0], by = pb.position[1], bz = pb.position[2], bk = pb.position[3];
        const float cx = pc.position[0], cy = pc.position[1], cz = pc.position[2], ck = pc.position[3];

        // Twice the signed area. Zero means a flat triangle.
        const float area = (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
        if (area == 0.0f) return;
        if (desc.culling == CullMode::Back && area * kFrontFaceSign < 0.0f) return;
        if (desc.culling == CullMode::Front && area * kFrontFaceSign > 0.0f) return;
        const float invArea = 1.0f / area;

        const float minX = std::min({ ax, bx, cx }), maxX = std::max({ ax, bx, cx });
        const float minY = std::min({ ay, by, cy }), maxY = std::max({ ay, by, cy });

        // Bounding box, clamped to the screen (in float, before the int cast).
        const float fx0 = std::max(0.0f, std::ceil(minX - 0.5f));
        const float fy0 = std::max(0.0f, std::ceil(minY - 0.5f));
        const float fx1 = std::min(static_cast<float>(desc.width),  std::floor(maxX - 0.5f) + 1.0f);
        const float fy1 = std::min(static_cast<float>(desc.height), std::floor(maxY - 0.5f) + 1.0f);
        if (fx0 >= fx1 || fy0 >= fy1) return; // fully offscreen

        Primitive p{};
        p.x0 = static_cast<int>(fx0);
        p.y0 = static_cast<int>(fy0);
        p.x1 = static_cast<int>(fx1);
        p.y1 = static_cast<int>(fy1);

        const auto& uv0 = v[ia].texcoord;
        const auto& uv1 = v[ib].texcoord;
        const auto& uv2 = v[ic].texcoord;

        // Center of the first pixel.
        const float sx = static_cast<float>(p.x0) + 0.5f;
        const float sy = static_cast<float>(p.y0) + 0.5f;

        // Weight of the vertex opposite to the edge p -> q.
        const auto weightPlane = [&](float px, float py, float qx, float qy) {
            return Plane{
                ((sx - px) * (qy - py) - (sy - py) * (qx - px)) * invArea,
                (qy - py) * invArea,
                -(qx - px) * invArea
            };
        };
        p.w0 = weightPlane(bx, by, cx, cy);
        p.w1 = weightPlane(cx, cy, ax, ay);
        p.w2 = weightPlane(ax, ay, bx, by);

        p.u = Mix(p.w0, p.w1, p.w2, ak * uv0[0], bk * uv1[0], ck * uv2[0]);
        p.v = Mix(p.w0, p.w1, p.w2, ak * uv0[1], bk * uv1[1], ck * uv2[1]);

        p.z = Mix(p.w0, p.w1, p.w2, az, bz, cz);
        p.k = Mix(p.w0, p.w1, p.w2, ak, bk, ck);
        p.kv[0] = ak;
        p.kv[1] = bk;
        p.kv[2] = ck;

        p.i0 = ia;
        p.i1 = ib;
        p.i2 = ic;

        p.zKey = std::max({az, bz, cz});
        if (!colors.empty())
            p.transparent = colors[ia].a < 255 || colors[ib].a < 255 || colors[ic].a < 255;

        dst.push_back(p);
    }

    void AssemblePrimitives(std::span<const Vertex> screen, std::span<const std::uint32_t> indices,
        std::span<const Color> colors, const AssemblyDesc& desc, PrimitiveBatch& out) {

        const bool valid = indices.size() % 3 == 0
            && (colors.empty() || colors.size() >= screen.size())
            && desc.width > 0 && desc.height > 0;
        if (!valid) {
            out.chunks.clear();
            return;
        }

        const std::size_t triCount = indices.size() / 3;
        const std::size_t chunkCount = (triCount + kSetupChunk - 1) / kSetupChunk;
        out.chunks.resize(chunkCount);

        ParallelFor(GetPool(), chunkCount, [&](std::size_t c) {
            auto& dst = out.chunks[c];
            dst.clear(); // keeps memory, no new allocation
            const std::size_t begin = c * kSetupChunk;
            const std::size_t end = std::min(begin + kSetupChunk, triCount);
            for (std::size_t t = begin; t < end; ++t)
                AssembleTriangle(screen, indices, colors, t, desc, dst);
        });

    }
}
