/// @file SkyboxPass.cppm
/// @brief Module Pumpkin.Renderer.Passes.Skybox: equirectangular sky background.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Skybox;

import std;

import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Texture;
import Pumpkin.Renderer.Types;

export namespace Pumpkin {
    /// @brief Fills the whole frame buffer with a panorama (equirectangular texture) seen through the camera.
    ///
    /// For every pixel the view ray is converted to longitude / latitude to look up the texture (nearest). Exact lookups are done every
    /// 8 pixels and linearly interpolated between them (exact near poles / seams). Writes colour only, no depth: run it after
    /// ClearPass and before GeometryPass.
    ///
    /// @note Holds the texture by reference: it must outlive the pass. The ray tables and a re-packed copy of the texture are cached
    ///       (keyed on size / FOV and on the pixel pointer and size), so editing texels in place is not picked up.
    class PUMPKIN_API SkyboxPass final : public RenderPass {
    public:
        /// @brief Creates the pass. @param texture Panorama image (non-owning reference).
        explicit SkyboxPass(const Texture& texture)
            : m_texture(texture) {
        }

        /// @brief Profiler name. @return "Skybox".
        [[nodiscard]] std::string_view Name() const override {
            return "Skybox";
        }

        /// @brief Renders the sky into every pixel of the frame buffer.
        /// @param ctx Frame data (uses the camera basis and FOVY). Does nothing for an empty target or an empty texture.
        /// @thread_safety Parallel over row chunks via the engine thread pool.
        void Execute(RenderContext& ctx) override;

    private:
        const Texture& m_texture;

        /// @brief Per-column ray offsets (`width + 1` entries), cached for the current size and FOV.
        std::vector<float> m_rayX;
        /// @brief Per-row ray offsets, cached for the current size and FOV.
        std::vector<float> m_rayY;

        /// @brief Target size / FOV the ray tables were built for.
        int m_cachedWidth = 0;
        int m_cachedHeight = 0;
        float m_cachedFov = -1.0f;

        /// @brief Texture re-packed into frame-buffer pixel format, rebuilt when the source pointer or size changes.
        std::vector<std::uint32_t> m_packed;
        const void* m_packedSrc = nullptr;
        int m_packedW = 0, m_packedH = 0;
    };
}
