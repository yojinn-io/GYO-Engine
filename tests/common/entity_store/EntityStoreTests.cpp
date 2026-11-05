// Rule tests of the Engine entity store (docs/architecture/entity-store.md,
// Guard rows 1-3, 7 for the column functions, and 10). Synthetic element types
// only: no product type or name (AGENTS.md section 7). Each rule is shown to
// detect its listed mutants in docs/architecture/plans/entity-store/scripts/
// mutations.json.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "engine/entity_store/EntityColumns.hpp"
#include "engine/entity_store/EntityId.hpp"
#include "engine/entity_store/EntityKey.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {
using Engine::EntityStore::EntityId;

// Keyed by the primary EntityKey: the member id.
struct OnlyId final {
    EntityId id{};
    int payload{};
};
// Keyed by its EntityKey specialization; no member id.
struct OnlySpecialized final {
    EntityId handle{};
    int payload{};
};
// Has a member id and a specialization that returns another field: the
// specialization must win.
struct Both final {
    EntityId id{};
    EntityId handle{};
};
// Neither a member id nor a specialization: no column of it compiles.
struct Neither final {
    int payload{};
};
// Keyed by a specialization that counts every key read (comparison budget).
struct Counted final {
    EntityId key{};
};
std::size_t countedReads = 0;
} // namespace

// Declared before any use of EntityKey for these types (EntityKey.hpp,
// visibility rule).
namespace Engine::EntityStore {
template <>
struct EntityKey<OnlySpecialized> {
    [[nodiscard]] static constexpr EntityId Of(const OnlySpecialized& value) noexcept { return value.handle; }
};
template <>
struct EntityKey<Both> {
    [[nodiscard]] static constexpr EntityId Of(const Both& value) noexcept { return value.handle; }
};
template <>
struct EntityKey<Counted> {
    [[nodiscard]] static EntityId Of(const Counted& value) noexcept {
        ++countedReads;
        return value.key;
    }
};
} // namespace Engine::EntityStore

namespace {
using Engine::EntityStore::EntityOf;
using Engine::EntityStore::EraseEntity;
using Engine::EntityStore::FindEntity;
using Engine::EntityStore::HasEntityKey;
using Engine::EntityStore::InsertEntity;
using Engine::EntityStore::StrictlyAscending;

template <typename T>
std::vector<EntityId> Keys(const std::vector<T>& column) {
    std::vector<EntityId> keys;
    for (const auto& value : column) keys.push_back(EntityOf(value));
    return keys;
}
template <typename T>
std::vector<int> Payloads(const std::vector<T>& column) {
    std::vector<int> payloads;
    for (const auto& value : column) payloads.push_back(value.payload);
    return payloads;
}

// The three lookups of one key agree: a span, a const vector and a mutable
// vector find the same element, or all find nothing.
template <typename T>
const T* FindEverywhere(std::vector<T>& column, EntityId id) {
    const T* fromSpan = FindEntity(std::span<const T>(column), id);
    const T* fromConst = FindEntity(static_cast<const std::vector<T>&>(column), id);
    T* fromMutable = FindEntity(column, id);
    CHECK(fromSpan == fromConst);
    CHECK(fromSpan == fromMutable);
    return fromSpan;
}

// A type the column functions accept (Guard row 10, "neither").
template <typename T>
concept Insertable = requires(std::vector<T>& column, T value) {
    Engine::EntityStore::InsertEntity(column, value);
};
template <typename T>
concept Findable = requires(const std::vector<T>& column) {
    Engine::EntityStore::FindEntity(column, EntityId{});
};
} // namespace

TEST_CASE("Entity store keys: zero, the top bit and the maximum are ordinary keys") {
    // Guard row 1: the store reserves no value.
    Engine::Test::ScopedAssertionHandler throwing;
    constexpr EntityId top = EntityId{1} << 63;
    constexpr EntityId maximum = std::numeric_limits<EntityId>::max();
    std::vector<OnlyId> column;
    InsertEntity(column, OnlyId{top, 3});
    InsertEntity(column, OnlyId{7, 2});
    InsertEntity(column, OnlyId{maximum, 4});
    InsertEntity(column, OnlyId{0, 1});
    CHECK(Keys(column) == std::vector<EntityId>{0, 7, top, maximum});
    CHECK(StrictlyAscending(std::span<const OnlyId>(column)));
    for (const EntityId id : {EntityId{0}, top, maximum}) {
        const OnlyId* found = FindEverywhere(column, id);
        REQUIRE(found != nullptr);
        CHECK(found->id == id);
    }
    CHECK(FindEverywhere(column, 1) == nullptr);
    CHECK(FindEverywhere(column, maximum - 1) == nullptr);
    CHECK(EraseEntity(column, 0));
    CHECK(EraseEntity(column, maximum));
    CHECK(EraseEntity(column, top));
    CHECK(Keys(column) == std::vector<EntityId>{7});
    CHECK(FindEverywhere(column, 0) == nullptr);
}

