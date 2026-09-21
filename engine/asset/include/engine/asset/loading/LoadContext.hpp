#pragma once

#include <cstdint>
#include <string>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/AssetRequest.hpp"

namespace Engine::Asset::Core { class AssetStatistics; }

namespace Engine::Asset::Loading {

    // LoadContext：Pipeline/Loader に渡す実行時コンテキスト（DTO）
    // - AssetCatalog から引いた resolvedPath を入れて渡す
    // - request は任意（nullptr可）
    // - statistics は任意（nullptr可）
    struct LoadContext final {
        AssetId id{};
        AssetType type{};
        std::string resolvedPath;          // "assets/..." 形式（AssetCatalogで解決済み推奨）

        const AssetRequest* request = nullptr;
        Core::AssetStatistics* statistics = nullptr;

        std::uint64_t nowFrame = 0;        // 統計/寿命用に入れておくと便利（任意）

        // 便利関数（デバッグ用）
        bool HasPath() const noexcept { return !resolvedPath.empty(); }
    };

} // namespace Engine::Asset::Loading
