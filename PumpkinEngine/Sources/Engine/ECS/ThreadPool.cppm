module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.ThreadPool;

import std;

export namespace Pumpkin::ECS {
    class PUMPKIN_API ThreadPool {
    public:
        using Task = std::function<void()>;

        explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency()) {
            threadCount = std::max<std::size_t>(1, threadCount);
            m_workers.reserve(threadCount);
            for (std::size_t i = 0; i < threadCount; ++i)
                m_workers.emplace_back([this](const std::stop_token& stop) { WorkerLoop(stop); });
        }

        // m_workers is declared last, so the jthreads are joined before the queue/mutex die
        ~ThreadPool() {
            for (auto& worker : m_workers)
                worker.request_stop();
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        [[nodiscard]] std::size_t Size() const {
            return m_workers.size();
        }

        void Submit(Task task) {
            {
                std::scoped_lock lock(m_mutex);
                m_tasks.push_back(std::move(task));
            }
            m_cv.notify_one();
        }

        // Splits [0, count) in chunks of `grain` and calls fn(begin, end) on each chunk.
        // The calling thread works too and only waits for chunks that are running.
        // fn must be thread-safe and must not throw.
        template <typename F>
        void ParallelFor(std::size_t count, std::size_t grain, F&& fn) {
            if (count == 0)
                return;

            grain = std::max<std::size_t>(1, grain);
            const std::size_t chunks = (count + grain - 1) / grain;

            if (chunks == 1) {
                fn(std::size_t{0}, count);
                return;
            }

            struct State {
                std::atomic<std::size_t> next{0};
                std::atomic<std::size_t> done{0};
            };
            // Shared so a helper that starts after we return still touches valid memory.
            // Such a late helper finds no chunk left and never dereferences &fn.
            auto state = std::make_shared<State>();

            auto work = [state, chunks, count, grain, &fn] {
                for (;;) {
                    const std::size_t chunk = state->next.fetch_add(1, std::memory_order_relaxed);
                    if (chunk >= chunks)
                        return;

                    const std::size_t begin = chunk * grain;
                    fn(begin, std::min(count, begin + grain));

                    if (state->done.fetch_add(1, std::memory_order_acq_rel) + 1 == chunks)
                        state->done.notify_all();
                }
            };

            const std::size_t helpers = std::min(m_workers.size(), chunks - 1);
            for (std::size_t i = 0; i < helpers; ++i)
                Submit(work);

            work();

            for (auto seen = state->done.load(std::memory_order_acquire); seen != chunks;
                seen = state->done.load(std::memory_order_acquire))
                state->done.wait(seen);
        }

    private:
        void WorkerLoop(const std::stop_token& stop) {
            for (;;) {
                Task task;
                {
                    std::unique_lock lock(m_mutex);
                    // false only when stop was requested and the queue is empty
                    if (!m_cv.wait(lock, stop, [this] { return !m_tasks.empty(); }))
                        return;
                    task = std::move(m_tasks.front());
                    m_tasks.pop_front();
                }
                task();
            }
        }

        std::mutex m_mutex;
        std::condition_variable_any m_cv;
        std::deque<Task> m_tasks;
        std::vector<std::jthread> m_workers;
    };
} // namespace Pumpkin::ECS
