#include "gyo/ui_editor/ReadOnlyAssetCatalog.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"

#include <algorithm>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

template <typename Error>
[[nodiscard]] std::string DescribeError(const Error& error) {
    std::string result = error.message;
    if (!error.detail.empty()) {
        result += ": " + error.detail;
    }
    return result;
}

} // namespace

bool ReadOnlyAssetCatalog::Mount(
    const std::filesystem::path& catalogPath,
    const std::filesystem::path& assetRoot,
    std::string& error) {
    error.clear();
    if (catalogPath.empty() || assetRoot.empty()) {
        error = "catalog path and asset root are required";
        return false;
    }

    Engine::Asset::Resolver::AssetPathResolver::Options resolverOptions;
    resolverOptions.assetsRoot = assetRoot.string();
    resolverOptions.allowAbsolutePath = false;
    resolverOptions.allowEscapeAssetsRoot = false;
    Engine::Asset::Resolver::AssetPathResolver resolver(
        std::move(resolverOptions));
    Engine::Asset::Catalog::CatalogParser parser;
    Engine::Asset::AssetCatalog catalog;
    const auto loaded =
        catalog.LoadFromFile(catalogPath.string(), parser, resolver);
    if (!loaded) {
        error = DescribeError(loaded.error());
        return false;
    }

    std::vector<CatalogAsset> mounted;
    mounted.reserve(catalog.Entries().size());
    for (const Engine::Asset::Catalog::CatalogEntry* entry : catalog.Entries()) {
        if (entry == nullptr) {
            continue;
        }
        mounted.push_back({
            entry->id.debugName,
            entry->type.debugName,
            entry->sourcePath,
            std::filesystem::path{entry->resolvedPath},
        });
    }
    std::ranges::sort(mounted, {}, &CatalogAsset::id);

    catalogPath_ = std::filesystem::absolute(catalogPath).lexically_normal();
    assetRoot_ = std::filesystem::absolute(assetRoot).lexically_normal();
    assets_ = std::move(mounted);
    return true;
}

void ReadOnlyAssetCatalog::Unmount() noexcept {
    catalogPath_.clear();
    assetRoot_.clear();
    assets_.clear();
}

bool ReadOnlyAssetCatalog::IsMounted() const noexcept {
    return !catalogPath_.empty();
}

const std::filesystem::path& ReadOnlyAssetCatalog::CatalogPath() const noexcept {
    return catalogPath_;
}

const std::filesystem::path& ReadOnlyAssetCatalog::AssetRoot() const noexcept {
    return assetRoot_;
}

std::span<const CatalogAsset> ReadOnlyAssetCatalog::Assets() const noexcept {
    return assets_;
}

const CatalogAsset* ReadOnlyAssetCatalog::Find(
    const std::string_view id) const noexcept {
    const auto found = std::ranges::lower_bound(assets_, id, {}, &CatalogAsset::id);
    return found != assets_.end() && found->id == id ? &*found : nullptr;
}

} // namespace Gyo::Tools::UiEditor
