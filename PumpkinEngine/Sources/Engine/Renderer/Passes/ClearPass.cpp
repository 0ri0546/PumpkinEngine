module Pumpkin.Renderer.Passes.Clear;

import Pumpkin.Utils.ThreadPool;

namespace Pumpkin {
    void ClearPass::Execute(RenderContext& ctx) {
        const std::size_t n = static_cast<std::size_t>(ctx.fb.width) * ctx.fb.height;
        std::uint32_t* color = ctx.fb.pixels;
        float*    depth = ctx.db.pixels.data();

        constexpr std::size_t kChunk = 64 * 1024; // pixels per chunk
        const std::size_t chunks = (n + kChunk - 1) / kChunk;

        const auto clear = [&](const std::size_t c) {
            const std::size_t b = c * kChunk;
            const std::size_t e = std::min(n, b + kChunk);
            std::fill(color + b, color + e, m_ClearColor);
            std::fill(depth + b, depth + e, 1.0f);
        };

        if (chunks <= 1) clear(0);
        else ParallelFor(GetPool(), chunks, clear);
    }
}
