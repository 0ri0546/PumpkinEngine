#include "Rasterizer.hpp"

#include <Renderer/Types.hpp>
#include <Utils/Utils.hpp>

#include <algorithm>
#include <cmath>

namespace Pumpkin {

    // A value that changes in a straight line across the screen.
    // We store where it starts and how much it changes per pixel.
    struct Plane {
        float start; // value at the first pixel
        float dx;    // change per pixel to the right
        float dy;    // change per pixel down
    };

    // Returns w0 * a + w1 * b + w2 * c, for start, dx and dy.
    static Plane Mix(const Plane& w0, const Plane& w1, const Plane& w2,
        float a, float b, float c) {
        return {
            w0.start * a + w1.start * b + w2.start * c,
            w0.dx * a + w1.dx * b + w2.dx * c,
            w0.dy * a + w1.dy * b + w2.dy * c
        };
    }

    void Rasterize(
        const std::pair<Vec2u, Vec2u>& group,
        const std::vector<Vec4f>& v,
        const std::vector<Color>& colors,
        float* depth,
        uint32_t* pixels,
        uint32_t width
    ) {
        // Copy to locals (faster than reading v[i][j] repeatedly)
        const float ax = v[0][0], ay = v[0][1], az = v[0][2], ak = v[0][3];
        const float bx = v[1][0], by = v[1][1], bz = v[1][2], bk = v[1][3];
        const float cx = v[2][0], cy = v[2][1], cz = v[2][2], ck = v[2][3];

        const float area = (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
        if (area == 0.0f) {
            return; // flat triangle
        }
        const float invArea = 1.0f / area;

        // Bounding box, clamped to this thread's part of the screen
        const uint32_t x0 = static_cast<uint32_t>(std::max(static_cast<float>(group.first[0]), std::floor(std::min({ ax, bx, cx }))));
        const uint32_t x1 = static_cast<uint32_t>(std::min(static_cast<float>(group.second[0]), std::ceil(std::max({ ax, bx, cx })) + 1.0f));
        const uint32_t y0 = static_cast<uint32_t>(std::max(static_cast<float>(group.first[1]), std::floor(std::min({ ay, by, cy }))));
        const uint32_t y1 = static_cast<uint32_t>(std::min(static_cast<float>(group.second[1]), std::ceil(std::max({ ay, by, cy })) + 1.0f));
        if (x0 >= x1 || y0 >= y1) {
            return; // outside our part of the screen
        }

        // Center of the first pixel
        const float sx = x0 + 0.5f;
        const float sy = y0 + 0.5f;

        // Barycentric weight of the vertex opposite to the edge p -> q
        const auto weightPlane = [&](float px, float py, float qx, float qy) {
            return Plane{
                ((sx - px) * (qy - py) - (sy - py) * (qx - px)) * invArea,
                (qy - py) * invArea,
                -(qx - px) * invArea
            };
        };

        const Plane w0 = weightPlane(bx, by, cx, cy);
        const Plane w1 = weightPlane(cx, cy, ax, ay);
        const Plane w2 = weightPlane(ax, ay, bx, by);

        const Plane Z = Mix(w0, w1, w2, az, bz, cz);

        // Perspective-correct color = (R, G, B) / D
        // Top and bottom are both linear, so we divide once per drawn pixel.
        const Plane D = Mix(w0, w1, w2, ak, bk, ck);
        const Plane R = Mix(w0, w1, w2, ak * colors[0].r, bk * colors[1].r, ck * colors[2].r);
        const Plane G = Mix(w0, w1, w2, ak * colors[0].g, bk * colors[1].g, ck * colors[2].g);
        const Plane B = Mix(w0, w1, w2, ak * colors[0].b, bk * colors[1].b, ck * colors[2].b);

        for (uint32_t y = y0; y < y1; ++y) {
            const float rows = static_cast<float>(y - y0);

            // Values at the start of this row
            float cw0 = w0.start + w0.dy * rows;
            float cw1 = w1.start + w1.dy * rows;
            float cw2 = w2.start + w2.dy * rows;
            float curZ = Z.start + Z.dy * rows;
            float cd = D.start + D.dy * rows;
            float cr = R.start + R.dy * rows;
            float cg = G.start + G.dy * rows;
            float cb = B.start + B.dy * rows;

            float* depthRow = depth + static_cast<size_t>(y) * width;

            for (uint32_t x = x0; x < x1; ++x) {
                const bool insideTriangle = cw0 >= 0.0f && cw1 >= 0.0f && cw2 >= 0.0f;
                const bool depthTest = curZ >= 0.0f && curZ < depthRow[x];

                if (insideTriangle && depthTest) {
                    depthRow[x] = curZ;

                    const float inv = 1.0f / cd;

                    // int32_t first: faster than float -> uint32_t directly
                    const uint32_t r = static_cast<uint32_t>(static_cast<int32_t>(cr * inv));
                    const uint32_t g = static_cast<uint32_t>(static_cast<int32_t>(cg * inv));
                    const uint32_t b = static_cast<uint32_t>(static_cast<int32_t>(cb * inv));

                    uint32_t* pixelRow = pixels + static_cast<size_t>(y) * width;
                    pixelRow[x] = (255u << 24) | (r << 16) | (g << 8) | b;
                }

                // step to next pixel
                cw0 += w0.dx; cw1 += w1.dx; cw2 += w2.dx;
                curZ += Z.dx; cd += D.dx;
                cr += R.dx; cg += G.dx; cb += B.dx;
            }
        }
    }
}
