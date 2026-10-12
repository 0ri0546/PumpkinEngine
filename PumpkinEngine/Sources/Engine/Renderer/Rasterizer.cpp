#include "Rasterizer.hpp"

#include <Renderer/Types.hpp>
#include <Utils/Utils.hpp>

#include <algorithm>
#include <cmath>

namespace Pumpkin {
    void Rasterize(
		const std::pair<uint32_t2, uint32_t2>& group,
		const std::vector<ScreenVertex>& v,
		const std::vector<Color>& colors,
		float* depth,
		uint32_t* pixels,
		uint32_t width
	) {
		const auto edge = [](const ScreenVertex& a, const ScreenVertex& b, float px, float py) {
			return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
		};

		const float area = edge(v[0], v[1], v[2].x, v[2].y);
		if (area == 0.0f) {
			return; // degenerate
		}
		const float invArea = 1.0f / area;

		// bounding box of the triangle, clamped to this thread's rows/columns
		const float minXf = std::min({ v[0].x, v[1].x, v[2].x });
		const float maxXf = std::max({ v[0].x, v[1].x, v[2].x });
		const float minYf = std::min({ v[0].y, v[1].y, v[2].y });
		const float maxYf = std::max({ v[0].y, v[1].y, v[2].y });

		const uint32_t x0 = static_cast<uint32_t>(std::max(static_cast<float>(group.first.first), std::floor(minXf)));
		const uint32_t x1 = static_cast<uint32_t>(std::min(static_cast<float>(group.second.first), std::ceil(maxXf) + 1.0f));
		const uint32_t y0 = static_cast<uint32_t>(std::max(static_cast<float>(group.first.second), std::floor(minYf)));
		const uint32_t y1 = static_cast<uint32_t>(std::min(static_cast<float>(group.second.second), std::ceil(maxYf) + 1.0f));

		for (uint32_t y = y0; y < y1; ++y) {
			for (uint32_t x = x0; x < x1; ++x) {
				// sample at the pixel center
				const float px = x + 0.5f;
				const float py = y + 0.5f;

				// barycentric weights (work for either winding because of the division by area)
				const float w0 = edge(v[1], v[2], px, py) * invArea;
				const float w1 = edge(v[2], v[0], px, py) * invArea;
				const float w2 = edge(v[0], v[1], px, py) * invArea;

				if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
					continue; // outside the triangle
				}

				// depth is linear in screen space
				const float z = w0 * v[0].z + w1 * v[1].z + w2 * v[2].z;
				const size_t idx = static_cast<size_t>(y) * width + x;
				if (z < 0.0f || z >= depth[idx]) {
					continue;
				}
				depth[idx] = z;

				// perspective-correct weights for attributes (identical to w0..w2 when all w == 1)
				float b0 = w0 * v[0].invW;
				float b1 = w1 * v[1].invW;
				float b2 = w2 * v[2].invW;
				const float inv = 1.0f / (b0 + b1 + b2);
				b0 *= inv; b1 *= inv; b2 *= inv;

				pixels[idx] = ToRGBA(
					(b0 * colors[0].r + b1 * colors[1].r + b2 * colors[2].r) / 255.0f,
					(b0 * colors[0].g + b1 * colors[1].g + b2 * colors[2].g) / 255.0f,
					(b0 * colors[0].b + b1 * colors[1].b + b2 * colors[2].b) / 255.0f);
			}
		}
	}
}