TEST_CASE("Entity store insert: the ordered position, never an append") {
    // Guard row 2: descending runs, a key below every key, a key above every key.
    std::vector<OnlyId> column;
    for (const EntityId id : {EntityId{5}, EntityId{1}, EntityId{9}, EntityId{3}})
        InsertEntity(column, OnlyId{id, int(id) * 10});
    CHECK(Keys(column) == std::vector<EntityId>{1, 3, 5, 9});
    CHECK(Payloads(column) == std::vector<int>{10, 30, 50, 90});
    OnlyId& inserted = InsertEntity(column, OnlyId{0, -1});
    CHECK(inserted.id == 0);
    CHECK(inserted.payload == -1);
    InsertEntity(column, OnlyId{12, 120});
    InsertEntity(column, OnlyId{4, 40});
    CHECK(Keys(column) == std::vector<EntityId>{0, 1, 3, 4, 5, 9, 12});
    CHECK(Payloads(column) == std::vector<int>{-1, 10, 30, 40, 50, 90, 120});
    CHECK(StrictlyAscending(std::span<const OnlyId>(column)));
}

TEST_CASE("Entity store insert: a present key fails GYO_ASSERT and leaves the column") {
    // Guard row 7 (insert part).
    std::vector<OnlyId> column;
    InsertEntity(column, OnlyId{7, 70});
    InsertEntity(column, OnlyId{3, 30});
    GYO_CHECK_ASSERTS(InsertEntity(column, OnlyId{7, 71}));
    GYO_CHECK_ASSERTS(InsertEntity(column, OnlyId{3, 31}));
    CHECK(Keys(column) == std::vector<EntityId>{3, 7});
    CHECK(Payloads(column) == std::vector<int>{30, 70});
}

TEST_CASE("Entity store erase: keeps the order of the rest") {
    // Guard row 2: erase the middle and the ends. Swap-and-pop would give
    // 1, 9, 5 after erasing 3.
    Engine::Test::ScopedAssertionHandler throwing;
    std::vector<OnlyId> column;
    for (const EntityId id : {EntityId{5}, EntityId{1}, EntityId{9}, EntityId{3}, EntityId{7}})
        InsertEntity(column, OnlyId{id, int(id)});
    CHECK(EraseEntity(column, 3));
    CHECK(Keys(column) == std::vector<EntityId>{1, 5, 7, 9});
    CHECK(Payloads(column) == std::vector<int>{1, 5, 7, 9});
    // Absent: a result, not an error (Guard row 7).
    CHECK_FALSE(EraseEntity(column, 3));
    CHECK_FALSE(EraseEntity(column, 0));
    CHECK_FALSE(EraseEntity(column, 100));
    CHECK(EraseEntity(column, 1));
    CHECK(Keys(column) == std::vector<EntityId>{5, 7, 9});
    CHECK(EraseEntity(column, 9));
    CHECK(Keys(column) == std::vector<EntityId>{5, 7});
    CHECK(Payloads(column) == std::vector<int>{5, 7});
    CHECK(EraseEntity(column, 5));
    CHECK(EraseEntity(column, 7));
    CHECK(column.empty());
    CHECK_FALSE(EraseEntity(column, 5));
}

TEST_CASE("Entity store find: present, absent, below and above; span and vectors agree") {
    // Guard row 3.
    std::vector<OnlyId> column;
    for (const EntityId id : {EntityId{10}, EntityId{20}, EntityId{30}}) InsertEntity(column, OnlyId{id, int(id)});
    for (const EntityId id : {EntityId{10}, EntityId{20}, EntityId{30}}) {
        const OnlyId* found = FindEverywhere(column, id);
        REQUIRE(found != nullptr);
        CHECK(found->id == id);
        CHECK(found->payload == int(id));
    }
    CHECK(FindEverywhere(column, 15) == nullptr); // absent, between two keys
    CHECK(FindEverywhere(column, 25) == nullptr);
    CHECK(FindEverywhere(column, 9) == nullptr);  // below the first
    CHECK(FindEverywhere(column, 31) == nullptr); // above the last
    std::vector<OnlyId> empty;
    CHECK(FindEverywhere(empty, 0) == nullptr);
    // A mutable lookup writes through to the column; the span sees it.
    if (OnlyId* found = FindEntity(column, 20)) found->payload = 42;
    CHECK(FindEntity(std::span<const OnlyId>(column), 20)->payload == 42);
}

