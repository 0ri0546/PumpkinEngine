module;

#include <DLL/Export.hpp>

export module Pumpkin.Utils;

export import Pumpkin.Utils.Events;
export import Pumpkin.Utils.ObjLoader;
export import Pumpkin.Utils.RenderProfiler;
export import Pumpkin.Utils.ThreadPool;
export import Pumpkin.Utils.Window;

import Pumpkin.Renderer.Types;

import std;

export namespace Pumpkin {
    std::uint32_t PUMPKIN_API ToRGBA(float r, float g, float b, float a = 1.0f);

    // Color helpers
    std::vector<Color> GenerateRGBA(std::size_t count, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
    std::vector<Color> GenerateRandomRGB(std::size_t count);
    std::vector<Color> GenerateRandomRGBA(std::size_t count);

    // Time helpers
    float PUMPKIN_API GetEngineTime();
}
