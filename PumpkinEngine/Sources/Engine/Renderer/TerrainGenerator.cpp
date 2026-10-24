
#include "TerrainGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace Pumpkin {
    namespace {
        constexpr Color Water{ 35, 145, 220, 255 };
        constexpr Color Sand{ 230, 210, 145, 255 };
        constexpr Color Grass{ 100, 190, 75, 255 };
        constexpr Color DarkGrass{ 75, 155, 60, 255 };
        constexpr Color Rock{ 125, 115, 105, 255 };
        constexpr Color Snow{ 245, 245, 240, 255 };

        float Fade(float t) {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        std::uint32_t Hash(int x, int z, std::uint32_t seed) {
            std::uint32_t h =
                static_cast<std::uint32_t>(x) * 374761393u +
                static_cast<std::uint32_t>(z) * 668265263u +
                seed * 1442695041u;

            h = (h ^ (h >> 13)) * 1274126177u;
            return h ^ (h >> 16);
        }

        float RandomValue(int x, int z, std::uint32_t seed) {
            return static_cast<float>(Hash(x, z, seed) & 0x00FFFFFFu)
                / 8388607.5f - 1.0f;
        }

        float Noise(float x, float z, std::uint32_t seed) {
            const int x0 = static_cast<int>(std::floor(x));
            const int z0 = static_cast<int>(std::floor(z));

            const float tx = Fade(x - static_cast<float>(x0));
            const float tz = Fade(z - static_cast<float>(z0));

            const float a = RandomValue(x0, z0, seed);
            const float b = RandomValue(x0 + 1, z0, seed);
            const float c = RandomValue(x0, z0 + 1, seed);
            const float d = RandomValue(x0 + 1, z0 + 1, seed);

            const float ab = a + (b - a) * tx;
            const float cd = c + (d - c) * tx;

            return ab + (cd - ab) * tz;
        }

        float FractalNoise(
            float x,
            float z,
            const TerrainConfig& config,
            float scaleMultiplier = 1.0f
        ) {
            float frequency = config.noiseScale * scaleMultiplier;
            float amplitude = 1.0f;
            float value = 0.0f;
            float totalAmplitude = 0.0f;

            for (int octave = 0; octave < config.noiseOctaves; ++octave) {
                value += Noise(
                    x * frequency,
                    z * frequency,
                    config.seed + static_cast<std::uint32_t>(octave) * 1013u
                ) * amplitude;

                totalAmplitude += amplitude;
                amplitude *= config.persistence;
                frequency *= config.lacunarity;
            }

            return totalAmplitude > 0.0f
                ? value / totalAmplitude
                : 0.0f;
        }

        Color GetColor(float height, float slope, const TerrainConfig& config) {
            const float range = config.maxHeight - config.minHeight;

            const auto GetLevel = [&](float relativeLevel) {
                return std::clamp(
                    config.baseHeight + (relativeLevel - 0.5f) * range,
                    config.minHeight,
                    config.maxHeight
                );
                };

            const float sandHeight = GetLevel(config.sandLevel);
            const float grassHeight = GetLevel(config.grassLevel);
            const float rockHeight = GetLevel(config.rockLevel);
            const float snowHeight = GetLevel(config.snowLevel);

            if (height >= snowHeight)
                return Snow;

            if (height >= rockHeight)
                return Rock;

            if (slope < 0.75f && height >= grassHeight)
                return Rock;

            if (height < sandHeight)
                return Sand;

            return Grass;
        }
    }

    Mesh GenerateTerrain(const TerrainConfig& config) {
        if (config.cellsX <= 0 ||
            config.cellsZ <= 0 ||
            config.sizeX <= 0.0f ||
            config.sizeZ <= 0.0f ||
            config.maxHeight <= config.minHeight ||
            config.noiseScale <= 0.0f ||
            config.noiseOctaves < 1) {
            throw std::invalid_argument("Invalid terrain configuration");
        }

        Mesh mesh{};

        const int verticesX = config.cellsX + 1;
        const int verticesZ = config.cellsZ + 1;

        const std::size_t vertexCount =
            static_cast<std::size_t>(verticesX) * verticesZ;

        const std::size_t triangleCount =
            static_cast<std::size_t>(config.cellsX) *
            config.cellsZ * 2;

        std::vector<Vec3f> positions;
        std::vector<float> heights;

        positions.reserve(vertexCount);
        heights.reserve(vertexCount);

        const float stepX = config.sizeX / config.cellsX;
        const float stepZ = config.sizeZ / config.cellsZ;

        const float startX = -config.sizeX * 0.5f;
        const float startZ = -config.sizeZ * 0.5f;
        const float heightRange = config.maxHeight - config.minHeight;

        for (int z = 0; z < verticesZ; ++z) {
            for (int x = 0; x < verticesX; ++x) {
                const float px = startX + x * stepX;
                const float pz = startZ + z * stepZ;

                const float continental = FractalNoise(
                    px, pz, config, 0.35f
                );

                const float detail = FractalNoise(
                    px, pz, config, 1.0f
                );

                float elevation =
                    continental * 0.85f +
                    detail * 0.15f;

                elevation = std::copysign(
                    std::pow(std::abs(elevation), 0.7f),
                    elevation
                );

                const float height = std::clamp(
                    config.baseHeight + elevation *
                    (config.maxHeight - config.minHeight) * 0.5f,
                    config.minHeight,
                    config.maxHeight
                );

                positions.emplace_back(px, height, pz);
                heights.push_back(height);
            }
        }

        mesh.vertices.reserve(triangleCount * 3);
        mesh.colors.reserve(triangleCount * 3);
        mesh.indices.reserve(triangleCount * 3);

        const auto AddTriangle = [&](std::uint32_t i0,
            std::uint32_t i1,
            std::uint32_t i2) {
                const Vec3f& p0 = positions[i0];
                const Vec3f& p1 = positions[i1];
                const Vec3f& p2 = positions[i2];

                Vec3f normal = Cross(p1 - p0, p2 - p0).Normalized();

                if (normal[1] < 0.0f)
                    normal = normal * -1.0f;

                const float averageHeight =
                    (heights[i0] + heights[i1] + heights[i2]) / 3.0f;

                const float slope = std::clamp(normal[1], 0.0f, 1.0f);
                const Color color = GetColor(averageHeight, slope, config);

                const std::uint32_t base =
                    static_cast<std::uint32_t>(mesh.vertices.size());

                for (const Vec3f* p : { &p0, &p1, &p2 }) {
                    mesh.vertices.emplace_back(
                        Vec4f{ (*p)[0], (*p)[1], (*p)[2], 1.0f },
                        Vec2f{},
                        normal
                    );

                    mesh.colors.push_back(color);
                }

                mesh.indices.insert(
                    mesh.indices.end(),
                    { base, base + 1, base + 2 }
                );
            };

        for (int z = 0; z < config.cellsZ; ++z) {
            for (int x = 0; x < config.cellsX; ++x) {
                const auto a = static_cast<std::uint32_t>(
                    z * verticesX + x
                    );
                const auto b = a + 1;
                const auto c = a + verticesX;
                const auto d = c + 1;

                AddTriangle(a, c, b);
                AddTriangle(b, c, d);
            }
        }

        return mesh;
    }

    Mesh GenerateWater(const TerrainConfig& config) {
        if (config.cellsX <= 0 ||
            config.cellsZ <= 0 ||
            config.sizeX <= 0.0f ||
            config.sizeZ <= 0.0f ||
            config.maxHeight <= config.minHeight ||
            config.noiseScale <= 0.0f ||
            config.noiseOctaves < 1) {
            throw std::invalid_argument("Invalid terrain configuration");
        }

        struct Point {
            float x;
            float z;
            float height;
        };

        Mesh mesh{};

        const int verticesX = config.cellsX + 1;
        const int verticesZ = config.cellsZ + 1;

        const float stepX = config.sizeX / config.cellsX;
        const float stepZ = config.sizeZ / config.cellsZ;
        const float startX = -config.sizeX * 0.5f;
        const float startZ = -config.sizeZ * 0.5f;
        const float heightRange = config.maxHeight - config.minHeight;

        std::vector<Point> points;
        points.reserve(
            static_cast<std::size_t>(verticesX) * verticesZ
        );

        for (int z = 0; z < verticesZ; ++z) {
            for (int x = 0; x < verticesX; ++x) {
                const float px = startX + x * stepX;
                const float pz = startZ + z * stepZ;

                const float continental =
                    FractalNoise(px, pz, config, 0.35f);

                const float detail =
                    FractalNoise(px, pz, config, 1.0f);

                float elevation =
                    continental * 0.85f + detail * 0.15f;

                elevation = std::copysign(
                    std::pow(std::abs(elevation), 0.7f),
                    elevation
                );

                const float height = std::clamp(
                    config.baseHeight + elevation * heightRange * 0.5f,
                    config.minHeight,
                    config.maxHeight
                );

                points.push_back({ px, pz, height });
            }
        }

        constexpr Color Water{ 35, 145, 220, 255 };

        // Construit un triangle horizontal au niveau de l'eau.
        const auto AddWaterTriangle = [&](const Point& a,
            const Point& b,
            const Point& c) {
                const std::uint32_t base =
                    static_cast<std::uint32_t>(mesh.vertices.size());

                constexpr float waterOffset = 0.001f;
                const float waterY = config.waterLevel + waterOffset;

                for (const Point* p : { &a, &b, &c }) {
                    mesh.vertices.emplace_back(
                        Vec4f{ p->x, waterY, p->z, 1.0f },
                        Vec2f{},
                        Vec3f{ 0.0f, 1.0f, 0.0f }
                    );

                    mesh.colors.push_back(Water);
                }

                mesh.indices.insert(
                    mesh.indices.end(),
                    { base, base + 1, base + 2 }
                );
            };

        const auto ClipTriangle = [&](const Point& a,
            const Point& b,
            const Point& c) {
                const std::array<Point, 3> triangle{ a, b, c };
                std::vector<Point> polygon;
                polygon.reserve(4);

                for (int i = 0; i < 3; ++i) {
                    const Point& current = triangle[i];
                    const Point& next = triangle[(i + 1) % 3];

                    const bool currentUnder =
                        current.height < config.waterLevel;
                    const bool nextUnder =
                        next.height < config.waterLevel;

                    if (currentUnder)
                        polygon.push_back(current);

                    if (currentUnder != nextUnder) {
                        const float t =
                            (config.waterLevel - current.height) /
                            (next.height - current.height);

                        polygon.push_back({
                            current.x + (next.x - current.x) * t,
                            current.z + (next.z - current.z) * t,
                            config.waterLevel
                            });
                    }
                }

                if (polygon.size() < 3)
                    return;

                for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
                    AddWaterTriangle(
                        polygon[0],
                        polygon[i],
                        polygon[i + 1]
                    );
                }
            };

        for (int z = 0; z < config.cellsZ; ++z) {
            for (int x = 0; x < config.cellsX; ++x) {
                const auto a = static_cast<std::size_t>(
                    z * verticesX + x
                    );
                const auto b = a + 1;
                const auto c = a + verticesX;
                const auto d = c + 1;

                ClipTriangle(points[a], points[c], points[b]);
                ClipTriangle(points[b], points[c], points[d]);
            }
        }

        return mesh;
    }

    float GetTerrainLevel(
        float baseHeight,
        float minHeight,
        float maxHeight,
        float relativeLevel
    ) {
        const float range = maxHeight - minHeight;

        const float normalizedBase =
            (baseHeight - minHeight) / range;

        return std::clamp(
            baseHeight + (relativeLevel - 0.5f) * range,
            minHeight,
            maxHeight
        );
    }
}
