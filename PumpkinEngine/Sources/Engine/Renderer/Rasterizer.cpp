#include "Rasterizer.hpp"

#include <Renderer/Types.hpp>
#include <Utils/Utils.hpp>
#include <Utils/ThreadPool.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

namespace Pumpkin {
    namespace {

        using u32 = std::uint32_t;

        constexpr int kTileSize = 64;

        // Plane: a value that changes in a straight line across the screen.
        struct Plane {
            float start; // value at the first pixel
            float dx;    // change per pixel to the right
            float dy;    // change per pixel down

            // Value `row` pixels down and `col` pixels right of the start.
            [[nodiscard]] float At(float row, float col = 0.0f) const {
                return start + dy * row + dx * col;
            }
        };

        // Returns w0 * a + w1 * b + w2 * c for start, dx and dy.
        [[nodiscard]] Plane Mix(const Plane& w0, const Plane& w1, const Plane& w2,
                                float a, float b, float c) {
            return {
                w0.start * a + w1.start * b + w2.start * c,
                w0.dx    * a + w1.dx    * b + w2.dx    * c,
                w0.dy    * a + w1.dy    * b + w2.dy    * c
            };
        }

        // Everything the rasterizer needs to draw one triangle.
        struct Triangle {
            int x0, y0, x1, y1;  // screen bounding box, [x0, x1) x [y0, y1)
            bool transparent;
            float zKey;          // used to sort triangles
            Plane w0, w1, w2;    // barycentric weights
            Plane z;             // depth
            Plane k;             // perspective term (the "w" divide)
            Plane r, g, b, a;    // color channels, already multiplied by k
        };


        [[nodiscard]] ThreadPool& GetPool() {
            // Max threads - 2, counting the calling thread.
            static const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
            static const unsigned total = hw > 2 ? hw - 2 : 1;
            static ThreadPool pool(total - 1);
            return pool;
        }

