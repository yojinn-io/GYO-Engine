#pragma once

#include "gyo/AppConfig.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"

#include <SDL3/SDL_filesystem.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fps::tests {

// Tests use the same deployed roots as the app, with no source-tree fallback.
inline std::filesystem::path TestContentRoot(const char* relative) {
    const char* executableRoot = SDL_GetBasePath();
    if (!executableRoot) throw std::runtime_error("Cannot locate the test executable's content root");
    return std::filesystem::path{executableRoot} / relative;
}

inline std::filesystem::path TestAssetRoot() {
    return TestContentRoot(Gyo::AppConfig::Assets);
}

inline std::filesystem::path TestCommonAssetRoot() {
    return TestContentRoot(Gyo::AppConfig::CommonAssets);
}

inline Engine::Asset::AssetCatalog LoadTestCatalog() {
    const auto root = TestAssetRoot();
    Engine::Asset::Resolver::AssetPathResolver::Options options;
    options.assetsRoot = root.string();
    Engine::Asset::Resolver::AssetPathResolver resolver(std::move(options));
    Engine::Asset::Catalog::CatalogParser parser;
    Engine::Asset::AssetCatalog catalog;
    const auto result = catalog.LoadFromFile((root / "asset_catalog.json").string(), parser, resolver);
    if (!result) throw std::runtime_error(result.error().message + " " + result.error().detail);
    return catalog;
}

// Direct fixture reads (JSON mutation and native decoder reference bytes) still
// resolve identity through the catalog; filenames belong only to content data.
inline std::filesystem::path TestAssetPath(std::string_view name) {
    static const auto catalog = LoadTestCatalog();
    const auto* entry = catalog.Find(Engine::Asset::AssetId::FromString(name));
    if (!entry) throw std::runtime_error("Missing test catalog entry: " + std::string{name});
    return entry->resolvedPath;
}

} // namespace fps::tests
