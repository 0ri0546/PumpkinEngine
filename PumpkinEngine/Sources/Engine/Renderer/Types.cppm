module;

#include <SDL_FixedIncludes.h>
#include <SDL3/SDL_ttf.h>

export module Pumpkin.Renderer.Types;

import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {
    enum class TextureType {
        Albedo,
        Normal,
        AO,
        Diffuse,
        Metal,
        Roughness,
        Displacement
    };

    struct Texture {
        TextureType type = TextureType::Albedo;
        int width = 0;
        int height = 0;
        std::vector<std::uint32_t> pixels;
    };

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

	enum class ShaderId {
		PBR,
		BlinnPhong,
        Lambert,
		VertexColor, 
        Toon
	};

    // 8-bit color depth
    struct alignas(4) Color {
        std::uint8_t r{ 0 };
        std::uint8_t g{ 0 };
        std::uint8_t b{ 0 };
        std::uint8_t a{ 255 };
    };

    struct Material {
        // Factors
        Color baseColor{ 255, 255, 255, 255 };
        float metallic = 1.0f;
        float roughness = 1.0f;
        float ao = 1.0f;

        // Texture slots
        const Texture* albedo = nullptr;
        const Texture* normal = nullptr;
        const Texture* orm = nullptr; // R = AO, G = roughness, B = metallic

        // Pipeline state
        ShaderId      shader = ShaderId::PBR;
        DepthTestMode depthTest = DepthTestMode::Less;
        BlendMode     blend = BlendMode::Alpha;
        bool          depthWrite = true;
    };

    struct FrameBuffer {
        std::uint32_t* pixels{ nullptr };
        std::uint16_t width{}, height{};
    };

    struct DepthBuffer {
        std::vector<float> pixels{};
        std::uint16_t width{}, height{};
    };

    struct Vertex {
        Vec4f position;
        Vec2f texcoord;
        Vec3f normal;
    };

    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<Color> colors;
    };

    struct Text {
        Text(TTF_Font* font, const char* text, float size, SDL_Color color, int x, int y) {
            this->text = text;
            this->font = font;
            this->x = x, this->y = y;
            this->color = { color.b, color.g, color.r, color.a };
            this->surface = TTF_RenderText_Blended(
                font,
                text,
                0,
                this->color
            );
            this->fontSize = size;
            TTF_SetFontSize(this->font, this->fontSize);
        }

        Text& SetSize(float size) {
            if (size <= 0.0f)
                return *this;
            fontSize = size;
            return *this;
        }

        Text& SetFont(TTF_Font* newFont) {
            font = newFont;
            return *this;
        }

        Text& SetText(const char* txt) {
            text = txt;
            return *this;
        }

        Text& SetColor(SDL_Color rgba) {
            color = { rgba.b, rgba.g, rgba.r, rgba.a };
            return *this;
        }

        Text& SetPosition(int x, int y) {
            this->x = x,this->y = y;
            return *this;
        }

        Text& Build() {
            TTF_SetFontSize(font, fontSize);
            surface = TTF_RenderText_Blended(font, text, 0, color);
            return *this;
        }

        void Move(int x_dist, int y_dist) {
            this->x += x_dist, this->y += y_dist;
        }

        float fontSize;
        int x, y;
        SDL_Color color{};
        const char* text;
        SDL_Surface* surface;
        TTF_Font* font;
    };

    struct IndexKey {
        std::uint32_t v, t, n;
        bool operator==(const IndexKey&) const = default;
    };
}

template<>
struct std::hash<Pumpkin::IndexKey> {
    std::size_t operator()(const Pumpkin::IndexKey& k) const noexcept
    {
        auto mix = [](std::size_t h, std::size_t x) {
            return h ^ (x + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
        };
        std::size_t h = k.v;
        h = mix(h, k.t);
        h = mix(h, k.n);
        return h;
    }
};
