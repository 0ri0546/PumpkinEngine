#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <print>
#include <string_view>

namespace Prof {
    using Clock = std::chrono::steady_clock;

    enum Stage : std::size_t { Vertex, Clip, ToScreen, Assemble, Fragment, Count };
    inline constexpr std::array<std::string_view, Count> kNames = {
        "Vertex shader", "Clipping", "To screen space", "Primitive assembly", "Fragment shader"
    };

    struct Stats {
        std::array<double, Count> total{};   // ms, summed over the window
        std::array<double, Count> worst{};   // ms, worst single call
        unsigned frames = 0;
        bool warmedUp = false;
    };
    inline Stats g;

    class Scope {
    public:
        explicit Scope(Stage s) : m_stage(s), m_t0(Clock::now()) {}
        ~Scope() {
            if (!g.warmedUp) return;
            const double ms = std::chrono::duration<double, std::milli>(Clock::now() - m_t0).count();
            g.total[m_stage] += ms;
            g.worst[m_stage] = std::max(g.worst[m_stage], ms);
        }
    private:
        Stage m_stage;
        Clock::time_point m_t0;
    };

    // Called at the end of the main loop
    inline void EndFrame(unsigned interval = 120) {
        if (!g.warmedUp) { g.warmedUp = true; return; }   // drop frame 0
        if (++g.frames < interval) return;

        double sum = 0;
        for (double t : g.total) sum += t;

        std::println("{:<20} {:>10} {:>10} {:>7}", "stage", "avg ms", "worst ms", "share");
        std::println("{:-<20} {:->10} {:->10} {:->7}", "", "", "", "");
        for (std::size_t i = 0; i < Count; ++i) {
            std::println("{:<20} {:>10.3f} {:>10.3f} {:>6.1f}%",
                kNames[i], g.total[i] / g.frames, g.worst[i],
                sum > 0 ? 100.0 * g.total[i] / sum : 0.0);
        }
        std::println("{:<20} {:>10.3f}  ({:.0f} fps-equivalent)\n",
            "total", sum / g.frames, 1000.0 * g.frames / sum);

        g = Stats{ .warmedUp = true };
    }
}
