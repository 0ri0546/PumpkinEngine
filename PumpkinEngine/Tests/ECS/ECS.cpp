#include <gtest/gtest.h>

import Pumpkin.ECS;

import std;

using namespace Pumpkin::ECS;

namespace {
    bool Contains(const std::vector<Entity>& entities, Entity target) {
        return std::ranges::any_of(entities, [&](Entity e) { return e == target; });
    }

    class ECSTest : public ::testing::Test {
    protected:
        World m_World;

        void SetUp() override {
            m_World.Clear();
        }

        void TearDown() override {
            m_World.Clear();
        }
    };

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

    struct Tag {};

    // Heap-owning component: exercises the move paths of the sparse set (swap-and-pop).
    struct Name {
        std::string value;

        Name() = default;
        explicit Name(std::string v) : value(std::move(v)) {}
    };

    // Move-only component.
    struct Owned {
        std::unique_ptr<std::int32_t> ptr;

        explicit Owned(std::int32_t v) : ptr(std::make_unique<std::int32_t>(v)) {}
    };

    // Counts live instances to prove every constructed component is destroyed exactly once.
    struct Tracked {
        static inline std::int32_t live = 0;
        std::int32_t id;

        explicit Tracked(std::int32_t i) : id(i) {
            ++live;
        }
        Tracked(Tracked&& other) noexcept : id(other.id) {
            ++live;
        }
        Tracked& operator=(Tracked&& other) noexcept {
            id = other.id;
            return *this;
        }
        ~Tracked() {
            --live;
        }
    };

    // Only used by the DISABLED_ const-Get test (needs two component types with ordered ids).
    struct NullSlotLow {
        std::int32_t value = 0;
    };
    struct NullSlotHigh {
        std::int32_t value = 0;
    };
} // namespace

// ---------------------------------------------------------------------
// Entity lifecycle
// ---------------------------------------------------------------------

TEST_F(ECSTest, CreateEntityReturnsAliveEntity) {
    Entity e = m_World.CreateEntity();
    EXPECT_TRUE(m_World.IsAlive(e));
}

TEST_F(ECSTest, CreateEntityDoesNotReturnNullEntity) {
    Entity e = m_World.CreateEntity();
    EXPECT_FALSE(e == NullEntity);
}

TEST_F(ECSTest, NullEntityIsNotAlive) {
    EXPECT_FALSE(m_World.IsAlive(NullEntity));
}

TEST_F(ECSTest, CreateEntityReturnsDistinctEntities) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    EXPECT_FALSE(a == b);
    EXPECT_TRUE(m_World.IsAlive(a));
    EXPECT_TRUE(m_World.IsAlive(b));
}

TEST_F(ECSTest, DestroyEntityMakesItNotAlive) {
    Entity e = m_World.CreateEntity();
    m_World.DestroyEntity(e);
    EXPECT_FALSE(m_World.IsAlive(e));
}

TEST_F(ECSTest, DestroyingOneEntityDoesNotAffectOthers) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    m_World.DestroyEntity(a);

    EXPECT_FALSE(m_World.IsAlive(a));
    EXPECT_TRUE(m_World.IsAlive(b));
}

TEST_F(ECSTest, RecycledEntityIndexProducesNewGeneration) {
    Entity a = m_World.CreateEntity();
    m_World.DestroyEntity(a);

    Entity b = m_World.CreateEntity();

    if (a.index == b.index) {
        EXPECT_NE(a.generation, b.generation);
    }

    EXPECT_FALSE(m_World.IsAlive(a));
    EXPECT_TRUE(m_World.IsAlive(b));
}

TEST_F(ECSTest, StaleHandleAfterDestroyDoesNotAliasNewEntityAtSameIndex) {
    Entity a = m_World.CreateEntity();
    m_World.DestroyEntity(a);
    Entity b = m_World.CreateEntity();

    EXPECT_FALSE(m_World.IsAlive(a));
    EXPECT_TRUE(m_World.IsAlive(b));
}

// ---------------------------------------------------------------------
// Add / Get / Has / Remove
// ---------------------------------------------------------------------

TEST_F(ECSTest, AddComponentMakesHasReturnTrue) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 2.0f);

    EXPECT_TRUE(m_World.Has<Position>(e));
}

TEST_F(ECSTest, HasReturnsFalseWhenComponentNotAdded) {
    Entity e = m_World.CreateEntity();
    EXPECT_FALSE(m_World.Has<Position>(e));
}

TEST_F(ECSTest, HasReturnsFalseForUnknownComponentTypeNeverAdded) {
    Entity e = m_World.CreateEntity();
    EXPECT_FALSE(m_World.Has<Position>(e));
}

TEST_F(ECSTest, AddReturnsReferenceToConstructedComponent) {
    Entity e = m_World.CreateEntity();
    Position& pos = m_World.Add<Position>(e, 3.0f, 4.0f);

    EXPECT_FLOAT_EQ(pos.x, 3.0f);
    EXPECT_FLOAT_EQ(pos.y, 4.0f);
}

