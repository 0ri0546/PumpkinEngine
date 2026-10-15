#pragma once

#include <Renderer/Rasterizer.hpp>
#include <Renderer/Types.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Pumpkin {
    inline constexpr int kTileSize = 64;

    // A triangle id is chunk * kSetupChunk + index in the chunk.
    inline constexpr std::size_t kSetupChunk = 512;
    inline constexpr std::uint32_t kChunkShift = 9;
    static_assert(kSetupChunk == (1u << kChunkShift));

    struct SortItem {
        std::uint32_t key;
        std::uint32_t index; // triangle id
    };

    struct BinData {
        std::vector<SortItem> items;          // every tile list, one after the other
        std::vector<std::uint32_t> tileBegin; // tileCount + 1 entries
        std::vector<std::uint32_t> tileSplit; // end of the opaque part of each tile
    };

    /// @brief Sorts ascending by key, then by triangle id.
    void SortItems(std::span<SortItem> items, std::vector<SortItem>& scratch);

    /// @brief Bins the triangles in two parallel passes (count, then fill).
    ///
    /// The result is the same every frame: inside a tile, items are in triangle id order.
    /// Depth sorting is done later, per tile, in the draw worker.
    /// Layout of a tile: [ opaque items | transparent items ]
    void BinTriangles(const std::vector<std::vector<Triangle>>& chunks,
        int tilesX, int tilesY, BinData& bins);
}
