#pragma once

#include <Renderer/Types.hpp>

#include <cstdint>
#include <span>
#include <type_traits>
#include <algorithm>
#include <cmath>

namespace Pumpkin {
    /// @brief What the rasterizer knows about one covered pixel. This is the shader's input.
    struct FragmentInput {
        int x = 0, y = 0;
        float depth = 0.0f;
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;       // perspective-correct barycentric weights (sum = 1)
        std::uint32_t i0 = 0, i1 = 0, i2 = 0;        // vertex indices of the triangle
        std::uint32_t primitiveId = 0;

        /// a0 * b0 + a1 * b1 + a2 * b2, for any T that supports T * float and T + T.
        template <class T>
        [[nodiscard]] T Interpolate(const T& a0, const T& a1, const T& a2) const {
            return a0 * b0 + a1 * b1 + a2 * b2;
        }

        /// Interpolates a per-vertex attribute (normals, uvs, ...).
        template <class T>
        [[nodiscard]] T Interpolate(std::span<const T> attribute) const {
            return Interpolate(attribute[i0], attribute[i1], attribute[i2]);
        }

        /// Interpolates one float read out of each element, e.g. a color channel.
        template <class T, class Get>
        [[nodiscard]] float Interpolate(std::span<const T> attribute, Get&& get) const {
            return get(attribute[i0]) * b0 + get(attribute[i1]) * b1 + get(attribute[i2]) * b2;
        }
    };

    /// @brief Shader output. Channels are in [0, 255].
    struct PixelOutput {
        float r = 0.0f, g = 0.0f, b = 0.0f, a = 255.0f;
    };

    /// @brief A fragment shader is anything callable as `PixelOutput(const FragmentInput&)`:
    /// a lambda, or a struct with operator(). Its members are the "uniforms".
    template <class S>
    concept FragmentShader = std::is_invocable_r_v<PixelOutput, const S&, const FragmentInput&>;

    /// @brief Default shader: the interpolated vertex colors.
    struct VertexColorShader {
        std::span<const Color> colors;

        [[nodiscard]] PixelOutput operator()(const FragmentInput& f) const {
            return {
                f.Interpolate(colors, [](const Color& c) { return static_cast<float>(c.r); }),
                f.Interpolate(colors, [](const Color& c) { return static_cast<float>(c.g); }),
                f.Interpolate(colors, [](const Color& c) { return static_cast<float>(c.b); }),
                f.Interpolate(colors, [](const Color& c) { return static_cast<float>(c.a); })
            };
        }
    };

    struct LambertShader {
        std::span<const Vec3f> normals;
        std::span<const Color> colors;

        Vec3f lightDir{ 0.0f, 1.0f, 0.0f };
        Vec3f lightCol{ 1.0f, 1.0f, 1.0f };
        Vec3f viewDir{ 0.0f, 0.0f, 1.0f };

        float ambient = 0.3f;
        float specularStrength = 0.2f;
        float shininess = 32.0f;

        [[nodiscard]] PixelOutput operator()(const FragmentInput& f) const {
            const Vec3f n = f.Interpolate(normals).Normalized();

            const Vec3f l = lightDir.Normalized();
            const float diff = std::max(0.0f, Dot(n, l));
            const Vec3f lighting = lightCol * (ambient + diff);

            const PixelOutput color = VertexColorShader{ colors }(f);

            return {
                std::clamp(lighting[0] * color.r, 0.0f, 255.0f),
                std::clamp(lighting[1] * color.g, 0.0f, 255.0f),
                std::clamp(lighting[2] * color.b, 0.0f, 255.0f),
                color.a
            };
        }
    };
}
