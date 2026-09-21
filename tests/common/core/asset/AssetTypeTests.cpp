#include <doctest/doctest.h>

#include "engine/asset/AssetType.hpp"

#include <functional>
#include <unordered_map>

TEST_CASE("AssetType identity ignores diagnostic names") {
    Engine::Asset::AssetType first{42};
    first.debugName = "first";
    Engine::Asset::AssetType second{42};
    second.debugName = "second";

    CHECK(first == second);
    CHECK(std::hash<Engine::Asset::AssetType>{}(first) ==
          std::hash<Engine::Asset::AssetType>{}(second));
}

TEST_CASE("AssetType diagnostic names cannot merge different identities") {
    Engine::Asset::AssetType first{1};
    first.debugName = "texture";
    Engine::Asset::AssetType second{2};
    second.debugName = "texture";

    CHECK_FALSE(first == second);
    const std::unordered_map<Engine::Asset::AssetType, int> types{
        {first, 1},
        {second, 2},
    };
    CHECK(types.size() == 2);
}

TEST_CASE("AssetType invalid and standard values follow value identity") {
    CHECK_FALSE(Engine::Asset::AssetType::Invalid().IsValid());
    CHECK(Engine::Asset::AssetType::Texture() ==
          Engine::Asset::AssetType::FromString("texture"));
}