TEST_F(ECSTest, AddAggregateComponentWithBraceStyleArgs) {
    Entity e = m_World.CreateEntity();
    Velocity& vel = m_World.Add<Velocity>(e, 1.5f, -2.5f);

    EXPECT_FLOAT_EQ(vel.dx, 1.5f);
    EXPECT_FLOAT_EQ(vel.dy, -2.5f);
}

TEST_F(ECSTest, GetReturnsPreviouslyAddedComponent) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 5.0f, 6.0f);

    const Position& pos = m_World.Get<Position>(e);
    EXPECT_FLOAT_EQ(pos.x, 5.0f);
    EXPECT_FLOAT_EQ(pos.y, 6.0f);
}

TEST_F(ECSTest, GetAllowsMutationOfComponent) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 0.0f, 0.0f);

    m_World.Get<Position>(e).x = 42.0f;

    EXPECT_FLOAT_EQ(m_World.Get<Position>(e).x, 42.0f);
}

TEST_F(ECSTest, GetOnConstWorldReturnsComponent) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 7.0f, 8.0f);

    const World& constWorld = m_World;
    const Position& pos = constWorld.Get<Position>(e);

    EXPECT_FLOAT_EQ(pos.x, 7.0f);
    EXPECT_FLOAT_EQ(pos.y, 8.0f);
}

TEST_F(ECSTest, TryGetReturnsNullptrWhenComponentMissing) {
    Entity e = m_World.CreateEntity();
    EXPECT_EQ(m_World.TryGet<Position>(e), nullptr);
}

TEST_F(ECSTest, TryGetReturnsNullptrWhenPoolNeverCreated) {
    Entity e = m_World.CreateEntity();
    EXPECT_EQ(m_World.TryGet<Health>(e), nullptr);
}

TEST_F(ECSTest, TryGetReturnsValidPointerWhenComponentPresent) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 1.0f);

    Position* pos = m_World.TryGet<Position>(e);
    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 1.0f);
}

TEST_F(ECSTest, TryGetOnConstWorldReturnsConstPointer) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 9.0f, 9.0f);

    const World& constWorld = m_World;
    const Position* pos = constWorld.TryGet<Position>(e);

    ASSERT_NE(pos, nullptr);
    EXPECT_FLOAT_EQ(pos->x, 9.0f);
}

TEST_F(ECSTest, RemoveComponentMakesHasReturnFalse) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 1.0f);
    m_World.Remove<Position>(e);

    EXPECT_FALSE(m_World.Has<Position>(e));
    EXPECT_EQ(m_World.TryGet<Position>(e), nullptr);
}

TEST_F(ECSTest, RemovingOneComponentDoesNotAffectOtherComponents) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 1.0f);
    m_World.Add<Velocity>(e, 2.0f, 2.0f);

    m_World.Remove<Position>(e);

    EXPECT_FALSE(m_World.Has<Position>(e));
    EXPECT_TRUE(m_World.Has<Velocity>(e));
}

TEST_F(ECSTest, MultipleComponentTypesOnSameEntityAreIndependent) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 2.0f);
    m_World.Add<Velocity>(e, 3.0f, 4.0f);
    m_World.Add<Health>(e, 50);

    EXPECT_TRUE(m_World.Has<Position>(e));
    EXPECT_TRUE(m_World.Has<Velocity>(e));
    EXPECT_TRUE(m_World.Has<Health>(e));
    EXPECT_EQ(m_World.Get<Health>(e).value, 50);
}

TEST_F(ECSTest, TagLikeEmptyComponentCanBeAddedAndDetected) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Tag>(e);

    EXPECT_TRUE(m_World.Has<Tag>(e));
}

TEST_F(ECSTest, ComponentsOfSameTypeAreIndependentAcrossEntities) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    m_World.Add<Health>(a, 10);
    m_World.Add<Health>(b, 20);

    EXPECT_EQ(m_World.Get<Health>(a).value, 10);
    EXPECT_EQ(m_World.Get<Health>(b).value, 20);
}

// ---------------------------------------------------------------------
// Reserve
// ---------------------------------------------------------------------

TEST_F(ECSTest, ReserveDoesNotCreateComponentsOrEntities) {
    m_World.Reserve<Position, Velocity>(128);

    Entity e = m_World.CreateEntity();
    EXPECT_FALSE(m_World.Has<Position>(e));
    EXPECT_FALSE(m_World.Has<Velocity>(e));
}

TEST_F(ECSTest, ReserveThenAddStillWorksCorrectly) {
    m_World.Reserve<Position>(64);

    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 2.0f);

    EXPECT_TRUE(m_World.Has<Position>(e));
    EXPECT_FLOAT_EQ(m_World.Get<Position>(e).x, 1.0f);
}