        // ------------------------------------------------------------------
        // Step 1: setup. Do the heavy math once per triangle.
        // ------------------------------------------------------------------
        void BuildTriangles(std::span<const Vec4f> v, std::span<const Color> colors,
                            int width, int height, std::vector<Triangle>& out) {
            out.clear();

            for (size_t i = 2; i < v.size(); i += 3) {
                // Copy the vertices to locals (faster than indexing again and again).
                const float ax = v[i - 2][0], ay = v[i - 2][1], az = v[i - 2][2], ak = v[i - 2][3];
                const float bx = v[i - 1][0], by = v[i - 1][1], bz = v[i - 1][2], bk = v[i - 1][3];
                const float cx = v[i][0],     cy = v[i][1],     cz = v[i][2],     ck = v[i][3];

                // Twice the signed area. Zero means a flat triangle.
                const float area = (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
                if (area == 0.0f) continue;
                const float invArea = 1.0f / area;

                // Bounding box, clamped to the screen (in float, before the int cast).
                const float fx0 = std::max(0.0f, std::floor(std::min({ ax, bx, cx })));
                const float fy0 = std::max(0.0f, std::floor(std::min({ ay, by, cy })));
                const float fx1 = std::min(static_cast<float>(width),  std::ceil(std::max({ ax, bx, cx })) + 1.0f);
                const float fy1 = std::min(static_cast<float>(height), std::ceil(std::max({ ay, by, cy })) + 1.0f);
                if (fx0 >= fx1 || fy0 >= fy1) continue; // fully offscreen

                Triangle t;
                t.x0 = static_cast<int>(fx0);
                t.y0 = static_cast<int>(fy0);
                t.x1 = static_cast<int>(fx1);
                t.y1 = static_cast<int>(fy1);

                // Center of the first pixel.
                const float sx = static_cast<float>(t.x0) + 0.5f;
                const float sy = static_cast<float>(t.y0) + 0.5f;

                // Weight of the vertex opposite to the edge p -> q.
                const auto weightPlane = [&](float px, float py, float qx, float qy) {
                    return Plane{
                        ((sx - px) * (qy - py) - (sy - py) * (qx - px)) * invArea,
                        (qy - py) * invArea,
                        -(qx - px) * invArea
                    };
                };
                t.w0 = weightPlane(bx, by, cx, cy);
                t.w1 = weightPlane(cx, cy, ax, ay);
                t.w2 = weightPlane(ax, ay, bx, by);

                const Color& c0 = colors[i - 2];
                const Color& c1 = colors[i - 1];
                const Color& c2 = colors[i];

                t.transparent = c0.a < 255 || c1.a < 255 || c2.a < 255;
                t.zKey = az + bz + cz;

                t.z = Mix(t.w0, t.w1, t.w2, az, bz, cz);

                // Perspective-correct color = (color * k) / k.
                // Both parts are linear on screen, so we divide once per pixel.
                t.k = Mix(t.w0, t.w1, t.w2, ak, bk, ck);
                t.r = Mix(t.w0, t.w1, t.w2, ak * c0.r, bk * c1.r, ck * c2.r);
                t.g = Mix(t.w0, t.w1, t.w2, ak * c0.g, bk * c1.g, ck * c2.g);
                t.b = Mix(t.w0, t.w1, t.w2, ak * c0.b, bk * c1.b, ck * c2.b);
                t.a = Mix(t.w0, t.w1, t.w2, ak * c0.a, bk * c1.a, ck * c2.a);

                out.push_back(t);
            }
        }

        // Draw order, as indices (no copying of triangles).
        //  - Opaque first, front to back: hidden pixels get rejected early.
        //  - Transparent last, back to front: needed for correct blending.
        // A bigger z means farther away.
        void BuildDrawOrder(std::span<const Triangle> tris, std::vector<u32>& order) {
            order.resize(tris.size());
            std::ranges::iota(order, 0u);

            const auto zKey = [&](u32 i) { return tris[i].zKey; };

            const auto transparent = std::ranges::stable_partition(
                order, [&](u32 i) { return !tris[i].transparent; });
            const std::ranges::subrange opaque{ order.begin(), transparent.begin() };

            std::ranges::sort(opaque, std::less{}, zKey);
            std::ranges::sort(transparent, std::greater{}, zKey);
        }

        // ------------------------------------------------------------------
        // Step 2: binning. Each tile gets the list of triangles touching it.
        // The draw order is kept inside every list.
        // ------------------------------------------------------------------
        void BinTriangles(std::span<const Triangle> tris, std::span<const u32> order,
                          int tilesX, int tilesY, std::vector<std::vector<u32>>& bins) {
            bins.resize(static_cast<size_t>(tilesX) * tilesY);
            for (auto& bin : bins) bin.clear(); // keeps memory, no new allocation

            for (const u32 index : order) {
                const Triangle& t = tris[index];
                for (const int ty : std::views::iota(t.y0 / kTileSize, (t.y1 - 1) / kTileSize + 1))
                    for (const int tx : std::views::iota(t.x0 / kTileSize, (t.x1 - 1) / kTileSize + 1))
                        bins[static_cast<size_t>(ty) * tilesX + tx].push_back(index);
            }
        }

        // ------------------------------------------------------------------
        // Step 3: draw one triangle inside one tile.
        // ------------------------------------------------------------------

        // Keeps only the x values where `base + dx * x >= 0`.
        // `lo` and `hi` are the current span. Returns false if it becomes empty.
        [[nodiscard]] bool ClipSpan(float base, float dx, float& lo, float& hi) {
            if (dx > 0.0f)       lo = std::max(lo, std::ceil(-base / dx));
            else if (dx < 0.0f)  hi = std::min(hi, std::floor(-base / dx) + 1.0f);
            else if (base < 0.0f) return false;
            return lo < hi;
        }

        // float -> byte (truncates). Goes through int32 because it is faster.
        [[nodiscard]] u32 ToByte(float value) {
            return static_cast<u32>(static_cast<std::int32_t>(value));
        }

        [[nodiscard]] u32 PackColor(u32 r, u32 g, u32 b, u32 a = 255u) {
            return (a << 24) | (r << 16) | (g << 8) | b;
        }

        struct Tile {
            int x0, y0, x1, y1;
        };

        template <bool Transparent>
        void DrawInTile(const Triangle& t, const Tile& tile,
                        float* depth, u32* pixels, u32 width) {
            // Part of the triangle that is inside this tile.
            const int xa = std::max(t.x0, tile.x0), xb = std::min(t.x1, tile.x1);
            const int ya = std::max(t.y0, tile.y0), yb = std::min(t.y1, tile.y1);

            for (int y = ya; y < yb; ++y) {
                const auto row = static_cast<float>(y - t.y0);

                // Find the exact pixels of this row that are inside the triangle.
                auto lo = static_cast<float>(xa - t.x0);
                auto hi = static_cast<float>(xb - t.x0);
                if (!ClipSpan(t.w0.At(row), t.w0.dx, lo, hi)) continue;
                if (!ClipSpan(t.w1.At(row), t.w1.dx, lo, hi)) continue;
                if (!ClipSpan(t.w2.At(row), t.w2.dx, lo, hi)) continue;

                // Values at the first pixel of the span.
                float z = t.z.At(row, lo);
                float k = t.k.At(row, lo);
                float r = t.r.At(row, lo);
                float g = t.g.At(row, lo);
                float b = t.b.At(row, lo);
                float a = Transparent ? t.a.At(row, lo) : 0.0f;

                float* depthRow = depth + static_cast<size_t>(y) * width;
                u32* pixelRow = pixels + static_cast<size_t>(y) * width;

                static constexpr bool ortho = true;
                for (int x = t.x0 + static_cast<int>(lo), end = t.x0 + static_cast<int>(hi); x < end; ++x) {
                    // Depth test: smaller z is closer.
                    // Non orthographic projection doesn't draw behind the camera
                    if (ortho ? z < depthRow[x] : z >= 0.0f && z < depthRow[x]) {
                        const float inv = 1.0f / k;
                        const float sr = r * inv, sg = g * inv, sb = b * inv; // source color

                        if constexpr (!Transparent) {
                            depthRow[x] = z; // only opaque triangles write depth
                            pixelRow[x] = PackColor(ToByte(sr), ToByte(sg), ToByte(sb));
                        } else {
                            // Alpha blend: out = dst + (src - dst) * alpha
                            const float alpha = std::clamp(a * inv * (1.0f / 255.0f), 0.0f, 1.0f);
                            const u32 dst = pixelRow[x];
                            const auto dr = static_cast<float>((dst >> 16) & 0xFF);
                            const auto dg = static_cast<float>((dst >> 8) & 0xFF);
                            const auto db = static_cast<float>(dst & 0xFF);
                            pixelRow[x] = PackColor(
                                ToByte(std::lerp(dr, sr, alpha) + 0.5f),
                                ToByte(std::lerp(dg, sg, alpha) + 0.5f),
                                ToByte(std::lerp(db, sb, alpha) + 0.5f),
                                static_cast<uint32_t>(a));
                        }
                    }

                    // Step to the next pixel.
                    z += t.z.dx; k += t.k.dx;
                    r += t.r.dx; g += t.g.dx; b += t.b.dx;
                    if constexpr (Transparent) a += t.a.dx;
                }
            }
        }

        // Clears the color and depth of one tile.
        void ClearTile(const Tile& tile, float* depth, u32* pixels, u32 width, u32 clearColor) {
            const auto count = static_cast<size_t>(tile.x1 - tile.x0);
            for (int y = tile.y0; y < tile.y1; ++y) {
                const size_t offset = static_cast<size_t>(y) * width + tile.x0;
                std::ranges::fill(std::span{ depth + offset, count }, std::numeric_limits<float>::max());
                std::ranges::fill(std::span{ pixels + offset, count }, clearColor);
            }
        }

    } // namespace

