#pragma once

#include <Renderer/RenderPass.hpp>

namespace Pumpkin {
    class OutlinePass final : public RenderPass {
    public:
        [[nodiscard]] std::string_view Name() const override {
            return "Outline";
        }

        void Execute(RenderContext& ctx) override;

        float threshold = 0.02f;
        bool toonOnly = true;
    };
}