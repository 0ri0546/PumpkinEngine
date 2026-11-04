/// @file Config.cppm
/// @brief Module Pumpkin.Demo.Config: tuning constants shared by the demo modules.

export module Pumpkin.Demo.Config;

import Pumpkin.Renderer;
import std;

export namespace Pumpkin::Demo {
    inline constexpr std::uint64_t kDefaultTargetFps = 120;
    inline constexpr std::uint64_t kUncappedFps      = 9'999'999;

    // ECS tuning.
    inline constexpr std::size_t kReservedEntities   = 256;   // World::Reserve, avoids pool growth mid-game
    inline constexpr std::size_t kMaxSpawnedProps    = 48;    // upper bound on live spawned props
    inline constexpr float       kMaxEcsStep         = 0.1f;  // clamps dt after a stall so animations don't jump
    inline constexpr float       kFadeSeconds        = 1.5f;  // length of the "sink into the ground" phase
    inline constexpr float       kSinkSpeed          = 2.0f;  // world units per second while fading
    inline constexpr std::uint32_t kBobToggleSeconds = 10;    // direct (non-deferred) World edit demo period

    // Shaders cycled by the ShaderCycle component / used for spawned props.
    inline constexpr std::array kShowcaseShaders{
        ShaderId::Lambert,
        ShaderId::BlinnPhong,
        ShaderId::Toon,        // also exercises OutlinePass through the toon mask
        ShaderId::PBR
    };
}
