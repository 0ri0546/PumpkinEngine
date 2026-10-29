/// @file ECS.cppm
/// @brief Umbrella module Pumpkin.ECS: re-exports every ECS sub-module.
///
/// `import Pumpkin.ECS;` gives access to CommandBuffer, Component, Entity, Registry,
/// Scheduler, ThreadPool and World. (SparseSet and ComponentPool are reachable through
/// Registry's interface but are not re-exported here.)

export module Pumpkin.ECS;

export import Pumpkin.ECS.CommandBuffer;
export import Pumpkin.ECS.Component;
export import Pumpkin.ECS.Entity;
export import Pumpkin.ECS.Registry;
export import Pumpkin.ECS.Scheduler;
export import Pumpkin.ECS.ThreadPool;
export import Pumpkin.ECS.World;
