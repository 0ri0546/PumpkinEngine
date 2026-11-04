/// @file Diagnostics.cppm
/// @brief Module Pumpkin.Demo.Diagnostics: world snapshots and direct (non-deferred) World edits.

export module Pumpkin.Demo.Diagnostics;

import Pumpkin.Demo.WorldSetup;
import Pumpkin.ECS;
import std;

export namespace Pumpkin::Demo {
    /// @brief Prints a snapshot of the world using the query API (Query, QueryAny, TryPool, TryGet, Has).
    void ReportWorld(ECS::World& world, const SceneHandles& handles);

    /// @brief Adds or removes the cow's Bob component with direct World calls.
    ///
    /// Fine on the main thread *between* frames (the scheduler is idle). Inside a system the same
    /// change would go through ctx.Commands().
    void ToggleCowBob(ECS::World& world, ECS::Entity cow);

    /// @brief Name of the cow's current shader, or "-" if it has no material.
    [[nodiscard]] std::string CowShaderName(ECS::World& world, ECS::Entity cow);
}
