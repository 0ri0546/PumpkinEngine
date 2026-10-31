/// @file Particles.cppm
/// @brief Module Pumpkin.Renderer.Particles: emitter settings and the particle record.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Particles;

import Pumpkin.Math.Vec;

export namespace Pumpkin {

    /// @brief Settings of a continuous particle emitter (see ParticleSystem).
    ///
    /// Setters return `*this` so a configuration can be chained: `Particles{}.SetPosition(0, 1, 0).SetMaxParticles(200)`.
    struct PUMPKIN_API Particles {
        /// @brief Emitter position in world space.
        Vec3f pos{ 0.0f, 0.0f, 0.0f };
        /// @brief Emission direction (normalised when spawning; a zero vector adds no directional speed).
        Vec3f dir{ 0.0f, 1.0f, 0.0f };
        /// @brief Constant acceleration applied to every particle.
        Vec3f gravity{ 0.0f, -9.81f, 0.0f };

        /// @brief Particle count the system keeps alive (it respawns up to this number).
        int maxParticles = 50;

        /// @brief Shortest particle lifetime, in seconds.
        float minDuration = 0.5f;
        /// @brief Longest particle lifetime, in seconds.
        float maxDuration = 3.0f;
        /// @brief Half-size of the cube around `pos` in which particles spawn.
        float spawnRadius = 0.05f;
        /// @brief Base velocity of a new particle.
        Vec3f initialVelocity{ 0.0f, 10.0f, 0.0f };

        /// @brief Lower bound of the random speed along `dir`, added to `initialVelocity`.
        float speedMin = 0.7f;
        /// @brief Upper bound of the random speed along `dir`, added to `initialVelocity`.
        float speedMax = 1.3f;

        /// @brief RGBA colour (0..255) at birth.
        Vec4<int> colorMin = { 255, 255, 255, 255 };
        /// @brief RGBA colour (0..255) at the end of life; colours are interpolated linearly over the lifetime.
        Vec4<int> colorMax = { 0, 255, 127, 255 };

        /// @brief Sets `pos`. @return *this.
        Particles& SetPosition(float x, float y, float z) {
            pos = { x, y ,z };
            return *this;
        }

        /// @brief Sets `dir`. @return *this.
        Particles& SetDirection(float dirx, float diry, float dirz) {
            dir = { dirx, diry, dirz };
            return *this;
        }

        /// @brief Sets `gravity`. @return *this.
        Particles& SetGravity(float x, float y, float z) {
            gravity = { x, y, z };
            return *this;
        }

        /// @brief Sets `maxParticles`. @param nb Count (taken as a float and truncated to int). @return *this.
        Particles& SetMaxParticles(float nb) {
            maxParticles = nb;
            return *this;
        }

        /// @brief Sets `minDuration` (seconds). @return *this.
        Particles& SetMinDuration(float nb) {
            minDuration = nb;
            return *this;
        }

        /// @brief Sets `maxDuration` (seconds). @return *this.
        Particles& SetMaxDuration(float nb) {
            maxDuration = nb;
            return *this;
        }

        /// @brief Sets `spawnRadius`. @return *this.
        Particles& SetSpawnRadius(float nb) {
            spawnRadius = nb;
            return *this;
        }

        /// @brief Sets `initialVelocity`. @return *this.
        Particles& SetInitialVelocity(float x, float y, float z) {
            initialVelocity = { x, y, z };
            return *this;
        }

        /// @brief Sets `speedMin`. @return *this.
        Particles& SetSpeedMin(float nb) {
            speedMin = nb;
            return *this;
        }

        /// @brief Sets `speedMax`. @return *this.
        Particles& SetSpeedMax(float nb) {
            speedMax = nb;
            return *this;
        }

        /// @brief Sets `colorMin` (RGBA, 0..255). @return *this.
        Particles& SetColorMin(Vec4<int> color) {
            colorMin = color;
            return *this;
        }

        /// @brief Sets `colorMax` (RGBA, 0..255). @return *this.
        Particles& SetColorMax(Vec4<int> color) {
            colorMax = color;
            return *this;
        }

    };

    /// @brief State of one live particle (position, velocity, colour, age).
    struct Particle {
        /// @brief Current world-space position.
        Vec3f pos{ 0.0f, 0.0f, 0.0f };
        /// @brief Current velocity, in world units per second.
        Vec3f velocity{ 0.0f, 0.0f, 0.0f };
        /// @brief Acceleration applied to this particle (copied from the emitter at spawn).
        Vec3f gravity{ 0.0f, 0.0f, 0.0f };

        /// @brief Current RGBA colour (0..255), updated from the age.
        Vec4<int> color{ 255, 255, 255, 255 };

        /// @brief Total life span in seconds.
        float lifetime = 1.0f;
        /// @brief Time lived so far, in seconds.
        float age = 0.0f;

        /// @brief Tests whether the particle is still alive. @return `age < lifetime`.
        bool IsAlive() const {
            return age < lifetime;
        }

        
    };

}