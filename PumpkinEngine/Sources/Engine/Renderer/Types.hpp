#pragma once

#include <cstdint>
#include <vector>

#include <Math/Vec.h>

namespace Pumpkin {
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
