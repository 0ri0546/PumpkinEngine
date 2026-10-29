/// @file Scheduler.cppm
/// @brief Module Pumpkin.ECS.Scheduler: access declarations, system context and parallel system scheduler.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.Scheduler;

import Pumpkin.ECS.CommandBuffer;
import Pumpkin.ECS.Component;
import Pumpkin.ECS.ComponentPool;
import Pumpkin.ECS.Entity;
import Pumpkin.ECS.Registry;
import Pumpkin.ECS.ThreadPool;
import Pumpkin.ECS.World;

import std;

export namespace Pumpkin::ECS {

    /// @brief Declares which component types a system reads and writes.
    ///
    /// Two systems *conflict* when one writes something the other reads or writes; Write implies
    /// Read. The Scheduler uses this to decide which systems may run in parallel.
    ///
    /// @thread_safety Builder-style; not thread-safe while being configured. Const queries are safe afterwards.
    class PUMPKIN_API Access {
    public:
        /// @brief Declares read access to the given components.
        /// @tparam Ts Component types read by the system.
        /// @return *this, for chaining (`Access{}.Read<A>().Write<B>()`).
        template <Component... Ts>
        Access& Read() {
            (AddRead<Ts>(), ...);
            return *this;
        }

        /// @brief Declares write access (which implies read) to the given components.
        /// @tparam Ts Component types written by the system.
        /// @return *this, for chaining.
        template <Component... Ts>
        Access& Write() {
            (AddWrite<Ts>(), ...);
            return *this;
        }

        /// @brief Tests read permission.
        /// @param id Component type id (see ComponentTypeId).
        /// @return true if declared as read or write.
        bool CanRead(ComponentId id) const {
            return std::ranges::contains(m_reads, id) || std::ranges::contains(m_writes, id);
        }

        /// @brief Tests write permission.
        /// @param id Component type id.
        /// @return true if declared as write.
        bool CanWrite(ComponentId id) const {
            return std::ranges::contains(m_writes, id);
        }

        /// @brief Tells whether two systems must not run concurrently.
        /// @param other Access of the other system.
        /// @return true on write/write, write/read or read/write overlap; read/read never conflicts.
        bool ConflictsWith(const Access& other) const {
            const auto overlap = [](const auto& a, const auto& b) {
                return std::ranges::any_of(a, [&](ComponentId id) { return std::ranges::contains(b, id); });
            };
            return overlap(m_writes, other.m_writes) || overlap(m_writes, other.m_reads) ||
                   overlap(m_reads, other.m_writes);
        }

        /// @brief Creates every declared component pool up front.
        ///
        /// Done single-threaded so that Registry::Pool<T>() never resizes or allocates while systems run.
        ///
        /// @param registry Registry in which the pools are created.
        void EnsurePools(Registry& registry) const {
            for (auto ensure : m_ensure)
                ensure(registry);
        }

    private:
        /// @brief Records read access to @p T and a pool-creation callback for EnsurePools().
        template <Component T>
        void AddRead() {
            m_reads.push_back(ComponentTypeId<T>());
            m_ensure.push_back([](Registry& registry) { (void)registry.Pool<T>(); });
        }

        /// @brief Records write access to @p T and a pool-creation callback for EnsurePools().
        template <Component T>
        void AddWrite() {
            m_writes.push_back(ComponentTypeId<T>());
            m_ensure.push_back([](Registry& registry) { (void)registry.Pool<T>(); });
        }

        std::vector<ComponentId> m_reads;
        std::vector<ComponentId> m_writes;
        std::vector<void(*)(Registry&)> m_ensure;
    };

