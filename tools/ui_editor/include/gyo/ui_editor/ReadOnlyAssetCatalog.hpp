#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Gyo::Tools::UiEditor {

struct CatalogAsset final {
    std::string id;
    std::string type;
    std::string sourcePath;
    std::filesystem::path resolvedPath;
};

// Authoring-only, read-only view. There is intentionally no Add/Remove/Save API:
// publishing the exported UI JSON and registering it remains an app-author step.
class ReadOnlyAssetCatalog final {
public:
    [[nodiscard]] bool Mount(
        const std::filesystem::path& catalogPath,
        const std::filesystem::path& assetRoot,
        std::string& error);
    void Unmount() noexcept;

    [[nodiscard]] bool IsMounted() const noexcept;
    [[nodiscard]] const std::filesystem::path& CatalogPath() const noexcept;
    [[nodiscard]] const std::filesystem::path& AssetRoot() const noexcept;
    [[nodiscard]] std::span<const CatalogAsset> Assets() const noexcept;
    [[nodiscard]] const CatalogAsset* Find(std::string_view id) const noexcept;

private:
    std::filesystem::path catalogPath_;
    std::filesystem::path assetRoot_;
    std::vector<CatalogAsset> assets_;
};

} // namespace Gyo::Tools::UiEditor
