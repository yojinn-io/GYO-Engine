#include <doctest/doctest.h>
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using Engine::Asset::ContentManifest;

namespace {
struct ContentFixture {
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("gyo_content_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ContentFixture() { std::filesystem::create_directories(root); }
    ~ContentFixture() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    void Write(const std::string& name, const std::string& text) {
        std::ofstream(root / name) << text;
    }
};
}

TEST_CASE("Content manifest rejects malformed schema and root escapes") {
    for (const auto* json : {
        "not json", R"({"version":2,"catalogs":[],"shader_bundles":[]})",
        R"({"version":true,"catalogs":[],"shader_bundles":[]})",
        R"({"version":1,"catalogs":["../outside.json"],"shader_bundles":[]})",
        R"({"version":1,"catalogs":["/outside.json"],"shader_bundles":[]})",
        R"({"version":1,"catalogs":["C:/outside.json"],"shader_bundles":[]})",
        R"({"version":1,"catalogs":["same.json","same.json"],"shader_bundles":[]})",
        R"({"version":1,"catalogs":["content.json"],"shader_bundles":[]})",
        R"({"version":1,"catalogs":["bad\npath.json"],"shader_bundles":[]})",
        R"({"version":1,"catalogs":[],"shader_bundles":[{"name":"game","path":"../shaders"}]})",
        R"({"version":1,"catalogs":[],"shader_bundles":[{"name":"not-valid","path":"shaders/game"}]})",
        R"({"version":1,"catalogs":[],"shader_bundles":[{"name":"game","path":"content.json/nested"}]})",
        R"({"version":1,"catalogs":["catalog.json"],"shader_bundles":[{"name":"game","path":"catalog.json/nested"}]})",
        R"({"version":1,"catalogs":[],"shader_bundles":[{"name":"game","path":"shaders"},{"name":"builtin","path":"shaders/nested"}]})",
        R"({"version":1,"catalogs":[],"shader_bundles":[{"name":"game","path":"a"},{"name":"game","path":"b"}]})"}) {
        INFO(json);
        CHECK_FALSE(ContentManifest::Parse(json, "test-content.json"));
    }
}

// A catalog name is content.json text, so UTF-8, also outside the Windows ANSI
// code page (os-path-utf8-sweep, D63): it is joined to the root as UTF-8.
TEST_CASE("Content manifest loads a catalog named outside the ANSI code page") {
    ContentFixture fixture;
    fixture.Write("content.json",
        "{\"version\":1,\"catalogs\":[\"\xE8\xA8\x98\xE9\x8C\xB2-\xED\x95\x9C.json\"],\"shader_bundles\":[]}");
    std::ofstream(fixture.root / std::filesystem::path(u8"\u8a18\u9332-\ud55c.json"))
        << R"({"version":1,"assets":[{"id":"test.asset","type":"text","path":"a.txt"}]})";
    auto manifest = ContentManifest::Load(fixture.root);
    REQUIRE(manifest);
    auto catalog = manifest.value().LoadCatalogs(fixture.root);
    REQUIRE_MESSAGE(catalog, (catalog ? std::string{} : catalog.error().message + " " + catalog.error().detail));
    CHECK(catalog.value().Find(Engine::Asset::AssetId::FromString("test.asset")) != nullptr);
}

TEST_CASE("Content manifest loads only its own catalogs and reports absent content") {
    ContentFixture fixture;
    CHECK_FALSE(ContentManifest::Load(fixture.root));
    fixture.Write("content.json", R"({"version":1,"catalogs":["catalog.json"],"shader_bundles":[]})");
    auto manifest = ContentManifest::Load(fixture.root);
    REQUIRE(manifest);
    CHECK_FALSE(manifest.value().LoadCatalogs(fixture.root));
    fixture.Write("catalog.json", R"({"version":1,"assets":[{"id":"test.asset","type":"text","path":"missing.txt"}]})");
    auto catalog = manifest.value().LoadCatalogs(fixture.root);
    REQUIRE(catalog);
    const auto* entry = catalog.value().Find(Engine::Asset::AssetId::FromString("test.asset"));
    REQUIRE(entry);
    CHECK(std::filesystem::path(entry->resolvedPath) == fixture.root / "missing.txt");
    Engine::Asset::Loading::NativeFileAssetSource source;
    CHECK_FALSE(source.ReadAll(entry->resolvedPath));
    fixture.Write("missing.txt", "present");
    CHECK(source.ReadAll(entry->resolvedPath));
    fixture.Write("catalog.json", R"({"version":1,"assets":[{"id":"test.asset","type":"text","path":"../outside.txt"}]})");
    CHECK_FALSE(manifest.value().LoadCatalogs(fixture.root));
    fixture.Write("catalog.json", R"({"version":1,"assets":[{"id":"test.asset","type":"text","path":"content.json"}]})");
    CHECK_FALSE(manifest.value().LoadCatalogs(fixture.root));
}

TEST_CASE("Runtime content ignores build-only source metadata") {
    auto manifest = ContentManifest::Parse(R"({"version":1,"catalogs":[],"shader_bundles":[
        {"name":"builtin","source":"../../engine/shaders/bundle.json","path":"shaders/builtin"}]})");
    REQUIRE(manifest);
    REQUIRE(manifest.value().shaderBundles.size() == 1);
    CHECK(manifest.value().shaderBundles.front().path == "shaders/builtin");
}

// Engine paths are UTF-8 (docs/architecture/io.md): a root outside the ANSI
// code page, Traditional Chinese and Japanese here, loads the manifest, the
// catalogs and the assets they name (os-subsystem P5).
TEST_CASE("Content manifest loads under a root outside the ANSI code page") {
    ContentFixture fixture;
    const auto root = fixture.root / std::filesystem::path(u8"\u8cc7\u6599\u593e-\u65e5\u672c");
    std::filesystem::create_directories(root);
    std::ofstream(root / "content.json") << R"({"version":1,"catalogs":["catalog.json"],"shader_bundles":[]})";
    std::ofstream(root / "catalog.json") << R"({"version":1,"assets":[{"id":"test.asset","type":"text","path":"a.txt"}]})";
    std::ofstream(root / "a.txt", std::ios::binary) << "UTF-8 root";
    auto manifest = ContentManifest::Load(root);
    REQUIRE_MESSAGE(manifest, (manifest ? std::string{} : manifest.error().detail));
    auto catalog = manifest.value().LoadCatalogs(root);
    REQUIRE_MESSAGE(catalog, (catalog ? std::string{} : catalog.error().detail));
    const auto* entry = catalog.value().Find(Engine::Asset::AssetId::FromString("test.asset"));
    REQUIRE(entry);
    Engine::Asset::Loading::NativeFileAssetSource source;
    const auto bytes = source.ReadAll(entry->resolvedPath);
    REQUIRE_MESSAGE(bytes, (bytes ? std::string{} : bytes.error().detail));
    CHECK(std::string(reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size()) == "UTF-8 root");
}
