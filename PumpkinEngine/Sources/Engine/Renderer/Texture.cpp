module;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

module Pumpkin.Renderer.Texture;

import std;

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
            static_cast<std::size_t>(width) * height
        );

        for (int i = 0; i < width * height; ++i) {
            const unsigned char r = image[i * 4 + 0];
            const unsigned char g = image[i * 4 + 1];
            const unsigned char b = image[i * 4 + 2];
            const unsigned char a = image[i * 4 + 3];

            texture.pixels[i] =
                (static_cast<std::uint32_t>(r) << 24) |
                (static_cast<std::uint32_t>(g) << 16) |
                (static_cast<std::uint32_t>(b) << 8) |
                static_cast<std::uint32_t>(a);
        }

        stbi_image_free(image);

        auto end = std::chrono::steady_clock::now();
        std::cout << "Loaded texture: " << filepath.filename().string() << " in " << std::chrono::duration<float, std::milli>(end - start).count() << " ms" << std::endl;

        return true;
    }

    // Nearest, raw packed pixel
    static std::uint32_t SampleTexturePacked(const Texture& t, float u, float v) {
        u -= std::floor(u);
        v -= std::floor(v);
        const int x = std::min(static_cast<int>(u * t.width), t.width - 1);
        const int y = std::min(static_cast<int>((1.0f - v) * t.height), t.height - 1);
        return t.pixels[static_cast<std::size_t>(y) * t.width + x];
    }

    static Color UnpackColor(std::uint32_t p) {
        return Color{ .r = static_cast<std::uint8_t>(p >> 24),
                      .g = static_cast<std::uint8_t>(p >> 16),
                      .b = static_cast<std::uint8_t>(p >> 8),
                      .a = static_cast<std::uint8_t>(p) };
    }

    // Drop-in replacements: same signature and behaviour as before.
    Color SampleTexture(const Texture& t, float u, float v) {
        if (t.width <= 0 || t.height <= 0 || t.pixels.empty())
            return Color{ 255, 255, 255, 255 };
        return UnpackColor(SampleTexturePacked(t, u, v));
    }

}
