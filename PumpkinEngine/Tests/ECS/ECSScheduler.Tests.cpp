// Multithreading tests: ThreadPool, CommandBuffer, Access, Scheduler and concurrent World reads.

#include <gtest/gtest.h>

import Pumpkin.ECS;

import std;

using namespace Pumpkin::ECS;
using namespace std::chrono_literals;

namespace {
    struct Position {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Velocity {
        float dx = 0.0f;
        float dy = 0.0f;
    };

    struct Health {
        std::int32_t value = 100;

        Health() = default;
        explicit Health(std::int32_t v) : value(v) {}
    };

    struct Hits {
        std::int32_t count = 0;
    };

    struct Marker {
        std::int32_t value = 0;
    };

    // Distinct component types, used to race ComponentTypeId initialisation.
    template <std::size_t N>
    struct Probe {
        std::int32_t value = 0;
    };

    constexpr float kDt = 0.5f; // exactly representable, keeps float expectations exact

    template <typename Pred>
    bool WaitFor(Pred&& pred, std::chrono::milliseconds timeout = 5000ms) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!pred()) {
            if (std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::yield();
        }
        return true;
    }

    // Holds a thread "busy" for about `duration` without sleeping. On Windows sleep_for() rounds up
    // to the ~15.6 ms timer tick, which made three of these tests take ~1.9 s for nothing.
    inline void BusyWait(std::chrono::microseconds duration) {
        const auto end = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < end)
            std::this_thread::yield();
    }

    class SchedulerTest : public ::testing::Test {
    protected:
        World m_World;

        std::vector<Entity> CreatePositions(std::size_t count) {
            std::vector<Entity> ids;
            ids.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                Entity e = m_World.CreateEntity();
                m_World.Add<Position>(e, static_cast<float>(i), 0.0f);
                ids.push_back(e);
            }
            return ids;
        }
    };

    // ---------------------------------------------------------------------------------
    // Determinism helpers: the same simulation scheduled with different worker counts
    // ---------------------------------------------------------------------------------
    void StepGravity(Velocity& v, float dt) {
        v.dy -= 9.5f * dt;
    }
    void StepMove(Position& p, const Velocity& v, float dt) {
        p.x += v.dx * dt;
        p.y += v.dy * dt;
    }
    void StepBurn(Health& h) {
        h.value -= 1;
    }

    std::vector<Entity> PopulateSimulation(World& world, std::size_t count) {
        std::vector<Entity> ids;
        ids.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const float f = static_cast<float>(i % 512) * 0.25f;
            Entity e = world.CreateEntity();
            world.Add<Position>(e, f, -f);
            world.Add<Velocity>(e, 1.0f + f, 0.5f * f);
            world.Add<Health>(e, (i % 100 == 0) ? 3 : 1000); // 1% die after 3 frames
            ids.push_back(e);
        }
        return ids;
    }

    std::vector<float> Snapshot(World& world, const std::vector<Entity>& ids) {
        std::vector<float> out;
        out.reserve(ids.size() * 3);
        for (Entity e : ids) {
            if (!world.IsAlive(e)) {
                out.insert(out.end(), {-1.0f, -1.0f, -1.0f});
                continue;
            }
            const auto& p = world.Get<Position>(e);
            out.push_back(p.x);
            out.push_back(p.y);
            out.push_back(static_cast<float>(world.Get<Health>(e).value));
        }
        return out;
    }

    std::vector<float> RunScheduledSimulation(std::size_t workers, std::size_t count, int frames) {
        World world;
        const auto ids = PopulateSimulation(world, count);

        Scheduler scheduler(world, workers);
        scheduler.AddSystem("Gravity", Access{}.Write<Velocity>(), [](SystemContext& ctx) {
            ctx.ParallelForEach<Velocity>([dt = ctx.DeltaTime()](Entity, Velocity& v) { StepGravity(v, dt); }, 256);
        });
        scheduler.AddSystem("Movement", Access{}.Read<Velocity>().Write<Position>(), [](SystemContext& ctx) {
            ctx.ParallelForEach<Position, Velocity>(
                [dt = ctx.DeltaTime()](Entity, Position& p, const Velocity& v) { StepMove(p, v, dt); }, 256);
        });
        scheduler.AddSystem("Burn", Access{}.Write<Health>(), [](SystemContext& ctx) {
            ctx.ParallelForEach<Health>([](Entity, Health& h) { StepBurn(h); }, 256);
        });
        scheduler.AddSystem("Reaper", Access{}.Read<Health>(), [](SystemContext& ctx) {
            ctx.ParallelForEach<Health>(
                [&ctx](Entity e, const Health& h) {
                    if (h.value <= 0)
                        ctx.Commands().Destroy(e);
                },
                256);
        });

        for (int i = 0; i < frames; ++i)
            scheduler.Run(kDt);

        return Snapshot(world, ids);
    }

    std::vector<float> RunSequentialSimulation(std::size_t count, int frames) {
        World world;
        const auto ids = PopulateSimulation(world, count);

        for (int i = 0; i < frames; ++i) {
            for ([[maybe_unused]] auto [e, v] : world.View<Velocity>())
                StepGravity(v, kDt);
            for ([[maybe_unused]] auto [e, p, v] : world.View<Position, Velocity>())
                StepMove(p, v, kDt);
            for ([[maybe_unused]] auto [e, h] : world.View<Health>())
                StepBurn(h);

            std::vector<Entity> dead;
            for (auto [e, h] : world.View<Health>())
                if (h.value <= 0)
                    dead.push_back(e);
            for (Entity e : dead)
                world.DestroyEntity(e);
        }
        return Snapshot(world, ids);
    }

    template <std::size_t... Is>
    std::array<ComponentId, sizeof...(Is)> CollectIds(std::index_sequence<Is...>) {
        return {ComponentTypeId<Probe<Is>>()...};
    }
} // namespace

// =====================================================================
// ThreadPool
// =====================================================================

TEST(ThreadPoolTest, SizeIsAtLeastOneAndRespectsTheRequest) {
    ThreadPool zero(0);
    ThreadPool three(3);

    EXPECT_GE(zero.Size(), 1u);
    EXPECT_EQ(three.Size(), 3u);
}

TEST(ThreadPoolTest, EverySubmittedTaskRuns) {
    constexpr std::int32_t count = 1000;
    std::atomic<std::int32_t> counter{0}; // declared before the pool: outlives its workers
    ThreadPool pool(4);

    for (std::int32_t i = 0; i < count; ++i)
        pool.Submit([&counter] { ++counter; });

    EXPECT_TRUE(WaitFor([&] { return counter.load() == count; }));
}

