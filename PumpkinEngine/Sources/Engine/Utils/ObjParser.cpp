module;

#include <cassert>

module Pumpkin.Utils.ObjLoader;

import std;

namespace Pumpkin {
    std::pair<attrib_t, std::vector<index_t>> LoadObj(const std::filesystem::path& filepath) {
        assert(!filepath.string().empty() && "Invalid file path!");
        assert(filepath.extension() == ".obj" && "File must be .obj!");

        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file || !file.is_open())
        {
            throw std::runtime_error("Couldn't load 3D model!");
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size + 1);
        if (!file.read(buffer.data(), size))
        {
            throw std::runtime_error("Failed to read file!");
        }
        buffer[size] = '\0';

        attrib_t attribs;
        std::vector<index_t> indices;

        std::size_t estimation = size / 25;
        attribs.vertices.reserve(estimation);
        attribs.texcoords.reserve(estimation);
        attribs.normals.reserve(estimation);
        indices.reserve((size * 2) / 5);

        char* ptr = buffer.data();
        char* end = ptr + size;

        auto parseFloat = [](char*& p) -> float
        {
            while (*p == ' ' || *p == '\t') ++p;

            float result = 0.0f;
            float sign = 1.0f;

            if (*p == '-')
            {
                sign = -1.0f;
                ++p;
            }
            else if (*p == '+') ++p;

            while (*p >= '0' && *p <= '9')
            {
                result = result * 10.0f + (*p - '0');
                ++p;
            }

            if (*p == '.')
            {
                ++p;
                float frac = 0.1f;
                while (*p >= '0' && *p <= '9')
                {
                    result += (*p - '0') * frac;
                    frac *= 0.1f;
                    ++p;
                }
            }

            // Handle scientific notation (e.g., 1.5e-3)
            if (*p == 'e' || *p == 'E') {
                ++p;
                int expSign = 1;
                if (*p == '-')
                {
                    expSign = -1;
                    ++p;
                }
                else if (*p == '+') ++p;

                int exponent = 0;
                while (*p >= '0' && *p <= '9')
                {
                    exponent = exponent * 10 + (*p - '0');
                    ++p;
                }

                float exponentFactor = 1.0f;
                for (int i = 0; i < exponent; ++i)
                {
                    exponentFactor *= 10.0f;
                }
                result *= (expSign == -1) ? (1.0f / exponentFactor) : exponentFactor;
            }

            return result * sign;
        };

        auto parseInt = [](char*& p) -> int
        {
            while (*p == ' ' || *p == '\t') ++p;

            int result = 0;
            bool negative = false;

            if (*p == '-')
            {
                negative = true;
                ++p;
            }

            while (*p >= '0' && *p <= '9')
            {
                result = result * 10 + (*p - '0');
                ++p;
            }

            return negative ? -result : result;
        };

        auto skipWhitespace = [](char*& p)
        {
            while (*p == ' ' || *p == '\t')
            {
                ++p;
            }
        };

        auto skipLine = [](char*& p, char* end)
        {
            while (p < end && *p != '\n')
            {
                ++p;
            }
            if (p < end)
            {
                ++p;
            }
        };

        unsigned int vCount = 0, vtCount = 0, vnCount = 0, fCount = 0;

