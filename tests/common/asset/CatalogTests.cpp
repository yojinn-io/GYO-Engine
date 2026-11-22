// The catalog Data Contract (declaration 5.3; Design Invariant AS-12): every invalid example of
// contract/invalid is refused with the code of the rule it breaks, every valid example reads, master's
// content passes as it was, and what is written reads back equal.
#include <doctest/doctest.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "engine/asset/Catalog.hpp"

namespace Asset = GYO::Engine::Asset;
using Asset::CatalogFailureCode;

namespace {

std::string Example(const std::string& name) { return std::string(GYO_ASSET_CONTRACT_DIR) + "/" + name; }

struct InvalidExample {
    const char* name;
    CatalogFailureCode code;
};

// One row per directory of contract/invalid; the prefix names the (A) rule of 5.3 it breaks.
const InvalidExample kInvalid[] = {
    {"r1-not-json", CatalogFailureCode::NotJson},
    {"r1-top-not-object", CatalogFailureCode::BadShape},
    {"r1-version-missing", CatalogFailureCode::BadVersion},
    {"r1-version-not-integer", CatalogFailureCode::BadVersion},
    {"r1-version-unknown", CatalogFailureCode::UnknownVersion},
    {"r1-catalogs-not-array", CatalogFailureCode::BadShape},
    {"r1-catalog-path-escapes", CatalogFailureCode::BadCatalogPath},
    {"r1-catalog-is-content-json", CatalogFailureCode::BadCatalogPath},
    {"r1-catalog-listed-twice", CatalogFailureCode::DuplicateCatalog},
    {"r1-catalog-unreadable", CatalogFailureCode::Unreadable},
    {"r2-not-json", CatalogFailureCode::NotJson},
    {"r2-version-unknown", CatalogFailureCode::UnknownVersion},
    {"r2-assets-not-array", CatalogFailureCode::BadShape},
    {"r2-entry-not-object", CatalogFailureCode::BadShape},
    {"r2-id-missing", CatalogFailureCode::BadShape},
    {"r2-type-empty", CatalogFailureCode::BadShape},
    {"r3-id-twice-in-one-catalog", CatalogFailureCode::DuplicateId},
    {"r3-id-in-two-catalogs", CatalogFailureCode::DuplicateId},
    {"r4-id-uppercase", CatalogFailureCode::BadId},
    {"r4-id-too-long", CatalogFailureCode::BadId},
    {"r5-path-escapes", CatalogFailureCode::BadPath},
    {"r5-path-backslash", CatalogFailureCode::BadPath},
    {"r5-path-absolute", CatalogFailureCode::BadPath},
    {"r5-path-is-content-json", CatalogFailureCode::BadPath},
    {"r6-no-extension", CatalogFailureCode::NoFormat},
    {"r6-dot-only-in-directory", CatalogFailureCode::NoFormat},
    {"r7-format-uppercase", CatalogFailureCode::BadFormat},
    {"r7-format-not-string", CatalogFailureCode::BadFormat},
    {"r7-settings-not-object", CatalogFailureCode::SettingsNotObject},
};

const char* const kValid[] = {
    "type-without-loader", "two-ids-one-file", "unknown-members-kept", "format-and-settings", "master-object-fps-pvp",
};

Asset::Content Read(const std::string& name) {
    auto content = Asset::ReadContent(Example("valid/" + name));
    if (!content) FAIL("reading valid/" << name << ": " << GYO::Engine::Base::Describe(content.error()));
    return std::move(*content);
}

const Asset::CatalogEntry* Find(const Asset::Catalog& catalog, const std::string& id) {
    const auto found = std::find_if(catalog.assets.begin(), catalog.assets.end(),
                                    [&](const Asset::CatalogEntry& entry) { return entry.id.Text() == id; });
    return found == catalog.assets.end() ? nullptr : &*found;
}

} // namespace

TEST_CASE("catalog: every invalid example is refused with the code of its rule") {
    for (const InvalidExample& example : kInvalid) {
        const std::string name = example.name;
        CAPTURE(name);
        auto content = Asset::ReadContent(Example("invalid/" + name));
        if (content) {
            FAIL_CHECK("accepted invalid example " << name);
            continue;
        }
        CHECK_MESSAGE(std::string(Asset::ToString(content.error().code)) == Asset::ToString(example.code),
                      "wrong code for invalid example " << name << ": "
                                                     << GYO::Engine::Base::Describe(content.error()));
    }
}

