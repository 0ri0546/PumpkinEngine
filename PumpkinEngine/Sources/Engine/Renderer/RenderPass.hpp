#pragma once

#include <Math/Mat.h>

#include <Renderer/Camera.hpp>
#include <Renderer/Texture.hpp>
#include <Renderer/Types.hpp>

#include <span>
#include <string_view>

namespace Pumpkin {
    struct DrawItem {
        Mesh*          mesh;
        Mat4f*         model;
        const Texture* albedo;
        const Texture* normalMap = nullptr;
        const Texture* metalMap = nullptr;
        const Texture* roughMap = nullptr;
        const Texture* ambientMap = nullptr;
		float ambient   = 0.0f;
		float metalness = 0.0f;
		float roughness = 0.0f;
    };

    struct RenderContext {
        FrameBuffer&  fb;
        DepthBuffer&  db;
        const Camera& camera;
        float         deltaTime{};
        std::span<DrawItem> scene;
    };

    class RenderPass {
    public:
        virtual ~RenderPass() = default;
        [[nodiscard]] virtual std::string_view Name() const = 0;
        virtual void OnResize(int /*w*/, int /*h*/) {}   // for passes that own buffers
        virtual void Execute(RenderContext& ctx) = 0;
    };
}