    /// @brief The interface a running system uses to reach the World.
    ///
    /// Every component access is validated against the system's declared Access; undeclared
    /// access throws instead of silently racing with other systems. Contexts are built by the
    /// Scheduler, one per system invocation.
    ///
    /// @thread_safety Used by one system at a time; ParallelForEach fans out to pool threads internally.
    class PUMPKIN_API SystemContext {
    public:
        /// @brief Builds a context (normally done by the Scheduler, not by user code).
        /// @param registry  Registry the system accesses.
        /// @param world     World, used for liveness checks.
        /// @param access    Declared access of the system (must outlive the context).
        /// @param commands  The system's own command buffer.
        /// @param pool      Thread pool used by ParallelForEach.
        /// @param deltaTime Frame time in seconds.
        SystemContext(Registry& registry, const World& world, const Access& access, CommandBuffer& commands,
                      ThreadPool& pool, float deltaTime)
            : m_registry(registry),
              m_world(world),
              m_access(access),
              m_commands(commands),
              m_pool(pool),
              m_deltaTime(deltaTime) {}

        /// @brief Frame time. @return Seconds passed to Scheduler::Run().
        float DeltaTime() const {
            return m_deltaTime;
        }

        /// @brief Liveness check (World::IsAlive).
        /// @param entity Handle to test.
        /// @return true if alive. Safe during Run: entities only change in the deferred apply phase.
        bool IsAlive(Entity entity) const {
            return m_world.IsAlive(entity);
        }

        /// @brief The command buffer used to defer structural changes (create/destroy/add/remove).
        /// @return Buffer applied after the frame, in registration order.
        CommandBuffer& Commands() {
            return m_commands;
        }

        /// @brief Read-only access to a component pool.
        /// @tparam T Component type.
        /// @return Const pool of @p T.
        /// @throws std::logic_error if @p T was not declared (Access::Read or Write).
        template <Component T>
        const ComponentPool<T>& Read() const {
            Require(m_access.CanRead(ComponentTypeId<T>()), "reading", typeid(T).name());
            return std::as_const(m_registry).Pool<T>();
        }

        /// @brief Mutable access to a component pool.
        /// @tparam T Component type.
        /// @return Mutable pool of @p T.
        /// @throws std::logic_error if @p T was not declared with Access::Write.
        /// @warning Do not add/remove components through the pool during a frame; use Commands().
        template <Component T>
        ComponentPool<T>& Write() {
            Require(m_access.CanWrite(ComponentTypeId<T>()), "writing", typeid(T).name());
            return m_registry.Pool<T>();
        }

        /// @brief Sequential multi-component view.
        /// @tparam Ts Component types iterated (each must be declared).
        /// @return View over entities having all of @p Ts.
        /// @throws std::logic_error if any component was not declared.
        /// @warning Only *read* permission is checked, yet the view yields mutable references:
        ///          declare Access::Write for every component you modify through it.
        template <Component... Ts>
        Registry::QueryView<Ts...> View() {
            (Require(m_access.CanRead(ComponentTypeId<Ts>()), "viewing", typeid(Ts).name()), ...);
            return m_registry.View<Ts...>();
        }

        /// @brief Iterates all entities having every component of @p Ts, in parallel.
        ///
        /// `fn(Entity, Ts&...)` is called once per matching entity, from several pool threads. The
        /// driver range is cut in chunks of @p grain entities; each entity belongs to exactly one
        /// chunk, so touching *that entity's* components is race-free.
        ///
        /// @tparam Ts Component types iterated (each must be declared).
        /// @tparam F  Callable invocable as `fn(Entity, Ts&...)`.
        /// @param fn    Per-entity callback.
        /// @param grain Entities per chunk (default 2048).
        /// @throws std::logic_error if any component was not declared.
        /// @pre @p fn must not touch other entities' components (unless read-only), must not mutate
        ///      shared captured state without synchronisation, and must not throw.
        /// @warning Like View(), only read permission is checked: declare Write for components you modify.
        /// @thread_safety Blocks the caller until all chunks finish. Record structural changes with Commands() (thread-safe).
        template <Component... Ts, typename F>
        void ParallelForEach(F&& fn, std::size_t grain = 2048) {
            (Require(m_access.CanRead(ComponentTypeId<Ts>()), "iterating", typeid(Ts).name()), ...);

            auto view = m_registry.View<Ts...>();
            m_pool.ParallelFor(view.Size(), grain, [&](std::size_t begin, std::size_t end) {
                for (auto item : view.Slice(begin, end - begin))
                    std::apply(fn, item);
            });
        }

