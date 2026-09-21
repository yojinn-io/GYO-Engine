#pragma once

#include "engine/asset/AssetCatalog.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Asset {

// Runtime content has one root. Build-only shader sources are deliberately not
// represented here; deployed manifests contain only bundle identities/paths.
struct ContentShaderBundle final {
    std::string name;
    std::string path;
};

struct ContentManifest final {
    std::vector<std::string> catalogs;
    std::vector<ContentShaderBundle> shaderBundles;

    [[nodiscard]] static Base::Result<ContentManifest, AssetError> Parse(
        std::string_view text, std::string_view sourceName = "content.json");
    [[nodiscard]] static Base::Result<ContentManifest, AssetError> Load(
        const std::filesystem::path& assetRoot);
    [[nodiscard]] Base::Result<AssetCatalog, AssetError> LoadCatalogs(
        const std::filesystem::path& assetRoot) const;
};

} // namespace Engine::Asset
