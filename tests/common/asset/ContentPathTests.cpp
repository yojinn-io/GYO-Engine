// AssetId and content paths (declaration 5.2, 5.3 (1), 5.17).
#include <doctest/doctest.h>

#include <string>
#include <unordered_set>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/ContentPath.hpp"

namespace Asset = GYO::Engine::Asset;

TEST_CASE("path: an AssetId is its string") {
    CHECK(Asset::AssetId("char.hero") == Asset::AssetId(std::string("char.hero")));
    CHECK(Asset::AssetId("char.hero") != Asset::AssetId("char.hero2"));
    CHECK(Asset::AssetId("a") < Asset::AssetId("b"));
    std::unordered_set<Asset::AssetId> ids{Asset::AssetId("x"), Asset::AssetId("x"), Asset::AssetId("y")};
    CHECK(ids.size() == 2);
}

TEST_CASE("path: the form of an id") {
    CHECK(Asset::IsValidAssetId("object_fps_pvp.player.character"));
    CHECK(Asset::IsValidAssetId("0-9._-"));
    CHECK(Asset::IsValidAssetId(std::string(128, 'z')));
    CHECK_FALSE(Asset::IsValidAssetId(std::string(129, 'z')));
    CHECK_FALSE(Asset::IsValidAssetId(""));
    CHECK_FALSE(Asset::IsValidAssetId("Char.hero"));
    CHECK_FALSE(Asset::IsValidAssetId("char hero"));
    CHECK_FALSE(Asset::IsValidAssetId("char/hero"));
    CHECK_FALSE(Asset::IsValidAssetId("char:hero"));
    CHECK_FALSE(Asset::IsValidAssetId("caf\xc3\xa9"));
}

TEST_CASE("path: a relative content path") {
    CHECK(Asset::IsRelativeContentPath("a.txt"));
    CHECK(Asset::IsRelativeContentPath("maps/region_0_0.bin"));
    CHECK(Asset::IsRelativeContentPath("a/.hidden/b..c"));
    CHECK_FALSE(Asset::IsRelativeContentPath(""));
    CHECK_FALSE(Asset::IsRelativeContentPath("."));
    CHECK_FALSE(Asset::IsRelativeContentPath("/a.txt"));
    CHECK_FALSE(Asset::IsRelativeContentPath("a/"));
    CHECK_FALSE(Asset::IsRelativeContentPath("a//b"));
    CHECK_FALSE(Asset::IsRelativeContentPath("a/./b"));
    CHECK_FALSE(Asset::IsRelativeContentPath("a/../b"));
    CHECK_FALSE(Asset::IsRelativeContentPath("a\\b"));
    CHECK_FALSE(Asset::IsRelativeContentPath("C:/a"));
    CHECK_FALSE(Asset::IsRelativeContentPath("a\tb"));
    CHECK_FALSE(Asset::IsRelativeContentPath(std::string("a\0b", 3)));
}

TEST_CASE("path: the extension of a path") {
    CHECK(Asset::PathExtension("a/b.TXT") == "txt");
    CHECK(Asset::PathExtension("a/b.tar.gz") == "gz");
    CHECK(Asset::PathExtension("a.v2/b") == "");
    CHECK(Asset::PathExtension("a/b.") == "");
    CHECK(Asset::PathExtension("b") == "");
}

TEST_CASE("path: a referenced path resolves against the parent's directory") {
    const std::string parent = "models/hero/hero.fbx";
    struct Row {
        const char* name;
        const char* reference;
        const char* expected; // nullptr: BadReference
    };
    const Row rows[] = {
        {"sibling", "textures/skin.png", "models/hero/textures/skin.png"},
        {"parent-segment", "../shared/a.png", "models/shared/a.png"},
        {"dot-segment", "./x.png", "models/hero/x.png"},
        {"case-kept", "Tex/A.PNG", "models/hero/Tex/A.PNG"},
        {"to-the-root", "../../top.png", "top.png"},
        {"escapes-root", "../../../a.png", nullptr},
        {"absolute", "/abs/a.png", nullptr},
        {"absolute-then-up", "/../a.png", nullptr},
        {"backslash", "tex\\a.png", nullptr},
        {"drive", "C:/a.png", nullptr},
        {"repeated-slash", "a//b.png", nullptr},
        {"empty", "", nullptr},
        {"nothing-left", "../..", nullptr},
    };
    for (const Row& row : rows) {
        const std::string name = row.name;
        CAPTURE(name);
        const auto resolved = Asset::ResolveReferencePath(parent, row.reference);
        if (row.expected == nullptr) {
            CHECK_MESSAGE(!resolved.has_value(), "resolved " << name << " to " << resolved.value_or(""));
        } else {
            CHECK_MESSAGE(resolved == std::optional<std::string>(row.expected), "wrong resolution of " << name);
        }
    }
    CHECK(Asset::ResolveReferencePath("top.fbx", "a.png") == std::optional<std::string>("a.png"));
    CHECK_FALSE(Asset::ResolveReferencePath("top.fbx", "../a.png").has_value());
}
