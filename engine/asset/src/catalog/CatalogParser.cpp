#include "engine/asset/catalog/CatalogParser.hpp"

#include <nlohmann/json.hpp>

namespace Engine::Asset::Catalog {

    Base::Result<std::vector<RawCatalogEntry>, AssetError>
    CatalogParser::Parse(std::string_view catalogText, std::string_view sourceName) {
        using json = nlohmann::json;

        json j;
        try {
            j = json::parse(catalogText.begin(), catalogText.end());
        } catch (...) {
            return Base::Result<std::vector<RawCatalogEntry>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::ParseFailed, "CatalogParser: JSON parse failed", std::string(sourceName)));
        }

        if (!j.is_object() || !j.contains("assets") || !j["assets"].is_array()) {
            return Base::Result<std::vector<RawCatalogEntry>, AssetError>::Err(
                AssetError::Make(AssetErrorCode::ParseFailed, "CatalogParser: invalid schema (need { assets: [] })", std::string(sourceName)));
        }

        std::vector<RawCatalogEntry> out;
        for (const auto& a : j["assets"]) {
            if (!a.is_object()) continue;

            RawCatalogEntry e;
            if (a.contains("id")   && a["id"].is_string())   e.id   = a["id"].get<std::string>();
            if (a.contains("type") && a["type"].is_string()) e.type = a["type"].get<std::string>();
            if (a.contains("path") && a["path"].is_string()) e.path = a["path"].get<std::string>();

            if (e.id.empty() || e.type.empty() || e.path.empty()) {
                return Base::Result<std::vector<RawCatalogEntry>, AssetError>::Err(
                    AssetError::Make(AssetErrorCode::InvalidCatalogEntry, "CatalogParser: missing id/type/path", std::string(sourceName)));
            }

            out.push_back(std::move(e));
        }

        return Base::Result<std::vector<RawCatalogEntry>, AssetError>::Ok(std::move(out));
    }

} // namespace Engine::Asset::Catalog