TEST(ThreadPoolTest, TasksRunOnWorkerThreadsNotOnTheCaller) {
    std::atomic<bool> done{false};
    std::thread::id workerId;
    ThreadPool pool(2);

    pool.Submit([&] {
        workerId = std::this_thread::get_id();
        done = true;
    });

    ASSERT_TRUE(WaitFor([&] { return done.load(); }));
    EXPECT_NE(workerId, std::this_thread::get_id());
}

TEST(ThreadPoolTest, DestructorDrainsTasksStillQueued) {
    std::atomic<std::int32_t> counter{0};
    {
        ThreadPool pool(2);
        for (int i = 0; i < 200; ++i)
            pool.Submit([&counter] {
                BusyWait(50us);
                ++counter;
            });
    }
    EXPECT_EQ(counter.load(), 200);
}

TEST(ThreadPoolTest, ConcurrentSubmittersAreSafe) {
    constexpr std::int32_t perThread = 1000;
    constexpr std::int32_t threads = 8;
    std::atomic<std::int32_t> counter{0};
    ThreadPool pool(4);
    {
        std::vector<std::jthread> submitters;
        for (std::int32_t t = 0; t < threads; ++t)
            submitters.emplace_back([&] {
                for (std::int32_t i = 0; i < perThread; ++i)
                    pool.Submit([&counter] { ++counter; });
            });
    }
    EXPECT_TRUE(WaitFor([&] { return counter.load() == perThread * threads; }));
}

