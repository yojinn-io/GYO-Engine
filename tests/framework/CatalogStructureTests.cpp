// Catalog registration as structure (core) and as text (the JSON binding):
// docs/framework/architecture.md, Design Invariants 3 and 4; framework-rework
// D-1 (the catalog-level cases of the content contract).
#include <doctest/doctest.h>

#include "gyo/framework/CatalogPlan.hpp"
#include "gyo/framework/json/CatalogText.hpp"

#include "engine/asset/ContentManifest.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace Gyo::Tools::Framework;

namespace {

constexpr const char* kCatalog = R"({
  "version": 1,
  "assets": [
    { "id": "game.text.settings", "type": "text", "path": "settings.json" },
    { "id": "game.audio.shot",
      "type": "sound", "path": "audio/shot.wav" },

    { "id": "game.font.ui", "type": "font", "path": "fonts/ui.ttf" }
  ]
}
)";

CatalogErrorCode Refusal(const std::string_view text, const CatalogEntry& entry, const RegistrationPolicy& policy = {}) {
    const auto result = RegisterAsset(text, entry, policy);
    REQUIRE_FALSE(result.has_value());
    return result.error().code;
}

} // namespace

TEST_CASE("C1: one catalog follows the catalog-level cases of the content contract") {
    std::ifstream fixture(GYO_ASSET_CONTRACT_FIXTURE);
    REQUIRE(fixture.is_open());
    const auto cases = nlohmann::json::parse(fixture).at("cases");
    int applicable = 0;
    int excluded = 0;
    int valid = 0;
    for (const auto& test : cases) {
        const std::string name = test.at("name").get<std::string>();
        INFO(name);
        // Only a valid content root that lists exactly one catalog and no shader
        // bundle: across catalogs and against bundles ContentManifest decides.
        const auto manifest = Engine::Asset::ContentManifest::Parse(test.at("content").dump());
        if (!manifest || manifest->catalogs.size() != 1 || !manifest->shaderBundles.empty()) {
            ++excluded;
            continue;
        }
        ++applicable;
        const bool expected = test.at("valid").get<bool>();
        valid += expected ? 1 : 0;
        const std::string text = test.at("catalogs").at(manifest->catalogs.front()).dump();
        const auto read = ReadCatalog(text);
        CHECK(static_cast<bool>(read) == expected);
    }
    // Declared before the cases ran (framework-rework HANDOFF, catalog-structure).
    CHECK(applicable == 34);
    CHECK(excluded == 20);
    CHECK(valid == 4);
}

TEST_CASE("C2: a new entry follows the last one and every other byte stays") {
    const auto update = RegisterAsset(kCatalog, {"game.audio.reload", "sound", "audio/reload.wav"});
    REQUIRE(update.has_value());
    CHECK(update->added);
    CHECK_FALSE(update->previousPath.has_value());
    const std::string catalog = kCatalog;
    const std::size_t last = catalog.find("\"fonts/ui.ttf\" }") + std::string_view("\"fonts/ui.ttf\" }").size();
    CHECK(update->text == catalog.substr(0, last) +
                              ",\n    { \"id\": \"game.audio.reload\", \"type\": \"sound\", \"path\": \"audio/reload.wav\" }" +
                              catalog.substr(last));
}

TEST_CASE("C3: an existing entry changes only its values, with its previous path") {
    const auto update = RegisterAsset(kCatalog, {"game.audio.shot", "sound", "audio/shot_v2.wav"});
    REQUIRE(update.has_value());
    CHECK_FALSE(update->added);
    REQUIRE(update->previousPath.has_value());
    CHECK(*update->previousPath == "audio/shot.wav");
    // Only the path value changes; the entry keeps its two-line layout (D-6).
    std::string expected = kCatalog;
    const std::size_t at = expected.find("\"audio/shot.wav\"");
    expected.replace(at, std::string_view("\"audio/shot.wav\"").size(), "\"audio/shot_v2.wav\"");
    CHECK(update->text == expected);

    const auto same = RegisterAsset(kCatalog, {"game.audio.shot", "sound", "audio/shot.wav"});
    REQUIRE(same.has_value());
    CHECK_FALSE(same->previousPath.has_value());
}

TEST_CASE("C4: an empty assets array gets the entry on its own line") {
    const auto update = RegisterAsset("{\n  \"version\": 1,\n  \"assets\": []\n}\n", {"a.b", "sound", "b.wav"});
    REQUIRE(update.has_value());
    CHECK(update->text ==
          "{\n  \"version\": 1,\n  \"assets\": [\n    { \"id\": \"a.b\", \"type\": \"sound\", \"path\": \"b.wav\" }\n  ]\n}\n");
}

TEST_CASE("C5: the policy refuses another type and a taken path by default, and allows them when asked") {
    // Another type for an existing id.
    CHECK(Refusal(kCatalog, {"game.text.settings", "sound", "settings.json"}) == CatalogErrorCode::InvalidRegistration);
    const auto retyped = RegisterAsset(kCatalog, {"game.text.settings", "sound", "settings.json"},
                                       RegistrationPolicy{.allowTypeChange = true});
    REQUIRE(retyped.has_value());
    CHECK_FALSE(retyped->added);
    // A path registered under another id (audio_editor's convert_taken_path).
    CHECK(Refusal(kCatalog, {"game.audio.new", "sound", "fonts/ui.ttf"}) == CatalogErrorCode::InvalidRegistration);
    const auto shared = RegisterAsset(kCatalog, {"game.audio.new", "sound", "fonts/ui.ttf"},
                                      RegistrationPolicy{.allowSharedPath = true});
    REQUIRE(shared.has_value());
    CHECK(shared->added);
}

