#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/CatalogFailure.hpp"

// The catalog, a Data Contract owned by asset (declaration
// docs/architecture/target/v2/asset-declaration.zh-Hant.md, 5.3; Design
// Invariant AS-12). The entry file at the content root is content.json:
//
//     {"version": 1, "catalogs": ["asset_catalog.json"], "shader_bundles": ...}
//
// and each catalog it lists maps ids to content:
//
//     {"version": 1, "assets": [{"id": "char.hero", "type": "text",
//                                "path": "characters/hero.json",
//                                "format": "json", "settings": {}}]}
//
// "format" and "settings" are optional. "shader_bundles" belongs to render: it
// is optional, kept as written and not checked. Top-level members and entry
// members this version does not know are kept as written and not read, so
// content written for a later version with new optional members still reads.
//
// Reading checks the (A) rules of 5.3 and stops at the first one broken. The
// same checks run before writing, so what is written reads back equal. Hand
// written, script written and tool written content are equally valid. Checks
// against registered loaders (the (B) rules) belong to the store, not here.

namespace GYO::Engine::Asset {

inline constexpr std::int64_t kContentManifestVersion = 1;
inline constexpr std::int64_t kCatalogVersion = 1;

// A member kept as written: its key and its value as compact JSON text.
struct PreservedMember final {
    std::string key;
    std::string json;

    [[nodiscard]] friend bool operator==(const PreservedMember&, const PreservedMember&) = default;
};

struct CatalogEntry final {
    AssetId id;
    // The type name (AssetTypeName) of the C++ type the content loads as.
    std::string type;
    // A relative content path.
    std::string path;
    // As written; EffectiveFormat gives the format a loader is chosen by.
    std::optional<std::string> format;
    // A JSON object as compact text, when written.
    std::optional<std::string> settings;
    // Members other than id, type, path, format and settings, in document order.
    std::vector<PreservedMember> extra;

    [[nodiscard]] friend bool operator==(const CatalogEntry&, const CatalogEntry&) = default;
};

// [a-z0-9]+: the form of a written "format" (rule 7).
[[nodiscard]] bool IsValidFormat(std::string_view format) noexcept;

// "format" when written, otherwise the path's extension (PathExtension).
[[nodiscard]] std::string EffectiveFormat(const CatalogEntry& entry);

struct Catalog final {
    std::vector<CatalogEntry> assets;
    // Top-level members other than version and assets, in document order.
    std::vector<PreservedMember> extra;

    [[nodiscard]] friend bool operator==(const Catalog&, const Catalog&) = default;
};

struct ContentManifest final {
    // Relative content paths of the catalogs, in order.
    std::vector<std::string> catalogs;
    // The "shader_bundles" value as compact JSON text, when written.
    std::optional<std::string> shaderBundles;
    // Top-level members other than version, catalogs and shader_bundles.
    std::vector<PreservedMember> extra;

    [[nodiscard]] friend bool operator==(const ContentManifest&, const ContentManifest&) = default;
};

// One content root: its content.json and the catalogs it lists, catalogs[i]
// being the file manifest.catalogs[i].
struct Content final {
    ContentManifest manifest;
    std::vector<Catalog> catalogs;

    [[nodiscard]] friend bool operator==(const Content&, const Content&) = default;
};

// sourceName goes into failure details (for example "content.json").
[[nodiscard]] CatalogResult<ContentManifest> ParseContentManifest(std::string_view text,
                                                                  std::string_view sourceName = {});
// Rules 2 and 4 to 7, and rule 3 within this catalog.
[[nodiscard]] CatalogResult<Catalog> ParseCatalog(std::string_view text, std::string_view sourceName = {});

// The rules on values already in memory (what a tool built).
[[nodiscard]] CatalogResult<void> ValidateContentManifest(const ContentManifest& manifest);
[[nodiscard]] CatalogResult<void> ValidateCatalog(const Catalog& catalog, std::string_view sourceName = {});
// The manifest, every catalog, one catalog per listed path, and rule 3 across
// all catalogs.
[[nodiscard]] CatalogResult<void> ValidateContent(const Content& content);

// Validates, then writes compact JSON.
[[nodiscard]] CatalogResult<std::string> WriteContentManifest(const ContentManifest& manifest);
[[nodiscard]] CatalogResult<std::string> WriteCatalog(const Catalog& catalog);

// Reads contentRoot/content.json and every catalog it lists, and checks all of
// rules 1 to 7. contentRoot is a native path in UTF-8.
[[nodiscard]] CatalogResult<Content> ReadContent(std::string_view contentRoot);

} // namespace GYO::Engine::Asset