TEST_F(ECSTest, ReserveWithSingleComponentTypeCompiles) {
    m_World.Reserve<Health>(16);
    SUCCEED();
}

// ---------------------------------------------------------------------
// Query (all-of semantics)
// ---------------------------------------------------------------------

TEST_F(ECSTest, QuerySingleComponentReturnsOnlyMatchingEntities) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    Entity c = m_World.CreateEntity();

    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Position>(c, 0.0f, 0.0f);
    m_World.Add<Velocity>(b, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.Query<Position>();

    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(Contains(results, a));
    EXPECT_TRUE(Contains(results, c));
    EXPECT_FALSE(Contains(results, b));
}

TEST_F(ECSTest, QueryMultipleComponentsRequiresAllOfThem) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Velocity>(a, 0.0f, 0.0f);

    m_World.Add<Position>(b, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.Query<Position, Velocity>();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results.front() == a);
}

TEST_F(ECSTest, QueryReturnsEmptyWhenNoEntitiesMatch) {
    Entity a = m_World.CreateEntity();
    m_World.Add<Position>(a, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.Query<Velocity>();
    EXPECT_TRUE(results.empty());
}

TEST_F(ECSTest, QueryReturnsEmptyWhenPoolNeverCreated) {
    m_World.CreateEntity();
    std::vector<Entity> results = m_World.Query<Health>();
    EXPECT_TRUE(results.empty());
}

TEST_F(ECSTest, QueryExcludesDestroyedEntities) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Position>(b, 0.0f, 0.0f);

    m_World.DestroyEntity(a);

    std::vector<Entity> results = m_World.Query<Position>();

    EXPECT_FALSE(Contains(results, a));
    EXPECT_TRUE(Contains(results, b));
}

// ---------------------------------------------------------------------
// QueryAny (any-of semantics)
// ---------------------------------------------------------------------

TEST_F(ECSTest, QueryAnyReturnsEntitiesWithAtLeastOneComponent) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    Entity c = m_World.CreateEntity();

    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Velocity>(b, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.QueryAny<Position, Velocity>();

    EXPECT_EQ(results.size(), 2u);
    EXPECT_TRUE(Contains(results, a));
    EXPECT_TRUE(Contains(results, b));
    EXPECT_FALSE(Contains(results, c));
}

TEST_F(ECSTest, QueryAnyDoesNotDuplicateEntityWithBothComponents) {
    Entity a = m_World.CreateEntity();
    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Velocity>(a, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.QueryAny<Position, Velocity>();

    EXPECT_EQ(std::ranges::count_if(results, [&](Entity e) { return e == a; }), 1);
}

TEST_F(ECSTest, QueryAnyReturnsEmptyWhenNoEntitiesHaveAnyComponent) {
    m_World.CreateEntity();
    std::vector<Entity> results = m_World.QueryAny<Position, Velocity>();
    EXPECT_TRUE(results.empty());
}

// ---------------------------------------------------------------------
// View
// ---------------------------------------------------------------------

TEST_F(ECSTest, ViewIteratesOnlyMatchingEntities) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    m_World.Add<Position>(a, 1.0f, 1.0f);
    m_World.Add<Velocity>(a, 2.0f, 2.0f);

    m_World.Add<Position>(b, 3.0f, 3.0f);

    std::size_t count = 0;
    for (auto [e, pos, vel] : m_World.View<Position, Velocity>()) {
        EXPECT_TRUE(e == a);
        EXPECT_FALSE(e == b);
        ++count;
    }

    EXPECT_EQ(count, 1u);
}

TEST_F(ECSTest, ViewSingleComponentVisitsAllMatchingEntities) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    m_World.Add<Health>(a, 1);
    m_World.Add<Health>(b, 2);

    std::size_t count = 0;
    for (auto [e, health] : m_World.View<Health>()) {
        EXPECT_TRUE(e == a || e == b);
        ++count;
    }

    EXPECT_EQ(count, 2u);
}

TEST_F(ECSTest, ViewOverEmptyPoolYieldsNoIterations) {
    m_World.CreateEntity();

    std::size_t count = 0;
    for ([[maybe_unused]] auto [e, pos] : m_World.View<Position>()) {
        ++count;
    }

    EXPECT_EQ(count, 0u);
}

// ---------------------------------------------------------------------
// Entity lifecycle: edge cases
// ---------------------------------------------------------------------

TEST_F(ECSTest, CreateEntityNeverReturnsIndexZero) {
    for (std::size_t i = 0; i < 3000; ++i)
        ASSERT_NE(m_World.CreateEntity().index, 0u);
}

TEST_F(ECSTest, DestroyingNullEntityIsANoOp) {
    Entity a = m_World.CreateEntity();

    m_World.DestroyEntity(NullEntity);

    EXPECT_TRUE(m_World.IsAlive(a));
}

