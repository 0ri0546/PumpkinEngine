module;

#include <DLL/Export.hpp>

export module Pumpkin.Utils.RenderProfiler;

import std;

export namespace Pumpkin::Prof {
    using Clock = std::chrono::steady_clock;

    enum Stage : std::size_t { Vertex, Clip, ToScreen, Assemble, Fragment, Count };
    inline constexpr std::array<std::string_view, Count> kNames = {
        "Vertex shader", "Clipping", "To screen space", "Primitive assembly", "Fragment shader"
    };

    struct Entry {
        std::string_view name;
        double total = 0.0;   // ms, summed over the window
        double worst = 0.0;   // ms, worst single call
    };

    struct PUMPKIN_API Stats {
        std::vector<Entry> entries;
        double topLevel = 0.0;
        unsigned frames = 0;
        int depth = 0;
        bool warmedUp = false;
        Clock::time_point windowStart{};

        Stats() {
            for (const auto n : kNames) entries.push_back(Entry{ .name = n });
        }

        std::size_t Find(std::string_view name) {
            for (std::size_t i = 0; i < entries.size(); ++i)
                if (entries[i].name == name) return i;
            entries.push_back(Entry{ .name = name });
            return entries.size() - 1;
        }

        void Reset() {
            for (auto& e : entries) e.total = e.worst = 0.0;
            topLevel = 0.0;
            frames = 0;
            windowStart = Clock::now();
        }
    };
    PUMPKIN_API Stats g;

    class Scope {
    public:
        explicit Scope(std::string_view name)
            : m_index(g.Find(name)), m_depth(g.depth++), m_t0(Clock::now()) {}

        explicit Scope(Stage s) : Scope(kNames[s]) {}

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        ~Scope() {
            --g.depth;
            if (!g.warmedUp) return;

            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - m_t0).count();
            Entry& e = g.entries[m_index];
            e.total += ms;
            e.worst = std::max(e.worst, ms);
            if (m_depth == 0) g.topLevel += ms;
        }

    private:
        std::size_t       m_index;
        int               m_depth;
        Clock::time_point m_t0;
    };

    // Called at the end of the main loop
    inline void EndFrame(Clock::duration interval = std::chrono::seconds(1)) {
        if (!g.warmedUp) { // drop frame 0
            g.warmedUp = true;
            g.windowStart = Clock::now();
            return;
        }

        ++g.frames;
        const auto elapsed = Clock::now() - g.windowStart;
        if (elapsed < interval) return;

        const double seconds = std::chrono::duration<double>(elapsed).count();

        std::size_t width = 20;
        std::size_t i = 0;
        for (const auto& e : g.entries) width = std::max(width, e.name.size());

        auto printDashes = [&width] {
            std::println("{:-<{}} {:->10} {:->10} {:->7}", "", width, "", "", "");
        };

        std::println("{:<{}} {:>10} {:>10} {:>7}", "Stage", width, "avg ms", "worst ms", "share");
        printDashes();
        for (const auto& e : g.entries) {
            if (i++ == Count) {
                printDashes();
                std::println("{:<{}} {:>10} {:>10} {:>7}", "Render pass", width, "avg ms", "worst ms", "share");
                printDashes();
            }
            std::println("{:<{}} {:>10.3f} {:>10.3f} {:>6.1f}%",
                e.name, width,
                e.total / g.frames, e.worst,
                g.topLevel > 0 ? 100.0 * e.total / g.topLevel : 0.0);
        }

        const double avgMs = g.topLevel / g.frames;
        printDashes();
        std::println("{:<{}} {:>10.3f}  ({:.0f} fps-equivalent)",
            "total", width, avgMs, avgMs > 0 ? 1000.0 / avgMs : 0.0);
        std::println("{:<{}} {:>10.1f} fps over {:.2f}s\n",
            "measured", width, g.frames / seconds, seconds);

        g.Reset();
    }
}
