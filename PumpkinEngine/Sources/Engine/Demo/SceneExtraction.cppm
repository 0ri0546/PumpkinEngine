/// @file SceneExtraction.cppm
/// @brief Module Pumpkin.Demo.SceneExtraction: ECS -> renderer bridge.

export module Pumpkin.Demo.SceneExtraction;

import Pumpkin.ECS;
import Pumpkin.Renderer;
import std;

export namespace Pumpkin::Demo {
    /// @brief Rebuilds the draw list from every entity that has a mesh, a model matrix and a material.
    ///
    /// DrawItem holds pointers into component pools, which stay valid as long as no structural change happens
    /// before rendering: scheduler.Run() (which applies the command buffers) must therefore be called *before* this.
    ///
    /// @param world World to read.
    /// @param scene Cleared, then refilled. The vector may reallocate: refresh any span over it afterwards.
    void ExtractScene(ECS::World& world, std::vector<DrawItem>& scene);
}
