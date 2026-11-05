/// @file ParticlePresets.cppm
/// @brief Module Pumpkin.Demo.ParticlePresets: emitter settings used by the demo (not ECS-driven).

export module Pumpkin.Demo.ParticlePresets;

import Pumpkin.Renderer;

export namespace Pumpkin::Demo {
    /// @brief Snow falling over the whole terrain.
    [[nodiscard]] Particles MakeSnowParticles();

    /// @brief Fountain spraying to the left of the origin.
    [[nodiscard]] Particles MakeFountainParticles();
}