TEST(ThreadPoolTest, ParallelForVisitsEveryIndexExactlyOnce) {
    struct Case {
        std::size_t count;
        std::size_t grain;
    };
    const std::array cases{Case{0, 16},   Case{1, 16},     Case{15, 16},     Case{16, 16}, Case{17, 16},
                           Case{1000, 1}, Case{1000, 7},   Case{1000, 1000}, Case{1000, 100'000},
                           Case{12'345, 0}}; // grain 0 is treated as 1
    ThreadPool pool(4);

    for (const Case& c : cases) {
        SCOPED_TRACE(std::format("count={} grain={}", c.count, c.grain));
        std::vector<std::atomic<std::int32_t>> hits(c.count);

        pool.ParallelFor(c.count, c.grain, [&](std::size_t begin, std::size_t end) {
            for (std::size_t i = begin; i < end; ++i)
                hits[i].fetch_add(1);
        });

        const auto wrong = std::ranges::count_if(hits, [](const auto& h) { return h.load() != 1; });
        EXPECT_EQ(wrong, 0);
    }
}

TEST(ThreadPoolTest, ParallelForChunksStayInRangeAndTileTheRange) {
    constexpr std::size_t count = 10'007;
    constexpr std::size_t grain = 100;
    std::mutex mutex;
    std::vector<std::pair<std::size_t, std::size_t>> chunks;
    ThreadPool pool(4);

    pool.ParallelFor(count, grain, [&](std::size_t begin, std::size_t end) {
        std::scoped_lock lock(mutex);
        chunks.emplace_back(begin, end);
    });

    std::ranges::sort(chunks);
    std::size_t expectedBegin = 0;
    for (const auto& [begin, end] : chunks) {
        EXPECT_EQ(begin, expectedBegin);
        EXPECT_LT(begin, end);
        EXPECT_LE(end - begin, grain);
        expectedBegin = end;
    }
    EXPECT_EQ(expectedBegin, count);
}

TEST(ThreadPoolTest, ParallelForComputesTheCorrectSum) {
    constexpr std::uint64_t count = 1'000'000;
    std::atomic<std::uint64_t> total{0};
    ThreadPool pool(4);

    pool.ParallelFor(count, 1000, [&](std::size_t begin, std::size_t end) {
        std::uint64_t local = 0;
        for (std::size_t i = begin; i < end; ++i)
            local += i;
        total += local;
    });

    EXPECT_EQ(total.load(), count * (count - 1) / 2);
}

TEST(ThreadPoolTest, ParallelForWorksWithASingleWorker) {
    std::atomic<std::int64_t> total{0};
    ThreadPool pool(1);

    pool.ParallelFor(5000, 10, [&](std::size_t begin, std::size_t end) {
        total += static_cast<std::int64_t>(end - begin);
    });

    EXPECT_EQ(total.load(), 5000);
}

TEST(ThreadPoolTest, ParallelForFromInsideTheOnlyWorkerDoesNotDeadlock) {
    std::atomic<std::int64_t> total{0};
    std::promise<void> finished;
    auto future = finished.get_future();
    ThreadPool pool(1); // worst case: the caller is the only thread that can make progress

    pool.Submit([&] {
        pool.ParallelFor(10'000, 10, [&](std::size_t begin, std::size_t end) {
            total += static_cast<std::int64_t>(end - begin);
        });
        finished.set_value();
    });

    ASSERT_EQ(future.wait_for(10s), std::future_status::ready);
    EXPECT_EQ(total.load(), 10'000);
}

TEST(ThreadPoolTest, NestedParallelForFromMoreTasksThanWorkersDoesNotDeadlock) {
    constexpr std::int32_t tasks = 8;
    std::atomic<std::int32_t> finished{0};
    std::atomic<std::int64_t> total{0};
    ThreadPool pool(2);

    for (std::int32_t t = 0; t < tasks; ++t)
        pool.Submit([&] {
            pool.ParallelFor(5000, 50, [&](std::size_t begin, std::size_t end) {
                total += static_cast<std::int64_t>(end - begin);
            });
            ++finished;
        });

    ASSERT_TRUE(WaitFor([&] { return finished.load() == tasks; }, 10'000ms));
    EXPECT_EQ(total.load(), static_cast<std::int64_t>(tasks) * 5000);
}

// =====================================================================
// CommandBuffer
// =====================================================================

class CommandBufferTest : public ::testing::Test {
protected:
    World m_World;
    CommandBuffer m_Commands;
};

TEST_F(CommandBufferTest, NothingHappensUntilApply) {
    Entity e = m_World.CreateEntity();

    m_Commands.Destroy(e);
    EXPECT_TRUE(m_World.IsAlive(e));

    m_Commands.Apply(m_World);
    EXPECT_FALSE(m_World.IsAlive(e));
}

TEST_F(CommandBufferTest, ApplyOnAnEmptyBufferIsSafe) {
    EXPECT_NO_THROW(m_Commands.Apply(m_World));
}

TEST_F(CommandBufferTest, ApplyConsumesTheCommands) {
    m_Commands.Spawn([](World& world, Entity e) { world.Add<Marker>(e); });

    m_Commands.Apply(m_World);
    m_Commands.Apply(m_World);

    EXPECT_EQ(m_World.Query<Marker>().size(), 1u);
}

TEST_F(CommandBufferTest, AddForwardsConstructorArguments) {
    Entity e = m_World.CreateEntity();

    m_Commands.Add<Health>(e, 7);
    m_Commands.Add<Position>(e, 1.0f, 2.0f);
    m_Commands.Apply(m_World);

    EXPECT_EQ(m_World.Get<Health>(e).value, 7);
    EXPECT_FLOAT_EQ(m_World.Get<Position>(e).y, 2.0f);
}

TEST_F(CommandBufferTest, CommandsRunInRecordingOrder) {
    Entity addedThenRemoved = m_World.CreateEntity();
    Entity removedThenAdded = m_World.CreateEntity();
    m_World.Add<Health>(removedThenAdded, 1);

    m_Commands.Add<Health>(addedThenRemoved, 5);
    m_Commands.Remove<Health>(addedThenRemoved);
    m_Commands.Remove<Health>(removedThenAdded);
    m_Commands.Add<Health>(removedThenAdded, 9);
    m_Commands.Apply(m_World);

    EXPECT_FALSE(m_World.Has<Health>(addedThenRemoved));
    ASSERT_TRUE(m_World.Has<Health>(removedThenAdded));
    EXPECT_EQ(m_World.Get<Health>(removedThenAdded).value, 9);
}

TEST_F(CommandBufferTest, CommandsOnAnEntityDestroyedEarlierInTheBufferAreSkipped) {
    Entity e = m_World.CreateEntity();

    m_Commands.Destroy(e);
    m_Commands.Add<Position>(e, 1.0f, 1.0f); // would throw if applied to a dead entity
    m_Commands.Remove<Position>(e);

    EXPECT_NO_THROW(m_Commands.Apply(m_World));
    EXPECT_FALSE(m_World.IsAlive(e));
}

TEST_F(CommandBufferTest, AddToAnEntityThatDiedBeforeApplyDoesNotThrow) {
    Entity e = m_World.CreateEntity();
    m_Commands.Add<Position>(e, 1.0f, 1.0f);

    m_World.DestroyEntity(e);

    EXPECT_NO_THROW(m_Commands.Apply(m_World));
    EXPECT_FALSE(m_World.Has<Position>(e));
}

TEST_F(CommandBufferTest, SpawnCreatesTheEntityAndHandsItToInit) {
    Entity spawned = NullEntity;

    m_Commands.Spawn([&](World& world, Entity e) {
        world.Add<Position>(e, 3.0f, 4.0f);
        spawned = e;
    });
    EXPECT_TRUE(m_World.Query<Position>().empty()); // nothing yet
    m_Commands.Apply(m_World);

    ASSERT_FALSE(spawned == NullEntity);
    EXPECT_TRUE(m_World.IsAlive(spawned));
    EXPECT_FLOAT_EQ(m_World.Get<Position>(spawned).x, 3.0f);
}

TEST_F(CommandBufferTest, ManySpawnsProduceDistinctEntities) {
    for (int i = 0; i < 100; ++i)
        m_Commands.Spawn([](World& world, Entity e) { world.Add<Marker>(e); });
    m_Commands.Apply(m_World);

    std::vector<Entity> spawned = m_World.Query<Marker>();
    std::set<std::uint32_t> indices;
    for (Entity e : spawned)
        indices.insert(e.index);

    EXPECT_EQ(spawned.size(), 100u);
    EXPECT_EQ(indices.size(), 100u);
}

TEST_F(CommandBufferTest, RecordingFromManyThreadsLosesNothing) {
    constexpr std::size_t threads = 8;
    constexpr std::size_t perThread = 500;
    {
        std::vector<std::jthread> recorders;
        for (std::size_t t = 0; t < threads; ++t)
            recorders.emplace_back([&] {
                for (std::size_t i = 0; i < perThread; ++i)
                    m_Commands.Spawn([](World& world, Entity e) { world.Add<Marker>(e); });
            });
    }

    m_Commands.Apply(m_World);

    EXPECT_EQ(m_World.Query<Marker>().size(), threads * perThread);
}

TEST_F(CommandBufferTest, ConcurrentAddsToDisjointEntitiesAreAllApplied) {
    constexpr std::size_t threads = 8;
    constexpr std::size_t perThread = 500;
    std::vector<Entity> ids;
    for (std::size_t i = 0; i < threads * perThread; ++i)
        ids.push_back(m_World.CreateEntity());
    {
        std::vector<std::jthread> recorders;
        for (std::size_t t = 0; t < threads; ++t)
            recorders.emplace_back([&, t] {
                for (std::size_t i = t * perThread; i < (t + 1) * perThread; ++i)
                    m_Commands.Add<Health>(ids[i], static_cast<std::int32_t>(i));
            });
    }

    m_Commands.Apply(m_World);

    for (std::size_t i = 0; i < ids.size(); ++i) {
        ASSERT_TRUE(m_World.Has<Health>(ids[i]));
        ASSERT_EQ(m_World.Get<Health>(ids[i]).value, static_cast<std::int32_t>(i));
    }
}

// =====================================================================
// Access
// =====================================================================

TEST(AccessTest, ReadDoesNotImplyWrite) {
    Access access;
    access.Read<Position>();

    EXPECT_TRUE(access.CanRead(ComponentTypeId<Position>()));
    EXPECT_FALSE(access.CanWrite(ComponentTypeId<Position>()));
}

TEST(AccessTest, WriteImpliesRead) {
    Access access;
    access.Write<Position>();

    EXPECT_TRUE(access.CanRead(ComponentTypeId<Position>()));
    EXPECT_TRUE(access.CanWrite(ComponentTypeId<Position>()));
}

TEST(AccessTest, UndeclaredComponentsAreDenied) {
    Access access;
    access.Read<Position>().Write<Velocity>();

    EXPECT_FALSE(access.CanRead(ComponentTypeId<Health>()));
    EXPECT_FALSE(access.CanWrite(ComponentTypeId<Health>()));
    EXPECT_FALSE(Access{}.CanRead(ComponentTypeId<Position>()));
}

TEST(AccessTest, DuplicateDeclarationsAreHarmless) {
    Access access;
    access.Read<Position, Position>().Write<Velocity>().Write<Velocity>();

    EXPECT_TRUE(access.CanRead(ComponentTypeId<Position>()));
    EXPECT_TRUE(access.CanWrite(ComponentTypeId<Velocity>()));
}

TEST(AccessTest, ConflictMatrix) {
    Access readP;
    readP.Read<Position>();
    Access readP2;
    readP2.Read<Position>();
    Access writeP;
    writeP.Write<Position>();
    Access writeP2;
    writeP2.Write<Position>();
    Access writeV;
    writeV.Write<Velocity>();
    Access readPwriteV;
    readPwriteV.Read<Position>().Write<Velocity>();
    Access empty;

    EXPECT_TRUE(writeP.ConflictsWith(writeP2));  // write / write
    EXPECT_TRUE(writeP.ConflictsWith(readP));    // write / read
    EXPECT_TRUE(readP.ConflictsWith(writeP));    // read / write
    EXPECT_FALSE(readP.ConflictsWith(readP2));   // read / read
    EXPECT_FALSE(writeP.ConflictsWith(writeV));  // disjoint
    EXPECT_TRUE(readPwriteV.ConflictsWith(writeP)); // partial overlap
    EXPECT_TRUE(readPwriteV.ConflictsWith(writeV));
    EXPECT_FALSE(empty.ConflictsWith(writeP));
    EXPECT_FALSE(empty.ConflictsWith(empty));
}

TEST(AccessTest, ConflictIsSymmetric) {
    Access a;
    a.Read<Position>().Write<Health>();
    Access b;
    b.Write<Position>();
    Access c;
    c.Read<Health>();
    Access d;
    d.Write<Velocity>();

    const std::array<const Access*, 4> all{&a, &b, &c, &d};
    for (const Access* x : all)
        for (const Access* y : all)
            EXPECT_EQ(x->ConflictsWith(*y), y->ConflictsWith(*x));
}

TEST(AccessTest, EnsurePoolsCreatesExactlyTheDeclaredPools) {
    Registry registry;
    Access access;
    access.Read<Position>().Write<Velocity>();

    EXPECT_FALSE(registry.HasPool<Position>());
    access.EnsurePools(registry);

    EXPECT_TRUE(registry.HasPool<Position>());
    EXPECT_TRUE(registry.HasPool<Velocity>());
    EXPECT_FALSE(registry.HasPool<Health>());
}

// =====================================================================
// Scheduler: basics
// =====================================================================

TEST_F(SchedulerTest, EmptySchedulerRunIsANoOp) {
    Scheduler scheduler(m_World, 2);

    EXPECT_NO_THROW(scheduler.Run(kDt));
    EXPECT_TRUE(scheduler.DescribeGraph().empty());
}

TEST_F(SchedulerTest, ThreadCountIsAtLeastOneAndRespectsTheRequest) {
    Scheduler zero(m_World, 0);
    Scheduler three(m_World, 3);

    EXPECT_GE(zero.ThreadCount(), 1u);
    EXPECT_EQ(three.ThreadCount(), 3u);
}

TEST_F(SchedulerTest, EverySystemRunsExactlyOncePerFrame) {
    std::atomic<std::int32_t> a{0};
    std::atomic<std::int32_t> b{0};
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("A", Access{}.Write<Position>(), [&](SystemContext&) { ++a; });
    scheduler.AddSystem("B", Access{}.Read<Velocity>(), [&](SystemContext&) { ++b; });

    for (int frame = 1; frame <= 50; ++frame) {
        scheduler.Run(kDt);
        ASSERT_EQ(a.load(), frame);
        ASSERT_EQ(b.load(), frame);
    }
}

TEST_F(SchedulerTest, SixtyFourIndependentSystemsAllRunEveryFrame) {
    std::array<std::atomic<std::int32_t>, 64> runs{};
    Scheduler scheduler(m_World, 4);
    for (std::size_t i = 0; i < runs.size(); ++i)
        scheduler.AddSystem(std::format("R{}", i), Access{}.Read<Position>(), [&runs, i](SystemContext&) { ++runs[i]; });

    for (int frame = 0; frame < 20; ++frame)
        scheduler.Run(kDt);

    for (const auto& count : runs)
        EXPECT_EQ(count.load(), 20);
}

TEST_F(SchedulerTest, DeltaTimeIsPassedToSystems) {
    float seen = 0.0f;
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Dt", Access{}, [&](SystemContext& ctx) { seen = ctx.DeltaTime(); });

    scheduler.Run(0.25f);

    EXPECT_FLOAT_EQ(seen, 0.25f);
}

TEST_F(SchedulerTest, DeclaredPoolsAreCreatedBeforeSystemsRun) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Reader", Access{}.Read<Velocity>(), [](SystemContext&) {});

    EXPECT_FALSE(m_World.GetRegistry().HasPool<Velocity>());
    scheduler.Run(kDt);
    EXPECT_TRUE(m_World.GetRegistry().HasPool<Velocity>());
}

TEST_F(SchedulerTest, SystemsAddedAfterTheFirstRunAreScheduledToo) {
    std::vector<std::int32_t> order;
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("First", Access{}.Write<Position>(), [&](SystemContext&) { order.push_back(0); });
    scheduler.Run(kDt);

    scheduler.AddSystem("Second", Access{}.Write<Position>(), [&](SystemContext&) { order.push_back(1); });
    order.clear();
    scheduler.Run(kDt);

    EXPECT_EQ(order, (std::vector<std::int32_t>{0, 1}));
}

TEST_F(SchedulerTest, DescribeGraphShowsWhoBlocksWhom) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Writer", Access{}.Write<Position>(), [](SystemContext&) {});
    scheduler.AddSystem("Reader", Access{}.Read<Position>(), [](SystemContext&) {});
    scheduler.AddSystem("Other", Access{}.Write<Velocity>(), [](SystemContext&) {});

    const std::string graph = scheduler.DescribeGraph();

    EXPECT_TRUE(graph.contains("Writer"));
    EXPECT_TRUE(graph.contains("Reader"));
    EXPECT_TRUE(graph.contains("Other"));
    EXPECT_TRUE(graph.contains("blocks: Reader"));
    EXPECT_FALSE(graph.contains("blocks: Other"));
}

// =====================================================================
// Scheduler: ParallelForEach
// =====================================================================

TEST_F(SchedulerTest, ParallelForEachUpdatesEveryEntity) {
    constexpr std::size_t count = 10'000;
    std::vector<Entity> ids;
    for (std::size_t i = 0; i < count; ++i) {
        Entity e = m_World.CreateEntity();
        m_World.Add<Position>(e, static_cast<float>(i), 0.0f);
        m_World.Add<Velocity>(e, 2.0f, 0.0f);
        ids.push_back(e);
    }

    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("Move", Access{}.Read<Velocity>().Write<Position>(), [](SystemContext& ctx) {
        ctx.ParallelForEach<Position, Velocity>(
            [dt = ctx.DeltaTime()](Entity, Position& p, const Velocity& v) { p.x += v.dx * dt; }, 128);
    });
    scheduler.Run(kDt);

    for (std::size_t i = 0; i < count; ++i)
        ASSERT_FLOAT_EQ(m_World.Get<Position>(ids[i]).x, static_cast<float>(i) + 1.0f);
}

TEST(SchedulerEdgeCases, ParallelForEachVisitsEachEntityExactlyOnceAcrossChunkBoundaries) {
    const std::array<std::size_t, 7> counts{0, 1, 2, 2047, 2048, 2049, 10'000};
    const std::array<std::size_t, 4> grains{1, 7, 2048, 1u << 20};

    for (const std::size_t count : counts) {
        for (const std::size_t grain : grains) {
            SCOPED_TRACE(std::format("count={} grain={}", count, grain));
            World world;
            for (std::size_t i = 0; i < count; ++i)
                world.Add<Hits>(world.CreateEntity());

            Scheduler scheduler(world, 4);
            scheduler.AddSystem("Count", Access{}.Write<Hits>(), [grain](SystemContext& ctx) {
                ctx.ParallelForEach<Hits>([](Entity, Hits& h) { ++h.count; }, grain);
            });
            scheduler.Run(kDt);

            std::size_t visited = 0;
            std::size_t wrong = 0;
            for ([[maybe_unused]] auto [e, h] : world.View<Hits>()) {
                ++visited;
                if (h.count != 1)
                    ++wrong;
            }
            EXPECT_EQ(visited, count);
            EXPECT_EQ(wrong, 0u);
        }
    }
}

TEST_F(SchedulerTest, ParallelForEachSkipsEntitiesMissingAComponent) {
    std::vector<Entity> ids = CreatePositions(1000);
    for (std::size_t i = 0; i < ids.size(); i += 10)
        m_World.Add<Velocity>(ids[i], 1.0f, 0.0f);

    std::atomic<std::int32_t> visited{0};
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("Both", Access{}.Read<Position>().Read<Velocity>(), [&](SystemContext& ctx) {
        ctx.ParallelForEach<Position, Velocity>([&](Entity, const Position&, const Velocity&) { ++visited; }, 16);
    });
    scheduler.Run(kDt);

    EXPECT_EQ(visited.load(), 100);
}

TEST_F(SchedulerTest, ParallelForEachOnAnEmptyWorldDoesNothing) {
    std::atomic<std::int32_t> visited{0};
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("Nothing", Access{}.Read<Position>(), [&](SystemContext& ctx) {
        ctx.ParallelForEach<Position>([&](Entity, const Position&) { ++visited; });
    });

    EXPECT_NO_THROW(scheduler.Run(kDt));
    EXPECT_EQ(visited.load(), 0);
}

TEST_F(SchedulerTest, ReadAndWriteAccessorsGiveDirectPoolAccess) {
    constexpr std::size_t count = 100;
    std::vector<Entity> ids;
    for (std::size_t i = 0; i < count; ++i) {
        Entity e = m_World.CreateEntity();
        m_World.Add<Position>(e, 0.0f, 0.0f);
        m_World.Add<Velocity>(e, 3.0f, 0.0f);
        ids.push_back(e);
    }

    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Pools", Access{}.Read<Velocity>().Write<Position>(), [](SystemContext& ctx) {
        const auto& velocities = ctx.Read<Velocity>();
        auto& positions = ctx.Write<Position>();
        const auto& alsoReadable = ctx.Read<Position>(); // Write implies read

        EXPECT_EQ(alsoReadable.Values().size(), positions.Values().size());
        for (std::size_t i = 0; i < positions.Values().size(); ++i)
            positions.Values()[i].x += velocities.Values()[i].dx;
    });
    scheduler.Run(kDt);

    for (Entity e : ids)
        EXPECT_FLOAT_EQ(m_World.Get<Position>(e).x, 3.0f);
}

// =====================================================================
// Scheduler: access enforcement
// =====================================================================

TEST_F(SchedulerTest, WritingAReadOnlyDeclaredComponentIsRejected) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Cheat", Access{}.Read<Position>(), [](SystemContext& ctx) { (void)ctx.Write<Position>(); });

    EXPECT_THROW(scheduler.Run(kDt), std::logic_error);
}

TEST_F(SchedulerTest, ReadingAnUndeclaredComponentIsRejected) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Cheat", Access{}.Read<Position>(), [](SystemContext& ctx) { (void)ctx.Read<Velocity>(); });

    EXPECT_THROW(scheduler.Run(kDt), std::logic_error);
}

