module Pumpkin.Renderer.MeshLoader;

import Pumpkin.Renderer.Types;
import Pumpkin.Utils.ObjLoader;
import Pumpkin.Utils.Utils;

import Pumpkin.Math.Vec;

import std;

namespace Pumpkin {
    namespace {
        constexpr float         kDefaultVertexW = 1.0f;
        constexpr std::uint32_t kInvalidIndex   = std::numeric_limits<std::uint32_t>::max();

        Vec3f Position3(const Vertex& vertex) {
            return Vec3f{ vertex.position[0], vertex.position[1], vertex.position[2] };
        }

        // Moves the mesh to the origin and scales its largest side to targetSize.
        void CenterAndScale(Mesh& mesh, float targetSize) {
            if (mesh.vertices.empty()) return;

            Vec3f minBounds = Position3(mesh.vertices[0]);
            Vec3f maxBounds = minBounds;

            for (const auto& vertex : mesh.vertices) {
                for (int axis = 0; axis < 3; ++axis) {
                    minBounds[axis] = std::min(minBounds[axis], vertex.position[axis]);
                    maxBounds[axis] = std::max(maxBounds[axis], vertex.position[axis]);
                }
            }

            const Vec3f center = (minBounds + maxBounds) * 0.5f;
            const Vec3f size = maxBounds - minBounds;
            const float maxSize = std::max({ size[0], size[1], size[2] });
            if (maxSize <= 0.0f) return;

            const float scale = targetSize / maxSize;
            for (auto& vertex : mesh.vertices) {
                for (int axis = 0; axis < 3; ++axis) {
                    vertex.position[axis] = (vertex.position[axis] - center[axis]) * scale;
                }
            }
        }
    }

    void GenerateNormals(Mesh& mesh) {
        for (auto& vertex : mesh.vertices)
            vertex.normal = Vec3f{};

        // Summing the unnormalized face normals weights each face by its area.
        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const std::uint32_t i0 = mesh.indices[i];
            const std::uint32_t i1 = mesh.indices[i + 1];
            const std::uint32_t i2 = mesh.indices[i + 2];

            const Vec3f p0 = Position3(mesh.vertices[i0]);
            const Vec3f p1 = Position3(mesh.vertices[i1]);
            const Vec3f p2 = Position3(mesh.vertices[i2]);

            const Vec3f normal = Cross(p1 - p0, p2 - p0);

            mesh.vertices[i0].normal += normal;
            mesh.vertices[i1].normal += normal;
            mesh.vertices[i2].normal += normal;
        }

        for (auto& vertex : mesh.vertices)
            vertex.normal = vertex.normal.Normalized();
    }

    Mesh MakeTestTriangle() {
        Mesh mesh{};
        mesh.vertices = {
            { { 0.0f,  1.0f, 0.5f, 1.0f }, {}, {} },
            { { -1.0f, -1.0f, 0.5f, 1.0f }, {}, {} },
            { { 1.0f,  -1.0f, 0.5f, 1.0f }, {}, {} }
        };
        mesh.colors = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 } };
        mesh.indices = { 0, 1, 2 };
        return mesh;
    }

    Mesh LoadMesh(const std::filesystem::path& filepath, Color color, bool deduplicate) {
        if (filepath.extension().string() != ".obj") return Mesh{};

        const auto [attribs, objIndices] = LoadObj(filepath);

        const auto validNormals = std::ranges::count_if(objIndices, [](const auto& idx) {
            return idx.normal_index != kInvalidIndex;
        });

        std::println("OBJ normals: {} | Face references with normals: {} / {}",
                     attribs.normals.size() / 3, validNormals, objIndices.size());

        Mesh mesh;
        mesh.vertices.reserve(objIndices.size());
        mesh.indices.reserve(objIndices.size());

        std::unordered_map<IndexKey, std::uint32_t> seen;
        if (deduplicate) seen.reserve(objIndices.size());

        for (const auto& idx : objIndices) {
            const std::size_t pos = static_cast<std::size_t>(idx.vertex_index) * 3;
            if (pos + 2 >= attribs.vertices.size()) {
                continue;
            }

            const auto vertexIndex = static_cast<std::uint32_t>(mesh.vertices.size());

            if (deduplicate) {
                const IndexKey key{
                    idx.vertex_index,
                    idx.texcoord_index,
                    idx.normal_index
                };

                // Duplicate: reuse the old vertex, add no new one.
                if (const auto it = seen.find(key); it != seen.end()) {
                    mesh.indices.push_back(it->second);
                    continue;
                }
                seen.emplace(key, vertexIndex);
            }

            Vec2f uv{};
            if (idx.texcoord_index != kInvalidIndex) {
                const std::size_t i = static_cast<std::size_t>(idx.texcoord_index) * 2;
                if (i + 1 < attribs.texcoords.size()) {
                    uv = { attribs.texcoords[i], attribs.texcoords[i + 1] };
                }
            }

            Vec3f normal{};
            if (idx.normal_index != kInvalidIndex) {
                const std::size_t i = static_cast<std::size_t>(idx.normal_index) * 3;
                if (i + 2 < attribs.normals.size()) {
                    normal = { attribs.normals[i], attribs.normals[i + 1], attribs.normals[i + 2] };
                }
            }

            // New vertex: its index is its position in mesh.vertices.
            mesh.indices.push_back(vertexIndex);
            mesh.vertices.emplace_back(
                Vec4f{
                    attribs.vertices[pos],
                    attribs.vertices[pos + 1],
                    attribs.vertices[pos + 2],
                    kDefaultVertexW
                },
                uv,
                normal
            );
        }

        CenterAndScale(mesh, 2.0f);

        mesh.colors = GenerateRGBA(mesh.vertices.size(), color.r, color.g, color.b, color.a);
        GenerateNormals(mesh);
        return mesh;
    }
}
