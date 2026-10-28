module Pumpkin.Renderer.Shaders;

import Pumpkin.Utils.ThreadPool;

import std;

namespace Pumpkin {
    ClipFlags VertexShader(std::span<const Vertex> vertices, std::vector<Vertex>& out,
                       ClipInfo& info, const Mat4f& model, const Camera& camera, float aspectRatio)
    {
        out.resize(vertices.size());
        info.codes.Reserve(vertices.size());
        info.dist.Reserve(vertices.size());

        const float fovRadians = camera.FOVY * (std::numbers::pi_v<float> / 180.0f);
        const Mat4f view       = camera.GetViewMatrix();
        const Mat4f projection = Mat4f::Perspective(fovRadians, aspectRatio, camera.nearFrame, camera.farFrame);

        const Mat4f viewModel = view * model;
        const Mat4f mvp       = projection * viewModel;

        std::atomic<std::uint32_t> andCode{ 0x3F }, orCode{ 0 };

        const auto transform = [&](std::size_t begin, std::size_t end) {
            std::uint32_t a = 0x3F, _or = 0;
            for (std::size_t i = begin; i < end; ++i) {
                const Vertex& v = vertices[i];
                Vertex& o = out[i];
                o = v; // keeps uv etc.

                o.position = mvp * v.position;

                const Vec4f n = viewModel * Vec4f{ v.normal[0], v.normal[1], v.normal[2], 0.0f };
                o.normal = Vec3f{ n[0], n[1], n[2] }.Normalized();

                const std::uint32_t c = Outcode(o.position);
                info.codes[i] = static_cast<std::uint8_t>(c);
                info.dist[i]  = o.position[2] + o.position[3];
                a &= c;
                _or |= c;
            }
            andCode.fetch_and(a, std::memory_order_relaxed);
            orCode.fetch_or(_or,  std::memory_order_relaxed);
        };

        constexpr std::size_t kChunk = 4096;
        const std::size_t count  = vertices.size();
        const std::size_t chunks = (count + kChunk - 1) / kChunk;
        if (chunks <= 1) {
            transform(0, count);
        } else {
            ParallelFor(GetPool(), chunks, [&](std::size_t c) {
                transform(c * kChunk, std::min(count, (c + 1) * kChunk));
            });
        }

        return { andCode.load(), orCode.load() };
    }
}
