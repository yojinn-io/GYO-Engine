#pragma once

#include <string>

#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetType.hpp"


namespace Engine::Asset::Catalog {

    struct CatalogEntry final {
        AssetId id{};
        AssetType type{};
        std::string sourcePath;   // assets/ からの相対パスを想定（例: "textures/player.png"）
        std::string resolvedPath; //

        // 将来拡張用（必要になったら足す）
        // std::string variant;   // 例: "hd", "sd"
        // uint64_t    fileSize = 0;
    };

} // namespace Engine::Asset::Catalog
