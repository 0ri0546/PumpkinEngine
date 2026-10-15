#pragma once

#include <Renderer/Types.hpp>

#include <span>

namespace Pumpkin {
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

    struct Tile {
        int x0, y0, x1, y1;
    };

	struct Rasterizer {
		uint32_t 	  ClearColor{ 0x00000000u };
		CullMode      Culling   { CullMode::Back };
		DepthTestMode DepthTest { DepthTestMode::Less };
		BlendMode     AlphaBlend{ BlendMode::Alpha };
		CameraType    Projection{ CameraType::Perspective };
		
		void RasterizeFrame(
			std::span<const Vertex> v,
			std::span<const uint32_t> indices,
			std::span<const Color> colors,
			const FrameBuffer& fb,
			DepthBuffer& db
		);

	private:
		void BuildDrawOrder(std::span<const Triangle> tris, std::vector<uint32_t>& order);
        void BuildTriangle(std::span<const Vertex> v, std::span<const uint32_t> indices, std::span<const Color> colors, size_t tri, int width, int height, std::vector<Triangle>& dst);
		void BuildTriangles(std::span<const Vertex> v, std::span<const uint32_t> indices, std::span<const Color> colors, int width, int height, std::vector<Triangle>& out);
		void BinTriangles(std::span<const Triangle> triangles, std::span<const uint32_t> order, int tilesX, int tilesY, std::vector<std::vector<uint32_t>>& bins);
        void ClearTile(const Tile& tile, float* depth, uint32_t* pixels, uint32_t width) const;
        template<bool Transparent>
        void DrawInTile(const Triangle& t, const Tile& tile, float* depth, uint32_t* pixels, uint32_t width);
    };
}