TEST_F(SchedulerTest, ViewOfAnUndeclaredComponentIsRejected) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Cheat", Access{}.Read<Position>(), [](SystemContext& ctx) {
        for ([[maybe_unused]] auto [e, h] : ctx.View<Health>()) {
        }
    });

    EXPECT_THROW(scheduler.Run(kDt), std::logic_error);
}

TEST_F(SchedulerTest, ParallelForEachWithOneUndeclaredComponentIsRejected) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Cheat", Access{}.Read<Position>(), [](SystemContext& ctx) {
        ctx.ParallelForEach<Position, Velocity>([](Entity, const Position&, const Velocity&) {});
    });

    EXPECT_THROW(scheduler.Run(kDt), std::logic_error);
}

TEST_F(SchedulerTest, ReadingAWriteDeclaredComponentIsAllowed) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Ok", Access{}.Write<Position>(), [](SystemContext& ctx) {
        (void)ctx.Read<Position>();
        for ([[maybe_unused]] auto [e, p] : ctx.View<Position>()) {
        }
    });

    EXPECT_NO_THROW(scheduler.Run(kDt));
}

TEST_F(SchedulerTest, SchedulerStaysUsableAfterARejectedAccess) {
    std::atomic<bool> cheat{true};
    std::atomic<std::int32_t> runs{0};
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Sometimes", Access{}.Read<Position>(), [&](SystemContext& ctx) {
        if (cheat)
            (void)ctx.Write<Position>();
        ++runs;
    });

    EXPECT_THROW(scheduler.Run(kDt), std::logic_error);
    cheat = false;
    EXPECT_NO_THROW(scheduler.Run(kDt));
    EXPECT_EQ(runs.load(), 1);
}

