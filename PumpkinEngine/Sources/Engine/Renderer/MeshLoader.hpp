#pragma once

#include <filesystem>

#include <Renderer/Types.hpp>

namespace Pumpkin {
    // Loads a .obj file (any other extension returns an empty mesh).
    // The mesh is centered on the origin and scaled to fit a 2-unit box, every
    // vertex gets `color`, and smooth normals are regenerated from the triangles.
    // With deduplicate, vertices sharing position / uv / normal are merged.
    [[nodiscard]] Mesh LoadMesh(
        const std::filesystem::path& filepath,
        Color color = Color{ .r = 255, .g = 255, .b = 255, .a = 255 },
        bool deduplicate = true
    );

    // Replaces the vertex normals with area-weighted smooth normals.
    void GenerateNormals(Mesh& mesh);

    // One RGB triangle in clip-friendly coordinates, handy to debug the pipeline.
    [[nodiscard]] Mesh MakeTestTriangle();
}