TEST_F(ECSTest, DestroyingSameEntityTwiceDoesNotCorruptTheFreeList) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();

    m_World.DestroyEntity(a);
    m_World.DestroyEntity(a);

    // If the index had been pushed twice, these two would share an index.
    Entity c = m_World.CreateEntity();
    Entity d = m_World.CreateEntity();

    EXPECT_NE(c.index, d.index);
    EXPECT_TRUE(m_World.IsAlive(b));
    EXPECT_TRUE(m_World.IsAlive(c));
    EXPECT_TRUE(m_World.IsAlive(d));
}

TEST_F(ECSTest, DestroyingStaleHandleDoesNotKillTheRecycledEntity) {
    Entity a = m_World.CreateEntity();
    m_World.DestroyEntity(a);
    Entity b = m_World.CreateEntity();

    m_World.DestroyEntity(a); // stale handle

    EXPECT_TRUE(m_World.IsAlive(b));
}

TEST_F(ECSTest, GenerationIncrementsOnEveryRecycle) {
    Entity e = m_World.CreateEntity();
    const std::uint32_t index = e.index;

    for (int i = 0; i < 10; ++i) {
        m_World.DestroyEntity(e);
        Entity next = m_World.CreateEntity();

        ASSERT_EQ(next.index, index); // only one free slot, so it must be reused
        EXPECT_EQ(next.generation, e.generation + 1);
        EXPECT_FALSE(m_World.IsAlive(e));
        e = next;
    }
    EXPECT_TRUE(m_World.IsAlive(e));
}

TEST_F(ECSTest, FreedIndicesAreReusedInsteadOfGrowing) {
    std::vector<Entity> first;
    std::uint32_t maxIndex = 0;
    for (int i = 0; i < 100; ++i) {
        first.push_back(m_World.CreateEntity());
        maxIndex = std::max(maxIndex, first.back().index);
    }

    for (Entity e : first)
        m_World.DestroyEntity(e);

    std::set<std::uint32_t> indices;
    for (int i = 0; i < 100; ++i) {
        Entity e = m_World.CreateEntity();
        EXPECT_LE(e.index, maxIndex);
        EXPECT_TRUE(m_World.IsAlive(e));
        indices.insert(e.index);
    }

    EXPECT_EQ(indices.size(), 100u);
    for (Entity e : first)
        EXPECT_FALSE(m_World.IsAlive(e));
}

TEST_F(ECSTest, ManyEntitiesCrossingInternalCapacityGrowth) {
    constexpr std::size_t count = 5000; // grows past the 1024 and 2048 steps
    std::vector<Entity> ids;
    ids.reserve(count);

    for (std::size_t i = 0; i < count; ++i) {
        Entity e = m_World.CreateEntity();
        m_World.Add<Health>(e, static_cast<std::int32_t>(i));
        ids.push_back(e);
    }

    std::set<std::uint32_t> indices;
    for (std::size_t i = 0; i < count; ++i) {
        ASSERT_TRUE(m_World.IsAlive(ids[i]));
        ASSERT_EQ(m_World.Get<Health>(ids[i]).value, static_cast<std::int32_t>(i));
        indices.insert(ids[i].index);
    }
    EXPECT_EQ(indices.size(), count);
}

TEST_F(ECSTest, ForgedHandlesAreHarmless) {
    Entity real = m_World.CreateEntity();
    m_World.Add<Position>(real, 1.0f, 1.0f);

    const Entity outOfRange{999'999u, 0u};
    const Entity wrongGeneration{real.index, real.generation + 7};

    EXPECT_FALSE(m_World.IsAlive(outOfRange));
    EXPECT_FALSE(m_World.IsAlive(wrongGeneration));
    EXPECT_FALSE(m_World.Has<Position>(outOfRange));
    EXPECT_FALSE(m_World.Has<Position>(wrongGeneration));
    EXPECT_EQ(m_World.TryGet<Position>(outOfRange), nullptr);
    EXPECT_EQ(m_World.TryGet<Position>(wrongGeneration), nullptr);

    m_World.DestroyEntity(outOfRange);
    m_World.DestroyEntity(wrongGeneration);
    m_World.Remove<Position>(outOfRange);
    m_World.Remove<Position>(wrongGeneration);

    EXPECT_TRUE(m_World.IsAlive(real));
    EXPECT_TRUE(m_World.Has<Position>(real));
}

TEST_F(ECSTest, RecycledEntityStartsWithoutComponents) {
    Entity a = m_World.CreateEntity();
    m_World.Add<Position>(a, 1.0f, 2.0f);
    m_World.Add<Health>(a, 5);
    m_World.DestroyEntity(a);

    Entity b = m_World.CreateEntity();

    EXPECT_FALSE(m_World.Has<Position>(b));
    EXPECT_FALSE(m_World.Has<Health>(b));
    EXPECT_FALSE(m_World.Has<Position>(a));
    EXPECT_EQ(m_World.TryGet<Position>(a), nullptr);
}

TEST_F(ECSTest, DestroyRemovesComponentsFromEveryPool) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Velocity>(a, 0.0f, 0.0f);
    m_World.Add<Health>(a, 1);
    m_World.Add<Position>(b, 0.0f, 0.0f);

    m_World.DestroyEntity(a);

    std::vector<Entity> positions = m_World.Query<Position>();
    EXPECT_EQ(positions.size(), 1u);
    EXPECT_TRUE(m_World.Query<Velocity>().empty());
    EXPECT_TRUE(m_World.Query<Health>().empty());
    EXPECT_TRUE(m_World.Has<Position>(b));
}

