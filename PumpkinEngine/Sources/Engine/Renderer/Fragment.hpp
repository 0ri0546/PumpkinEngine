#pragma once

#include <Renderer/Types.hpp>

#include <cstdint>
#include <span>
#include <type_traits>
#include <algorithm>
#include <cmath>
#include <numbers>

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
    concept FragmentShader_T = std::is_invocable_r_v<PixelOutput, const S&, const FragmentInput&>;

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

    struct BlinnPhongShader {
        std::span<const Vec3f> normals;
        std::span<const Color> colors;

        Vec3f lightDir{ 0.0f, 1.0f, 0.0f };
        Vec3f lightCol{ 1.0f, 1.0f, 1.0f };
        Vec3f viewDir{ 0.0f, 0.0f, 1.0f };

        float ambient = 0.3f;
        float specularStrength = 0.5f;
        float shininess = 32.0f;

        [[nodiscard]] PixelOutput operator()(const FragmentInput& f) const {
            const Vec3f n = f.Interpolate(normals).Normalized();
            const Vec3f l = lightDir.Normalized();
            const Vec3f v = viewDir.Normalized();

            const float ambientTerm = ambient;
            const float diff = std::max(0.0f, Dot(n, l));
            float spec = 0.0f;

            if (diff > 0.0f) {
                const Vec3f h = (l + v).Normalized();
                const float specAngle = std::max(0.0f, Dot(n, h));
                spec = specularStrength * std::pow(specAngle, shininess);
            }

            const PixelOutput color = VertexColorShader{ colors }(f);

            const float diffuseLighting = ambientTerm + diff;

            return {
                std::clamp(lightCol[0] * (color.r * diffuseLighting + 255.0f * spec), 0.0f, 255.0f),
                std::clamp(lightCol[1] * (color.g * diffuseLighting + 255.0f * spec), 0.0f, 255.0f),
                std::clamp(lightCol[2] * (color.b * diffuseLighting + 255.0f * spec), 0.0f, 255.0f),
                color.a
            };
        }
    };

    /// @brief Building blocks of the Cook-Torrance BRDF (GGX / Smith / Schlick).
    namespace PBR {
        inline constexpr float Pi = std::numbers::pi_v<float>;

        /// Normal distribution (GGX / Trowbridge-Reitz): how many microfacets face the half vector.
        [[nodiscard]] inline float DistributionGGX(float nDotH, float alpha) {
            const float a2 = alpha * alpha;
            const float d = nDotH * nDotH * (a2 - 1.0f) + 1.0f;
            return a2 / (Pi * d * d + 1e-7f);
        }

        /// Geometry term for one direction (Schlick-GGX): microfacet self-shadowing.
        [[nodiscard]] inline float GeometrySchlickGGX(float nDotX, float k) {
            return nDotX / (nDotX * (1.0f - k) + k);
        }

        /// Smith: shadowing from the view direction times masking from the light direction.
        [[nodiscard]] inline float GeometrySmith(float nDotV, float nDotL, float roughness) {
            const float r = roughness + 1.0f;
            const float k = (r * r) / 8.0f;       // k for direct lighting
            return GeometrySchlickGGX(nDotV, k) * GeometrySchlickGGX(nDotL, k);
        }

        /// Fresnel (Schlick): reflectance goes up at grazing angles.
        [[nodiscard]] inline float FresnelSchlick(float cosTheta, float f0) {
            const float m = 1.0f - cosTheta;
            const float m2 = m * m;
            return f0 + (1.0f - f0) * (m2 * m2 * m);
        }
    }

    /// @brief Metallic / roughness PBR with one directional light (Cook-Torrance specular + Lambert diffuse).
    struct PBRShader {
        std::span<const Vec3f> normals;
        std::span<const Color> colors;               // base color (albedo)

        Vec3f lightDir{ 0.0f, 1.0f, 0.0f };          // points toward the light
        Vec3f lightCol{ 1.0f, 1.0f, 1.0f };
        Vec3f viewDir{ 0.0f, 0.0f, 1.0f };           // points toward the camera

        float lightIntensity = 3.0f;
        float ambient = 0.1f;
        float metallic = 0.0f;                       // 0 = dielectric, 1 = metal
        float roughness = 0.5f;                      // 0 = mirror, 1 = fully rough

        [[nodiscard]] PixelOutput operator()(const FragmentInput& f) const {
            const Vec3f n = f.Interpolate(normals).Normalized();
            const Vec3f l = lightDir.Normalized();
            const Vec3f v = viewDir.Normalized();
            const Vec3f h = (l + v).Normalized();

            const float nDotL = std::max(0.0f, Dot(n, l));
            const float nDotV = std::max(1e-4f, Dot(n, v));
            const float nDotH = std::max(0.0f, Dot(n, h));
            const float vDotH = std::max(0.0f, Dot(v, h));

            const float rough = std::clamp(roughness, 0.045f, 1.0f);   // avoids a zero-size highlight
            const float alpha = rough * rough;

            const PixelOutput color = VertexColorShader{ colors }(f);
            const float albedo[3] = { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f };

            // Specular term (same for all channels, only F changes per channel)
            const float D = PBR::DistributionGGX(nDotH, alpha);
            const float G = PBR::GeometrySmith(nDotV, nDotL, rough);

            float out[3];
            for (int i = 0; i < 3; ++i) {
                // Base reflectivity: 0.04 for dielectrics, the albedo for metals.
                const float f0 = 0.04f + (albedo[i] - 0.04f) * metallic;
                const float F = PBR::FresnelSchlick(vDotH, f0);

                const float specular = (D * G * F) / (4.0f * nDotV * nDotL + 1e-4f);

                // Energy conservation: what is reflected is not diffused. Metals have no diffuse.
                const float kD = (1.0f - F) * (1.0f - metallic);
                const float diffuse = kD * albedo[i] / PBR::Pi;

                const float direct = (diffuse + specular) * lightCol[i] * lightIntensity * nDotL;
                const float ambientTerm = ambient * albedo[i] * (1.0f - metallic);

                out[i] = std::clamp((direct + ambientTerm) * 255.0f, 0.0f, 255.0f);
            }

            return { out[0], out[1], out[2], color.a };
        }
    };
}
