// Unit tests of the product's entity-column adapter (entity-model E-5): a
// transition-shape component keyed by playerId through the EntityKey
// specialization in EntityColumns.hpp. The column mechanism itself (ordered
// insert and erase, lookup, id-keyed components) is the Engine entity store's
// and is tested in tests/common/entity_store (docs/architecture/entity-store.md).
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

// A type with only playerId has a key through the adapter's EntityKey
// specialization; that the key is the playerId value is checked at run time
// below (the adapter's mutant in mutations.json must reach that case). A type
// with both playerId and id is refused by the specialization's static_assert
// (EntityColumns.hpp); it cannot be written here as a case, since this
// translation unit would then not compile.
static_assert(Engine::EntityStore::HasEntityKey<Legacy>);

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

TEST_CASE("Entity columns refuse a duplicate id") {
    Engine::Test::ScopedAssertionHandler throwing;
    std::vector<Legacy> column;
    InsertEntity(column, Legacy{7, 70});
    CHECK_THROWS_AS(InsertEntity(column, Legacy{7, 71}), Engine::Base::AssertionFailure);
    CHECK(Ids(column) == std::vector<EntityId>{7});
    CHECK(FindEntity(column, 7)->payload == 70);
}
