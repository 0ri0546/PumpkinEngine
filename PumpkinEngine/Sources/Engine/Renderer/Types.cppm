/// @file Types.cppm
/// @brief Module Pumpkin.Renderer.Types: plain data types shared by every stage of the software renderer.

module;

#include <DLL/Export.hpp>
#include <SDL_FixedIncludes.h>
#include <SDL3/SDL_ttf.h>

export module Pumpkin.Renderer.Types;

import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {

    /// @brief CPU image: row-major packed pixels.
    ///
    /// Each pixel is `0xRRGGBBAA` (red in the most significant byte), the top image row first.
    /// `pixels.size()` must equal `width * height`; a texture with a non-positive size or no
    /// pixels is treated as "absent" by the rasterizer and by SampleTexture().
    ///
    /// @see LoadTexture, SampleTexture
    struct PUMPKIN_API Texture {
        int width = 0;
        int height = 0;
        std::vector<std::uint32_t> pixels;
    };

    /// @brief A value that varies linearly across the screen (`start + dx * col + dy * row`).
    ///
    /// Used for barycentric weights, depth, 1/w and UVs so the rasterizer can step them with one
    /// addition per pixel. `start` is the value at the centre of the primitive's first pixel
    /// (`Primitive::x0`, `Primitive::y0`).
    struct Plane {
        float start; // value at the first pixel
        float dx;    // change per pixel to the right
        float dy;    // change per pixel down

        /// @brief Evaluates the plane.
        /// @param row Pixels down from the first pixel.
        /// @param col Pixels right of the first pixel (default 0).
        /// @return `start + dy * row + dx * col`.
        // Value `row` pixels down and `col` pixels right of the start.
        [[nodiscard]] float At(float row, float col = 0.0f) const {
            return start + dy * row + dx * col;
        }
    };

    /// @brief Pixel rectangle `[x0, x1) x [y0, y1)` (end coordinates exclusive) processed by one raster worker.
    /// @see kTileSize
    struct Tile {
        int x0, y0, x1, y1;
    };

	/// @brief Which triangle faces are discarded during primitive assembly.
	///
	/// The facing is derived from the sign of the triangle's screen-space area; flat triangles
	/// are always discarded.
	/// @see AssemblyDesc::culling
	enum class CullMode {
		None,
		Back,
		Front
	};

	/// @brief Projection model of a Camera (read by the Rasterizer; see FragmentShader() for a caveat).
	enum class CameraType {
		Perspective,
		Orthographic
	};

	/// @brief Comparison used to decide whether a fragment passes the depth test.
	///
	/// A fragment passes when `fragmentDepth <op> storedDepth`. `Equal` / `NotEqual` use an
	/// absolute tolerance of 1e-6; `None` always passes. Depth is in [0, 1], smaller is closer.
	/// @see Material::depthTest
	enum class DepthTestMode {
		None,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		Equal,
		NotEqual
	};

	/// @brief How a transparent fragment is combined with the pixel already in the frame buffer.
	///
	/// With `d` the destination channel, `s` the source channel (both 0..255) and `a` the source alpha in [0, 1]:
	/// - `None`: `s`
	/// - `Alpha`: `d + (s - d) * a`
	/// - `Additive`: `min(d + s * a, 255)`
	/// - `Multiply`: `d + (d * s / 255 - d) * a`
	///
	/// @note Only primitives flagged transparent (a vertex colour alpha below 255) are blended;
	///       opaque primitives overwrite the pixel.
	enum class BlendMode {
		None,
		Alpha,
		Additive,
		Multiply
	};

	/// @brief Selects which built-in fragment shader GeometryPass runs for a Material.
	/// @see PBRShader, BlinnPhongShader, LambertShader, VertexColorShader, ToonShader, AlbedoColorShader, WaterShader
	enum class ShaderId {
		PBR,
		BlinnPhong,
        Lambert,
		VertexColor, 
        Toon,
        AlbedoColor,
	    Water
	};

    /// @brief 8-bit-per-channel RGBA colour (default opaque black), aligned to 4 bytes.
    ///
    /// Used for per-vertex colours (Mesh::colors) and material base colours. It is *not* the
    /// frame buffer pixel layout (see FrameBuffer).
    struct alignas(4) PUMPKIN_API Color {
        std::uint8_t r{ 0 };
        std::uint8_t g{ 0 };
        std::uint8_t b{ 0 };
        std::uint8_t a{ 255 };
    };

    /// @brief Surface description attached to a DrawItem: colour/PBR factors, texture slots and pipeline state.
    ///
    /// Texture pointers are non-owning: the Texture objects must outlive every frame that uses the material.
    /// @note Per-pixel base colour comes from Mesh::colors (and the albedo texture); `baseColor.a` is only used
    ///       by GeometryPass to order draw items.
    struct PUMPKIN_API Material {
        /// @brief Base colour factor. Its alpha decides whether GeometryPass treats the item as transparent (< 255) when sorting.
        Color baseColor{ 255, 255, 255, 255 };
        /// @brief Metallic factor, multiplied with the ORM map's blue channel (PBR shader).
        float metallic = 1.0f;
        /// @brief Roughness factor, multiplied with the ORM map's green channel (PBR shader).
        float roughness = 1.0f;
        /// @brief In the built-in PBR path GeometryPass passes this value to PBRShader::ambient (ambient light amount).
        float ao = 1.0f;

        /// @brief Optional albedo map; nullptr or an empty texture means "none".
        const Texture* albedo = nullptr;
        /// @brief Optional tangent-space normal map; nullptr or an empty texture means "none".
        const Texture* normal = nullptr;
        /// @brief Optional packed map: R = ambient occlusion, G = roughness, B = metallic.
        const Texture* orm = nullptr; // R = AO, G = roughness, B = metallic

        /// @brief Fragment shader used to shade this material.
        ShaderId      shader = ShaderId::PBR;
        /// @brief Depth comparison applied to this material's fragments.
        DepthTestMode depthTest = DepthTestMode::Less;
        /// @brief Blend mode used when the primitive is transparent.
        BlendMode     blend = BlendMode::Alpha;
        /// @brief Intended depth-write switch.
        /// @warning Not read by the rasterizer at the moment: only opaque primitives write depth and
        ///          transparent ones never do, regardless of this flag.
        bool          depthWrite = true;
        bool          animatedWaterOndulation = false;
        bool          animatedWaterWave = false;
    };

    /// @brief Non-owning view of the colour target.
    ///
    /// Each pixel is a `std::uint32_t` laid out as `0xAARRGGBB` (blue in the lowest byte), row-major,
    /// `width * height` entries, no padding between rows.
    ///
    /// @note Dimensions are 16-bit: at most 65535 x 65535.
    struct PUMPKIN_API FrameBuffer {
        std::uint32_t* pixels{ nullptr };
        std::uint16_t width{}, height{};
    };

    /// @brief Per-pixel depth target, same dimensions as the FrameBuffer.
    ///
    /// Values are in [0, 1] with 1 meaning "nothing drawn" (ClearPass resets to 1) and smaller values closer to the camera.
    struct PUMPKIN_API DepthBuffer {
        std::vector<float> pixels{};
        std::uint16_t width{}, height{};
    };

    /// @brief One mesh vertex: position, texture coordinate and normal (9 contiguous floats).
    ///
    /// The meaning of `position` depends on the pipeline stage:
    /// - model space: `(x, y, z, 1)`;
    /// - clip space (after VertexShader): `(x, y, z, w)`;
    /// - screen space (after ToScreen): `(pixelX, pixelY, depth01, 1 / w)`.
    ///
    /// @note ToScreen's SIMD path reads vertices as 9-float records, so the layout must not change.
    struct Vertex {
        Vec4f position;
        Vec2f texcoord;
        Vec3f normal;
    };

    /// @brief Indexed triangle mesh.
    ///
    /// `indices` is a triangle list (3 per triangle). `colors` holds one colour per vertex and should have
    /// `vertices.size()` entries (ClipNearPlane only keeps `min(vertices, colors)` vertices).
    struct PUMPKIN_API Mesh {
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<Color> colors;
    };

    /// @brief A line of text rasterised with SDL_ttf, ready to be blitted by TextPass.
    ///
    /// The text is rendered once into `surface` by the constructor and again by every Build().
    /// The setters only change the stored parameters; call Build() to re-render.
    ///
    /// @warning `text` is stored as a raw pointer and is not copied: the string must outlive the Text.
    /// @warning Text has no destructor: the owner must release `surface` (SDL_DestroySurface),
    ///          including the previous one before calling Build() again, or it leaks.
    /// @note `color` is stored with red and blue swapped (the frame buffer is blitted as ABGR by TextPass).
    struct PUMPKIN_API Text {
        /// @brief Creates the text and renders it immediately.
        /// @param font  Loaded SDL_ttf font (not owned). Its size is set to @p size.
        /// @param text  Null-terminated string (not copied, see class warning).
        /// @param size  Font size in points.
        /// @param color Text colour (RGBA).
        /// @param x     Left position in frame-buffer pixels.
        /// @param y     Top position in frame-buffer pixels.
        /// @post `surface` is the blended rendering of @p text, or nullptr if SDL_ttf failed.
        Text(TTF_Font* font, const char* text, float size, SDL_Color color, int x, int y) {
            this->text = text;
            this->font = font;
            this->x = x, this->y = y;
            this->color = { color.b, color.g, color.r, color.a };
            this->fontSize = size;
            TTF_SetFontSize(this->font, this->fontSize);
            this->surface = TTF_RenderText_Blended(
                font,
                text,
                0,
                this->color
            );
        }

        /// @brief Sets the font size used by the next Build().
        /// @param size Size in points; ignored if <= 0.
        /// @return *this.
        Text& SetSize(float size) {
            if (size <= 0.0f)
                return *this;
            fontSize = size;
            return *this;
        }

        /// @brief Sets the font used by the next Build().
        /// @param newFont Loaded font (not owned).
        /// @return *this.
        Text& SetFont(TTF_Font* newFont) {
            font = newFont;
            return *this;
        }

        /// @brief Sets the string used by the next Build().
        /// @param txt Null-terminated string (not copied; must outlive the Text).
        /// @return *this.
        Text& SetText(const char* txt) {
            text = txt;
            return *this;
        }

        /// @brief Sets the colour used by the next Build() (stored with red/blue swapped).
        /// @param rgba Text colour.
        /// @return *this.
        Text& SetColor(SDL_Color rgba) {
            color = { rgba.b, rgba.g, rgba.r, rgba.a };
            return *this;
        }

        /// @brief Moves the text to an absolute position (no re-render needed).
        /// @param x Left position in pixels.
        /// @param y Top position in pixels.
        /// @return *this.
        Text& SetPosition(int x, int y) {
            this->x = x,this->y = y;
            return *this;
        }

        /// @brief Re-renders `surface` from the current font, size, string and colour.
        /// @return *this.
        /// @warning The previous surface is not freed.
        Text& Build() {
            TTF_SetFontSize(font, fontSize);
            surface = TTF_RenderText_Blended(font, text, 0, color);
            return *this;
        }

        /// @brief Moves the text relative to its current position.
        /// @param x_dist Horizontal offset in pixels (positive = right).
        /// @param y_dist Vertical offset in pixels (positive = down).
        void Move(int x_dist, int y_dist) {
            this->x += x_dist, this->y += y_dist;
        }

        /// @brief Font size in points.
        float fontSize;
        /// @brief Top-left position in frame-buffer pixels.
        int x, y;
        /// @brief Colour passed to SDL_ttf (red and blue swapped, see class note).
        SDL_Color color{};
        /// @brief Non-owning pointer to the displayed string.
        const char* text;
        /// @brief Rendered text bitmap (owned by the caller, may be nullptr).
        SDL_Surface* surface;
        /// @brief Font used for rendering (not owned).
        TTF_Font* font;
    };

    /// @brief OBJ face-corner key (position, texcoord, normal indices) used to merge duplicate vertices in LoadMesh().
    struct IndexKey {
        std::uint32_t v, t, n;
        bool operator==(const IndexKey&) const = default;
    };
}

template<>
/// @brief Hash for IndexKey (boost-style hash combine of the three indices).
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
