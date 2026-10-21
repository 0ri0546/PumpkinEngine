#include "ParticlePass.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Pumpkin {

    void ParticlePass::Execute(RenderContext& ctx) {
        if (!ctx.fb.pixels || ctx.fb.width == 0 || ctx.fb.height == 0)
            return;

        m_system.Update(ctx.deltaTime);

        const float aspect =
            static_cast<float>(ctx.fb.width) /
            static_cast<float>(ctx.fb.height);

        const Mat4f view = ctx.camera.getViewMatrix();

        const float fov =
            ctx.camera.FOVY * std::numbers::pi_v<float> / 180.0f;

        const Mat4f projection = Mat4f::Perspective(
            fov, aspect, ctx.camera.nearFrame, ctx.camera.farFrame
        );

        const Mat4f viewProjection = projection * view;

        for (const Particle& particle : m_system.GetParticles()) {
            if (!particle.IsAlive())
                continue;

            const Vec4f clip = viewProjection * Vec4f{
                particle.pos[0],
                particle.pos[1],
                particle.pos[2],
                1.0f
            };

            if (clip[3] <= 0.0f)
                continue;

            const float xNdc = clip[0] / clip[3];
            const float yNdc = clip[1] / clip[3];
            const float zNdc = clip[2] / clip[3];

            if (xNdc < -1.0f || xNdc > 1.0f ||
                yNdc < -1.0f || yNdc > 1.0f ||
                zNdc < -1.0f || zNdc > 1.0f)
                continue;

            const int x = static_cast<int>(
                (xNdc * 0.5f + 0.5f) * ctx.fb.width
                );

            const int y = static_cast<int>(
                (1.0f - (yNdc * 0.5f + 0.5f)) * ctx.fb.height
                );

            if (x < 0 || x >= ctx.fb.width ||
                y < 0 || y >= ctx.fb.height)
                continue;

            const auto r = static_cast<std::uint32_t>(
                std::clamp(particle.color[0], 0, 255));
            const auto g = static_cast<std::uint32_t>(
                std::clamp(particle.color[1], 0, 255));
            const auto b = static_cast<std::uint32_t>(
                std::clamp(particle.color[2], 0, 255));
            const auto a = static_cast<std::uint32_t>(
                std::clamp(particle.color[3], 0, 255));

            ctx.fb.pixels[
                static_cast<size_t>(y) * ctx.fb.width + x
            ] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

}