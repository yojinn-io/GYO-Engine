#include "engine/asset/ContentManifest.hpp"

#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

namespace Engine::Asset {
namespace {

bool IsRelativeContentPath(const std::string& path) {
    if (path.empty() || path == "." || path.front() == '/' ||
        path.find('\\') != std::string::npos || path.find(':') != std::string::npos ||
        std::any_of(path.begin(), path.end(), [](unsigned char value) { return value < 32; })) return false;
    const std::filesystem::path relative(path);
    for (const auto& part : relative) {
        if (part == ".." || part == ".") return false;
    }
    return relative.generic_string() == path && !relative.has_root_path();
}

bool IsBundleName(const std::string& name) {
    if (name.empty() || name.front() < 'a' || name.front() > 'z') return false;
    return std::all_of(name.begin(), name.end(), [](unsigned char value) {
        return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '_';
    });
}

bool Overlaps(const std::string& left, const std::string& right) {
    return left == right || left.starts_with(right + '/') || right.starts_with(left + '/');
}

AssetError Invalid(const std::string_view source, const std::string& field) {
    return AssetError::Make(AssetErrorCode::ParseFailed,
        "ContentManifest: invalid " + field, std::string(source));
}

} // namespace

Base::Result<ContentManifest, AssetError> ContentManifest::Parse(
    const std::string_view text, const std::string_view sourceName) {
    using Result = Base::Result<ContentManifest, AssetError>;
    try {
        const auto data = nlohmann::json::parse(text);
        if (!data.is_object() || !data.contains("version") ||
            !data["version"].is_number_integer() || data["version"] != 1 ||
            !data.contains("catalogs") || !data["catalogs"].is_array() ||
            !data.contains("shader_bundles") || !data["shader_bundles"].is_array()) {
            return Result::Err(Invalid(sourceName, "version/catalogs/shader_bundles"));
        }
        ContentManifest manifest;
        std::set<std::string> paths;
        std::set<std::string> names;
        for (const auto& value : data["catalogs"]) {
            if (!value.is_string() || !IsRelativeContentPath(value.get<std::string>()) || value == "content.json" ||
                !paths.insert(value.get<std::string>()).second) {
                return Result::Err(Invalid(sourceName, "catalogs: expected unique root-relative paths"));
            }
            manifest.catalogs.push_back(value.get<std::string>());
        }
        for (const auto& value : data["shader_bundles"]) {
            if (!value.is_object() || !value.contains("name") || !value["name"].is_string() ||
                !value.contains("path") || !value["path"].is_string()) {
                return Result::Err(Invalid(sourceName, "shader_bundles: expected name and path"));
            }
            ContentShaderBundle bundle{value["name"].get<std::string>(), value["path"].get<std::string>()};
            if (!IsBundleName(bundle.name) || !names.insert(bundle.name).second ||
                !IsRelativeContentPath(bundle.path) || Overlaps(bundle.path, "content.json") ||
                std::any_of(paths.begin(), paths.end(), [&](const auto& path) { return Overlaps(bundle.path, path); })) {
                return Result::Err(Invalid(sourceName, "shader_bundles: duplicate identity or invalid path"));
            }
            paths.insert(bundle.path);
            manifest.shaderBundles.push_back(std::move(bundle));
        }
        return Result::Ok(std::move(manifest));
    } catch (const nlohmann::json::exception& error) {
        return Result::Err(Invalid(sourceName, std::string{"JSON: "} + error.what()));
    }
}

Base::Result<ContentManifest, AssetError> ContentManifest::Load(
    const std::filesystem::path& assetRoot) {
    const auto path = assetRoot / "content.json";
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return Base::Result<ContentManifest, AssetError>::Err(
        AssetError::Make(AssetErrorCode::SourceNotFound, "Cannot open content manifest", path.string()));
    const std::string text{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    return Parse(text, path.string());
}

Base::Result<AssetCatalog, AssetError> ContentManifest::LoadCatalogs(
    const std::filesystem::path& assetRoot) const {
    AssetCatalog catalog;
    Catalog::CatalogParser parser;
    Resolver::AssetPathResolver::Options options;
    options.assetsRoot = assetRoot.string();
    options.allowAbsolutePath = false;
    options.allowEscapeAssetsRoot = false;
    options.allowSchemes = false;
    Resolver::AssetPathResolver resolver(std::move(options));
    for (const auto& path : catalogs) {
        auto loaded = catalog.AppendFromFile((assetRoot / path).string(), parser, resolver);
        if (!loaded) return Base::Result<AssetCatalog, AssetError>::Err(std::move(loaded.error()));
    }
    for (const auto* entry : catalog.Entries()) {
        if (!IsRelativeContentPath(entry->sourcePath) || entry->sourcePath == "content.json") {
            return Base::Result<AssetCatalog, AssetError>::Err(Invalid("content.json", "catalog asset path: " + entry->sourcePath));
        }
        for (const auto& bundle : shaderBundles) {
            if (Overlaps(entry->sourcePath, bundle.path)) {
                return Base::Result<AssetCatalog, AssetError>::Err(Invalid("content.json", "asset overlaps shader bundle: " + entry->sourcePath));
            }
        }
    }
    return Base::Result<AssetCatalog, AssetError>::Ok(std::move(catalog));
}

} // namespace Engine::Asset
