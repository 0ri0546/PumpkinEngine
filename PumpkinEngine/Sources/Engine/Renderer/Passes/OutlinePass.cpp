module Pumpkin.Renderer.Passes.Outline;

import std;

import Pumpkin.Renderer.Types;
import Pumpkin.Utils.ThreadPool;

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

        const float n = ctx.camera.nearFrame;
        const float f = ctx.camera.farFrame;

        if (n <= 0.0f || f <= n)
            return;

        const std::size_t count = static_cast<std::size_t>(width) * height;
        if (ctx.db.pixels.size() < count || ctx.toonMask.size() < count)
            return;

        const float* depth = ctx.db.pixels.data();
        const std::uint8_t* mask = ctx.toonMask.data();
        std::uint32_t* out = ctx.fb.pixels;

        // Constants
        const float a = 2.0f * n * f;
        const float b = f + n;
        const float c = f - n;
        const auto Linear = [=](float d) {
            return a / (b - (2.0f * d - 1.0f) * c);
        };

        const float thr = threshold;

        // Parallel over rows
        ParallelFor(GetPool(), static_cast<std::size_t>(height - 2),
            [&](std::size_t r) {
                const int y = static_cast<int>(r) + 1;
                const std::size_t row = static_cast<std::size_t>(y) * width;

                for (int x = 1; x < width - 1; ++x) {
                    const std::size_t i = row + x;

                    if (!mask[i])            // not a toon pixel: nothing to do
                        continue;

                    const float d = depth[i];
                    if (!(d < 1.0f))         // rejects invalid
                        continue;

                    const float l = depth[i - 1];
                    const float rr = depth[i + 1];
                    const float u = depth[i - width];
                    const float dn = depth[i + width];
                    if (l >= 1.0f || rr >= 1.0f || u >= 1.0f || dn >= 1.0f)
                        continue;

                    const float edge =
                        std::abs(Linear(rr) - Linear(l)) +
                        std::abs(Linear(dn) - Linear(u));

                    if (edge > thr * Linear(d))
                        out[i] = 0xFF000000;
                }
            });
    }
}