TEST_CASE("C6: an entry the catalog rules refuse is not registered") {
    CHECK(Refusal(kCatalog, {"", "sound", "x.wav"}) == CatalogErrorCode::InvalidRegistration);
    CHECK(Refusal(kCatalog, {"game.audio.new", "", "x.wav"}) == CatalogErrorCode::InvalidRegistration);
    for (const char* path : {"", "/abs.wav", "a/../b.wav", "./a.wav", "a//b.wav", "a\\b.wav", "C:/a.wav", "content.json",
                             "a\n.wav"}) {
        CAPTURE(path);
        CHECK(Refusal(kCatalog, {"game.audio.new", "sound", path}) == CatalogErrorCode::InvalidRegistration);
    }
    CHECK(Refusal(R"({"version": 2, "assets": []})", {"a", "sound", "a.wav"}) == CatalogErrorCode::InvalidCatalog);
    CHECK(Refusal("not json", {"a", "sound", "a.wav"}) == CatalogErrorCode::InvalidCatalog);
}

TEST_CASE("C7: PlanRegistration places the entry by its id") {
    const std::vector<CatalogEntry> entries{{"a", "text", "a.txt"}, {"b", "text", "b.txt"}};
    REQUIRE(CheckCatalogEntries(entries).has_value());
    const auto update = PlanRegistration(entries, {"b", "text", "c.txt"});
    REQUIRE(update.has_value());
    CHECK_FALSE(update->added);
    CHECK(update->index == 1);
    CHECK(update->previousPath == std::optional<std::string>("b.txt"));
    const auto add = PlanRegistration(entries, {"c", "text", "c.txt"});
    REQUIRE(add.has_value());
    CHECK(add->added);
    CHECK(add->index == 2);
}

// D-6 (framework-rework): an update keeps the entry's other fields. The content
// contract keeps unknown optional metadata (cases.json
// valid-custom-type-and-metadata), so a rewrite may change only the type and
// path values.
TEST_CASE("E1: updating an entry changes only its type and path values") {
    constexpr const char* catalog = R"({
  "version": 1,
  "assets": [
    { "id": "fixture", "type": "custom.designer-data", "path": "data.txt", "author": "fixture",
      "meta": { "path": "nested/keep.txt", "type": "keep" } },
    { "id": "other", "type": "text", "path": "other.txt" }
  ]
}
)";
    const auto update = RegisterAsset(catalog, {"fixture", "custom.designer-data", "data/v2 \"q\".txt"});
    REQUIRE(update.has_value());
    CHECK_FALSE(update->added);
    CHECK(update->previousPath == std::optional<std::string>("data.txt"));
    std::string expected = catalog;
    const std::size_t at = expected.find("\"data.txt\"");
    expected.replace(at, std::string_view("\"data.txt\"").size(), R"("data/v2 \"q\".txt")");
    CHECK(update->text == expected);
}

TEST_CASE("E2: a type change the policy allows keeps the other fields, in any key order") {
    constexpr const char* catalog =
        "{\"version\":1,\"assets\":[{\"path\" : \"a.txt\",\"author\":\"x\",\"type\":\"text\",\"id\":\"a\"}]}";
    const auto update =
        RegisterAsset(catalog, {"a", "custom.data", "b.txt"}, RegistrationPolicy{.allowTypeChange = true});
    REQUIRE(update.has_value());
    CHECK(update->text ==
          "{\"version\":1,\"assets\":[{\"path\" : \"b.txt\",\"author\":\"x\",\"type\":\"custom.data\",\"id\":\"a\"}]}");
}

TEST_CASE("E3: the contract's metadata case keeps its metadata through an update") {
    std::ifstream fixture(GYO_ASSET_CONTRACT_FIXTURE);
    REQUIRE(fixture.is_open());
    const auto cases = nlohmann::json::parse(fixture).at("cases");
    const auto found = std::find_if(cases.begin(), cases.end(), [](const auto& test) {
        return test.at("name") == "valid-custom-type-and-metadata";
    });
    REQUIRE(found != cases.end());
    const std::string text = found->at("catalogs").at("catalog.json").dump(2);
    const auto update = RegisterAsset(text, {"fixture", "custom.designer-data", "data2.txt"});
    REQUIRE(update.has_value());
    const auto entry = nlohmann::json::parse(update->text).at("assets").at(0);
    CHECK(entry.at("path") == "data2.txt");
    CHECK(entry.at("author") == "fixture");
}

TEST_CASE("E4: a nested member named like type or path before the entry's own is left alone") {
    constexpr const char* catalog =
        R"({"version": 1, "assets": [{"meta": {"type": "keep", "path": "keep.txt"}, "id": "a", "type": "text", "path": "a.txt"}]})";
    const auto update = RegisterAsset(catalog, {"a", "text", "b.txt"});
    REQUIRE(update.has_value());
    CHECK(update->text ==
          R"({"version": 1, "assets": [{"meta": {"type": "keep", "path": "keep.txt"}, "id": "a", "type": "text", "path": "b.txt"}]})");
}
