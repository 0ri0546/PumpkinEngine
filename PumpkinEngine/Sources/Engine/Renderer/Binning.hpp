#pragma once

#include <Renderer/PrimitiveAssembly.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Pumpkin {
    inline constexpr int kTileSize = 64;

    struct SortItem {
        std::uint32_t key;
        std::uint32_t index; // primitive id
    };

    struct BinData {
        std::vector<SortItem> items;          // every tile list, one after the other
        std::vector<std::uint32_t> tileBegin; // tileCount + 1 entries
        std::vector<std::uint32_t> tileSplit; // end of the opaque part of each tile
    };

    /// @brief Sorts ascending by key, then by triangle id.
    void SortItems(std::span<SortItem> items, std::vector<SortItem>& scratch);

    /// @brief Bins the primitives in two parallel passes (count, then fill).
    ///
    /// The result is the same every frame: inside a tile, items are in primitive id order.
    /// Depth sorting is done later, per tile, in the draw worker.
    /// Layout of a tile: [ opaque items | transparent items ]
    void BinPrimitives(const PrimitiveBatch& batch, int tilesX, int tilesY, BinData& bins);
}
