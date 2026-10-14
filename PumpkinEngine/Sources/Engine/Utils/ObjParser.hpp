#pragma once

// STL
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

/*
 * Notes :
 * v  = vertex position (x y z)
 * vt = vertex uv coordinate (u v)
 * vn = vertex normal (x y z)
 * f = face definition (v/vt/vn v/vt/vn v/vt/vn)
 * l = line definition <variadic> v1 v2 v3...
 */

namespace Pumpkin
{
    typedef struct
    {
        uint32_t vertex_index;
        uint32_t texcoord_index;
        uint32_t normal_index;
    } index_t;

    typedef struct
    {
        std::vector<float> vertices;
        std::vector<float> texcoords;
        std::vector<float> normals;
    } attrib_t;

    /// @brief Parses a wavefront OBJ file and returns packed data
    ///
    /// @param filepath File path to the concerned wavefront OBJ file.
    std::pair<attrib_t, std::vector<index_t>> LoadObj(const std::filesystem::path& filepath);
} // namespace Pumpkin