// ---------------------------------------------------------------------
// Component edge cases
// ---------------------------------------------------------------------

TEST_F(ECSTest, AddingSameComponentTwiceKeepsExistingValueAndDoesNotDuplicate) {
    // Documents current behaviour: Add is a no-op returning the existing component.
    Entity e = m_World.CreateEntity();
    m_World.Add<Health>(e, 10);

    Health& again = m_World.Add<Health>(e, 99);

    EXPECT_EQ(again.value, 10);
    EXPECT_EQ(m_World.Get<Health>(e).value, 10);
    EXPECT_EQ(m_World.Query<Health>().size(), 1u);
}

TEST_F(ECSTest, RemoveThenReAddStoresFreshValue) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Health>(e, 10);
    m_World.Remove<Health>(e);
    m_World.Add<Health>(e, 20);

    EXPECT_EQ(m_World.Get<Health>(e).value, 20);
    EXPECT_EQ(m_World.Query<Health>().size(), 1u);
}

TEST_F(ECSTest, RemovingMissingComponentIsANoOp) {
    Entity e = m_World.CreateEntity();

    EXPECT_NO_THROW(m_World.Remove<Health>(e)); // pool never created
    m_World.Add<Position>(e, 1.0f, 1.0f);
    m_World.Remove<Position>(e);
    EXPECT_NO_THROW(m_World.Remove<Position>(e)); // already removed
    EXPECT_TRUE(m_World.IsAlive(e));
}

TEST_F(ECSTest, RemoveOnDeadOrNullEntityIsANoOp) {
    Entity a = m_World.CreateEntity();
    m_World.Add<Position>(a, 1.0f, 1.0f);
    m_World.DestroyEntity(a);

    EXPECT_NO_THROW(m_World.Remove<Position>(a));
    EXPECT_NO_THROW(m_World.Remove<Position>(NullEntity));
}

TEST_F(ECSTest, AddToDeadEntityThrows) {
    Entity e = m_World.CreateEntity();
    m_World.DestroyEntity(e);

    EXPECT_THROW(m_World.Add<Position>(e, 1.0f, 1.0f), std::invalid_argument);
    EXPECT_FALSE(m_World.Has<Position>(e));
}

TEST_F(ECSTest, AddToNullEntityThrows) {
    EXPECT_THROW(m_World.Add<Position>(NullEntity, 1.0f, 1.0f), std::invalid_argument);
}

TEST_F(ECSTest, AddToStaleHandleThrowsAndDoesNotTouchTheRecycledEntity) {
    Entity a = m_World.CreateEntity();
    m_World.DestroyEntity(a);
    Entity b = m_World.CreateEntity();

    EXPECT_THROW(m_World.Add<Position>(a, 1.0f, 1.0f), std::invalid_argument);
    EXPECT_FALSE(m_World.Has<Position>(b));
}

TEST_F(ECSTest, GetMissingComponentThrowsOutOfRange) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(a, 1.0f, 1.0f);

    EXPECT_THROW(m_World.Get<Position>(b), std::out_of_range); // pool exists, entity missing
    EXPECT_THROW(m_World.Get<Velocity>(a), std::out_of_range); // pool never existed
}

TEST_F(ECSTest, GetOnDestroyedEntityThrowsOutOfRange) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 1.0f);
    m_World.DestroyEntity(e);

    EXPECT_THROW(m_World.Get<Position>(e), std::out_of_range);
}

TEST_F(ECSTest, ConstGetMissingComponentInExistingPoolThrows) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(a, 1.0f, 1.0f);

    const World& constWorld = m_World;
    EXPECT_THROW(constWorld.Get<Position>(b), std::out_of_range);
}

// Known bug: Registry::Pool() const dereferences a null unique_ptr when the pool of T was
// never created but a pool with a higher component id exists (m_pools.at() only checks the
// vector bounds). Enable once Pool() const throws when the slot is empty.
TEST_F(ECSTest, DISABLED_ConstGetOnNeverCreatedPoolThrowsInsteadOfCrashing) {
    (void)ComponentTypeId<NullSlotLow>();
    (void)ComponentTypeId<NullSlotHigh>(); // guaranteed id(High) > id(Low)

    Entity e = m_World.CreateEntity();
    m_World.Add<NullSlotHigh>(e); // m_pools grows past Low's slot, which stays null

    const World& constWorld = m_World;
    EXPECT_THROW(constWorld.Get<NullSlotLow>(e), std::out_of_range);
}

