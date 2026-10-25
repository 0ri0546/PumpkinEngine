export module Pumpkin.Renderer.Particles;

import Pumpkin.Math.Vec;

export namespace Pumpkin {

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

        Particles& SetPosition(float x, float y, float z) {
            pos = { x, y ,z };
            return *this;
        }

        Particles& SetDirection(float dirx, float diry, float dirz) {
            dir = { dirx, diry, dirz };
            return *this;
        }

        Particles& SetGravity(float x, float y, float z) {
            gravity = { x, y, z };
            return *this;
        }

        Particles& SetMaxParticles(float nb) {
            maxParticles = nb;
            return *this;
        }

        Particles& SetMinDuration(float nb) {
            minDuration = nb;
            return *this;
        }

        Particles& SetMaxDuration(float nb) {
            maxDuration = nb;
            return *this;
        }

        Particles& SetSpawnRadius(float nb) {
            spawnRadius = nb;
            return *this;
        }

        Particles& SetInitialVelocity(float x, float y, float z) {
            initialVelocity = { x, y, z };
            return *this;
        }

        Particles& SetSpeedMin(float nb) {
            speedMin = nb;
            return *this;
        }

        Particles& SetSpeedMax(float nb) {
            speedMax = nb;
            return *this;
        }

        Particles& SetColorMin(Vec4<int> color) {
            colorMin = color;
            return *this;
        }

        Particles& SetColorMax(Vec4<int> color) {
            colorMax = color;
            return *this;
        }

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