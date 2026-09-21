#include "doctest/doctest.h"
#include <filesystem>

#include "engine/asset/resolver/AssetPathResolver.hpp"

namespace fs = std::filesystem;
using Engine::Asset::Resolver::AssetPathResolver;

TEST_CASE("AssetPathResolver: normal join") {
    fs::path root = fs::temp_directory_path() / "asset_test_root";
    AssetPathResolver::Options options;
    options.assetsRoot = root.string();
    AssetPathResolver r(options);

    auto out = r.Resolve("textures/a.ppm");
    REQUIRE(out); // ここは value() を読むので REQUIRE が安全
    CHECK(out.value().Str() == (root / "textures/a.ppm").generic_string());
    CHECK(out.value().Filename() == "a.ppm");
    CHECK(out.value().Extension() == ".ppm");
    CHECK(out.value().IsAbsoluteLike());
}

TEST_CASE("AssetPathResolver: reject absolute path") {
    fs::path root = fs::temp_directory_path() / "asset_test_root";
    AssetPathResolver::Options options;
    options.assetsRoot = root.string();
    AssetPathResolver r(options);

#ifdef _WIN32
    auto out = r.Resolve("C:\\Windows\\win.ini");
#else
    auto out = r.Resolve("/etc/passwd");
#endif

    REQUIRE(!out); // CHECK ではなく REQUIRE
    CHECK(out.error().code == Engine::Asset::AssetErrorCode::InvalidPath);
}

TEST_CASE("AssetPathResolver: reject escape root") {
    fs::path root = fs::temp_directory_path() / "asset_test_root";
    AssetPathResolver::Options options;
    options.assetsRoot = root.string();
    AssetPathResolver r(options);

    auto out = r.Resolve("../outside.txt");

    REQUIRE(!out);
    CHECK(out.error().code == Engine::Asset::AssetErrorCode::PathEscapesRoot);
}

TEST_CASE("AssetPathResolver: allow dot segments within root") {
    fs::path root = fs::temp_directory_path() / "asset_test_root";
    AssetPathResolver::Options options;
    options.assetsRoot = root.string();
    AssetPathResolver r(options);

    auto out = r.Resolve("a/../b.txt");

    REQUIRE(out);
    CHECK_FALSE(out.value().HasTraversal());
    CHECK(out.value().Str() == (root / "b.txt").generic_string());
}

TEST_CASE("AssetPathResolver: preserve absolute root prefixes") {
    for (const auto* root : {"/game/assets", "C:/game/assets"}) {
        AssetPathResolver::Options options;
        options.assetsRoot = root;
        AssetPathResolver r(options);

        const auto out = r.Resolve("textures/a.ppm");

        REQUIRE(out);
        CHECK(out.value().IsAbsoluteLike());
        CHECK(out.value().Str() == std::string(root) + "/textures/a.ppm");
    }
}

TEST_CASE("AssetPathResolver: allow absolute paths without prepending assets root") {
    AssetPathResolver::Options options;
    options.assetsRoot = "assets/game";
    options.allowAbsolutePath = true;
    AssetPathResolver r(options);

    for (const auto* prefix : {"/shared", "C:/shared"}) {
        const auto out = r.Resolve(std::string(prefix) + "/textures/../a.ppm");

        REQUIRE(out);
        CHECK(out.value().IsAbsoluteLike());
        CHECK(out.value().Str() == std::string(prefix) + "/a.ppm");
    }
}

TEST_CASE("AssetPathResolver: normalize a scheme path under assets root") {
    AssetPathResolver::Options options;
    options.assetsRoot = "assets/game";
    AssetPathResolver r(options);

    const auto out = r.Resolve("res://textures\\\\./nested//../a.ppm");

    REQUIRE(out);
    CHECK(out.value().Str() == "assets/game/textures/a.ppm");
    CHECK(out.value().Parent().Str() == "assets/game/textures");
    CHECK_FALSE(out.value().IsAbsoluteLike());
}

TEST_CASE("AssetPathResolver: allow escape root when explicitly enabled") {
    AssetPathResolver::Options options;
    options.assetsRoot = "assets/game";
    options.allowEscapeAssetsRoot = true;
    AssetPathResolver r(options);

    const auto out = r.Resolve("../shared/a.ppm");

    REQUIRE(out);
    CHECK(out.value().Str() == "assets/shared/a.ppm");
    CHECK_FALSE(out.value().HasTraversal());
}
