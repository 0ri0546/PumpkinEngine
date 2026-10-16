#pragma once

#include <chrono>
#include <cstddef>
#include <print>

namespace Pumpkin {
    /// @brief Per-stage frame timer. Prints the average cost once per second.
    struct StageTimer {
        using Clock = std::chrono::steady_clock;

        double buildMs = 0.0, binMs = 0.0, drawMs = 0.0;
        int frames = 0;
        Clock::time_point lastPrint = Clock::now();

        static double Ms(Clock::time_point a, Clock::time_point b) {
            return std::chrono::duration<double, std::milli>(b - a).count();
        }

        void EndFrame(std::size_t triangleCount) {
            ++frames;
            const auto now = Clock::now();
            if (Ms(lastPrint, now) < 1000.0) return;

            std::println("build {:.3f} | bin {:.3f} | draw {:.3f} ms | {} triangles",
                buildMs / frames, binMs / frames, drawMs / frames, triangleCount);
            buildMs = binMs = drawMs = 0.0;
            frames = 0;
            lastPrint = now;
        }
    };

    inline StageTimer gTimer;
}
