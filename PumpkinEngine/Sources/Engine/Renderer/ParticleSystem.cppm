export module Pumpkin.Renderer.ParticleSystem;

import std;

import Pumpkin.Math.Vec;

import Pumpkin.Renderer.Particles;

export namespace Pumpkin {

    class ParticleSystem {
    public:
        explicit ParticleSystem(Particles settings)
            : m_settings(settings) {
            m_particles.reserve(
                static_cast<std::size_t>(std::max(0, settings.maxParticles))
            );
        }

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

        [[nodiscard]]
        const std::vector<Particle>& GetParticles() const {
            return m_particles;
        }

    private:
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