#pragma once

#include <cstdint>
#include <vector>

#include <Math/Vec.h>
#include <SDL3/SDL_ttf.h>

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

    struct Tile {
        int x0, y0, x1, y1;
    };

	enum class CullMode {
		None,
		Back,
		Front
	};

	enum class CameraType {
		Perspective,
		Orthographic
	};

	enum class DepthTestMode {
		None,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		Equal,
		NotEqual
	};

	enum class BlendMode {
		None,
		Alpha,
		Additive,
		Multiply
	};

    // 8-bit color depth
    struct alignas(4) Color {
        uint8_t r{ 0 };
        uint8_t g{ 0 };
        uint8_t b{ 0 };
        uint8_t a{ 255 };
    };

    struct FrameBuffer {
        uint32_t* pixels{ nullptr };
        uint16_t width{}, height{};
    };

    struct DepthBuffer {
        std::vector<float> pixels{};
        uint16_t width{}, height{};
    };

    struct Vertex {
        Vec4f position;
        Vec2f texcoord;
        Vec3f normal;
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<Color> colors;
    };

    struct Text {
        Text(TTF_Font* font, const char* text, float size, SDL_Color color, int x, int y) {
            this->text = text;
            this->font = font;
            this->x = x, this->y = y;
            this->surface = TTF_RenderText_Blended(
                font,
                text,
                0,
                color
            );
            this->fontSize = size;
            TTF_SetFontSize(this->font, this->fontSize);
        }
        float fontSize;
        int x, y;
        SDL_Color color;
        const char* text;
        SDL_Surface* surface;
        TTF_Font* font;
    };
}

struct IndexKey {
    uint32_t v, t, n;
    bool operator==(const IndexKey&) const = default;
};

template<>
struct std::hash<IndexKey> {
    size_t operator()(const IndexKey& k) const noexcept
    {
        auto mix = [](size_t h, size_t x) {
            return h ^ (x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
        };
        size_t h = k.v;
        h = mix(h, k.t);
        h = mix(h, k.n);
        return h;
    }
};
