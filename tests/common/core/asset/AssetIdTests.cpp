#include "doctest/doctest.h"

#include "engine/asset/AssetId.hpp"

#include <functional>
#include <unordered_map>
#include <unordered_set>

TEST_CASE("AssetId identity ignores diagnostic names") {
    Engine::Asset::AssetId first{42};
    first.debugName = "first-name";

    Engine::Asset::AssetId second{42};
    second.debugName = "second-name";

    Engine::Asset::AssetId unnamed{42};

    CHECK(first == second);
    CHECK(first == unnamed);
    CHECK(second == unnamed);
    CHECK(std::hash<Engine::Asset::AssetId>{}(first) ==
          std::hash<Engine::Asset::AssetId>{}(second));
}

TEST_CASE("AssetId diagnostic names cannot merge different identities") {
    Engine::Asset::AssetId first{1};
    first.debugName = "shared-diagnostic-name";

    Engine::Asset::AssetId second{2};
    second.debugName = "shared-diagnostic-name";

    CHECK(first != second);

    const std::unordered_set<Engine::Asset::AssetId> identities{first, second};
    CHECK(identities.size() == 2);
}

TEST_CASE("AssetId unordered lookup follows value identity") {
    Engine::Asset::AssetId inserted{99};
    inserted.debugName = "catalog-name";

    Engine::Asset::AssetId lookup{99};
    lookup.debugName = "controller-diagnostic-name";

    const std::unordered_map<Engine::Asset::AssetId, int> values{{inserted, 7}};
    REQUIRE(values.contains(lookup));
    CHECK(values.at(lookup) == 7);
}
