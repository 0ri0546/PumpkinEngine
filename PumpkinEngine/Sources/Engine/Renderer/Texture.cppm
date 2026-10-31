/// @file Texture.cppm
/// @brief Module Pumpkin.Renderer.Texture: image loading and nearest-neighbour sampling.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Texture;

import std;

import Pumpkin.Renderer.Types;

export namespace Pumpkin {

    /// @brief Loads an image file into a Texture (decoded by stb_image, forced to 4 channels).
    /// @param texture  Receives the pixels as `0xRRGGBBAA`. Reset to an empty texture when loading fails.
    /// @param filepath Path of an image supported by stb_image (PNG, JPEG, BMP, TGA...).
    /// @return true on success, false if the file could not be read or decoded.
    /// @note Prints "Loaded texture: <name> in <ms> ms" to standard output on success.
    /// @thread_safety Thread-safe for distinct @p texture objects.
    bool PUMPKIN_API LoadTexture(
        Texture& texture,
        const std::filesystem::path& filepath
    );

    /// @brief Nearest-neighbour texture lookup with repeat wrapping.
    /// @param texture Texture to sample.
    /// @param u Horizontal coordinate; only the fractional part is used (wraps).
    /// @param v Vertical coordinate, 0 at the bottom row and 1 at the top row (the image is flipped); wraps.
    /// @return The texel colour, or opaque white if the texture is empty.
    /// @thread_safety Thread-safe (read-only).
    Color SampleTexture(
        const Texture& texture,
        float u,
        float v
    );

}
