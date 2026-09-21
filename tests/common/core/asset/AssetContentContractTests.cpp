#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
struct ContractWorkspace {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("gyo_asset_contract_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ContractWorkspace() { std::filesystem::create_directories(root); }
    ~ContractWorkspace() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
};
}

TEST_CASE("Runtime content agrees with assembly and package contract fixtures") {
    std::ifstream fixture(GYO_ASSET_CONTRACT_FIXTURE);
    REQUIRE(fixture.is_open());
    const auto cases = nlohmann::json::parse(fixture).at("cases");
    REQUIRE_FALSE(cases.empty());
    for (const auto& test : cases) {
        INFO(test.at("name").get<std::string>());
        const bool expected = test.at("valid").get<bool>();
        const auto manifest = Engine::Asset::ContentManifest::Parse(test.at("content").dump());
        if (!manifest) {
            CHECK_FALSE(expected);
            continue;
        }
        ContractWorkspace workspace;
        for (const auto& [name, data] : test.at("catalogs").items()) {
            std::ofstream output(workspace.root / name, std::ios::binary);
            REQUIRE(output.is_open());
            output << data.dump();
        }
        const auto catalog = manifest.value().LoadCatalogs(workspace.root);
        const std::string failure = catalog ? std::string{} : catalog.error().message;
        INFO(failure);
        CHECK(static_cast<bool>(catalog) == expected);
    }
}

TEST_CASE("Runtime content rejects invalid raw JSON even in unknown metadata") {
    std::ifstream fixture(std::filesystem::path(GYO_ASSET_CONTRACT_FIXTURE).parent_path() / "raw_cases.json");
    REQUIRE(fixture.is_open());
    const auto cases = nlohmann::json::parse(fixture).at("cases");
    REQUIRE_FALSE(cases.empty());
    for (const auto& test : cases) {
        INFO(test.at("name").get<std::string>());
        const bool expected = test.at("valid").get<bool>();
        const auto text = test.at("text").get<std::string>();
        if (test.at("kind") == "catalog") {
            Engine::Asset::Catalog::CatalogParser parser;
            CHECK(static_cast<bool>(parser.Parse(text)) == expected);
        } else {
            REQUIRE(test.at("kind") == "content");
            CHECK(static_cast<bool>(Engine::Asset::ContentManifest::Parse(text)) == expected);
        }
    }
}
