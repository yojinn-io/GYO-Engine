#include "gyo/framework/ReadOnlyAssetCatalog.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/os/Files.hpp"
#include "gyo/framework/FileService.hpp"

#include <algorithm>
#include <utility>

namespace Gyo::Tools::Framework {
namespace {

[[nodiscard]] CatalogError LoadFailed(std::string message, std::string detail = {}) {
    return CatalogError::Make(CatalogErrorCode::LoadFailed, std::move(message), std::move(detail));
}

[[nodiscard]] CatalogError LoadFailed(const Engine::Asset::AssetError& cause) {
    return LoadFailed(cause.message, Engine::Base::CauseDetail(cause));
}

} // namespace

Result<void, CatalogError> ReadOnlyAssetCatalog::MountRoot(
    const std::filesystem::path& assetRoot) {
    if (assetRoot.empty()) return Engine::Base::Err(LoadFailed("asset root is required"));
    const auto root = AbsolutePath(assetRoot);
    const auto manifest = Engine::Asset::ContentManifest::Load(root);
    if (!manifest) return Engine::Base::Err(LoadFailed(manifest.error()));
    auto catalog = manifest.value().LoadCatalogs(root);
    if (!catalog) return Engine::Base::Err(LoadFailed(catalog.error()));
    Commit(std::move(catalog).value(), {}, root);
    return {};
}

Result<void, CatalogError> ReadOnlyAssetCatalog::Mount(
    const std::filesystem::path& catalogPath,
    const std::filesystem::path& assetRoot) {
    if (catalogPath.empty() || assetRoot.empty()) {
        return Engine::Base::Err(LoadFailed("catalog path and asset root are required"));
    }

    Engine::Asset::Resolver::AssetPathResolver::Options resolverOptions;
    const auto root = AbsolutePath(assetRoot);
    // Engine paths are UTF-8 (docs/architecture/io.md), not path::string()'s ANSI code page.
    auto rootText = Engine::Os::Files::PathToUtf8(root);
    if (!rootText) return Engine::Base::Err(LoadFailed("asset root has no UTF-8 form", Engine::Base::CauseDetail(rootText.error())));
    auto catalogText = Engine::Os::Files::PathToUtf8(catalogPath);
    if (!catalogText) return Engine::Base::Err(LoadFailed("catalog path has no UTF-8 form", Engine::Base::CauseDetail(catalogText.error())));
    resolverOptions.assetsRoot = std::move(rootText.value());
    resolverOptions.allowAbsolutePath = false;
    resolverOptions.allowEscapeAssetsRoot = false;
    resolverOptions.allowSchemes = false;
    Engine::Asset::Resolver::AssetPathResolver resolver(
        std::move(resolverOptions));
    Engine::Asset::Catalog::CatalogParser parser;
    Engine::Asset::AssetCatalog catalog;
    const auto loaded =
        catalog.LoadFromFile(catalogText.value(), parser, resolver);
    if (!loaded) return Engine::Base::Err(LoadFailed(loaded.error()));

    Commit(std::move(catalog), AbsolutePath(catalogPath), root);
    return {};
}

void ReadOnlyAssetCatalog::Commit(Engine::Asset::AssetCatalog catalog,
    const std::filesystem::path& catalogPath, const std::filesystem::path& assetRoot) {
    std::vector<CatalogAsset> mounted;
    mounted.reserve(catalog.Entries().size());
    for (const Engine::Asset::Catalog::CatalogEntry* entry : catalog.Entries()) {
        if (entry == nullptr) {
            continue;
        }
        // The resolver gives UTF-8 built from the UTF-8 root, so it converts back;
        // never through the ANSI code page.
        auto resolved = Engine::Os::Files::PathFromUtf8(entry->resolvedPath);
        mounted.push_back({
            entry->id.debugName,
            entry->type.debugName,
            entry->sourcePath,
            resolved ? std::move(*resolved) : std::filesystem::path{},
        });
    }
    std::ranges::sort(mounted, {}, &CatalogAsset::id);

    catalogPath_ = catalogPath;
    assetRoot_ = assetRoot;
    assets_ = std::move(mounted);
    catalog_ = std::move(catalog);
}

void ReadOnlyAssetCatalog::Unmount() noexcept {
    catalogPath_.clear();
    assetRoot_.clear();
    assets_.clear();
    catalog_.Clear();
}

bool ReadOnlyAssetCatalog::IsMounted() const noexcept {
    return !assetRoot_.empty();
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

const Engine::Asset::AssetCatalog& ReadOnlyAssetCatalog::ParsedCatalog() const noexcept {
    return catalog_;
}

} // namespace Gyo::Tools::Framework
