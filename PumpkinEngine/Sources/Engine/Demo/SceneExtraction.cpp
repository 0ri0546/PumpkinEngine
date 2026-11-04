module Pumpkin.Demo.SceneExtraction;

import Pumpkin.Demo.Components;
import Pumpkin.ECS;
import Pumpkin.Renderer;
import std;

using namespace Pumpkin::ECS;
using namespace Pumpkin::Demo::Comp;

namespace Pumpkin::Demo {
    /// Rebuilds the list of draw items from every entity that has a mesh, a model matrix and a
    /// material. DrawItem holds pointers into component pools, which stay valid as long as no
    /// structural change happens before rendering: scheduler.Run() (which applies the command
    /// buffers) is therefore always called *before* this function.
    void ExtractScene(World& world, std::vector<DrawItem>& scene) {
        scene.clear();

        for ([[maybe_unused]] auto [entity, mesh, model, surface]
             : world.View<MeshRef, ModelMatrix, SurfaceMaterial>()) {
            if (mesh.mesh == nullptr) {
                continue;
            }

            scene.push_back(DrawItem{
                .mesh = mesh.mesh,
                .model = &model.value,
                .material = &surface.value
            });
        }
    }
}
