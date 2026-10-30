module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.RenderPass;

import Pumpkin.Renderer.Camera;
import Pumpkin.Renderer.Types;

import Pumpkin.Math.Mat;

import std;

export namespace Pumpkin {
    struct PUMPKIN_API DrawItem {
        const Mesh*     mesh;
        const Mat4f*    model;
		const Material* material;
    };

    struct PUMPKIN_API RenderContext {
        FrameBuffer&  fb;
        DepthBuffer&  db;
        const Camera& camera;
        float         deltaTime{};
        std::span<DrawItem> scene;
        std::vector<std::uint8_t>& toonMask; // for tooning, only saves tooned materials data
    };

    class PUMPKIN_API RenderPass {
    public:
        virtual ~RenderPass() = default;
        [[nodiscard]] virtual std::string_view Name() const = 0;
        virtual void OnResize(int /*w*/, int /*h*/) {}   // for passes that own buffers
        virtual void Execute(RenderContext& ctx) = 0;
    };
}
