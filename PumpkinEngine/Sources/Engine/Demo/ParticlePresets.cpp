module Pumpkin.Demo.ParticlePresets;

import Pumpkin.Renderer;

namespace Pumpkin::Demo {
    Particles MakeSnowParticles() {
        Particles particles{};
        particles.
            SetPosition(0.0f, 3.0f, 0.0f).
            SetDirection(0.0f, -1.0f, 0.0f).
            SetMaxParticles(2000).
            SetMinDuration(0.5f).
            SetMaxDuration(3.0f).
            SetSpeedMin(0.1f).
            SetSpeedMax(0.4f).
            SetGravity(0.0f, -0.62f, 0.0f).
            SetInitialVelocity(0.0f, 0.0f, 0.0f).
            SetColorMin({ 255, 210, 210, 255 }).
            SetColorMax({ 255, 255, 255, 255 }).
            SetSpawnRadius(20);
        return particles;
    }

    Particles MakeFountainParticles() {
        Particles particles{};
        particles.
            SetPosition(3.0f, 7.0f, 0.0f).
            SetDirection(-1.0f, 1.0f, 0.3f).
            SetMaxParticles(500).
            SetMinDuration(0.5f).
            SetMaxDuration(3.0f).
            SetSpeedMin(0.7f).
            SetSpeedMax(1.3f).
            SetGravity(0.0f, -9.81f, 0.0f).
            SetInitialVelocity(0.0f, 2.0f, 0.0f).
            SetColorMin({ 255, 0, 255, 255 }).
            SetColorMax({ 255, 255, 255, 255 });
        return particles;
    }
}
