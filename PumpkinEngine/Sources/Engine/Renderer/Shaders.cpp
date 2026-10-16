#include "Shaders.hpp"

#include <Renderer/Rasterizer.hpp>

namespace Pumpkin {

    std::vector<Vertex> VertexShader(
        const std::vector<Vertex>& vertices,
        const Camera& camera,
        float aspectRatio)
    {
        std::vector<Vertex> clip;
        clip.reserve(vertices.size());

        const float fovRadians = camera.FOVY * (3.14159265f / 180.0f);

        const Mat4f view = camera.getViewMatrix();
        const Mat4f projection = Mat4f::Perspective(
            fovRadians,
            aspectRatio,
            camera.nearFrame,
            camera.farFrame
        );

        const Mat4f viewProjection = projection * view;

        for (const auto& v : vertices)
        {
            Vertex transformed = v;
            transformed.position = viewProjection * v.position;
            clip.push_back(transformed);
        }

        return clip;
    }
}