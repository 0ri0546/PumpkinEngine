/// @file Binning.cppm
/// @brief Module Pumpkin.Renderer.Passes.Binning: assigns primitives to screen tiles and sorts depth keys.

export module Pumpkin.Renderer.Passes.Binning;

import Pumpkin.Renderer.PrimitiveAssembly;

import std;

export namespace Pumpkin {
    /// @brief Tile edge in pixels; the rasterizer's unit of parallel work.
    inline constexpr int kTileSize = 64;

    /// @brief Entry of a tile list: a sortable depth key and the id of the primitive it refers to.
    struct SortItem {
        std::uint32_t key;
        std::uint32_t index; // primitive id
    };

    /// @brief Output of BinPrimitives(): all tile lists packed in one array.
    ///
    /// Tile `t` owns `items[tileBegin[t], tileBegin[t + 1])`; the first part `[tileBegin[t], tileSplit[t])`
    /// is opaque, the rest transparent. Reused between frames to avoid reallocations.
    struct BinData {
        std::vector<SortItem> items;          // every tile list, one after the other
        std::vector<std::uint32_t> tileBegin; // tileCount + 1 entries
        std::vector<std::uint32_t> tileSplit; // end of the opaque part of each tile
    };

    /// @brief Sorts ascending by key, then by triangle id.
    ///
    /// Uses `std::ranges::sort` below 256 items and a stable 3-pass, 11-bit LSD radix sort above (keys are
    /// compared as unsigned 32-bit integers).
    ///
    /// @param items   Items to sort in place.
    /// @param scratch Temporary storage, grown if needed; give each thread its own.
    /// @thread_safety Thread-safe as long as callers use distinct @p items / @p scratch.
    void SortItems(std::span<SortItem> items, std::vector<SortItem>& scratch);

    /// @brief Bins the primitives in two parallel passes (count, then fill).
    ///
    /// The result is the same every frame: inside a tile, items are in primitive id order.
    /// Depth sorting is done later, per tile, in the draw worker.
    /// Layout of a tile: [ opaque items | transparent items ]
    ///
    /// @param batch  Primitives to bin.
    /// @param tilesX Number of tile columns (`ceil(width / kTileSize)`).
    /// @param tilesY Number of tile rows.
    /// @param bins   Receives the tile lists (previous contents are replaced).
    /// @note Opaque items carry the order-preserving integer form of their depth key (ascending = front to
    ///       back once sorted); transparent items carry the bitwise complement (ascending = back to front).
    /// @warning Not reentrant: the per-job counters live in a function-local static vector.
    /// @thread_safety Counting and filling run in parallel on the engine thread pool; call from one thread at a time.
    void BinPrimitives(const PrimitiveBatch& batch, int tilesX, int tilesY, BinData& bins);
}
