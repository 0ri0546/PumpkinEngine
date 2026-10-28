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

    // What a system reads / writes. Two systems conflict when one writes something the
    // other reads or writes. Write implies read
    class Access {
    public:
        template <Component... Ts>
        Access& Read() {
            (AddRead<Ts>(), ...);
            return *this;
        }

        template <Component... Ts>
        Access& Write() {
            (AddWrite<Ts>(), ...);
            return *this;
        }

        bool CanRead(ComponentId id) const {
            return std::ranges::contains(m_reads, id) || std::ranges::contains(m_writes, id);
        }

        bool CanWrite(ComponentId id) const {
            return std::ranges::contains(m_writes, id);
        }

        bool ConflictsWith(const Access& other) const {
            const auto overlap = [](const auto& a, const auto& b) {
                return std::ranges::any_of(a, [&](ComponentId id) { return std::ranges::contains(b, id); });
            };
            return overlap(m_writes, other.m_writes) || overlap(m_writes, other.m_reads) ||
                   overlap(m_reads, other.m_writes);
        }

        // Creates every declared pool up front, single-threaded, so that Registry::Pool<T>()
        // never has to resize m_pools / allocate while systems run.
        void EnsurePools(Registry& registry) const {
            for (auto ensure : m_ensure)
                ensure(registry);
        }

    private:
        template <Component T>
        void AddRead() {
            m_reads.push_back(ComponentTypeId<T>());
            m_ensure.push_back([](Registry& registry) { (void)registry.Pool<T>(); });
        }

        template <Component T>
        void AddWrite() {
            m_writes.push_back(ComponentTypeId<T>());
            m_ensure.push_back([](Registry& registry) { (void)registry.Pool<T>(); });
        }

        std::vector<ComponentId> m_reads;
        std::vector<ComponentId> m_writes;
        std::vector<void(*)(Registry&)> m_ensure;
    };

    // What a system sees. Every component access is checked against the declared Access
    class SystemContext {
    public:
        SystemContext(Registry& registry, const World& world, const Access& access, CommandBuffer& commands,
                      ThreadPool& pool, float deltaTime)
            : m_registry(registry),
              m_world(world),
              m_access(access),
              m_commands(commands),
              m_pool(pool),
              m_deltaTime(deltaTime) {}

        float DeltaTime() const {
            return m_deltaTime;
        }

        bool IsAlive(Entity entity) const {
            return m_world.IsAlive(entity);
        }

        CommandBuffer& Commands() {
            return m_commands;
        }

        template <Component T>
        const ComponentPool<T>& Read() const {
            Require(m_access.CanRead(ComponentTypeId<T>()), "reading", typeid(T).name());
            return std::as_const(m_registry).Pool<T>();
        }

        template <Component T>
        ComponentPool<T>& Write() {
            Require(m_access.CanWrite(ComponentTypeId<T>()), "writing", typeid(T).name());
            return m_registry.Pool<T>();
        }

        // Sequential multi-component view. Every component must be declared (read or write).
        template <Component... Ts>
        Registry::QueryView<Ts...> View() {
            (Require(m_access.CanRead(ComponentTypeId<Ts>()), "viewing", typeid(Ts).name()), ...);
            return m_registry.View<Ts...>();
        }

        // Parallel iteration: fn(Entity, Ts&...) is called once per matching entity, from
        // several threads. Each entity belongs to exactly one chunk, so touching *that entity's*
        // components is race-free. fn must not touch other entities' components (unless
        // read-only), and must not mutate shared captured state without synchronisation.
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

    // Scheduler: systems run in registration order *semantically* (a later system that
    // conflicts with an earlier one waits for it), and in parallel whenever access allows
    class Scheduler {
    public:
        using SystemFn = std::function<void(SystemContext&)>;

        explicit Scheduler(World& world, std::size_t threadCount = std::thread::hardware_concurrency())
            : m_world(world),
              m_pool(threadCount) {}

        template <std::invocable<SystemContext&> F>
        void AddSystem(std::string name, Access access, F&& fn) {
            auto& node = m_nodes.emplace_back();
            node.name = std::move(name);
            node.access = std::move(access);
            node.fn = std::forward<F>(fn);
            m_dirty = true;
        }

        // Runs one frame. Blocks until every system finished, then applies command buffers
        // in registration order. Rethrows the first exception thrown by a system.
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

        // Report / debugging: which system waits for which.
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

        [[nodiscard]] std::size_t ThreadCount() const {
            return m_pool.Size();
        }

    private:
        struct Node {
            std::string name;
            Access access;
            SystemFn fn;
            CommandBuffer commands;
            std::vector<std::size_t> dependents;
            std::size_t dependencyCount = 0;
        };

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

        // Edge i -> j (i < j) when their accesses conflict: j must wait for i.
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
