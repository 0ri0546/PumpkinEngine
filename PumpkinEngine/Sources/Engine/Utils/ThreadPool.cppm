export module Pumpkin.Utils.ThreadPool;

import std;

export namespace Pumpkin {
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

    [[nodiscard]] ThreadPool& GetPool();

    template <class Pool, class Fn>
    void ParallelFor(Pool&& pool, std::size_t count, Fn&& fn) {
        std::atomic<std::size_t> next{ 0 };
        const std::function<void()> worker = [&] {
            for (;;) {
                const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= count) return;
                fn(i);
            }
        };
        pool.Run(worker);
    }
}