    private:
        /// @brief Throws if an access was not declared.
        /// @param allowed Whether the Access permits the operation.
        /// @param what    Verb used in the message ("reading", "writing", ...).
        /// @param type    Mangled name of the component type, for diagnostics.
        /// @throws std::logic_error when @p allowed is false.
        static void Require(bool allowed, std::string_view what, const char* type) {
            if (!allowed)
                throw std::logic_error(std::format("System is {} component '{}' without declaring it", what, type));
        }

        Registry& m_registry;
        const World& m_world;
        const Access& m_access;
        CommandBuffer& m_commands;
        ThreadPool& m_pool;
        float m_deltaTime;
    };

    /// @brief Runs systems in parallel while respecting their declared data conflicts.
    ///
    /// Systems keep registration order *semantically*: a later system that conflicts with an
    /// earlier one waits for it, while non-conflicting systems run concurrently on the pool. The
    /// dependency graph is built lazily on the first Run()/DescribeGraph() after AddSystem().
    /// Each system has its own CommandBuffer, applied in registration order once the frame is done.
    ///
    /// @thread_safety Not thread-safe: call AddSystem()/Run() from a single thread and never
    ///                call Run() re-entrantly or modify the World meanwhile.
    class PUMPKIN_API Scheduler {
    public:
        /// @brief Signature of a system: receives its SystemContext.
        using SystemFn = std::function<void(SystemContext&)>;

        /// @brief Creates a scheduler with its own worker pool.
        /// @param world       World the systems operate on; must outlive the scheduler.
        /// @param threadCount Worker threads (default: hardware concurrency, minimum 1).
        explicit Scheduler(World& world, std::size_t threadCount = std::thread::hardware_concurrency())
            : m_world(world),
              m_pool(threadCount) {}

        /// @brief Registers a system.
        ///
        /// @tparam F Callable invocable with `SystemContext&`.
        /// @param name   Label shown by DescribeGraph().
        /// @param access Components the system reads/writes (drives parallelism and access checks).
        /// @param fn     The system body.
        /// @note Registration order defines the semantic execution order between conflicting systems.
        /// @warning Do not call while Run() is executing. Marks the graph for rebuild.
        template <std::invocable<SystemContext&> F>
        void AddSystem(std::string name, Access access, F&& fn) {
            auto& node = m_nodes.emplace_back();
            node.name = std::move(name);
            node.access = std::move(access);
            node.fn = std::forward<F>(fn);
            m_dirty = true;
        }

        /// @brief Executes one frame.
        ///
        /// Launches every system without dependencies, starts each dependent as soon as all the
        /// systems it waits for are done, blocks until all finished, then applies the command buffers
        /// in registration order (single-threaded).
        ///
        /// @param deltaTime Frame time in seconds, exposed as SystemContext::DeltaTime().
        /// @throws Rethrows the first exception thrown by a system, *after* the command buffers were
        ///         applied. A throwing system does not cancel its dependents.
        /// @note No-op when no system is registered. If applying a command buffer itself throws,
        ///       that exception propagates instead.
        /// @thread_safety Call from one thread only; the calling thread blocks and does not run systems.
        void Run(float deltaTime) {
            if (m_dirty)
                Build();
            if (m_nodes.empty())
                return;

            RunState state(m_nodes.size(), deltaTime);
            for (std::size_t i = 0; i < m_nodes.size(); ++i)
                state.remaining[i].store(m_nodes[i].dependencyCount, std::memory_order_relaxed);
            for (std::size_t i = 0; i < m_nodes.size(); ++i)
                if (m_nodes[i].dependencyCount == 0)
                    Launch(i, state);

            {
                std::unique_lock lock(state.mutex);
                state.done.wait(lock, [&] { return state.finished == m_nodes.size(); });
            }

            for (auto& node : m_nodes)
                node.commands.Apply(m_world);

            if (state.error)
                std::rethrow_exception(state.error);
        }

