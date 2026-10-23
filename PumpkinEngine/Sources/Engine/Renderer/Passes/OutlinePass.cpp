#include "OutlinePass.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include <Utils/ThreadPool.hpp>

namespace Pumpkin {
    void OutlinePass::Execute(RenderContext& ctx) {
        const int width = ctx.fb.width;
        const int height = ctx.fb.height;

        if (width < 3 || height < 3)
            return;

        if (toonOnly) {
            const bool hasToon = std::ranges::any_of(
                ctx.scene,
                [](const DrawItem& item) {
                    return item.material &&
                        item.material->shader == ShaderId::Toon;
                }
            );

            if (!hasToon)
                return;
        }

        const float nearPlane = ctx.camera.nearFrame;
        const float farPlane = ctx.camera.farFrame;

        if (nearPlane <= 0.0f || farPlane <= nearPlane)
            return;

        const auto& depth = ctx.db.pixels;

        const std::size_t count =
            static_cast<std::size_t>(width) * height;

        if (depth.size() < count)
            return;

        const std::vector<float> sourceDepth(depth.begin(), depth.begin() + count);

        const auto linearizeDepth = [nearPlane, farPlane](float d) {
            const float zNdc = 2.0f * d - 1.0f;

            return (2.0f * nearPlane * farPlane) /
                (farPlane + nearPlane -
                    zNdc * (farPlane - nearPlane));
            };

        constexpr std::size_t chunkSize = 16384;
        const std::size_t chunkCount =
            (count + chunkSize - 1) / chunkSize;

        ParallelFor(GetPool(), chunkCount, [&](std::size_t chunk) {
            const std::size_t begin = chunk * chunkSize;
            const std::size_t end = std::min(count, begin + chunkSize);

            for (std::size_t i = begin; i < end; ++i) {
                const int x = static_cast<int>(i % width);
                const int y = static_cast<int>(i / width);

                if (x == 0 || y == 0 ||
                    x == width - 1 || y == height - 1)
                    continue;

                const float d = sourceDepth[i];

                if (!std::isfinite(d) || d >= 1.0f)
                    continue;

                const float center = linearizeDepth(d);

                const float leftDepth = sourceDepth[i - 1];
                const float rightDepth = sourceDepth[i + 1];
                const float upDepth = sourceDepth[i - width];
                const float downDepth = sourceDepth[i + width];

                if (leftDepth >= 1.0f || rightDepth >= 1.0f ||
                    upDepth >= 1.0f || downDepth >= 1.0f)
                    continue;

                const float left = linearizeDepth(leftDepth);
                const float right = linearizeDepth(rightDepth);
                const float up = linearizeDepth(upDepth);
                const float down = linearizeDepth(downDepth);

                const float edge =
                    std::abs(right - left) +
                    std::abs(down - up);

                if (edge > threshold * center)
                    ctx.fb.pixels[i] = 0xFF000000;
            }
            });
    }
}