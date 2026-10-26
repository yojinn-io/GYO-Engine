#include "engine/asset/ContentManifest.hpp"

#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "engine/os/Files.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <set>

namespace Engine::Asset {
namespace {

// Engine paths are UTF-8 (docs/architecture/io.md), never the ANSI code page of path::string().
Base::Result<std::string, AssetError> Utf8Path(const std::filesystem::path& path) {
    auto text = Os::Files::PathToUtf8(path);
    if (!text) {
        return Base::Err(AssetError::Make(AssetErrorCode::InvalidPath, "Content path has no UTF-8 form",
                                          Base::CauseDetail(text.error())));
    }
    return std::move(text.value());
}

// A content path is content.json text, so UTF-8; joined as UTF-8, never read in the ANSI code page.
Base::Result<std::filesystem::path, AssetError> ContentPath(const std::string& path) {
    auto native = Os::Files::PathFromUtf8(path);
    if (!native) {
        return Base::Err(AssetError::Make(AssetErrorCode::InvalidPath, "Content path is not UTF-8",
                                          Base::CauseDetail(native.error())));
    }
    return std::move(native.value());
}

bool IsRelativeContentPath(const std::string& path) {
    if (path.empty() || path == "." || path.front() == '/' ||
        path.find('\\') != std::string::npos || path.find(':') != std::string::npos ||
        std::any_of(path.begin(), path.end(), [](unsigned char value) { return value < 32; })) return false;
    // Validate the serialized contract independently of host filesystem rules.
    // Empty segments also reject trailing and repeated separators.
    std::size_t start = 0;
    while (true) {
        const auto end = path.find('/', start);
        const auto part = std::string_view(path).substr(start,
            end == std::string::npos ? std::string::npos : end - start);
        if (part.empty() || part == ".." || part == ".") return false;
        if (end == std::string::npos) return true;
        start = end + 1;
    }
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
    try {
        const auto data = nlohmann::json::parse(text);
        if (!data.is_object() || !data.contains("version") ||
            !data["version"].is_number_integer() || data["version"] != 1 ||
            !data.contains("catalogs") || !data["catalogs"].is_array() ||
            !data.contains("shader_bundles") || !data["shader_bundles"].is_array()) {
            return Base::Err(Invalid(sourceName, "version/catalogs/shader_bundles"));
        }
        ContentManifest manifest;
        std::set<std::string> paths;
        std::set<std::string> names;
        for (const auto& value : data["catalogs"]) {
            if (!value.is_string() || !IsRelativeContentPath(value.get<std::string>()) || value == "content.json" ||
                !paths.insert(value.get<std::string>()).second) {
                return Base::Err(Invalid(sourceName, "catalogs: expected unique root-relative paths"));
            }
            manifest.catalogs.push_back(value.get<std::string>());
        }
        for (const auto& value : data["shader_bundles"]) {
            if (!value.is_object() || !value.contains("name") || !value["name"].is_string() ||
                !value.contains("path") || !value["path"].is_string()) {
                return Base::Err(Invalid(sourceName, "shader_bundles: expected name and path"));
            }
            ContentShaderBundle bundle{value["name"].get<std::string>(), value["path"].get<std::string>()};
            if (!IsBundleName(bundle.name) || !names.insert(bundle.name).second ||
                !IsRelativeContentPath(bundle.path) || Overlaps(bundle.path, "content.json") ||
                std::any_of(paths.begin(), paths.end(), [&](const auto& path) { return Overlaps(bundle.path, path); })) {
                return Base::Err(Invalid(sourceName, "shader_bundles: duplicate identity or invalid path"));
            }
            paths.insert(bundle.path);
            manifest.shaderBundles.push_back(std::move(bundle));
        }
        return std::move(manifest);
    } catch (const nlohmann::json::exception& error) {
        return Base::Err(Invalid(sourceName, std::string{"JSON: "} + error.what()));
    }
}

Base::Result<ContentManifest, AssetError> ContentManifest::Load(
    const std::filesystem::path& assetRoot) {
    auto path = Utf8Path(assetRoot / "content.json");
    if (!path) return Base::Err(std::move(path.error()));
    Loading::NativeFileAssetSource source;
    auto bytes = source.ReadAll(path.value());
    if (!bytes) {
        return Base::Err(AssetError::Make(AssetErrorCode::SourceNotFound, "Cannot open content manifest",
                                          bytes.error().detail));
    }
    const auto& data = bytes.value();
    return Parse(std::string(reinterpret_cast<const char*>(data.data()), data.size()), path.value());
}

Base::Result<AssetCatalog, AssetError> ContentManifest::LoadCatalogs(
    const std::filesystem::path& assetRoot) const {
    AssetCatalog catalog;
    Catalog::CatalogParser parser;
    Resolver::AssetPathResolver::Options options;
    auto root = Utf8Path(assetRoot);
    if (!root) return Base::Err(std::move(root.error()));
    options.assetsRoot = std::move(root.value());
    options.allowAbsolutePath = false;
    options.allowEscapeAssetsRoot = false;
    options.allowSchemes = false;
    Resolver::AssetPathResolver resolver(std::move(options));
    for (const auto& path : catalogs) {
        auto name = ContentPath(path);
        if (!name) return Base::Err(std::move(name.error()));
        auto catalogPath = Utf8Path(assetRoot / name.value());
        if (!catalogPath) return Base::Err(std::move(catalogPath.error()));
        auto loaded = catalog.AppendFromFile(catalogPath.value(), parser, resolver);
        if (!loaded) return Base::Err(std::move(loaded.error()));
    }
    for (const auto* entry : catalog.Entries()) {
        if (!IsRelativeContentPath(entry->sourcePath) || entry->sourcePath == "content.json") {
            return Base::Err(Invalid("content.json", "catalog asset path: " + entry->sourcePath));
        }
        for (const auto& bundle : shaderBundles) {
            if (Overlaps(entry->sourcePath, bundle.path)) {
                return Base::Err(Invalid("content.json", "asset overlaps shader bundle: " + entry->sourcePath));
            }
        }
    }
    return std::move(catalog);
}

} // namespace Engine::Asset
