/// @file TerrainGenerator.cpp
/// @brief Implementation of Pumpkin.Renderer.TerrainGenerator: value-noise height field, biome colouring and meshing.

module Pumpkin.Renderer.TerrainGenerator;

import Pumpkin.Renderer.Types;

import Pumpkin.Math.Vec;

import std;

namespace Pumpkin {
    namespace {
        /// @brief Water colour
        constexpr Color Water{ 35, 145, 220, 127 };
        /// @brief Biome colour palette (opaque).
        constexpr Color Sand{ 230, 210, 145, 255 };
        constexpr Color Grass{ 100, 190, 75, 255 };
        constexpr Color Rock{ 125, 115, 105, 255 };
        constexpr Color Snow{ 245, 245, 240, 255 };

        /// @brief Quintic smoothstep `6t^5 - 15t^4 + 10t^3` used to interpolate lattice values.
        float Fade(float t) {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        /// @brief Integer hash of a lattice point and a seed.
        std::uint32_t Hash(int x, int z, std::uint32_t seed) {
            std::uint32_t h =
                static_cast<std::uint32_t>(x) * 374761393u +
                static_cast<std::uint32_t>(z) * 668265263u +
                seed * 1442695041u;

            h = (h ^ (h >> 13)) * 1274126177u;
            return h ^ (h >> 16);
        }

        /// @brief Deterministic pseudo-random value in [-1, 1] for a lattice point.
        float RandomValue(int x, int z, std::uint32_t seed) {
            return static_cast<float>(Hash(x, z, seed) & 0x00FFFFFFu)
                * (1.0f / 8388607.5f) - 1.0f;
        }

        /// @brief 2D value noise: smooth bilinear interpolation of the four surrounding lattice values (range about [-1, 1]).
        float Noise(float x, float z, std::uint32_t seed) {
            const float fx = std::floor(x);
            const float fz = std::floor(z);
            const int x0 = static_cast<int>(fx);
            const int z0 = static_cast<int>(fz);

            const float tx = Fade(x - fx);
            const float tz = Fade(z - fz);

            const float a = RandomValue(x0, z0, seed);
            const float b = RandomValue(x0 + 1, z0, seed);
            const float c = RandomValue(x0, z0 + 1, seed);
            const float d = RandomValue(x0 + 1, z0 + 1, seed);

            const float ab = a + (b - a) * tx;
            const float cd = c + (d - c) * tx;

            return ab + (cd - ab) * tz;
        }

        /// @brief Sum of `noiseOctaves` noise layers (frequency x lacunarity, amplitude x persistence) normalised by the total amplitude.
        /// @param scaleMultiplier Multiplies `noiseScale` for this call.
        float FractalNoise(float x, float z, const TerrainConfig& config,
                           float scaleMultiplier) {
            float frequency = config.noiseScale * scaleMultiplier;
            float amplitude = 1.0f;
            float value = 0.0f;
            float totalAmplitude = 0.0f;

            for (int octave = 0; octave < config.noiseOctaves; ++octave) {
                value += Noise(x * frequency, z * frequency,
                    config.seed + static_cast<std::uint32_t>(octave) * 1013u)
                    * amplitude;
                totalAmplitude += amplitude;
                amplitude *= config.persistence;
                frequency *= config.lacunarity;
            }
            return value / totalAmplitude; // octaves >= 1 and amplitude > 0
        }

        /// @brief Throws std::invalid_argument if the configuration is unusable.
        void Validate(const TerrainConfig& c) {
            if (c.cellsX <= 0 || c.cellsZ <= 0 ||
                c.sizeX <= 0.0f || c.sizeZ <= 0.0f ||
                c.maxHeight <= c.minHeight ||
                c.noiseScale <= 0.0f || c.noiseOctaves < 1) {
                throw std::invalid_argument("Invalid terrain configuration");
            }
        }

        /// @brief Absolute heights of the biome thresholds.
        struct Levels { float sand, grass, rock, snow; };

        /// @brief Converts the relative biome levels of a config to absolute heights with GetTerrainLevel().
        Levels ComputeLevels(const TerrainConfig& c) {
            const auto Level = [&](float relative) {
                return GetTerrainLevel(c.baseHeight, c.minHeight, c.maxHeight, relative);
            };
            return { Level(c.sandLevel), Level(c.grassLevel),
                     Level(c.rockLevel), Level(c.snowLevel) };
        }

        /// @brief Picks the biome colour of a triangle.
        /// @param height Average height of the triangle.
        /// @param slope  Y component of the face normal (1 = flat, small = steep).
        /// Snow above the snow level, rock above the rock level, rock on steep slopes (< 0.75) above the grass level, sand below the sand level, grass otherwise.
        Color GetColor(float height, float slope, const Levels& l) {
            if (height >= l.snow) return Snow;
            if (height >= l.rock) return Rock;
            if (slope < 0.75f && height >= l.grass) return Rock;
            if (height < l.sand) return Sand;
            return Grass;
        }

        /// @brief Cell size and origin of the terrain grid (centred on the origin in X/Z), shared by terrain and water.
        struct Grid {
            int vx;        // vertices per row (cellsX + 1)
            float stepX;
            float stepZ;
            float startX;
            float startZ;

            explicit Grid(const TerrainConfig& c)
                : vx(c.cellsX + 1)
                , stepX(c.sizeX / static_cast<float>(c.cellsX))
                , stepZ(c.sizeZ / static_cast<float>(c.cellsZ))
                , startX(-c.sizeX * 0.5f)
                , startZ(-c.sizeZ * 0.5f) {}
        };

        /// @brief Computes the (cellsZ + 1) x (cellsX + 1) row-major height field.
        ///
        /// Blends a low-frequency (x0.35) and a normal-frequency fractal noise (85% / 15%), reshapes it with an exponent of 0.7,
        /// scales it by half the height range around `baseHeight` and clamps to [minHeight, maxHeight].
        std::vector<float> BuildHeights(const TerrainConfig& c, const Grid& g) {
            const int vz = c.cellsZ + 1;
            const float halfRange = (c.maxHeight - c.minHeight) * 0.5f;

            std::vector<float> heights(static_cast<std::size_t>(g.vx) * vz);

            for (int z = 0; z < vz; ++z) {
                const float pz = g.startZ + z * g.stepZ;
                float* row = &heights[static_cast<std::size_t>(z) * g.vx];
                for (int x = 0; x < g.vx; ++x) {
                    const float px = g.startX + x * g.stepX;

                    float e = FractalNoise(px, pz, c, 0.35f) * 0.85f +
                              FractalNoise(px, pz, c, 1.0f) * 0.15f;
                    e = std::copysign(std::pow(std::abs(e), 0.7f), e);

                    row[x] = std::clamp(c.baseHeight + e * halfRange, c.minHeight, c.maxHeight);
                }
            }
            return heights;
        }

        /// @brief Cell range `[x0, x1) x [z0, z1)`.
        struct CellRange { int x0, x1, z0, z1; };

        /// @brief Builds flat-shaded triangles (two per cell, unique vertices each) for a range of cells.
        /// @param g      Grid. @param h Height field. @param levels Biome heights. @param r Cell range.
        /// @param bounds Optional output: local-space bounding box of the range (may be null).
        Mesh BuildTerrainRange(const Grid& g, const std::vector<float>& h,
                               const Levels& levels, const CellRange& r, AABB* bounds)
        {
            const float sxz = g.stepX * g.stepZ;

            const std::size_t triangleCount =
                static_cast<std::size_t>(r.x1 - r.x0) * (r.z1 - r.z0) * 2;

            Mesh mesh{};
            mesh.vertices.reserve(triangleCount * 3);
            mesh.colors.reserve(triangleCount * 3);
            mesh.indices.reserve(triangleCount * 3);

            struct P { float x, y, z; };

            // Emits one flat-shaded triangle. (nx, ny, nz) is the unnormalized face normal.
            const auto Emit = [&](const P& p0, const P& p1, const P& p2,
                                  float nx, float ny, float nz) {
                const float invLen = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
                const Vec3f normal{ nx * invLen, ny * invLen, nz * invLen };

                const float avg = (p0.y + p1.y + p2.y) * (1.0f / 3.0f);
                const Color color = GetColor(avg, normal[1], levels);

                const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
                for (const P* p : { &p0, &p1, &p2 }) {
                    mesh.vertices.emplace_back(Vec4f{ p->x, p->y, p->z, 1.0f }, Vec2f{}, normal);
                    mesh.colors.push_back(color);
                }
                mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2 });
            };

