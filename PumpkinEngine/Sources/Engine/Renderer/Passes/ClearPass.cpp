#include "ClearPass.hpp"

#include <Utils/ThreadPool.hpp>

namespace Pumpkin {
    void ClearPass::Execute(RenderContext& ctx) {
        const size_t n = static_cast<size_t>(ctx.fb.width) * ctx.fb.height;
        uint32_t* color = ctx.fb.pixels;
        float*    depth = ctx.db.pixels.data();

        constexpr size_t kChunk = 64 * 1024; // pixels per chunk
        const size_t chunks = (n + kChunk - 1) / kChunk;

        const auto clear = [&](const size_t c) {
            const size_t b = c * kChunk;
            const size_t e = std::min(n, b + kChunk);
            std::fill(color + b, color + e, m_ClearColor);
            std::fill(depth + b, depth + e, 1.0f);
        };

        if (chunks <= 1) clear(0);
        else ParallelFor(GetPool(), chunks, clear);
    }
}
