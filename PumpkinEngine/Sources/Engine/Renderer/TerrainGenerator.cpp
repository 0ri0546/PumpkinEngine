module Pumpkin.Renderer.TerrainGenerator;

import Pumpkin.Renderer.Types;

import Pumpkin.Math.Vec;

import std;

namespace Pumpkin {
    namespace {
        // Biome colors
        constexpr Color Water{ 35, 145, 220, 127 };
        constexpr Color Sand{ 230, 210, 145, 255 };
        constexpr Color Grass{ 100, 190, 75, 255 };
        constexpr Color Rock{ 125, 115, 105, 255 };
        constexpr Color Snow{ 245, 245, 240, 255 };

        // Noise

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
                * (1.0f / 8388607.5f) - 1.0f;
        }

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

        // Config helpers

        void Validate(const TerrainConfig& c) {
            if (c.cellsX <= 0 || c.cellsZ <= 0 ||
                c.sizeX <= 0.0f || c.sizeZ <= 0.0f ||
                c.maxHeight <= c.minHeight ||
                c.noiseScale <= 0.0f || c.noiseOctaves < 1) {
                throw std::invalid_argument("Invalid terrain configuration");
            }
        }

        struct Levels { float sand, grass, rock, snow; };

        Levels ComputeLevels(const TerrainConfig& c) {
            const auto Level = [&](float relative) {
                return GetTerrainLevel(c.baseHeight, c.minHeight, c.maxHeight, relative);
            };
            return { Level(c.sandLevel), Level(c.grassLevel),
                     Level(c.rockLevel), Level(c.snowLevel) };
        }

        Color GetColor(float height, float slope, const Levels& l) {
            if (height >= l.snow) return Snow;
            if (height >= l.rock) return Rock;
            if (slope < 0.75f && height >= l.grass) return Rock;
            if (height < l.sand) return Sand;
            return Grass;
        }

        // Height grid

        // Cell size and world-space origin of the grid, shared by terrain and water.
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

        // Row-major (cellsZ+1) x (cellsX+1) height field, shared by terrain and water.
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

        // Terrain meshing

        // Cell range [x0, x1) x [z0, z1).
        struct CellRange { int x0, x1, z0, z1; };

        // Builds flat-shaded triangles for a range of cells.
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

    // Public API

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