// =====================================================================
// Scheduler: ordering and concurrency guarantees
// =====================================================================

TEST_F(SchedulerTest, ConflictingSystemsRunInRegistrationOrder) {
    constexpr std::int32_t systems = 20;
    std::vector<std::int32_t> order; // no lock: conflicting systems must never overlap
    Scheduler scheduler(m_World, 8);
    for (std::int32_t id = 0; id < systems; ++id)
        scheduler.AddSystem(std::format("S{}", id), Access{}.Write<Position>(),
                            [&order, id](SystemContext&) { order.push_back(id); });

    std::vector<std::int32_t> expected(systems);
    std::iota(expected.begin(), expected.end(), 0);

    for (int frame = 0; frame < 50; ++frame) {
        order.clear();
        scheduler.Run(kDt);
        ASSERT_EQ(order, expected) << "frame " << frame;
    }
}

TEST_F(SchedulerTest, ChainedWritersObserveEachOthersResults) {
    constexpr std::size_t count = 5000;
    std::vector<Entity> ids = CreatePositions(count);

    Scheduler scheduler(m_World, 8);
    scheduler.AddSystem("Set", Access{}.Write<Position>(), [](SystemContext& ctx) {
        ctx.ParallelForEach<Position>([](Entity, Position& p) { p.x = 1.0f; }, 64);
    });
    scheduler.AddSystem("Triple", Access{}.Write<Position>(), [](SystemContext& ctx) {
        ctx.ParallelForEach<Position>([](Entity, Position& p) { p.x *= 3.0f; }, 64);
    });
    scheduler.AddSystem("AddTwo", Access{}.Write<Position>(), [](SystemContext& ctx) {
        ctx.ParallelForEach<Position>([](Entity, Position& p) { p.x += 2.0f; }, 64);
    });

    for (int frame = 0; frame < 50; ++frame) {
        scheduler.Run(kDt);
        for (Entity e : ids)
            ASSERT_FLOAT_EQ(m_World.Get<Position>(e).x, 5.0f) << "frame " << frame; // (1 * 3) + 2
    }
}

