// Unit tests of the entity columns (entity-model E-5): ordered insert, ordered
// erase, lookup. The authority digest never exercises these alone, so the
// behaviour is pinned here before any column is wired into the Match.
#include <doctest/doctest.h>

#include "AssertTestSupport.hpp"
#include "RetroFPS/Pvp/EntityColumns.hpp"

#include <vector>

namespace {
using namespace fps::pvp;

struct Legacy final { // the transition shape: PlayerState, CombatState, PlayerAnimation
    PlayerId playerId{};
    int payload{};
};
struct Fresh final { // the shape of new components
    EntityId id{};
    int payload{};
};

template <typename T>
std::vector<EntityId> Ids(const std::vector<T>& column) {
    std::vector<EntityId> ids;
    for (const auto& value : column) ids.push_back(EntityOf(value));
    return ids;
}
} // namespace

TEST_CASE("Entity columns insert at the ordered position, not at the end") {
    std::vector<Legacy> column;
    InsertEntity(column, Legacy{3, 30});
    InsertEntity(column, Legacy{1, 10});
    InsertEntity(column, Legacy{2, 20});
    CHECK(Ids(column) == std::vector<EntityId>{1, 2, 3});
    CHECK(StrictlyAscending(std::span<const Legacy>(column)));
    REQUIRE(FindEntity(column, 2) != nullptr);
    CHECK(FindEntity(column, 2)->payload == 20);
    CHECK(FindEntity(column, 4) == nullptr);
    CHECK(FindEntity(column, NoEntity) == nullptr);
}

TEST_CASE("Entity columns erase in place and keep the order of the rest") {
    std::vector<Fresh> column;
    for (EntityId id : {EntityId{5}, EntityId{1}, EntityId{9}, EntityId{3}}) InsertEntity(column, Fresh{id, int(id)});
    CHECK(Ids(column) == std::vector<EntityId>{1, 3, 5, 9});
    CHECK(EraseEntity(column, 3));
    CHECK(Ids(column) == std::vector<EntityId>{1, 5, 9}); // swap-and-pop would give 1, 9, 5
    CHECK_FALSE(EraseEntity(column, 3));
    CHECK(EraseEntity(column, 1));
    CHECK(EraseEntity(column, 9));
    CHECK(Ids(column) == std::vector<EntityId>{5});
    CHECK(EraseEntity(column, 5));
    CHECK(column.empty());
    CHECK_FALSE(EraseEntity(column, 5));
}

TEST_CASE("Entity columns refuse a duplicate id") {
    Engine::Test::ScopedAssertionHandler throwing;
    std::vector<Legacy> column;
    InsertEntity(column, Legacy{7, 70});
    CHECK_THROWS_AS(InsertEntity(column, Legacy{7, 71}), Engine::Base::AssertionFailure);
    CHECK(Ids(column) == std::vector<EntityId>{7});
    CHECK(FindEntity(column, 7)->payload == 70);
}

TEST_CASE("Entity columns find through a mutable reference") {
    std::vector<Fresh> column;
    InsertEntity(column, Fresh{2, 0});
    if (auto* found = FindEntity(column, 2)) found->payload = 42;
    CHECK(FindEntity(std::span<const Fresh>(column), 2)->payload == 42);
    std::vector<Fresh> unordered{{2, 0}, {1, 0}};
    CHECK_FALSE(StrictlyAscending(std::span<const Fresh>(unordered)));
    std::vector<Fresh> duplicate{{1, 0}, {1, 0}};
    CHECK_FALSE(StrictlyAscending(std::span<const Fresh>(duplicate)));
}
