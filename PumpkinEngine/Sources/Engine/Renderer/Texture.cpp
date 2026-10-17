
#include "Texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace Pumpkin {

    bool LoadTexture(
        Texture& texture,
        const std::filesystem::path& filepath
    ) {
        auto start = std::chrono::steady_clock::now();
        int width = 0;
        int height = 0;
        int channels = 0;

        const std::string path = filepath.string();

        unsigned char* image = stbi_load(
            path.c_str(),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha
        );

        if (!image) {
            texture.width = 0;
            texture.height = 0;
            texture.pixels.clear();
            return false;
        }

        texture.width = width;
        texture.height = height;

        texture.pixels.resize(
            static_cast<size_t>(width) * height
        );

        for (int i = 0; i < width * height; ++i) {
            const unsigned char r = image[i * 4 + 0];
            const unsigned char g = image[i * 4 + 1];
            const unsigned char b = image[i * 4 + 2];
            const unsigned char a = image[i * 4 + 3];

            texture.pixels[i] =
                (static_cast<uint32_t>(a) << 24) |
                (static_cast<uint32_t>(r) << 16) |
                (static_cast<uint32_t>(g) << 8) |
                static_cast<uint32_t>(b);
        }

        stbi_image_free(image);

        auto end = std::chrono::steady_clock::now();
        std::cout << "Loaded texture: " << filepath.filename().string() << " in " << std::chrono::duration<float, std::milli>(end - start).count() << " ms" << std::endl;

        return true;
    }

    uint32_t SampleTexture(
        const Texture& texture,
        float u,
        float v
    ) {
        if (texture.width <= 0 ||
            texture.height <= 0 ||
            texture.pixels.empty()) {
            return 0xFFFFFFFF;
        }

        // R�p�tition de la texture
        u -= std::floor(u);
        v -= std::floor(v);

        // Conversion UV vers coordonn�es pixel
        const int x = std::clamp(
            static_cast<int>(u * texture.width),
            0,
            texture.width - 1
        );

        const int y = std::clamp(
            static_cast<int>((1.0f - v) * texture.height),
            0,
            texture.height - 1
        );

        const size_t index =
            static_cast<size_t>(y) * texture.width + x;

        return texture.pixels[index];
    }

}