        /// @brief Human-readable dependency graph for reports and debugging.
        /// @return One line per system: `[index] name  -> blocks: <systems that must wait for it>`.
        /// @note Builds the graph first if systems were added since the last build.
        /// Report / debugging: which system waits for which.
        std::string DescribeGraph() {
            if (m_dirty)
                Build();

            std::string out;
            for (std::size_t i = 0; i < m_nodes.size(); ++i) {
                out += std::format("[{}] {}", i, m_nodes[i].name);
                if (m_nodes[i].dependents.empty()) {
                    out += '\n';
                    continue;
                }
                out += "  -> blocks:";
                for (const auto d : m_nodes[i].dependents)
                    out += std::format(" {}", m_nodes[d].name);
                out += '\n';
            }
            return out;
        }

        /// @brief Number of worker threads. @return Pool size.
        [[nodiscard]] std::size_t ThreadCount() const {
            return m_pool.Size();
        }

    private:
        /// @brief Registered system with its access, buffer and dependency edges.
        struct Node {
            std::string name;
            Access access;
            SystemFn fn;
            CommandBuffer commands;
            std::vector<std::size_t> dependents;
            std::size_t dependencyCount = 0;
        };

        /// @brief Per-frame synchronisation state (pending-dependency counters, completion count, first error).
        struct RunState {
            RunState(std::size_t count, float dt)
                : remaining(std::make_unique<std::atomic<std::size_t>[]>(count)),
                  deltaTime(dt) {}

            std::unique_ptr<std::atomic<std::size_t>[]> remaining;
            float deltaTime;
            std::mutex mutex; // guards finished + error
            std::condition_variable done;
            std::size_t finished = 0;
            std::exception_ptr error;
        };

        /// @brief (Re)computes the dependency graph.
        ///
        /// Creates the declared pools, then adds an edge i -> j (i < j) for every pair whose accesses
        /// conflict, so j waits for i. Cost is O(n^2) conflict checks in the number of systems.
        void Build() {
            Registry& registry = m_world.GetRegistry();

            for (auto& node : m_nodes) {
                node.dependents.clear();
                node.dependencyCount = 0;
                node.access.EnsurePools(registry);
            }

            for (std::size_t j = 0; j < m_nodes.size(); ++j) {
                for (std::size_t i = 0; i < j; ++i) {
                    if (m_nodes[i].access.ConflictsWith(m_nodes[j].access)) {
                        m_nodes[i].dependents.push_back(j);
                        ++m_nodes[j].dependencyCount;
                    }
                }
            }
            m_dirty = false;
        }

        /// @brief Submits one system to the pool.
        ///
        /// The task runs the system (catching any exception into @c state.error), releases its
        /// dependents (launching those whose counter reaches 0), then signals completion under the lock.
        ///
        /// @param index System index in the node list.
        /// @param state State of the running frame.
        void Launch(std::size_t index, RunState& state) {
            m_pool.Submit([this, index, &state] {
                Node& node = m_nodes[index];
                std::exception_ptr error;

                try {
                    SystemContext context(m_world.GetRegistry(), m_world, node.access, node.commands, m_pool,
                                          state.deltaTime);
                    node.fn(context);
                } catch (...) {
                    error = std::current_exception();
                }

                for (const auto dependent : node.dependents)
                    if (state.remaining[dependent].fetch_sub(1, std::memory_order_acq_rel) == 1)
                        Launch(dependent, state);

                std::scoped_lock lock(state.mutex);
                if (error && !state.error)
                    state.error = error;
                ++state.finished;
                state.done.notify_one(); // under the lock: Run() cannot destroy state before we unlock
            });
        }

        World& m_world;
        ThreadPool m_pool;
        std::deque<Node> m_nodes; // deque: Node holds a mutex (non-movable) and must never relocate
        bool m_dirty = true;
    };
} // namespace Pumpkin::ECS
