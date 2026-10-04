#include "engine/asset/AssetCatalog.hpp"

#include <fstream>
#include <sstream>

#include "engine/asset/AssetType.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"

namespace Engine::Asset {

    void AssetCatalog::Clear() {
        map_.clear();
    }

    const Catalog::CatalogEntry* AssetCatalog::Find(const AssetId& id) const noexcept {
        auto it = map_.find(id);
        return (it == map_.end()) ? nullptr : &it->second;
    }

    std::vector<const Catalog::CatalogEntry*> AssetCatalog::Entries() const {
        std::vector<const Catalog::CatalogEntry*> out;
        out.reserve(map_.size());
        for (auto& kv : map_) out.push_back(&kv.second);
        return out;
    }

    static Base::Result<std::string, AssetError>
    ReadAllText(std::string_view path) {
        std::ifstream ifs(std::string(path), std::ios::in | std::ios::binary);
        if (!ifs) {
            return Base::Err(AssetError::Make(AssetErrorCode::SourceReadFailed, "AssetCatalog: cannot open catalog file", std::string(path)));
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        return ss.str();
    }

    Base::Result<void, AssetError>
    AssetCatalog::LoadFromFile(std::string_view catalogJsonPath,
                               Catalog::CatalogParser& parser,
                               const Resolver::AssetPathResolver& resolver) {
        Clear();

        auto textR = ReadAllText(catalogJsonPath);
        if (!textR) return Base::Err(std::move(textR.error()));

        auto rawR = parser.Parse(textR.value(), catalogJsonPath);
        if (!rawR) return Base::Err(std::move(rawR.error()));

        return BuildFromRaw_(rawR.value(), resolver);
    }

    Base::Result<void, AssetError>
    AssetCatalog::AppendFromFile(std::string_view catalogJsonPath,
                                Catalog::CatalogParser& parser,
                                const Resolver::AssetPathResolver& resolver) {
        AssetCatalog pending;
        auto loaded = pending.LoadFromFile(catalogJsonPath, parser, resolver);
        if (!loaded) return loaded;
        for (const auto& [id, entry] : pending.map_) {
            if (map_.contains(id)) {
                return Base::Err(AssetError::Make(
                    AssetErrorCode::InvalidCatalogEntry,
                    "AssetCatalog: duplicated id across catalogs", id.debugName));
            }
        }
        // Reserve before transferring nodes; rehashing preserves references to
        // existing elements. Parse/path/duplicate errors never mutate map_.
        map_.reserve(map_.size() + pending.map_.size());
        map_.merge(pending.map_);
        return {};
    }

    Base::Result<void, AssetError>
    AssetCatalog::BuildFromRaw_(const std::vector<Catalog::RawCatalogEntry>& raw,
                                const Resolver::AssetPathResolver& resolver) {
        for (const auto& r : raw) {
            // ここは AssetId/AssetType 実装に合わせる
            // （FromString が無いなら、ここだけ差し替え）
            const AssetId id = AssetId::FromString(r.id);
            const AssetType type = AssetType::FromString(r.type);

            // 重複IDはエラー（Catalogの一意性保証）
            if (map_.find(id) != map_.end()) {
                return Base::Err(AssetError::Make(AssetErrorCode::InvalidCatalogEntry, "AssetCatalog: duplicated id", r.id));
            }

            // ★ここで resolvedPath を確定させる（root脱出などもここで弾く）
            auto rp = resolver.Resolve(r.path);
            if (!rp) {
                // Keep the resolver's code (InvalidPath or PathEscapesRoot); the
                // detail names the catalog entry.
                return Base::Err(AssetError::Make(rp.error().code, rp.error().message, r.id));
            }

            Catalog::CatalogEntry e;
            e.id = id;
            e.type = type;
            e.sourcePath = r.path;
            e.resolvedPath = rp.value().Str();

            map_.emplace(e.id, std::move(e));
        }

        return {};
    }

} // namespace Engine::Asset
