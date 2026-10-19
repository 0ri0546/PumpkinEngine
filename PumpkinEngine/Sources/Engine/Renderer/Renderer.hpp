#pragma once

#include <Renderer/RenderPass.hpp>
#include <Utils/RenderProfiler.hpp>

namespace Pumpkin {
    class Renderer {
    public:
        template <std::derived_from<RenderPass> T, class... Args>
        T& AddPass(Args&&... args) {
            auto& p = m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            return static_cast<T&>(*p);
        }

        void Resize(int w, int h) { for (auto& p : m_passes) p->OnResize(w, h); }

        void Render(RenderContext& ctx) {
            for (auto& p : m_passes) {
                Prof::Scope scope(p->Name());
                p->Execute(ctx);
            }
        }
    private:
        std::vector<std::unique_ptr<RenderPass>> m_passes;
    };
}
