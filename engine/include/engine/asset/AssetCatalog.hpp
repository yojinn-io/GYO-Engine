#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "engine/asset/AssetError.hpp"
#include "engine/asset/AssetId.hpp"
#include "engine/base/Result.hpp"
#include "engine/asset/catalog/CatalogEntry.hpp"
#include "engine/base/Error.hpp"

namespace Engine::Asset::Catalog {
    struct RawCatalogEntry;
    class CatalogParser;
}

namespace Engine::Asset::Resolver {
    class AssetPathResolver;
}

namespace Engine::Asset {

    using AssetError = Base::Error<AssetErrorCode>;

    class AssetCatalog final {
    public:
        AssetCatalog() = default;

        void Clear();

        // resolvedPath込みで構築する
        Base::Result<void, AssetError>
        LoadFromFile(std::string_view catalogJsonPath,
                     Catalog::CatalogParser& parser,
                     const Resolver::AssetPathResolver& resolver);

        // Append an independently rooted catalog. Validation failure leaves all
        // existing entries (and pointers returned by Find) unchanged.
        Base::Result<void, AssetError>
        AppendFromFile(std::string_view catalogJsonPath,
                       Catalog::CatalogParser& parser,
                       const Resolver::AssetPathResolver& resolver);

        const Catalog::CatalogEntry* Find(const AssetId& id) const noexcept;

        // 任意：watch登録したい場合などに全件列挙
        std::vector<const Catalog::CatalogEntry*> Entries() const;

    private:
        Base::Result<void, AssetError>
        BuildFromRaw_(const std::vector<Catalog::RawCatalogEntry>& raw,
                      const Resolver::AssetPathResolver& resolver);

    private:
        std::unordered_map<AssetId, Catalog::CatalogEntry> map_;
    };

} // namespace Engine::Asset
