module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Texture;

import std;

import Pumpkin.Renderer.Types;

export namespace Pumpkin {

    bool PUMPKIN_API LoadTexture(
        Texture& texture,
        const std::filesystem::path& filepath
    );

    Color SampleTexture(
        const Texture& texture,
        float u,
        float v
    );

}