TEST_F(ECSTest, HasAndTryGetAreSafeOnNullAndDeadEntities) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 1.0f);
    m_World.DestroyEntity(e);

    EXPECT_FALSE(m_World.Has<Position>(NullEntity));
    EXPECT_FALSE(m_World.Has<Position>(e));
    EXPECT_EQ(m_World.TryGet<Position>(NullEntity), nullptr);
    EXPECT_EQ(m_World.TryGet<Position>(e), nullptr);
}

TEST_F(ECSTest, ReserveZeroIsHarmless) {
    m_World.Reserve<Position>(0);

    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 1.0f, 2.0f);

    EXPECT_TRUE(m_World.Has<Position>(e));
}

// ---------------------------------------------------------------------
// Swap-and-pop integrity and non-trivial component types
// ---------------------------------------------------------------------

TEST_F(ECSTest, RemovingFromTheMiddleKeepsOtherComponentsIntact) {
    std::array<Entity, 5> es;
    for (std::size_t i = 0; i < es.size(); ++i) {
        es[i] = m_World.CreateEntity();
        m_World.Add<Health>(es[i], static_cast<std::int32_t>(i * 10));
    }

    m_World.Remove<Health>(es[2]);

    EXPECT_FALSE(m_World.Has<Health>(es[2]));
    EXPECT_EQ(m_World.Get<Health>(es[0]).value, 0);
    EXPECT_EQ(m_World.Get<Health>(es[1]).value, 10);
    EXPECT_EQ(m_World.Get<Health>(es[3]).value, 30);
    EXPECT_EQ(m_World.Get<Health>(es[4]).value, 40);

    m_World.Remove<Health>(es[0]); // first
    m_World.Remove<Health>(es[4]); // last
    EXPECT_EQ(m_World.Get<Health>(es[1]).value, 10);
    EXPECT_EQ(m_World.Get<Health>(es[3]).value, 30);

    m_World.Add<Health>(es[2], 99);
    EXPECT_EQ(m_World.Get<Health>(es[2]).value, 99);
    EXPECT_EQ(m_World.Query<Health>().size(), 3u);
}

TEST_F(ECSTest, PoolCanBeEmptiedAndRefilled) {
    std::vector<Entity> ids;
    for (int i = 0; i < 50; ++i) {
        ids.push_back(m_World.CreateEntity());
        m_World.Add<Health>(ids.back(), i);
    }

    for (Entity e : ids)
        m_World.Remove<Health>(e);
    EXPECT_TRUE(m_World.Query<Health>().empty());

    for (std::size_t i = 0; i < ids.size(); ++i)
        m_World.Add<Health>(ids[i], static_cast<std::int32_t>(i) + 1000);

    for (std::size_t i = 0; i < ids.size(); ++i)
        EXPECT_EQ(m_World.Get<Health>(ids[i]).value, static_cast<std::int32_t>(i) + 1000);
}

TEST_F(ECSTest, HeapOwningComponentsSurviveSwapRemove) {
    const std::string longText(200, 'x'); // beyond small-string optimisation

    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    Entity c = m_World.CreateEntity();
    m_World.Add<Name>(a, "alpha");
    m_World.Add<Name>(b, longText + "beta");
    m_World.Add<Name>(c, longText + "gamma");

    m_World.Remove<Name>(a);

    EXPECT_EQ(m_World.Get<Name>(b).value, longText + "beta");
    EXPECT_EQ(m_World.Get<Name>(c).value, longText + "gamma");
}

TEST_F(ECSTest, MoveOnlyComponentsAreSupported) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    Entity c = m_World.CreateEntity();
    m_World.Add<Owned>(a, 1);
    m_World.Add<Owned>(b, 2);
    m_World.Add<Owned>(c, 3);

    m_World.Remove<Owned>(b);

    ASSERT_NE(m_World.Get<Owned>(a).ptr, nullptr);
    ASSERT_NE(m_World.Get<Owned>(c).ptr, nullptr);
    EXPECT_EQ(*m_World.Get<Owned>(a).ptr, 1);
    EXPECT_EQ(*m_World.Get<Owned>(c).ptr, 3);
}

TEST_F(ECSTest, ComponentDestructorsRunExactlyOnce) {
    const std::int32_t baseline = Tracked::live;

    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    Entity c = m_World.CreateEntity();
    m_World.Add<Tracked>(a, 1);
    m_World.Add<Tracked>(b, 2);
    m_World.Add<Tracked>(c, 3);
    EXPECT_EQ(Tracked::live, baseline + 3);

    m_World.Remove<Tracked>(a);
    EXPECT_EQ(Tracked::live, baseline + 2);

    m_World.DestroyEntity(b);
    EXPECT_EQ(Tracked::live, baseline + 1);

    m_World.Clear();
    EXPECT_EQ(Tracked::live, baseline);
}

