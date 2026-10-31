/// @file MeshLoader.cppm
/// @brief Module Pumpkin.Renderer.MeshLoader: OBJ loading, normal generation and a debug triangle.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.MeshLoader;

import std;

import Pumpkin.Renderer.Types;

export namespace Pumpkin {
    /// @brief Loads a Wavefront `.obj` file into a Mesh.
    ///
    /// The mesh is centred on the origin and uniformly scaled so its largest side measures 2 units; every vertex gets
    /// @p color; vertex `w` is 1. Normals from the file are discarded: smooth, area-weighted normals are regenerated from
    /// the triangles (see GenerateNormals()). Face corners referencing a missing position are skipped.
    ///
    /// @param filepath    Path of the model; any extension other than `.obj` returns an empty mesh.
    /// @param color       Colour assigned to every vertex.
    /// @param deduplicate When true, corners sharing the same position / uv / normal indices share one vertex.
    /// @return The loaded mesh (empty for a non-`.obj` path).
    /// @throws std::runtime_error if the file cannot be opened or read (from the OBJ parser).
    /// @note Prints OBJ normal statistics to standard output.
    /// @thread_safety Thread-safe for distinct files / outputs.
    [[nodiscard]] Mesh PUMPKIN_API LoadMesh(
        const std::filesystem::path& filepath,
        Color color = Color{ .r = 255, .g = 255, .b = 255, .a = 255 },
        bool deduplicate = true
    );

    /// @brief Replaces every vertex normal with an area-weighted smooth normal.
    ///
    /// Each triangle adds its unnormalised face normal (twice its area, so larger faces weigh more) to its three vertices,
    /// then the sums are normalised.
    ///
    /// @param mesh Mesh to modify; `indices` must be a triangle list with every index below `vertices.size()`.
    /// @note Vertices not used by any triangle end up with the result of normalising a zero vector.
    void PUMPKIN_API GenerateNormals(Mesh& mesh);

    /// @brief Builds a single red/green/blue triangle, handy to debug the pipeline.
    /// @return Mesh with 3 vertices at z = 0.5 (x, y in [-1, 1]), per-vertex colours red, green, blue and indices {0, 1, 2}.
    [[nodiscard]] Mesh PUMPKIN_API MakeTestTriangle();
}