TEST_CASE("Entity store find: a lookup reads at most 2 log2 n + 2 keys") {
    // Guard row 3, comparison budget: binary search on the column itself.
    constexpr std::size_t count = 1024;
    constexpr std::size_t budget = 2 * 10 + 2; // 2 * ceil(log2(1024)) + 2
    std::vector<Counted> column;
    column.reserve(count);
    for (std::size_t i = 0; i < count; ++i) column.push_back(Counted{EntityId(2 * i + 2)}); // already ascending
    REQUIRE(StrictlyAscending(std::span<const Counted>(column)));
    std::size_t worst = 0;
    for (EntityId id = 0; id <= 2 * count + 3; ++id) {
        countedReads = 0;
        const Counted* fromSpan = FindEntity(std::span<const Counted>(column), id);
        worst = countedReads > worst ? countedReads : worst;
        countedReads = 0;
        const Counted* fromConst = FindEntity(static_cast<const std::vector<Counted>&>(column), id);
        worst = countedReads > worst ? countedReads : worst;
        countedReads = 0;
        const Counted* fromMutable = FindEntity(column, id);
        worst = countedReads > worst ? countedReads : worst;
        const bool present = id >= 2 && id <= 2 * count && id % 2 == 0;
        CHECK((fromSpan != nullptr) == present);
        CHECK(fromSpan == fromConst);
        CHECK(fromSpan == fromMutable);
    }
    INFO("worst key reads per lookup: " << worst);
    CHECK(worst <= budget);
    CHECK(worst > 0);
}

TEST_CASE("Entity store StrictlyAscending: no descent, no duplicate") {
    CHECK(StrictlyAscending(std::span<const OnlyId>()));
    const std::vector<OnlyId> one{{4, 0}};
    CHECK(StrictlyAscending(std::span<const OnlyId>(one)));
    const std::vector<OnlyId> ascending{{1, 0}, {2, 0}, {9, 0}};
    CHECK(StrictlyAscending(std::span<const OnlyId>(ascending)));
    const std::vector<OnlyId> descending{{2, 0}, {1, 0}};
    CHECK_FALSE(StrictlyAscending(std::span<const OnlyId>(descending)));
    const std::vector<OnlyId> duplicate{{1, 0}, {1, 0}};
    CHECK_FALSE(StrictlyAscending(std::span<const OnlyId>(duplicate)));
    const std::vector<OnlyId> lateDescent{{1, 0}, {5, 0}, {3, 0}};
    CHECK_FALSE(StrictlyAscending(std::span<const OnlyId>(lateDescent)));
}

TEST_CASE("Entity store key access: member id, specialization, both, neither") {
    // Guard row 10.
    static_assert(HasEntityKey<OnlyId>);
    static_assert(HasEntityKey<OnlySpecialized>);
    static_assert(HasEntityKey<Both>);
    static_assert(!HasEntityKey<Neither>);
    static_assert(Insertable<OnlyId> && Findable<OnlyId>);
    static_assert(!Insertable<Neither> && !Findable<Neither>);

    CHECK(EntityOf(OnlyId{6, 0}) == 6);
    CHECK(EntityOf(OnlySpecialized{8, 0}) == 8);
    CHECK(EntityOf(Both{1, 9}) == 9); // the specialization wins over the member id

    std::vector<OnlySpecialized> specialized;
    InsertEntity(specialized, OnlySpecialized{30, 3});
    InsertEntity(specialized, OnlySpecialized{10, 1});
    CHECK(Keys(specialized) == std::vector<EntityId>{10, 30});
    REQUIRE(FindEntity(specialized, 30) != nullptr);
    CHECK(FindEntity(specialized, 30)->payload == 3);

    // Ordered and found by the specialization's field, against the order of id.
    std::vector<Both> both;
    InsertEntity(both, Both{1, 30});
    InsertEntity(both, Both{2, 10});
    InsertEntity(both, Both{3, 20});
    CHECK(Keys(both) == std::vector<EntityId>{10, 20, 30});
    REQUIRE(FindEntity(both, 10) != nullptr);
    CHECK(FindEntity(both, 10)->id == 2);
    CHECK(FindEntity(both, 1) == nullptr);
    CHECK(EraseEntity(both, 20));
    CHECK_FALSE(EraseEntity(both, 3));
    CHECK(Keys(both) == std::vector<EntityId>{10, 30});
}
