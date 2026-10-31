#pragma once

#include "gyo/framework/FrameworkError.hpp"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/asset/AssetCatalog.hpp"

namespace Gyo::Tools::Framework {

struct CatalogAsset final {
    std::string id;
    std::string type;
    std::string sourcePath;
    std::filesystem::path resolvedPath;
};

// Authoring-only, read-only view (Design Invariant 4). There is intentionally
// no Add/Remove/Save API: writing a catalog is CatalogRegistration's, and only
// a tool whose own Design Invariants allow it calls that.
class ReadOnlyAssetCatalog final {
public:
    // A content root (content.json and every catalog it declares).
    [[nodiscard]] Result<void, CatalogError> MountRoot(const std::filesystem::path& assetRoot);
    // One explicit catalog whose entries resolve under assetRoot.
    [[nodiscard]] Result<void, CatalogError> Mount(
        const std::filesystem::path& catalogPath,
        const std::filesystem::path& assetRoot);
    void Unmount() noexcept;

    [[nodiscard]] bool IsMounted() const noexcept;
    // Empty for a complete content-root mount; otherwise the explicit catalog.
    [[nodiscard]] const std::filesystem::path& CatalogPath() const noexcept;
    [[nodiscard]] const std::filesystem::path& AssetRoot() const noexcept;
    [[nodiscard]] std::span<const CatalogAsset> Assets() const noexcept;
    [[nodiscard]] const CatalogAsset* Find(std::string_view id) const noexcept;
    [[nodiscard]] const Engine::Asset::AssetCatalog& ParsedCatalog() const noexcept;

private:
    void Commit(Engine::Asset::AssetCatalog catalog,
        const std::filesystem::path& catalogPath, const std::filesystem::path& assetRoot);
    std::filesystem::path catalogPath_;
    std::filesystem::path assetRoot_;
    std::vector<CatalogAsset> assets_;
    Engine::Asset::AssetCatalog catalog_;
};

} // namespace Gyo::Tools::Framework