TEST_F(SchedulerTest, ReadersOfTheSameComponentRunConcurrently) {
    std::atomic<std::int32_t> arrived{0};
    std::atomic<std::int32_t> rendezvous{0};
    const auto reader = [&](SystemContext&) {
        ++arrived;
        if (WaitFor([&] { return arrived.load() >= 2; }, 3000ms))
            ++rendezvous;
    };

    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("R1", Access{}.Read<Position>(), reader);
    scheduler.AddSystem("R2", Access{}.Read<Position>(), reader);
    scheduler.Run(kDt);

    EXPECT_EQ(rendezvous.load(), 2); // both saw the other one running
}

TEST_F(SchedulerTest, SystemsWithDisjointAccessRunConcurrently) {
    std::atomic<std::int32_t> arrived{0};
    std::atomic<std::int32_t> rendezvous{0};
    const auto body = [&](SystemContext&) {
        ++arrived;
        if (WaitFor([&] { return arrived.load() >= 2; }, 3000ms))
            ++rendezvous;
    };

    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("WritesPosition", Access{}.Write<Position>(), body);
    scheduler.AddSystem("WritesVelocity", Access{}.Write<Velocity>(), body);
    scheduler.Run(kDt);

    EXPECT_EQ(rendezvous.load(), 2);
}

TEST_F(SchedulerTest, ConflictingSystemsNeverOverlap) {
    std::atomic<std::int32_t> active{0};
    std::atomic<std::int32_t> peak{0};
    const auto body = [&](SystemContext&) {
        const std::int32_t now = ++active;
        std::int32_t seen = peak.load();
        while (now > seen && !peak.compare_exchange_weak(seen, now)) {
        }
        BusyWait(100us);
        --active;
    };

    Scheduler scheduler(m_World, 8);
    for (int i = 0; i < 4; ++i)
        scheduler.AddSystem(std::format("W{}", i), Access{}.Write<Position>(), body);

    for (int frame = 0; frame < 30; ++frame)
        scheduler.Run(kDt);

    EXPECT_EQ(peak.load(), 1);
}

TEST_F(SchedulerTest, WritersNeverOverlapWithReadersOrOtherWriters) {
    std::atomic<std::int32_t> readers{0};
    std::atomic<std::int32_t> writers{0};
    std::atomic<std::int32_t> violations{0};

    const auto reader = [&](SystemContext&) {
        ++readers;
        if (writers.load() > 0)
            ++violations;
        BusyWait(100us);
        if (writers.load() > 0)
            ++violations;
        --readers;
    };
    const auto writer = [&](SystemContext&) {
        if (++writers > 1)
            ++violations;
        if (readers.load() > 0)
            ++violations;
        BusyWait(100us);
        if (readers.load() > 0)
            ++violations;
        --writers;
    };

    Scheduler scheduler(m_World, 8);
    scheduler.AddSystem("R1", Access{}.Read<Position>(), reader);
    scheduler.AddSystem("R2", Access{}.Read<Position>(), reader);
    scheduler.AddSystem("W1", Access{}.Write<Position>(), writer);
    scheduler.AddSystem("R3", Access{}.Read<Position>(), reader);
    scheduler.AddSystem("R4", Access{}.Read<Position>(), reader);
    scheduler.AddSystem("W2", Access{}.Write<Position>(), writer);

    for (int frame = 0; frame < 30; ++frame)
        scheduler.Run(kDt);

    EXPECT_EQ(violations.load(), 0);
}

TEST_F(SchedulerTest, ASystemStartsAsSoonAsItsOwnDependenciesFinish) {
    // "Blocked" is unrelated to everything and waits for "Dependent", which only depends on
    // "Producer". A scheduler that runs systems in barrier-separated stages would deadlock here.
    std::atomic<bool> dependentRan{false};
    std::atomic<bool> blockedSawIt{false};

    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("Blocked", Access{}.Write<Velocity>(), [&](SystemContext&) {
        blockedSawIt = WaitFor([&] { return dependentRan.load(); }, 3000ms);
    });
    scheduler.AddSystem("Producer", Access{}.Write<Position>(), [](SystemContext&) {});
    scheduler.AddSystem("Dependent", Access{}.Read<Position>(), [&](SystemContext&) { dependentRan = true; });
    scheduler.Run(kDt);

    EXPECT_TRUE(blockedSawIt.load());
}

