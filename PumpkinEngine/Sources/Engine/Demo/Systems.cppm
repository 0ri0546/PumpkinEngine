/// @file Systems.cppm
/// @brief Module Pumpkin.Demo.Systems: the demo's ECS systems and their registration.
///
/// Rules the scheduler relies on (see Access):
///  - declare Write<> for every component you *modify*, even through ParallelForEach/View
///    (they only check read permission);
///  - never add/remove components or entities directly: use ctx.Commands(), applied after
///    all systems of the frame finished.
///
/// System graph (derived from the declared Access of each system):
/// @code
///   Spawner ---> Lifetime              (Spawner reads what Lifetime writes)
///   Spin ----------------+
///   Bob ---> Fade -------+--> BuildModel
///   ShaderCycle                        (touches nothing the others touch)
/// @endcode

export module Pumpkin.Demo.Systems;

import Pumpkin.ECS;

export namespace Pumpkin::Demo {
    /// @brief Registers every demo system. Registration order is the *semantic* order between systems that conflict.
    /// @param scheduler Scheduler receiving the systems.
    void RegisterSystems(ECS::Scheduler& scheduler);
}
