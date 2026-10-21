#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include <Renderer/Types.hpp>

namespace Pumpkin {

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
        std::vector<uint32_t> pixels;
    };

    bool LoadTexture(
        Texture& texture,
        const std::filesystem::path& filepath
    );

    Color SampleTexture(
        const Texture& texture,
        float u,
        float v
    );

}