TEST_CASE("catalog: what the rules deliberately accept reads") {
    SUBCASE("a type no loader is registered for") {
        const Asset::Content content = Read("type-without-loader");
        REQUIRE(content.catalogs.size() == 1);
        REQUIRE(content.catalogs[0].assets.size() == 1);
        CHECK(content.catalogs[0].assets[0].type == "made_up_type");
    }
    SUBCASE("two ids naming one file") {
        const Asset::Content content = Read("two-ids-one-file");
        REQUIRE(content.catalogs[0].assets.size() == 2);
        CHECK(content.catalogs[0].assets[0].path == content.catalogs[0].assets[1].path);
    }
    SUBCASE("unknown members are kept, not read") {
        const Asset::Content content = Read("unknown-members-kept");
        REQUIRE(content.manifest.extra.size() == 1);
        CHECK(content.manifest.extra[0] == Asset::PreservedMember{"future_manifest_key", R"({"a":[1,2.5,"x"]})"});
        CHECK_FALSE(content.manifest.shaderBundles.has_value());
        REQUIRE(content.catalogs[0].extra.size() == 1);
        CHECK(content.catalogs[0].extra[0] ==
              Asset::PreservedMember{"generator", R"({"tool":"audio_editor","rev":7})"});
        const Asset::CatalogEntry& entry = content.catalogs[0].assets[0];
        REQUIRE(entry.extra.size() == 2);
        CHECK(entry.extra[0] == Asset::PreservedMember{"note", R"("hand written")"});
        CHECK(entry.extra[1] == Asset::PreservedMember{"tags", R"(["a","b"])"});
    }
    SUBCASE("format, settings, the extension and the id at its limits") {
        const Asset::Content content = Read("format-and-settings");
        const Asset::Catalog& catalog = content.catalogs[0];
        const Asset::CatalogEntry* region = Find(catalog, "sample.region");
        REQUIRE(region != nullptr);
        CHECK(Asset::EffectiveFormat(*region) == "gyoreg2");
        const Asset::CatalogEntry* upper = Find(catalog, "sample.upper");
        REQUIRE(upper != nullptr);
        CHECK_FALSE(upper->format.has_value());
        CHECK_MESSAGE(Asset::EffectiveFormat(*upper) == "txt", "the extension is not lowercased");
        const Asset::CatalogEntry* engine = Find(catalog, "sample.engine");
        REQUIRE(engine != nullptr);
        CHECK(Asset::EffectiveFormat(*engine) == "gyotxt");
        REQUIRE(engine->settings.has_value());
        CHECK(*engine->settings == R"({"trim":true,"limit":10})");
        CHECK_MESSAGE(Find(catalog, std::string(128, 'a')) != nullptr, "an id of 128 characters is refused");
        CHECK(Find(catalog, "0-9._-") != nullptr);
    }
}

TEST_CASE("catalog: master's content passes as it was") {
    const Asset::Content content = Read("master-object-fps-pvp");
    CHECK(content.manifest.catalogs == std::vector<std::string>{"asset_catalog.json"});
    REQUIRE(content.manifest.shaderBundles.has_value());
    CHECK(content.manifest.shaderBundles->find(R"("source":"../../engine/render/shaders/builtin/bundle.json")") !=
          std::string::npos);
    REQUIRE(content.catalogs.size() == 1);
    const Asset::Catalog& catalog = content.catalogs[0];
    CHECK(catalog.assets.size() == 34);
    CHECK(std::count_if(catalog.assets.begin(), catalog.assets.end(),
                        [](const Asset::CatalogEntry& entry) { return entry.type == "text"; }) == 20);
    for (const Asset::CatalogEntry& entry : catalog.assets) {
        CAPTURE(entry.id.Text());
        CHECK_FALSE(entry.format.has_value());
        const std::string format = Asset::EffectiveFormat(entry);
        CHECK((format == "json" || format == "png" || format == "wav" || format == "fbx" || format == "ttf"));
    }
    const Asset::CatalogEntry* first = Find(catalog, "object_fps_pvp.player.presentation");
    REQUIRE(first != nullptr);
    CHECK(first->path == "characters/player/presentation.json");
}

TEST_CASE("catalog: what is written reads back equal") {
    for (const std::string name : kValid) {
        CAPTURE(name);
        const Asset::Content content = Read(name);
        auto manifestText = Asset::WriteContentManifest(content.manifest);
        REQUIRE(manifestText);
        auto manifest = Asset::ParseContentManifest(*manifestText, "content.json");
        REQUIRE(manifest);
        CHECK_MESSAGE(*manifest == content.manifest, "content.json does not read back equal: " << *manifestText);
        for (std::size_t index = 0; index < content.catalogs.size(); ++index) {
            auto catalogText = Asset::WriteCatalog(content.catalogs[index]);
            REQUIRE(catalogText);
            auto catalog = Asset::ParseCatalog(*catalogText, content.manifest.catalogs[index]);
            REQUIRE_MESSAGE(catalog, "a written catalog does not read back: " << *catalogText);
            CHECK_MESSAGE(*catalog == content.catalogs[index], "the catalog does not read back equal: " << *catalogText);
            // Writing what was read back gives the same text.
            auto again = Asset::WriteCatalog(*catalog);
            REQUIRE(again);
            CHECK(*again == *catalogText);
        }
    }
}

TEST_CASE("catalog: writing checks the same rules as reading") {
    Asset::Catalog catalog;
    catalog.assets.push_back(Asset::CatalogEntry{Asset::AssetId("Bad Id"), "text", "docs/a.txt", {}, {}, {}});
    auto written = Asset::WriteCatalog(catalog);
    REQUIRE_MESSAGE(!written, "a catalog that breaks rule 4 was written");
    CHECK(written.error().code == CatalogFailureCode::BadId);

    Asset::Content content;
    content.manifest.catalogs = {"a.json", "b.json"};
    content.catalogs.resize(2);
    content.catalogs[0].assets.push_back(Asset::CatalogEntry{Asset::AssetId("same"), "text", "a.txt", {}, {}, {}});
    content.catalogs[1].assets.push_back(Asset::CatalogEntry{Asset::AssetId("same"), "text", "b.txt", {}, {}, {}});
    auto valid = Asset::ValidateContent(content);
    REQUIRE(!valid);
    CHECK(valid.error().code == CatalogFailureCode::DuplicateId);
}

TEST_CASE("catalog: one catalog read alone is checked for repeated ids") {
    // A tool reads one catalog without its content.json; rule 3 inside that catalog still holds.
    std::ifstream in(Example("invalid/r3-id-twice-in-one-catalog/asset_catalog.json"), std::ios::binary);
    REQUIRE(in);
    std::stringstream text;
    text << in.rdbuf();
    auto catalog = Asset::ParseCatalog(text.str(), "asset_catalog.json");
    REQUIRE_MESSAGE(!catalog, "a catalog read alone accepts a repeated id");
    CHECK(catalog.error().code == CatalogFailureCode::DuplicateId);
}