TEST_F(ECSTest, ComponentsAreDestroyedWhenTheWorldIsDestroyed) {
    const std::int32_t baseline = Tracked::live;
    {
        World local;
        for (std::int32_t i = 0; i < 100; ++i) {
            Entity e = local.CreateEntity();
            local.Add<Tracked>(e, i);
        }
        EXPECT_EQ(Tracked::live, baseline + 100);
    }
    EXPECT_EQ(Tracked::live, baseline);
}

// ---------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------

TEST_F(ECSTest, ClearInvalidatesAllHandlesAndRemovesAllComponents) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(a, 1.0f, 1.0f);
    m_World.Add<Health>(b, 5);

    m_World.Clear();

    EXPECT_FALSE(m_World.IsAlive(a));
    EXPECT_FALSE(m_World.IsAlive(b));
    EXPECT_FALSE(m_World.Has<Position>(a));
    EXPECT_TRUE(m_World.Query<Position>().empty());
    EXPECT_TRUE(m_World.Query<Health>().empty());
}

TEST_F(ECSTest, WorldIsUsableAfterClear) {
    Entity a = m_World.CreateEntity();
    m_World.Clear();

    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(b, 2.0f, 3.0f);

    EXPECT_FALSE(m_World.IsAlive(a));
    EXPECT_TRUE(m_World.IsAlive(b));
    EXPECT_FLOAT_EQ(m_World.Get<Position>(b).x, 2.0f);
    if (a.index == b.index)
        EXPECT_NE(a.generation, b.generation);
}

TEST_F(ECSTest, ClearOnEmptyWorldAndRepeatedClearAreSafe) {
    EXPECT_NO_THROW(m_World.Clear());
    EXPECT_NO_THROW(m_World.Clear());

    m_World.CreateEntity();
    EXPECT_NO_THROW(m_World.Clear());
    EXPECT_NO_THROW(m_World.Clear());
}

TEST_F(ECSTest, ClearAfterDestroysDoesNotProduceDuplicateIndices) {
    std::vector<Entity> ids;
    for (int i = 0; i < 10; ++i)
        ids.push_back(m_World.CreateEntity());
    for (int i = 0; i < 5; ++i)
        m_World.DestroyEntity(ids[static_cast<std::size_t>(i)]);

    m_World.Clear();

    std::set<std::uint32_t> indices;
    for (int i = 0; i < 20; ++i) {
        Entity e = m_World.CreateEntity();
        EXPECT_TRUE(m_World.IsAlive(e));
        indices.insert(e.index);
    }
    EXPECT_EQ(indices.size(), 20u);
}

TEST_F(ECSTest, HandlesFromBeforeClearNeverComeBackToLife) {
    std::vector<Entity> old;
    for (int i = 0; i < 50; ++i)
        old.push_back(m_World.CreateEntity());

    m_World.Clear();

    for (int i = 0; i < 50; ++i)
        m_World.CreateEntity(); // reuses the same indices

    for (Entity e : old)
        EXPECT_FALSE(m_World.IsAlive(e));
}

// ---------------------------------------------------------------------
// Query / View: edge cases
// ---------------------------------------------------------------------

TEST_F(ECSTest, QueryWithAMissingPoolReturnsEmpty) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.Query<Position, Health>(); // no Health pool
    EXPECT_TRUE(results.empty());
}

TEST_F(ECSTest, QueryWithThreeComponents) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Velocity>(a, 0.0f, 0.0f);
    m_World.Add<Health>(a, 1);
    m_World.Add<Position>(b, 0.0f, 0.0f);
    m_World.Add<Velocity>(b, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.Query<Position, Velocity, Health>();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results.front() == a);
}

TEST_F(ECSTest, QueryNoLongerMatchesAfterAComponentIsRemoved) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 0.0f, 0.0f);
    m_World.Add<Velocity>(e, 0.0f, 0.0f);

    m_World.Remove<Velocity>(e);

    std::vector<Entity> both = m_World.Query<Position, Velocity>();
    std::vector<Entity> onlyPosition = m_World.Query<Position>();
    EXPECT_TRUE(both.empty());
    EXPECT_EQ(onlyPosition.size(), 1u);
}

TEST_F(ECSTest, QueryResultDoesNotDependOnComponentOrder) {
    for (int i = 0; i < 50; ++i) {
        Entity e = m_World.CreateEntity();
        if (i % 2 == 0)
            m_World.Add<Position>(e, 0.0f, 0.0f);
        if (i % 3 == 0)
            m_World.Add<Velocity>(e, 0.0f, 0.0f);
    }

    std::vector<Entity> ab = m_World.Query<Position, Velocity>();
    std::vector<Entity> ba = m_World.Query<Velocity, Position>();
    const auto byIndex = [](Entity l, Entity r) { return l.index < r.index; };
    std::ranges::sort(ab, byIndex);
    std::ranges::sort(ba, byIndex);

    EXPECT_EQ(ab.size(), 9u); // multiples of 6 below 50
    EXPECT_TRUE(std::ranges::equal(ab, ba));
}