TEST_F(SchedulerTest, FewWorkersManyParallelSystemsDoNotDeadlock) {
    constexpr std::size_t count = 20'000;
    constexpr std::size_t systems = 8;
    CreatePositions(count);

    std::array<std::atomic<std::uint64_t>, systems> sums{};
    Scheduler scheduler(m_World, 2);
    for (std::size_t s = 0; s < systems; ++s)
        scheduler.AddSystem(std::format("Sum{}", s), Access{}.Read<Position>(), [&sums, s](SystemContext& ctx) {
            ctx.ParallelForEach<Position>(
                [&sums, s](Entity, const Position& p) { sums[s] += static_cast<std::uint64_t>(p.x); }, 64);
        });

    scheduler.Run(kDt);

    const std::uint64_t expected = static_cast<std::uint64_t>(count) * (count - 1) / 2;
    for (const auto& sum : sums)
        EXPECT_EQ(sum.load(), expected);
}

// =====================================================================
// Scheduler: exceptions
// =====================================================================

TEST_F(SchedulerTest, ExceptionFromASystemIsRethrownByRun) {
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Boom", Access{}, [](SystemContext&) { throw std::runtime_error("boom"); });

    try {
        scheduler.Run(kDt);
        FAIL() << "Run() should have rethrown";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "boom");
    }
}

TEST_F(SchedulerTest, OtherSystemsStillCompleteWhenOneThrows) {
    std::atomic<std::int32_t> ran{0};
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("Boom", Access{}.Write<Position>(), [](SystemContext&) { throw std::runtime_error("boom"); });
    scheduler.AddSystem("Fine", Access{}.Write<Velocity>(), [&](SystemContext&) { ++ran; });

    EXPECT_THROW(scheduler.Run(kDt), std::runtime_error);
    EXPECT_EQ(ran.load(), 1);
}

TEST_F(SchedulerTest, SchedulerRemainsUsableAfterASystemThrew) {
    std::atomic<bool> shouldThrow{true};
    std::atomic<std::int32_t> completed{0};
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Flaky", Access{}.Write<Position>(), [&](SystemContext&) {
        if (shouldThrow)
            throw std::runtime_error("first frame fails");
        ++completed;
    });

    EXPECT_THROW(scheduler.Run(kDt), std::runtime_error);
    shouldThrow = false;
    EXPECT_NO_THROW(scheduler.Run(kDt));
    EXPECT_NO_THROW(scheduler.Run(kDt));
    EXPECT_EQ(completed.load(), 2);
}

TEST_F(SchedulerTest, SeveralThrowingSystemsStillSurfaceOneException) {
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("A", Access{}.Write<Position>(), [](SystemContext&) { throw std::runtime_error("a"); });
    scheduler.AddSystem("B", Access{}.Write<Velocity>(), [](SystemContext&) { throw std::runtime_error("b"); });

    EXPECT_THROW(scheduler.Run(kDt), std::runtime_error);
}

// =====================================================================
// Scheduler: command buffers
// =====================================================================

TEST_F(SchedulerTest, DestroyIsDeferredUntilEverySystemOfTheFrameFinished) {
    constexpr std::size_t count = 1000;
    std::vector<Entity> ids = CreatePositions(count);
    for (Entity e : ids)
        m_World.Add<Health>(e, 1);
    const Entity target = ids.front();

    std::atomic<bool> recorded{false};
    std::atomic<std::size_t> seenByObserver{0};
    std::atomic<bool> targetAliveForObserver{false};

    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("Killer", Access{}.Read<Health>(), [&](SystemContext& ctx) {
        ctx.Commands().Destroy(target);
        recorded = true;
    });
    scheduler.AddSystem("Observer", Access{}.Read<Position>(), [&](SystemContext& ctx) {
        WaitFor([&] { return recorded.load(); }, 2000ms); // make the interesting interleaving likely
        std::size_t n = 0;
        for ([[maybe_unused]] auto [e, p] : ctx.View<Position>())
            ++n;
        seenByObserver = n;
        targetAliveForObserver = ctx.IsAlive(target);
    });
    scheduler.Run(kDt);

    EXPECT_EQ(seenByObserver.load(), count);
    EXPECT_TRUE(targetAliveForObserver.load());
    EXPECT_FALSE(m_World.IsAlive(target)); // applied once the frame is over
}

TEST_F(SchedulerTest, CommandsAreAppliedInSystemRegistrationOrder) {
    Entity fromA = NullEntity;
    Entity fromB = NullEntity;
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("A", Access{}.Read<Health>(), [&](SystemContext& ctx) {
        ctx.Commands().Spawn([&](World&, Entity e) { fromA = e; });
    });
    scheduler.AddSystem("B", Access{}.Read<Position>(), [&](SystemContext& ctx) {
        ctx.Commands().Spawn([&](World&, Entity e) { fromB = e; });
    });

    for (int frame = 0; frame < 50; ++frame) {
        scheduler.Run(kDt); // A and B run in parallel, in any order...
        ASSERT_LT(fromA.index, fromB.index) << "frame " << frame; // ...but A's commands apply first
    }
}

TEST_F(SchedulerTest, ParallelRecordedDestroysAreAllApplied) {
    constexpr std::size_t count = 20'000;
    std::vector<Entity> ids;
    for (std::size_t i = 0; i < count; ++i) {
        Entity e = m_World.CreateEntity();
        m_World.Add<Health>(e, (i % 2 == 0) ? 0 : 10);
        ids.push_back(e);
    }

    Scheduler scheduler(m_World, 8);
    scheduler.AddSystem("Reaper", Access{}.Read<Health>(), [](SystemContext& ctx) {
        ctx.ParallelForEach<Health>(
            [&ctx](Entity e, const Health& h) {
                if (h.value <= 0)
                    ctx.Commands().Destroy(e);
            },
            64);
    });
    scheduler.Run(kDt);

    for (std::size_t i = 0; i < count; ++i)
        ASSERT_EQ(m_World.IsAlive(ids[i]), i % 2 != 0) << "entity " << i;
}

TEST_F(SchedulerTest, SpawnedEntitiesAppearAfterTheFrameAndOnlyOnce) {
    bool firstFrame = true;
    std::atomic<std::size_t> seenDuringFrame{99};
    Scheduler scheduler(m_World, 2);
    scheduler.AddSystem("Spawner", Access{}.Read<Marker>(), [&](SystemContext& ctx) {
        if (!firstFrame)
            return;
        for (int i = 0; i < 100; ++i)
            ctx.Commands().Spawn([](World& world, Entity e) { world.Add<Position>(e, 1.0f, 1.0f); });
        std::size_t n = 0;
        for ([[maybe_unused]] auto [e, m] : ctx.View<Marker>())
            ++n;
        seenDuringFrame = n;
    });

    scheduler.Run(kDt);
    EXPECT_EQ(m_World.Query<Position>().size(), 100u);

    firstFrame = false;
    scheduler.Run(kDt);
    EXPECT_EQ(m_World.Query<Position>().size(), 100u); // buffer was cleared, nothing replayed
    EXPECT_EQ(seenDuringFrame.load(), 0u);
}

