/// @file ThreadPool.cppm
/// @brief Module Pumpkin.ECS.ThreadPool: worker pool used by the ECS Scheduler.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.ThreadPool;

import std;

export namespace Pumpkin::ECS {
    /// @brief Fixed-size pool of worker threads executing queued tasks, with a parallel-for helper.
    ///
    /// Workers are `std::jthread`s waiting on a condition variable. Tasks are executed in FIFO
    /// order of submission (across workers, completion order is unspecified).
    ///
    /// @thread_safety Submit(), ParallelFor() and Size() may be called from any thread,
    ///                including from inside a task running on the pool.
    class PUMPKIN_API ThreadPool {
    public:
        /// @brief Unit of work executed by a worker.
        using Task = std::function<void()>;

        /// @brief Starts the worker threads.
        /// @param threadCount Number of workers; clamped to at least 1. Defaults to
        ///                    `std::thread::hardware_concurrency()`.
        explicit ThreadPool(std::size_t threadCount = std::thread::hardware_concurrency()) {
            threadCount = std::max<std::size_t>(1, threadCount);
            m_workers.reserve(threadCount);
            for (std::size_t i = 0; i < threadCount; ++i)
                m_workers.emplace_back([this](const std::stop_token& stop) { WorkerLoop(stop); });
        }

        /// @brief Requests stop and joins the workers.
        ///
        /// Workers finish the tasks still in the queue before exiting, so destruction blocks until
        /// the queue is drained.
        ///
        /// @note Members are ordered so that `m_workers` is destroyed first, which joins the threads
        ///       before the queue and mutex they use are destroyed.
        // m_workers is declared last, so the jthreads are joined before the queue/mutex die
        ~ThreadPool() {
            for (auto& worker : m_workers)
                worker.request_stop();
        }

        /// @brief Non-copyable (owns threads).
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /// @brief Number of worker threads.
        /// @return Worker count (>= 1).
        [[nodiscard]] std::size_t Size() const {
            return m_workers.size();
        }

        /// @brief Queues a task and wakes one worker.
        /// @param task Callable to run on a worker thread.
        /// @warning An exception escaping @p task terminates the program (it would leave the
        ///          worker thread). Catch inside the task, as Scheduler does.
        /// @thread_safety Thread-safe.
        void Submit(Task task) {
            {
                std::scoped_lock lock(m_mutex);
                m_tasks.push_back(std::move(task));
            }
            m_cv.notify_one();
        }

        /// @brief Splits `[0, count)` into chunks and runs @p fn on each chunk, in parallel.
        ///
        /// The range is cut into `ceil(count / grain)` chunks claimed through an atomic counter.
        /// Up to `min(Size(), chunks - 1)` helper tasks are submitted; the calling thread also
        /// processes chunks, then waits only for chunks that are still running. Because the caller
        /// participates, it is safe to call from inside a pool task (no deadlock from pool exhaustion).
        /// A single chunk runs inline on the caller without touching the queue.
        ///
        /// @tparam F Callable invocable as `fn(std::size_t begin, std::size_t end)`; `end` is exclusive.
        /// @param count Size of the index range; 0 returns immediately.
        /// @param grain Chunk size (clamped to at least 1). Smaller values balance load better but cost more scheduling.
        /// @param fn    Per-chunk callback, invoked concurrently from several threads.
        /// @pre @p fn is thread-safe and does not throw (an exception on a worker terminates the program).
        /// @post Every index in `[0, count)` was processed exactly once when the function returns.
        /// @thread_safety Thread-safe; blocks the calling thread until completion.
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
        /// @brief Worker thread body: pops and runs tasks until stop is requested and the queue is empty.
        /// @param stop Stop token of the owning `std::jthread`.
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