TEST_F(ECSTest, QueryAnyExcludesDestroyedEntitiesAndHandlesMissingPools) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Position>(b, 0.0f, 0.0f);
    m_World.DestroyEntity(a);

    std::vector<Entity> results = m_World.QueryAny<Position, Health>(); // Health pool missing

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results.front() == b);
}

TEST_F(ECSTest, TagComponentsParticipateInQueries) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Tag>(a);
    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Position>(b, 0.0f, 0.0f);

    std::vector<Entity> results = m_World.Query<Tag, Position>();

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results.front() == a);
}

TEST_F(ECSTest, ViewAllowsMutatingComponents) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    m_World.Add<Position>(a, 0.0f, 0.0f);
    m_World.Add<Position>(b, 1.0f, 1.0f);

    for ([[maybe_unused]] auto [e, pos] : m_World.View<Position>())
        pos.x += 10.0f;

    EXPECT_FLOAT_EQ(m_World.Get<Position>(a).x, 10.0f);
    EXPECT_FLOAT_EQ(m_World.Get<Position>(b).x, 11.0f);
}

TEST_F(ECSTest, ViewDrivenBySmallestPoolStillFiltersCorrectly) {
    std::set<std::uint32_t> expected;
    for (int i = 0; i < 100; ++i) {
        Entity e = m_World.CreateEntity();
        m_World.Add<Position>(e, 0.0f, 0.0f);
        if (i % 30 == 0) {
            m_World.Add<Velocity>(e, 0.0f, 0.0f);
            expected.insert(e.index);
        }
    }

    std::set<std::uint32_t> seen;
    for ([[maybe_unused]] auto [e, pos, vel] : m_World.View<Position, Velocity>())
        seen.insert(e.index);

    EXPECT_EQ(seen.size(), 4u); // i = 0, 30, 60, 90
    EXPECT_EQ(seen, expected);
}

TEST_F(ECSTest, ViewWithAMissingPoolYieldsNothing) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 0.0f, 0.0f);

    std::size_t count = 0;
    for ([[maybe_unused]] auto [en, pos, health] : m_World.View<Position, Health>())
        ++count;

    EXPECT_EQ(count, 0u);
}

TEST_F(ECSTest, ViewSkipsDestroyedEntitiesAndRemovedComponents) {
    Entity a = m_World.CreateEntity();
    Entity b = m_World.CreateEntity();
    Entity c = m_World.CreateEntity();
    for (Entity e : {a, b, c}) {
        m_World.Add<Position>(e, 0.0f, 0.0f);
        m_World.Add<Velocity>(e, 0.0f, 0.0f);
    }

    m_World.DestroyEntity(a);
    m_World.Remove<Velocity>(b);

    std::vector<Entity> seen;
    for ([[maybe_unused]] auto [e, pos, vel] : m_World.View<Position, Velocity>())
        seen.push_back(e);

    ASSERT_EQ(seen.size(), 1u);
    EXPECT_TRUE(seen.front() == c);
}

TEST_F(ECSTest, ViewIsEmptyAfterClear) {
    Entity e = m_World.CreateEntity();
    m_World.Add<Position>(e, 0.0f, 0.0f);
    m_World.Clear();

    std::size_t count = 0;
    for ([[maybe_unused]] auto [en, pos] : m_World.View<Position>())
        ++count;

    EXPECT_EQ(count, 0u);
}

TEST_F(ECSTest, ViewAndQueryAgreeOnTheMatchingSet) {
    for (int i = 0; i < 200; ++i) {
        Entity e = m_World.CreateEntity();
        if (i % 2 == 0)
            m_World.Add<Position>(e, 0.0f, 0.0f);
        if (i % 3 == 0)
            m_World.Add<Velocity>(e, 0.0f, 0.0f);
        if (i % 5 == 0)
            m_World.Add<Health>(e, i);
        if (i % 7 == 0)
            m_World.DestroyEntity(e);
    }

    std::vector<Entity> viaQuery = m_World.Query<Position, Velocity, Health>();
    std::vector<Entity> viaView;
    for ([[maybe_unused]] auto [e, p, v, h] : m_World.View<Position, Velocity, Health>())
        viaView.push_back(e);

    const auto byIndex = [](Entity l, Entity r) { return l.index < r.index; };
    std::ranges::sort(viaQuery, byIndex);
    std::ranges::sort(viaView, byIndex);

    EXPECT_FALSE(viaQuery.empty());
    EXPECT_TRUE(std::ranges::equal(viaQuery, viaView));
}
