#include "ThreadPool.hpp"

#include <barrier>
#include <functional>
#include <vector>

namespace Pumpkin {
    // `workers` extra threads. The calling thread also works in Run().
    ThreadPool::ThreadPool(unsigned workers) : m_sync(workers + 1) {
        for (unsigned i = 0; i < workers; ++i)
            m_threads.emplace_back([this] { WorkerLoop(); });
    }

    ThreadPool::~ThreadPool() {
        if (m_threads.empty()) return;
        m_stop = true;
        m_sync.arrive_and_wait(); // wake the workers so they see m_stop
    }

    // Runs `job` on all threads and waits until everyone is done.
    void ThreadPool::Run(const std::function<void()>& job) {
        if (m_threads.empty()) {
            job();
            return;
        }
        m_job = &job;
        m_sync.arrive_and_wait(); // start
        job();
        m_sync.arrive_and_wait(); // finish
    }

    void ThreadPool::WorkerLoop() {
        for (;;) {
            m_sync.arrive_and_wait(); // wait for work
            if (m_stop) return;
            (*m_job)();
            m_sync.arrive_and_wait(); // report done
        }
    }

    [[nodiscard]] ThreadPool& GetPool() {
        // Max threads - 2, counting the calling thread.
        static const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
        static const unsigned total = hw > 2 ? hw - 2 : 1;
        static ThreadPool pool(total - 1);
        return pool;
    }
}