            float minY = std::numeric_limits<float>::max();
            float maxY = std::numeric_limits<float>::lowest();

            for (int z = r.z0; z < r.z1; ++z) {
                const float z0 = g.startZ + z * g.stepZ;
                const float z1 = z0 + g.stepZ;
                const float* r0 = &h[static_cast<std::size_t>(z) * g.vx];
                const float* r1 = r0 + g.vx;

                for (int x = r.x0; x < r.x1; ++x) {
                    const float x0 = g.startX + x * g.stepX;
                    const float x1 = x0 + g.stepX;

                    const float ha = r0[x], hb = r0[x + 1];
                    const float hc = r1[x], hd = r1[x + 1];
                    minY = std::min({ minY, ha, hb, hc, hd });
                    maxY = std::max({ maxY, ha, hb, hc, hd });

                    const P a{ x0, ha, z0 }, b{ x1, hb, z0 };
                    const P c{ x0, hc, z1 }, d{ x1, hd, z1 };

                    Emit(a, c, b, -g.stepZ * (hb - ha), sxz, -g.stepX * (hc - ha));
                    Emit(b, c, d,  g.stepZ * (hc - hd), sxz, -g.stepX * (hd - hb));
                }
            }

            if (bounds) {
                *bounds = { Vec3f{ g.startX + r.x0 * g.stepX, minY, g.startZ + r.z0 * g.stepZ },
                            Vec3f{ g.startX + r.x1 * g.stepX, maxY, g.startZ + r.z1 * g.stepZ } };
            }
            return mesh;
        }
    }

    Mesh GenerateTerrain(const TerrainConfig& config) {
        Validate(config);
        const Grid grid(config);
        return BuildTerrainRange(grid, BuildHeights(config, grid), ComputeLevels(config),
                                 { 0, config.cellsX, 0, config.cellsZ }, nullptr);
    }

    std::vector<TerrainChunk> GenerateTerrainChunks(const TerrainConfig& config, int chunkCells) {
        Validate(config);
        if (chunkCells <= 0) {
            throw std::invalid_argument("Terrain chunk size must be positive");
        }

        const Grid grid(config);
        const std::vector<float> heights = BuildHeights(config, grid);
        const Levels levels = ComputeLevels(config);

        std::vector<TerrainChunk> chunks;
        chunks.reserve(static_cast<std::size_t>((config.cellsX + chunkCells - 1) / chunkCells) *
                       ((config.cellsZ + chunkCells - 1) / chunkCells));

        for (int z = 0; z < config.cellsZ; z += chunkCells) {
            for (int x = 0; x < config.cellsX; x += chunkCells) {
                TerrainChunk& chunk = chunks.emplace_back();
                chunk.mesh = BuildTerrainRange(
                    grid, heights, levels,
                    { x, std::min(x + chunkCells, config.cellsX),
                      z, std::min(z + chunkCells, config.cellsZ) },
                    &chunk.bounds);
            }
        }
        return chunks;
    }

    Mesh GeneratePineTrees(const TerrainConfig& config, int treeCount)
    {
        Validate(config);

        const Grid grid(config);
        const auto heights = BuildHeights(config, grid);

        Mesh mesh{};
        if (treeCount <= 0)
            return mesh;

        constexpr Color trunkColor{ 145, 100, 65, 255 };

        std::mt19937 rng(config.seed + 12345u);

        // Appends one flat-shaded triangle (own 3 vertices, face normal). Degenerate triangles are skipped.
        // Vertices are emitted in the order c, b, a. The normal is computed once from (a, b, c).
        auto emitTriangle = [&](Vec3f a, Vec3f b, Vec3f c, Color color)
        {
            const Vec3f ab = b - a;
            const Vec3f ac = a - c;

            Vec3f normal{
                ab[1] * ac[2] - ab[2] * ac[1],
                ab[2] * ac[0] - ab[0] * ac[2],
                ab[0] * ac[1] - ab[1] * ac[0]
            };

            const float length = std::sqrt(
                normal[0] * normal[0] +
                normal[1] * normal[1] +
                normal[2] * normal[2]);

            if (length <= 0.00001f)
                return;

            normal[0] /= length;
            normal[1] /= length;
            normal[2] /= length;

            const auto base =
                static_cast<std::uint32_t>(mesh.vertices.size());

            for (const Vec3f* p : { &c, &b, &a })
            {
                mesh.vertices.emplace_back(
                    Vec4f{ (*p)[0], (*p)[1], (*p)[2], 1.0f },
                    Vec2f{},
                    normal
                );

                mesh.colors.push_back(color);
            }

            mesh.indices.insert(
                mesh.indices.end(),
                { base, base + 1, base + 2 });
        };

        // Ground height at (x, z): bilinear interpolation of the height field used by the terrain.
        auto getHeight = [&](float x, float z)
        {
            const float gx = std::clamp(
                (x - grid.startX) / grid.stepX,
                0.0f, static_cast<float>(config.cellsX));

            const float gz = std::clamp(
                (z - grid.startZ) / grid.stepZ,
                0.0f, static_cast<float>(config.cellsZ));

            const int x0 = std::min(
                static_cast<int>(gx), config.cellsX - 1);
            const int z0 = std::min(
                static_cast<int>(gz), config.cellsZ - 1);

            const float tx = gx - x0;
            const float tz = gz - z0;

            const auto at = [&](int ix, int iz)
            {
                return heights[
                    static_cast<std::size_t>(iz) * grid.vx + ix];
            };

            const float h0 = at(x0, z0) * (1.0f - tx)
                           + at(x0 + 1, z0) * tx;
            const float h1 = at(x0, z0 + 1) * (1.0f - tx)
                           + at(x0 + 1, z0 + 1) * tx;

            return h0 * (1.0f - tz) + h1 * tz;
        };

        std::uniform_real_distribution<float> posX(
            grid.startX + 1.0f, -grid.startX - 1.0f);
        std::uniform_real_distribution<float> posZ(
            grid.startZ + 1.0f, -grid.startZ - 1.0f);
        std::uniform_real_distribution<float> heightScale(0.3f, 0.9f);
        std::uniform_real_distribution<float> widthScale(0.5f, 0.8f);

        constexpr int sides = 5;

        for (int i = 0; i < treeCount; ++i)
        {
            const float x = posX(rng);
            const float z = posZ(rng);
            const float ground = getHeight(x, z);

            const float normalizedHeight = std::clamp(
                (ground - config.minHeight) /
                (config.maxHeight - config.minHeight),
                0.0f, 1.0f
            );

            const bool isSnow = normalizedHeight >= config.snowLevel;

            // Low-frequency noise defines broad forest regions.
            constexpr float forestNoiseScale = 0.5f;
            constexpr float forestThreshold = 0.01f;

            const float forestNoise =
                FractalNoise(x, z, config, forestNoiseScale);

            if (forestNoise < forestThreshold)
                continue;

            // Keep trees on land, above the water level.
            if (ground <= config.waterLevel + 0.15f)
                continue;

            const Color foliageColor = isSnow
                ? Color{ 255, 255, 255, 255 }
                : Color{ 135, 220, 100, 255 };

            const float scaleH = heightScale(rng);
            const float scaleW = widthScale(rng);

            const float trunkHeight = 0.7f * scaleH;
            const float trunkRadius = 0.12f * scaleW;

            // Trunk: low-poly cylinder.
            for (int s = 0; s < sides; ++s)
            {
                const float a0 = 2.0f * std::numbers::pi_v<float>
                               * s / sides;
                const float a1 = 2.0f * std::numbers::pi_v<float>
                               * (s + 1) / sides;

                const Vec3f p0{
                    x + std::cos(a0) * trunkRadius,
                    ground,
                    z + std::sin(a0) * trunkRadius
                };
                const Vec3f p1{
                    x + std::cos(a1) * trunkRadius,
                    ground,
                    z + std::sin(a1) * trunkRadius
                };
                const Vec3f p2{
                    p1[0], ground + trunkHeight, p1[2]
                };
                const Vec3f p3{
                    p0[0], ground + trunkHeight, p0[2]
                };

                emitTriangle(p0, p1, p2, trunkColor);
                emitTriangle(p0, p2, p3, trunkColor);
            }

            // Three overlapping conical layers of foliage.
            for (int layer = 0; layer < 3; ++layer)
            {
                const float layerBase =
                    ground + trunkHeight + layer * 0.45f * scaleH;
                const float radius =
                    (0.65f - layer * 0.15f) * scaleW;
                const float tipY = layerBase + 0.8f * scaleH;

                const Vec3f tip{x, tipY, z};

                for (int s = 0; s < sides; ++s)
                {
                    const float a0 = 2.0f * std::numbers::pi_v<float>
                                   * s / sides;
                    const float a1 = 2.0f * std::numbers::pi_v<float>
                                   * (s + 1) / sides;

                    const Vec3f p0{
                        x + std::cos(a0) * radius,
                        layerBase,
                        z + std::sin(a0) * radius
                    };
                    const Vec3f p1{
                        x + std::cos(a1) * radius,
                        layerBase,
                        z + std::sin(a1) * radius
                    };

                    emitTriangle(p0, p1, tip, foliageColor);
                }
            }
        }

        return mesh;
    }

    Mesh GenerateWater(const TerrainConfig& config) {
        Validate(config);

        const Grid g(config);
        const float level = config.waterLevel;
        const float waterY = level + 0.001f;

        const std::vector<float> h = BuildHeights(config, g);

        struct Point { float x, z, height; };
        Mesh mesh{};

        const auto AddWaterTriangle = [&](const Point& a, const Point& b, const Point& c) {
            const auto base = static_cast<std::uint32_t>(mesh.vertices.size());
            for (const Point* p : { &a, &b, &c }) {

                mesh.vertices.emplace_back(
                    Vec4f{ p->x, waterY, p->z, 1.0f },
                    Vec2f{ p->x, p->z },
                    Vec3f{ 0.0f, 1.0f, 0.0f }
                );

                mesh.colors.push_back(Water);
            }
            mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2 });
        };

        // Sutherland-Hodgman against the plane y = level, keeping the part below it.
        const auto ClipTriangle = [&](const Point& a, const Point& b, const Point& c) {
            const std::array<Point, 3> tri{ a, b, c };
            std::array<Point, 4> poly;
            int n = 0;

            for (int i = 0; i < 3; ++i) {
                const Point& cur = tri[i];
                const Point& nxt = tri[(i + 1) % 3];
                const bool curUnder = cur.height < level;
                const bool nxtUnder = nxt.height < level;

                if (curUnder) poly[n++] = cur;
                if (curUnder != nxtUnder) {
                    const float t = (level - cur.height) / (nxt.height - cur.height);
                    poly[n++] = { cur.x + (nxt.x - cur.x) * t,
                                  cur.z + (nxt.z - cur.z) * t, level };
                }
            }

            for (int i = 1; i + 1 < n; ++i)
                AddWaterTriangle(poly[0], poly[i], poly[i + 1]);
        };

        for (int z = 0; z < config.cellsZ; ++z) {
            const float z0 = g.startZ + z * g.stepZ;
            const float z1 = z0 + g.stepZ;
            const float* r0 = &h[static_cast<std::size_t>(z) * g.vx];
            const float* r1 = r0 + g.vx;

            for (int x = 0; x < config.cellsX; ++x) {
                const float ha = r0[x], hb = r0[x + 1];
                const float hc = r1[x], hd = r1[x + 1];

                // Whole cell above water: skip both triangles.
                if (std::min(std::min(ha, hb), std::min(hc, hd)) >= level)
                    continue;

                const float x0 = g.startX + x * g.stepX;
                const float x1 = x0 + g.stepX;

                const Point a{ x0, z0, ha }, b{ x1, z0, hb };
                const Point c{ x0, z1, hc }, d{ x1, z1, hd };

                ClipTriangle(a, c, b);
                ClipTriangle(b, c, d);
            }
        }

        return mesh;
    }

    float GetTerrainLevel(float baseHeight, float minHeight, float maxHeight, float relativeLevel) {
        const float range = maxHeight - minHeight;
        return std::clamp(baseHeight + (relativeLevel - 0.5f) * range, minHeight, maxHeight);
    }
}
