/// @file Shaders.cpp
/// @brief Implementation of Pumpkin.Renderer.Shaders (vertex stage).

module Pumpkin.Renderer.Shaders;

import Pumpkin.Utils.ThreadPool;
import Pumpkin.Utils.Utils;

import std;

namespace Pumpkin {
    static void ApplyVertexEffectWaterOndulation(
        float phase,
        float& px, float& py, float& pz,
        float& nx, float& ny, float& nz)
    {
        constexpr float kAmplitude = 0.05f;
        constexpr float kFrequency = 1.0f;

        const float wave1 = std::sin(
            (px - pz * 0.7f) * kFrequency * 0.7f
            - phase * 0.8f);

        const float displacement =
            kAmplitude * (wave1) * 0.5f;

        py += displacement;
    }


    static void ApplyVertexEffectWaterWave(
        float phase,
        float& px, float& py, float& pz,
        float& nx, float& ny, float& nz)
    {
        struct Wave {
            float dirX;
            float dirZ;
            float amplitude;
            float wavelength;
            float speed;
            float steepness;
        };

        constexpr Wave waves[] = {
            {  -1.0f,  -0.7f, 0.08f, 3.5f, 0.7f, 0.45f }
        };

        float dx = 0.0f;
        float dy = 0.0f;
        float dz = 0.0f;

        constexpr float twoPi = std::numbers::pi_v<float> * 2.0f;

        for (const Wave& wave : waves) {
            const float dirLength = std::sqrt(
                wave.dirX * wave.dirX +
                wave.dirZ * wave.dirZ);

            const float dirX = wave.dirX / dirLength;
            const float dirZ = wave.dirZ / dirLength;

            const float k = twoPi / wave.wavelength;
            const float theta =
                k * (dirX * px + dirZ * pz)
                - phase * wave.speed;

            const float s = std::sin(theta);
            const float c = std::cos(theta);

            dx += wave.steepness * wave.amplitude * dirX * c;
            dy += wave.amplitude * s;
            dz += wave.steepness * wave.amplitude * dirZ * c;
        }

        px += dx;
        py += dy;
        pz += dz;
    }


    ClipFlags VertexShader(std::span<const Vertex> vertices, std::vector<Vertex>& out,
                   ClipInfo& info, const Mat4f& model, const Camera& camera, float aspectRatio,
                   bool animatedWaterOndulation, bool animatedWaterWave)
    {
        out.resize(vertices.size());
        info.codes.Reserve(vertices.size());
        info.dist.Reserve(vertices.size());

        const float fovRadians = camera.FOVY * (std::numbers::pi_v<float> / 180.0f);
        const Mat4f view       = camera.GetViewMatrix();
        const Mat4f projection = Mat4f::Perspective(fovRadians, aspectRatio, camera.nearFrame, camera.farFrame);

        const Mat4f viewModel = view * model;
        const Mat4f mvp       = projection * viewModel;

        // Per-call constants of the effect.
        constexpr float speed = 2.0f;
        const float phase      = GetEngineTime() * speed;

        std::atomic<std::uint32_t> andCode{ 0x3F }, orCode{ 0 };

        const auto transform = [&](std::size_t begin, std::size_t end) {
            std::uint32_t a = 0x3F, _or = 0;
            for (std::size_t i = begin; i < end; ++i) {
                const Vertex& v = vertices[i];
                Vertex& o = out[i];
                o = v; // keeps uv etc.

                float px = v.position[0], py = v.position[1], pz = v.position[2];
                float nx = v.normal[0],   ny = v.normal[1],   nz = v.normal[2];

                if (animatedWaterOndulation) {
                    ApplyVertexEffectWaterOndulation(
                        phase / 1.5f,
                        px, py, pz,
                        nx, ny, nz
                    );
                }
                if (animatedWaterWave) {
                    ApplyVertexEffectWaterWave(
                    phase / 1.5f,
                    px, py, pz,
                    nx, ny, nz
                    );
                }

                o.position = mvp * Vec4f{ px, py, pz, v.position[3] };

                const Vec4f n = viewModel * Vec4f{ nx, ny, nz, 0.0f };
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
