#pragma once

#include "Math/Vec.h"

namespace Pumpkin {

    struct Particles {
        Vec3f pos{ 0.0f, 0.0f, 0.0f };
        Vec3f dir{ 0.0f, 1.0f, 0.0f };
        Vec3f gravity{ 0.0f, -9.81f, 0.0f };

        int maxParticles = 50;

        float minDuration = 0.5f;
        float maxDuration = 3.0f;
        float spawnRadius = 0.05f;
        Vec3f initialVelocity{ 0.0f, 10.0f, 0.0f };

        float speedMin = 0.7f;
        float speedMax = 1.3f;

        Vec4<int> colorMin = { 255, 255, 255, 255 };
        Vec4<int> colorMax = { 0, 255, 127, 255 };
    };

    struct Particle {
        Vec3f pos{ 0.0f, 0.0f, 0.0f };
        Vec3f velocity{ 0.0f, 0.0f, 0.0f };
        Vec3f gravity{ 0.0f, 0.0f, 0.0f };

        Vec4<int> color{ 255, 255, 255, 255 };

        float lifetime = 1.0f;
        float age = 0.0f;

        bool IsAlive() const {
            return age < lifetime;
        }

        
    };

}