#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "engine/asset/AssetError.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace Engine::Asset::Catalog {
    using AssetError = Base::Error<AssetErrorCode>;

    struct RawCatalogEntry final {
        std::string id;    // stringのまま（ここではAssetIdに変換しない）
        std::string type;  // stringのまま
        std::string path;  // sourcePath（相対想定）
    };

    class CatalogParser final {
    public:
        // catalogText: JSON全文
        Base::Result<std::vector<RawCatalogEntry>, AssetError>
        Parse(std::string_view catalogText, std::string_view sourceName = "asset_catalog.json");
    };

} // namespace Engine::Asset::Catalog