    // Clears and draws a whole frame.
    void RasterizeFrame(
        std::span<const Vec4f> v,
        std::span<const Color> colors,
        const FrameBuffer& fb,
        DepthBuffer& db
    ) {
        if (fb.width == 0 || fb.height == 0) return;

        const int w = fb.width;
        const int h = fb.height;
        const int tilesX = (w + kTileSize - 1) / kTileSize;
        const int tilesY = (h + kTileSize - 1) / kTileSize;
        const auto tileCount = static_cast<u32>(tilesX * tilesY);

        // Reused every frame so we don't allocate again.
        static std::vector<Triangle> triangles;
        static std::vector<u32> order;
        static std::vector<std::vector<u32>> bins;

        const bool validInput = true; //v.size() % 3 == 0 && colors.size() >= v.size();
        if (validInput) {
            BuildTriangles(v, colors, w, h, triangles);
            BuildDrawOrder(triangles, order);
        } else {
            triangles.clear(); // bad input: just clear the screen
            order.clear();
        }
        BinTriangles(triangles, order, tilesX, tilesY, bins);

        // Each thread grabs the next free tile until none are left.
        std::atomic<u32> nextTile{ 0 };
        float* depth = db.pixels.data();

        const std::function<void()> worker = [&] {
            for (;;) {
                const u32 index = nextTile.fetch_add(1, std::memory_order_relaxed);
                if (index >= tileCount) return;

                const int x0 = static_cast<int>(index % tilesX) * kTileSize;
                const int y0 = static_cast<int>(index / tilesX) * kTileSize;
                const Tile tile{ x0, y0, std::min(x0 + kTileSize, w), std::min(y0 + kTileSize, h) };

                ClearTile(tile, depth, fb.pixels, fb.width, fb.clearColor);

                for (const u32 triIndex : bins[index]) {
                    const Triangle& t = triangles[triIndex];
                    if (t.transparent) DrawInTile<true>(t, tile, depth, fb.pixels, fb.width);
                    else               DrawInTile<false>(t, tile, depth, fb.pixels, fb.width);
                }
            }
        };

        GetPool().Run(worker);
    }
}