TEST_F(SchedulerTest, StructuralChangesBecomeVisibleOnlyInTheNextFrame) {
    constexpr std::size_t count = 2000;
    std::vector<Entity> ids = CreatePositions(count);

    std::int32_t frame = 0;
    std::atomic<std::size_t> seen{0};
    Scheduler scheduler(m_World, 4);
    scheduler.AddSystem("Reader", Access{}.Read<Position>().Read<Velocity>(), [&](SystemContext& ctx) {
        std::atomic<std::size_t> n{0};
        ctx.ParallelForEach<Position, Velocity>([&n](Entity, const Position&, const Velocity&) { ++n; }, 128);
        seen = n.load();
    });
    scheduler.AddSystem("Mutator", Access{}, [&](SystemContext& ctx) { // only records commands
        for (Entity e : ids) {
            if (frame % 2 == 0)
                ctx.Commands().Add<Velocity>(e, 1.0f, 1.0f);
            else
                ctx.Commands().Remove<Velocity>(e);
        }
    });

    for (frame = 0; frame < 10; ++frame) {
        scheduler.Run(kDt);
        // the reader saw the state left by the previous frame's commands
        EXPECT_EQ(seen.load(), frame % 2 == 1 ? count : 0u) << "frame " << frame;
    }
}

// =====================================================================
// Determinism
// =====================================================================

TEST(SchedulerDeterminism, ResultDoesNotDependOnTheNumberOfWorkers) {
    constexpr std::size_t count = 20'000;
    constexpr int frames = 20;

    const std::vector<float> oneWorker = RunScheduledSimulation(1, count, frames);
    const std::vector<float> twoWorkers = RunScheduledSimulation(2, count, frames);
    const std::vector<float> eightWorkers = RunScheduledSimulation(8, count, frames);

    EXPECT_TRUE(oneWorker == twoWorkers);
    EXPECT_TRUE(oneWorker == eightWorkers);
}

TEST(SchedulerDeterminism, ScheduledRunMatchesPlainSequentialLoops) {
    constexpr std::size_t count = 20'000;
    constexpr int frames = 20;

    const std::vector<float> scheduled = RunScheduledSimulation(8, count, frames);
    const std::vector<float> sequential = RunSequentialSimulation(count, frames);

    ASSERT_EQ(scheduled.size(), sequential.size());
    std::size_t different = 0;
    for (std::size_t i = 0; i < scheduled.size(); ++i)
        if (std::abs(scheduled[i] - sequential[i]) > 1e-3f)
            ++different;
    EXPECT_EQ(different, 0u);
}

TEST(SchedulerDeterminism, RepeatedRunsAreStable) {
    const std::vector<float> reference = RunScheduledSimulation(8, 5000, 10);
    for (int i = 0; i < 10; ++i)
        ASSERT_TRUE(RunScheduledSimulation(8, 5000, 10) == reference) << "run " << i;
}

// =====================================================================
// World read-only concurrency and component id registration
// =====================================================================

TEST(WorldConcurrencyTest, ConcurrentReadersOfAConstWorldSeeConsistentData) {
    constexpr std::size_t count = 20'000;
    World world;
    std::vector<Entity> ids;
    for (std::size_t i = 0; i < count; ++i) {
        Entity e = world.CreateEntity();
        world.Add<Position>(e, static_cast<float>(i), 0.0f);
        world.Add<Health>(e, static_cast<std::int32_t>(i));
        ids.push_back(e);
    }

    const World& readonly = world;
    std::atomic<std::size_t> errors{0};
    {
        std::vector<std::jthread> readers;
        for (int t = 0; t < 8; ++t)
            readers.emplace_back([&] {
                for (int round = 0; round < 5; ++round)
                    for (std::size_t i = 0; i < count; ++i) {
                        const Entity e = ids[i];
                        if (!readonly.IsAlive(e) || !readonly.Has<Position>(e))
                            ++errors;
                        if (readonly.Get<Position>(e).x != static_cast<float>(i))
                            ++errors;
                        const Health* h = readonly.TryGet<Health>(e);
                        if (h == nullptr || h->value != static_cast<std::int32_t>(i))
                            ++errors;
                    }
            });
    }

    EXPECT_EQ(errors.load(), 0u);
}

TEST(WorldConcurrencyTest, ConcurrentViewIterationOverExistingPoolsIsSafe) {
    constexpr std::size_t count = 10'000;
    World world;
    for (std::size_t i = 0; i < count; ++i) {
        Entity e = world.CreateEntity();
        world.Add<Position>(e, 1.0f, 0.0f);
        world.Add<Velocity>(e, 2.0f, 0.0f);
    }

    std::atomic<std::size_t> errors{0};
    {
        std::vector<std::jthread> readers;
        for (int t = 0; t < 8; ++t)
            readers.emplace_back([&] {
                std::size_t visited = 0;
                for ([[maybe_unused]] auto [e, p, v] : world.View<Position, Velocity>()) {
                    if (p.x != 1.0f || v.dx != 2.0f)
                        ++errors;
                    ++visited;
                }
                if (visited != count)
                    ++errors;
            });
    }

    EXPECT_EQ(errors.load(), 0u);
}

TEST(ComponentIdTest, IdsAreUniqueAndConsistentWhenFirstUsedFromManyThreads) {
    constexpr std::size_t types = 32;
    constexpr std::size_t threads = 8;

    std::array<std::array<ComponentId, types>, threads> results{};
    std::latch gate(1);
    {
        std::vector<std::jthread> workers;
        for (std::size_t t = 0; t < threads; ++t)
            workers.emplace_back([&, t] {
                gate.wait(); // release every thread at once to maximise the race
                results[t] = CollectIds(std::make_index_sequence<types>{});
            });
        gate.count_down();
    }

    const std::set<ComponentId> unique(results[0].begin(), results[0].end());
    EXPECT_EQ(unique.size(), types); // no two component types share an id
    for (std::size_t t = 1; t < threads; ++t)
        EXPECT_TRUE(results[t] == results[0]) << "thread " << t << " saw different ids";
}