        auto now = std::chrono::high_resolution_clock::now();
        while (ptr < end)
        {
            skipWhitespace(ptr);

            if (*ptr == '\0' || *ptr == '\n' || *ptr == '#')
            {
                skipLine(ptr, end);
                continue;
            }

            if (ptr[0] == 'v')
            {
                if (ptr[1] == ' ')
                {
                    // Vertex: v x y z
                    ptr += 2;
                    float x = parseFloat(ptr);
                    float y = parseFloat(ptr);
                    float z = parseFloat(ptr);

                    std::size_t base = attribs.vertices.size();
                    attribs.vertices.resize(base + 3);
                    attribs.vertices[base + 0] = x;
                    attribs.vertices[base + 1] = y;
                    attribs.vertices[base + 2] = z;
                    ++vCount;
                }
                else if (ptr[1] == 't')
                {
                    // UV: vt x y
                    ptr += 2;
                    float u = parseFloat(ptr);
                    float v = parseFloat(ptr);
                    std::size_t base = attribs.texcoords.size();
                    attribs.texcoords.resize(base + 2);
                    attribs.texcoords[base + 0] = u;
                    attribs.texcoords[base + 1] = v;
                    ++vtCount;
                }
                else if (ptr[1] == 'n')
                {
                    // Normal: vn x y z
                    ptr += 2;
                    float nx = parseFloat(ptr);
                    float ny = parseFloat(ptr);
                    float nz = parseFloat(ptr);

                    std::size_t base = attribs.normals.size();
                    attribs.normals.resize(base + 3);
                    attribs.normals[base + 0] = nx;
                    attribs.normals[base + 1] = ny;
                    attribs.normals[base + 2] = nz;
                    ++vnCount;
                }
            }
            else if (ptr[0] == 'l' && ptr[1] == ' ')
            {
                // Line: l v1 v2 ... or l v1/vt1 v2/vt2 ...
                ptr += 2;
                while (ptr < end && *ptr != '\n' && *ptr != '\r' && *ptr != '#')
                {
                    skipWhitespace(ptr);
                    if (*ptr == '\n' || *ptr == '\r' || *ptr == '#') break;

                    int v = parseInt(ptr);
                    int vt = -1;

                    if (*ptr == '/')
                    {
                        ++ptr;
                        vt = parseInt(ptr);
                    }

                    indices.emplace_back(
                        (v > 0)  ? (v - 1)  : std::numeric_limits<std::uint32_t>::max(),
                        (vt > 0) ? (vt - 1) : std::numeric_limits<std::uint32_t>::max(),
                        std::numeric_limits<std::uint32_t>::max()
                    );
                }
            }
            else if (ptr[0] == 'f' && ptr[1] == ' ')
            {
                // Face: f v/vt/vn ... (also supports v, v/vt, v//vn)
                ptr += 2;
                index_t face_buffer[16];
                std::size_t face_count = 0;

                while (ptr < end && *ptr != '\n' && *ptr != '\r' && *ptr != '#')
                {
                    skipWhitespace(ptr);
                    if (*ptr == '\n' || *ptr == '\r' || *ptr == '#') break;

                    int v = parseInt(ptr);
                    int vt = -1, vn = -1;

                    if (*ptr == '/')
                    {
                        ++ptr;
                        if (*ptr == '/')
                        {
                            ++ptr;
                            vn = parseInt(ptr);
                        }
                        else
                        {
                            vt = parseInt(ptr);
                            if (*ptr == '/')
                            {
                                ++ptr;
                                vn = parseInt(ptr);
                            }
                        }
                    }

                    if (face_count < 16)
                    {
                        face_buffer[face_count++] = {
                            .vertex_index   = (v > 0)  ? (v - 1)  : std::numeric_limits<std::uint32_t>::max(),
                            .texcoord_index = (vt > 0) ? (vt - 1) : std::numeric_limits<std::uint32_t>::max(),
                            .normal_index   = (vn > 0) ? (vn - 1) : std::numeric_limits<std::uint32_t>::max()
                        };
                    }
                }

                // Triangulate (fan triangulation)
                if (face_count == 3)
                {
                    indices.push_back(face_buffer[0]);
                    indices.push_back(face_buffer[1]);
                    indices.push_back(face_buffer[2]);
                    ++fCount;
                }
                else if (face_count == 4)
                {
                    indices.push_back(face_buffer[0]);
                    indices.push_back(face_buffer[1]);
                    indices.push_back(face_buffer[2]);

                    indices.push_back(face_buffer[0]);
                    indices.push_back(face_buffer[2]);
                    indices.push_back(face_buffer[3]);
                    fCount += 2;
                }
                else if (face_count > 4)
                {
                    for (std::size_t i = 1; i + 1 < face_count; ++i)
                    {
                        indices.push_back(face_buffer[0]);
                        indices.push_back(face_buffer[i]);
                        indices.push_back(face_buffer[i + 1]);
					    ++fCount;
                    }
                }
            }

            skipLine(ptr, end);
        }
        auto then = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(then - now);
	    std::cout << "Took " << duration.count() << " ms to parse " << filepath.filename().string() << " (" << vCount << " vertices, " << vtCount << " texcoords, " << vnCount << " normals, " << fCount << " faces)" << std::endl;

        attribs.vertices.shrink_to_fit();
        attribs.texcoords.shrink_to_fit();
        attribs.normals.shrink_to_fit();
        indices.shrink_to_fit();

        return {attribs, indices};
    }
} // namespace Pumpkin
