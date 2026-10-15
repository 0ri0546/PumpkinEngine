#include "Shaders.hpp"

#include <Renderer/Rasterizer.hpp>
#include <numbers>
#include <iostream>

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
            /*if (clip.empty()) {
                std::cout << "[DEBUG] First clip vertex: "
                    << transformed.position[0] << ", "
                    << transformed.position[1] << ", "
                    << transformed.position[2] << ", "
                    << transformed.position[3] << '\n';
            }*/
            clip.push_back(transformed);
        }

        return clip;
    }

    void PixelShader(
        const FrameBuffer& fb,
        DepthBuffer& db,
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices,
        const std::vector<Color>& colors
    ) {
        RasterizeFrame(vertices, indices, colors, fb, db);
    }

}