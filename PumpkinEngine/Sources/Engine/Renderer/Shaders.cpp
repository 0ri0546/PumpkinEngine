#include "Shaders.hpp"

#include <Renderer/Rasterizer.hpp>

namespace Pumpkin {

    Mat4f RotationY(float angle) {
        const float c = std::cos(angle);
        const float s = std::sin(angle);

        Mat4f m = Mat4f::Identity();
        m[0,0] = c;   m[0,2] = s;
        m[2,0] = -s;  m[2,2] = c;
        return m;
    }

    std::vector<Vertex> VertexShader(
        const std::vector<Vertex>& vertices,
        const Camera& camera,
        float aspectRatio)
    {
        static std::vector<Vertex> clip;
        clip.resize(vertices.size());

        const float fovRadians = camera.FOVY * (3.14159265f / 180.0f);

        const Mat4f view = camera.getViewMatrix();
        const Mat4f projection = Mat4f::Perspective(
            fovRadians,
            aspectRatio,
            camera.nearFrame,
            camera.farFrame
        );

        const Mat4f viewProjection = projection * view;

        size_t i = 0;
        for (const auto& v : vertices) {
			const static auto start = std::chrono::steady_clock::now();
			const auto now = std::chrono::steady_clock::now();
			const float elapsed = std::chrono::duration<float>(now - start).count();
            Vertex transformed = v;
            Mat4f model = RotationY(elapsed);
            transformed.position = viewProjection * (model * v.position);

            const Vec4f normal4{
                v.normal[0],
                v.normal[1],
                v.normal[2],
                0.0f
            };

            const Vec4f viewNormal = view * model * normal4;

            transformed.normal = Vec3f{
                viewNormal[0],
                viewNormal[1],
                viewNormal[2]
            }.Normalized();

            clip[i++] = transformed;
        }


        return clip;
    }
}
