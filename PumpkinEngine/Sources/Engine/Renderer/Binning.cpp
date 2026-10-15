#include <Renderer/Binning.hpp>

#include <Utils/ThreadPool.hpp>

#include <algorithm>
#include <array>
#include <bit>

namespace Pumpkin {
    static constexpr std::size_t kChunksPerJob = 32; // binning work unit
    static constexpr std::size_t kSmallSort = 256;

    [[nodiscard]] static std::uint32_t SortableKey(float f) {
        const auto u = std::bit_cast<std::uint32_t>(f);
        return u ^ (u >> 31 ? 0xFFFFFFFFu : 0x80000000u);
    }

    // LSD radix sort on a span, ascending, 3 passes of 11 bits. Stable.
    static void RadixSort(std::span<SortItem> items, std::vector<SortItem>& scratch) {
        if (scratch.size() < items.size()) scratch.resize(items.size());

        std::span<SortItem> src = items;
        std::span<SortItem> dst{ scratch.data(), items.size() };

        for (int pass = 0; pass < 3; ++pass) {
            const int shift = pass * 11;

            std::array<std::uint32_t, 2048> offset{};
            for (const auto& it : src) ++offset[(it.key >> shift) & 2047];

            std::uint32_t sum = 0;
            for (auto& o : offset) { const std::uint32_t n = o; o = sum; sum += n; }

            for (const auto& it : src) dst[offset[(it.key >> shift) & 2047]++] = it;
            std::swap(src, dst);
        }

        // 3 passes (odd): the result ended up in scratch
        std::ranges::copy(src, items.begin());
    }

    void SortItems(std::span<SortItem> items, std::vector<SortItem>& scratch) {
        if (items.size() < kSmallSort) {
            std::ranges::sort(items, [](const SortItem& a, const SortItem& b) {
                return a.key != b.key ? a.key < b.key : a.index < b.index;
            });
            return;
        }
        RadixSort(items, scratch);
    }

    // Calls fn(tileIndex) for every tile touched by the triangle's bounding box.
    template <class Fn>
    static void ForEachTile(const Triangle& t, int tilesX, Fn&& fn) {
        const int tx0 = t.x0 / kTileSize, tx1 = (t.x1 - 1) / kTileSize;
        const int ty0 = t.y0 / kTileSize, ty1 = (t.y1 - 1) / kTileSize;
        for (int ty = ty0; ty <= ty1; ++ty)
            for (int tx = tx0; tx <= tx1; ++tx)
                fn(static_cast<std::size_t>(ty) * tilesX + tx);
    }

    void BinTriangles(const std::vector<std::vector<Triangle>>& chunks,
        int tilesX, int tilesY, BinData& bins) {
        const std::size_t tileCount = static_cast<std::size_t>(tilesX) * tilesY;
        const std::size_t jobCount = (chunks.size() + kChunksPerJob - 1) / kChunksPerJob;

        // counts[(job * tileCount + tile) * 2 + cls], cls 0 = opaque, 1 = transparent
        static std::vector<std::uint32_t> counts;
        counts.assign(jobCount * tileCount * 2, 0);

        // Pass 1: count
        ParallelFor(GetPool(), jobCount, [&](std::size_t job) {
            std::uint32_t* c = counts.data() + job * tileCount * 2;
            const std::size_t end = std::min((job + 1) * kChunksPerJob, chunks.size());
            for (std::size_t ci = job * kChunksPerJob; ci < end; ++ci)
                for (const Triangle& t : chunks[ci]) {
                    const std::size_t cls = t.transparent ? 1 : 0;
                    ForEachTile(t, tilesX, [&](std::size_t tile) { ++c[tile * 2 + cls]; });
                }
        });

        // Prefix sum: turns each counter into the first write position of its job
        bins.tileBegin.resize(tileCount + 1);
        bins.tileSplit.resize(tileCount);
        std::uint32_t sum = 0;
        for (std::size_t tile = 0; tile < tileCount; ++tile) {
            bins.tileBegin[tile] = sum;
            for (std::size_t cls = 0; cls < 2; ++cls) {
                for (std::size_t job = 0; job < jobCount; ++job) {
                    std::uint32_t& c = counts[(job * tileCount + tile) * 2 + cls];
                    const std::uint32_t n = c;
                    c = sum;
                    sum += n;
                }
                if (cls == 0) bins.tileSplit[tile] = sum;
            }
        }
        bins.tileBegin[tileCount] = sum;
        bins.items.resize(sum);

        // Pass 2: fill. Each job writes in its own ranges, no atomics needed.
        ParallelFor(GetPool(), jobCount, [&](std::size_t job) {
            std::uint32_t* cursor = counts.data() + job * tileCount * 2;
            SortItem* out = bins.items.data();
            const std::size_t end = std::min((job + 1) * kChunksPerJob, chunks.size());

            for (std::size_t ci = job * kChunksPerJob; ci < end; ++ci) {
                const auto& chunk = chunks[ci];
                for (std::uint32_t local = 0; local < chunk.size(); ++local) {
                    const Triangle& t = chunk[local];
                    const std::uint32_t id = (static_cast<std::uint32_t>(ci) << kChunkShift) | local;
                    const std::uint32_t key = SortableKey(t.zKey);

                    // Transparent: inverted key, so it ends up back to front
                    const std::size_t cls = t.transparent ? 1 : 0;
                    const SortItem item{ t.transparent ? ~key : key, id };
                    ForEachTile(t, tilesX, [&](std::size_t tile) {
                        out[cursor[tile * 2 + cls]++] = item;
                    });
                }
            }
        });
    }
}
