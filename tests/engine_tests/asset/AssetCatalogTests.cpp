#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/asset/AssetId.hpp"

namespace fs = std::filesystem;
using Engine::Asset::AssetCatalog;
using Engine::Asset::AssetId;
using Engine::Asset::Catalog::CatalogParser;
using Engine::Asset::Resolver::AssetPathResolver;

static void WriteText(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p.string(), std::ios::binary);
    ofs << s;
}

TEST_CASE("AssetCatalog: build entries with resolvedPath") {
    fs::path tmp = fs::temp_directory_path() / "asset_catalog_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    fs::path assetsRoot = tmp / "assets";
    fs::path catalogPath = tmp / "config/engine/asset_catalog.json";

    WriteText(catalogPath, R"({
      "assets":[
        {"id":"ui.title","type":"text","path":"ui/title.txt"}
      ]
    })");
    AssetPathResolver::Options options;
    options.assetsRoot = assetsRoot.string();
    AssetPathResolver resolver(options);
    CatalogParser parser;
    AssetCatalog catalog;

    auto r = catalog.LoadFromFile(catalogPath.string(), parser, resolver);
    CHECK(r);

    const auto* e = catalog.Find(AssetId::FromString("ui.title"));
    REQUIRE(e != nullptr);
    CHECK(!e->sourcePath.empty());
    CHECK(!e->resolvedPath.empty());
    CHECK(e->resolvedPath.find("assets") != std::string::npos);
}

TEST_CASE("AssetCatalog: duplicate id should fail") {
    fs::path tmp = fs::temp_directory_path() / "asset_catalog_test_dup";
    fs::remove_all(tmp);

    fs::path assetsRoot = tmp / "assets";
    fs::path catalogPath = tmp / "config/engine/asset_catalog.json";

    WriteText(catalogPath, R"({
      "assets":[
        {"id":"a","type":"text","path":"a.txt"},
        {"id":"a","type":"text","path":"b.txt"}
      ]
    })");

    AssetPathResolver::Options options;
    options.assetsRoot = assetsRoot.string();
    AssetPathResolver resolver(options);
    CatalogParser parser;
    AssetCatalog catalog;

    auto r = catalog.LoadFromFile(catalogPath.string(), parser, resolver);
    CHECK(!r);
    CHECK(r.error().code == Engine::Asset::AssetErrorCode::InvalidCatalogEntry);
}

TEST_CASE("AssetCatalog: independently rooted append is atomic and preserves entries") {
    const fs::path tmp = fs::temp_directory_path() / "gyo_catalog_composition_tests";
    const fs::path gameFile = tmp / "game.json";
    const fs::path commonFile = tmp / "common.json";
    WriteText(gameFile, R"({"assets":[
        {"id":"game.wall","type":"texture","path":"wall.png"}]})");
    WriteText(commonFile, R"({"assets":[
        {"id":"common.white","type":"texture","path":"white.png"}]})");
    AssetPathResolver::Options gameOptions;
    gameOptions.assetsRoot = (tmp / "game").string();
    AssetPathResolver::Options commonOptions;
    commonOptions.assetsRoot = (tmp / "common").string();
    AssetPathResolver gameResolver(gameOptions), commonResolver(commonOptions);
    CatalogParser parser;
    AssetCatalog catalog;
    REQUIRE(catalog.LoadFromFile(gameFile.string(), parser, gameResolver));
    const auto wallId = AssetId::FromString("game.wall");
    const auto* wall = catalog.Find(wallId);
    REQUIRE(catalog.AppendFromFile(commonFile.string(), parser, commonResolver));
    CHECK(catalog.Find(wallId) == wall);
    REQUIRE(catalog.Find(AssetId::FromString("common.white")) != nullptr);
    CHECK(catalog.Find(AssetId::FromString("common.white"))->resolvedPath ==
          AssetPathResolver::NormalizePath((tmp / "common/white.png").string()));

    SUBCASE("late duplicate cannot leave earlier additions behind") {
        WriteText(commonFile, R"({"assets":[
            {"id":"new.texture","type":"texture","path":"new.png"},
            {"id":"game.wall","type":"texture","path":"other.png"}]})");
    }
    SUBCASE("late path escape cannot leave earlier additions behind") {
        WriteText(commonFile, R"({"assets":[
            {"id":"new.texture","type":"texture","path":"new.png"},
            {"id":"escape","type":"texture","path":"../outside.png"}]})");
    }
    CHECK_FALSE(catalog.AppendFromFile(commonFile.string(), parser, commonResolver));
    CHECK(catalog.Entries().size() == 2);
    CHECK(catalog.Find(wallId) == wall);
    CHECK(catalog.Find(AssetId::FromString("new.texture")) == nullptr);
}
