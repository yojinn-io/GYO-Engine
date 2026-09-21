#include <doctest/doctest.h>
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

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

TEST_CASE("Content manifest loads only its own catalogs and reports absent content") {
    ContentFixture fixture;
    CHECK_FALSE(ContentManifest::Load(fixture.root));
    fixture.Write("content.json", R"({"version":1,"catalogs":["catalog.json"],"shader_bundles":[]})");
    auto manifest = ContentManifest::Load(fixture.root);
    REQUIRE(manifest);
    CHECK_FALSE(manifest.value().LoadCatalogs(fixture.root));
    fixture.Write("catalog.json", R"({"assets":[{"id":"test.asset","type":"text","path":"missing.txt"}]})");
    auto catalog = manifest.value().LoadCatalogs(fixture.root);
    REQUIRE(catalog);
    const auto* entry = catalog.value().Find(Engine::Asset::AssetId::FromString("test.asset"));
    REQUIRE(entry);
    CHECK(std::filesystem::path(entry->resolvedPath) == fixture.root / "missing.txt");
    Engine::Asset::Loading::NativeFileAssetSource source;
    CHECK_FALSE(source.ReadAll(entry->resolvedPath));
    fixture.Write("missing.txt", "present");
    CHECK(source.ReadAll(entry->resolvedPath));
    fixture.Write("catalog.json", R"({"assets":[{"id":"test.asset","type":"text","path":"../outside.txt"}]})");
    CHECK_FALSE(manifest.value().LoadCatalogs(fixture.root));
    fixture.Write("catalog.json", R"({"assets":[{"id":"test.asset","type":"text","path":"content.json"}]})");
    CHECK_FALSE(manifest.value().LoadCatalogs(fixture.root));
}

TEST_CASE("Runtime content ignores build-only source metadata") {
    auto manifest = ContentManifest::Parse(R"({"version":1,"catalogs":[],"shader_bundles":[
        {"name":"builtin","source":"../../engine/shaders/bundle.json","path":"shaders/builtin"}]})");
    REQUIRE(manifest);
    REQUIRE(manifest.value().shaderBundles.size() == 1);
    CHECK(manifest.value().shaderBundles.front().path == "shaders/builtin");
}
