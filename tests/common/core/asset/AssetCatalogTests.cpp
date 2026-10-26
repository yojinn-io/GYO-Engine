#include "doctest/doctest.h"

#include <filesystem>
#include <fstream>

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/asset/AssetId.hpp"
#include "PathText.hpp"

namespace fs = std::filesystem;
using Engine::Asset::AssetCatalog;
using Engine::Asset::AssetId;
using Engine::Asset::Catalog::CatalogParser;
using Engine::Asset::Resolver::AssetPathResolver;

static void WriteText(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p, std::ios::binary);
    ofs << s;
}

TEST_CASE("AssetCatalog: build entries with resolvedPath") {
    fs::path tmp = fs::temp_directory_path() / "asset_catalog_test";
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    fs::path assetsRoot = tmp / "assets";
    fs::path catalogPath = assetsRoot / "asset_catalog.json";

    WriteText(catalogPath, R"({
      "version":1,
      "assets":[
        {"id":"ui.title","type":"text","path":"ui/title.txt"}
      ]
    })");
    AssetPathResolver::Options options;
    options.assetsRoot = Engine::Test::Utf8Path(assetsRoot);
    AssetPathResolver resolver(options);
    CatalogParser parser;
    AssetCatalog catalog;

    auto r = catalog.LoadFromFile(Engine::Test::Utf8Path(catalogPath), parser, resolver);
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
    fs::path catalogPath = assetsRoot / "asset_catalog.json";

    WriteText(catalogPath, R"({
      "version":1,
      "assets":[
        {"id":"a","type":"text","path":"a.txt"},
        {"id":"a","type":"text","path":"b.txt"}
      ]
    })");

    AssetPathResolver::Options options;
    options.assetsRoot = Engine::Test::Utf8Path(assetsRoot);
    AssetPathResolver resolver(options);
    CatalogParser parser;
    AssetCatalog catalog;

    auto r = catalog.LoadFromFile(Engine::Test::Utf8Path(catalogPath), parser, resolver);
    CHECK(!r);
    CHECK(r.error().code == Engine::Asset::AssetErrorCode::InvalidCatalogEntry);
}

TEST_CASE("AssetCatalog: a path resolution failure keeps the resolver's code") {
    fs::path tmp = fs::temp_directory_path() / "asset_catalog_test_escape";
    fs::remove_all(tmp);

    fs::path assetsRoot = tmp / "assets";
    fs::path catalogPath = assetsRoot / "asset_catalog.json";

    WriteText(catalogPath, R"({
      "version":1,
      "assets":[
        {"id":"escape","type":"text","path":"../outside.txt"}
      ]
    })");

    AssetPathResolver::Options options;
    options.assetsRoot = Engine::Test::Utf8Path(assetsRoot);
    AssetPathResolver resolver(options);
    CatalogParser parser;
    AssetCatalog catalog;

    const auto r = catalog.LoadFromFile(Engine::Test::Utf8Path(catalogPath), parser, resolver);
    REQUIRE_FALSE(r);
    CHECK(r.error().code == Engine::Asset::AssetErrorCode::PathEscapesRoot);
    CHECK(r.error().detail.rfind("escape", 0) == 0); // the entry id, then the resolver detail
    fs::remove_all(tmp);
}

TEST_CASE("AssetCatalog: independently rooted append is atomic and preserves entries") {
    const fs::path tmp = fs::temp_directory_path() / "gyo_catalog_composition_tests";
    const fs::path gameFile = tmp / "game.json";
    const fs::path commonFile = tmp / "common.json";
    WriteText(gameFile, R"({"version":1,"assets":[
        {"id":"game.wall","type":"texture","path":"wall.png"}]})");
    WriteText(commonFile, R"({"version":1,"assets":[
        {"id":"common.white","type":"texture","path":"white.png"}]})");
    AssetPathResolver::Options gameOptions;
    gameOptions.assetsRoot = Engine::Test::Utf8Path(tmp / "game");
    AssetPathResolver::Options commonOptions;
    commonOptions.assetsRoot = Engine::Test::Utf8Path(tmp / "common");
    AssetPathResolver gameResolver(gameOptions), commonResolver(commonOptions);
    CatalogParser parser;
    AssetCatalog catalog;
    REQUIRE(catalog.LoadFromFile(Engine::Test::Utf8Path(gameFile), parser, gameResolver));
    const auto wallId = AssetId::FromString("game.wall");
    const auto* wall = catalog.Find(wallId);
    REQUIRE(catalog.AppendFromFile(Engine::Test::Utf8Path(commonFile), parser, commonResolver));
    CHECK(catalog.Find(wallId) == wall);
    REQUIRE(catalog.Find(AssetId::FromString("common.white")) != nullptr);
    CHECK(catalog.Find(AssetId::FromString("common.white"))->resolvedPath ==
          AssetPathResolver::NormalizePath(Engine::Test::Utf8Path(tmp / "common/white.png")));

    SUBCASE("late duplicate cannot leave earlier additions behind") {
        WriteText(commonFile, R"({"version":1,"assets":[
            {"id":"new.texture","type":"texture","path":"new.png"},
            {"id":"game.wall","type":"texture","path":"other.png"}]})");
    }
    SUBCASE("late path escape cannot leave earlier additions behind") {
        WriteText(commonFile, R"({"version":1,"assets":[
            {"id":"new.texture","type":"texture","path":"new.png"},
            {"id":"escape","type":"texture","path":"../outside.png"}]})");
    }
    CHECK_FALSE(catalog.AppendFromFile(Engine::Test::Utf8Path(commonFile), parser, commonResolver));
    CHECK(catalog.Entries().size() == 2);
    CHECK(catalog.Find(wallId) == wall);
    CHECK(catalog.Find(AssetId::FromString("new.texture")) == nullptr);
}
