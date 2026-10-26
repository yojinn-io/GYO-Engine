#include "doctest/doctest.h"
#include <filesystem>
#include <string>
#include <utility>

#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "PathText.hpp"

namespace fs = std::filesystem;
using Engine::Asset::Resolver::AssetPathResolver;

TEST_CASE("AssetPathResolver: normal join") {
    fs::path root = fs::temp_directory_path() / "asset_test_root";
    AssetPathResolver::Options options;
    options.assetsRoot = Engine::Test::Utf8Path(root);
    AssetPathResolver r(options);

    auto out = r.Resolve("textures/a.ppm");
    REQUIRE(out); // ここは value() を読むので REQUIRE が安全
    CHECK(out.value().Str() == Engine::Test::GenericUtf8Path(root / "textures/a.ppm"));
    CHECK(out.value().Filename() == "a.ppm");
    CHECK(out.value().Extension() == ".ppm");
    CHECK(out.value().IsAbsoluteLike());
}

TEST_CASE("AssetPathResolver: reject absolute path") {
    fs::path root = fs::temp_directory_path() / "asset_test_root";
    AssetPathResolver::Options options;
    options.assetsRoot = Engine::Test::Utf8Path(root);
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
    options.assetsRoot = Engine::Test::Utf8Path(root);
    AssetPathResolver r(options);

    auto out = r.Resolve("../outside.txt");

    REQUIRE(!out);
    CHECK(out.error().code == Engine::Asset::AssetErrorCode::PathEscapesRoot);
}

TEST_CASE("AssetPathResolver: allow dot segments within root") {
    fs::path root = fs::temp_directory_path() / "asset_test_root";
    AssetPathResolver::Options options;
    options.assetsRoot = Engine::Test::Utf8Path(root);
    AssetPathResolver r(options);

    auto out = r.Resolve("a/../b.txt");

    REQUIRE(out);
    CHECK_FALSE(out.value().HasTraversal());
    CHECK(out.value().Str() == Engine::Test::GenericUtf8Path(root / "b.txt"));
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

// The file:// rule of docs/architecture/io.md: a file URI always names an
// absolute path, so a resolver that allows schemes rejects it like any other
// absolute path; other schemes still resolve under the assets root.
TEST_CASE("AssetPathResolver: a file URI is absolute even when schemes are allowed") {
    AssetPathResolver::Options options;
    options.assetsRoot = "/game/shaders";
    REQUIRE(options.allowSchemes);
    AssetPathResolver r(options);

    for (const auto* path : {"file:///x.dxil", "file://localhost/x.dxil"}) {
        const auto out = r.Resolve(path);
        REQUIRE_FALSE(out);
        CHECK(out.error().code == Engine::Asset::AssetErrorCode::InvalidPath);
    }
    const auto res = r.Resolve("res:///x.dxil");
    REQUIRE(res);
    CHECK(res.value().Str() == "/game/shaders/x.dxil");
}

// An escape the option permits keeps its '..': the OS resolves it from where
// the root really is (docs/architecture/io.md Invariant 3, D54).
TEST_CASE("AssetPathResolver: allow escape root when explicitly enabled") {
    AssetPathResolver::Options options;
    options.assetsRoot = "assets/game";
    options.allowEscapeAssetsRoot = true;
    AssetPathResolver r(options);

    const auto out = r.Resolve("../shared/a.ppm");

    REQUIRE(out);
    CHECK(out.value().Str() == "assets/game/../shared/a.ppm");
    CHECK(out.value().HasTraversal());
}

// The assets root is a native path: it stays as written, '\' turned into '/',
// so its '..' and a UNC root's leading "//" reach the OS (D54). The catalog
// path is still cleaned and checked on its own, so it never leaves the root.
TEST_CASE("AssetPathResolver: keep the assets root as written") {
    const std::pair<const char*, const char*> roots[] = {
        {"/game/bin/../assets", "/game/bin/../assets"},
        {"../assets", "../assets"},
        {"./assets", "./assets"},
        {"//server/share/assets", "//server/share/assets"},
        {"C:\\game\\..\\assets", "C:/game/../assets"},
        {"/game//assets/", "/game//assets"},
    };
    for (const auto& [root, written] : roots) {
        CAPTURE(root);
        AssetPathResolver::Options options;
        options.assetsRoot = root;
        AssetPathResolver r(options);

        const auto out = r.Resolve("textures/./nested/../a.ppm");
        REQUIRE(out);
        CHECK(out.value().Str() == std::string(written) + "/textures/a.ppm");

        for (const auto* escape : {"../outside.txt", "a/../../outside.txt"}) {
            const auto refused = r.Resolve(escape);
            REQUIRE_FALSE(refused);
            CHECK(refused.error().code == Engine::Asset::AssetErrorCode::PathEscapesRoot);
        }
    }
}
