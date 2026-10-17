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

            const Vec4f normal4{
                v.normal[0],
                v.normal[1],
                v.normal[2],
                0.0f
            };

            const Vec4f viewNormal = view * normal4;

            transformed.normal = Vec3f{
                viewNormal[0],
                viewNormal[1],
                viewNormal[2]
            }.Normalized();

            clip.push_back(transformed);
        }


        return clip;
    }
}