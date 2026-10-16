
#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Pumpkin {

    struct Texture {
        int width = 0;
        int height = 0;
        std::vector<uint32_t> pixels;
    };

    bool LoadTexture(
        Texture& texture,
        const std::filesystem::path& filepath
    );

    uint32_t SampleTexture(
        const Texture& texture,
        float u,
        float v
    );

}
