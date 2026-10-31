/// @file ParticleSystem.cppm
/// @brief Module Pumpkin.Renderer.ParticleSystem: CPU simulation of a continuously refilled particle emitter.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.ParticleSystem;

import std;

import Pumpkin.Math.Vec;

import Pumpkin.Renderer.Particles;

export namespace Pumpkin {

    /// @brief Simulates the particles of one emitter (integration, colour ramp, expiry, respawn).
    ///
    /// The system always tries to hold `Particles::maxParticles` live particles: expired ones are removed and new ones spawned
    /// at the emitter on the same update. Rendering is done by ParticlePass.
    ///
    /// @thread_safety Not thread-safe; owns its random generator.
    class PUMPKIN_API ParticleSystem {
    public:
        /// @brief Creates the system and reserves room for `maxParticles` particles.
        /// @param settings Emitter settings (copied; they cannot be changed afterwards). No particle exists until the first Update().
        explicit ParticleSystem(Particles settings)
            : m_settings(settings) {
            m_particles.reserve(
                static_cast<std::size_t>(std::max(0, settings.maxParticles))
            );
        }

        /// @brief Advances the simulation.
        ///
        /// Ages the live particles, interpolates their colour from `colorMin` to `colorMax` over their lifetime, applies gravity to the
        /// velocity and the velocity to the position (semi-implicit Euler), removes the expired ones, then spawns new particles until
        /// `maxParticles` is reached.
        ///
        /// @param deltaTime Elapsed time in seconds. Values <= 0 are ignored (nothing is updated or spawned).
        void Update(float deltaTime) {

            if (deltaTime <= 0.0f)
                return;

            for (auto& particle : m_particles) {
                if (!particle.IsAlive())
                    continue;

                particle.age += deltaTime;

                const float t = std::clamp(
                    particle.age / particle.lifetime,
                    0.0f,
                    1.0f
                );

                for (int i = 0; i < 4; ++i) {
                    particle.color[i] = std::clamp(
                        static_cast<int>(std::lerp(
                            static_cast<float>(m_settings.colorMin[i]),
                            static_cast<float>(m_settings.colorMax[i]),
                            t
                        )),
                        0,
                        255
                    );
                }

                particle.velocity += particle.gravity * deltaTime;
                particle.pos += particle.velocity * deltaTime;
            }

            std::erase_if(m_particles, [](const Particle& particle) {
                return !particle.IsAlive();
                });

            while (m_particles.size() <
                static_cast<std::size_t>(std::max(0, m_settings.maxParticles))) {
                Spawn();
            }
        }

        /// @brief Live particles.
        /// @return Reference valid until the next Update().
        [[nodiscard]]
        const std::vector<Particle>& GetParticles() const {
            return m_particles;
        }

    private:
        /// @brief Creates one particle: random position in the spawn cube, velocity `initialVelocity + normalise(dir) * random(speedMin, speedMax)`,
        /// random lifetime in [minDuration, maxDuration], colour `colorMin`.
        void Spawn() {
            Particle particle{};

            const auto randomFloat = [this](float min, float max) {
                std::uniform_real_distribution<float> dist{ min, max };
                return dist(m_rng);
            };

            const float radius = m_settings.spawnRadius;
            const float x = randomFloat(-radius, radius);
            const float y = randomFloat(-radius, radius);
            const float z = randomFloat(-radius, radius);

            particle.pos = m_settings.pos + Vec3f{ x, y, z };

            Vec3f direction = m_settings.dir;

            if (direction.Length() > 0.0f)
                direction = direction.Normalized();

            particle.velocity =
                m_settings.initialVelocity +
                direction * randomFloat(
                    m_settings.speedMin,
                    m_settings.speedMax
                );

            particle.gravity = m_settings.gravity;

            particle.lifetime = randomFloat(
                m_settings.minDuration,
                m_settings.maxDuration
            );

            const float t = std::clamp(particle.age / particle.lifetime, 0.0f, 1.0f);

            particle.color = m_settings.colorMin;
            m_particles.push_back(particle);
        }

        Particles m_settings;
        std::vector<Particle> m_particles;
        std::mt19937 m_rng{ std::random_device{}() };
    };

}