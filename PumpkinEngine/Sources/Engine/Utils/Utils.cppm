export module Pumpkin.Utils.Utils;

import Pumpkin.Renderer.Types;

import std;

export namespace Pumpkin {
    std::uint32_t ToRGBA(float r, float g, float b, float a = 1.0f);

    // Color helpers
    std::vector<Color> GenerateRGBA(std::size_t count, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
    std::vector<Color> GenerateRandomRGB(std::size_t count);
    std::vector<Color> GenerateRandomRGBA(std::size_t count);
}
