#pragma once

#include <barrier>
#include <functional>
#include <thread>
#include <vector>

namespace Pumpkin {
    class ThreadPool {
    public:
        // `workers` extra threads. The calling thread also works in Run().
        explicit ThreadPool(unsigned workers);

        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        // Runs `job` on all threads and waits until everyone is done.
        void Run(const std::function<void()>& job);

    private:
        void WorkerLoop();

        std::barrier<> m_sync;
        const std::function<void()>* m_job = nullptr;
        bool m_stop = false;
        std::vector<std::jthread> m_threads; // last member: joined first
    };
}
