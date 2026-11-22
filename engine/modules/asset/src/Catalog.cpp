#include "engine/asset/Catalog.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "engine/asset/ContentPath.hpp"
#include "engine/asset/ReadWholeFile.hpp"
#include "engine/json/Document.hpp"
#include "engine/json/Writer.hpp"

namespace GYO::Engine::Asset {
namespace {

constexpr std::array<std::string_view, 3> kManifestKeys{"version", "catalogs", "shader_bundles"};
constexpr std::array<std::string_view, 2> kCatalogKeys{"version", "assets"};
constexpr std::array<std::string_view, 5> kEntryKeys{"id", "type", "path", "format", "settings"};

template <std::size_t N>
bool IsKnown(const std::array<std::string_view, N>& keys, const std::string_view key) {
    for (const std::string_view known : keys) {
        if (known == key) return true;
    }
    return false;
}

CatalogFailure Fail(const CatalogFailureCode code, std::string message, std::string detail = {}) {
    return CatalogFailure::Make(code, std::move(message), std::move(detail));
}

// "<source>: <where>", or just one of them.
std::string At(const std::string_view source, const std::string_view where) {
    std::string text{source};
    if (!text.empty() && !where.empty()) text += ": ";
    text += where;
    return text;
}

std::string EntryAt(const std::string_view source, const std::size_t index, const std::string_view member = {}) {
    std::string where = "assets[" + std::to_string(index) + "]";
    if (!member.empty()) {
        where += '.';
        where += member;
    }
    return At(source, where);
}

// A parsed value as compact JSON text.
CatalogResult<std::string> Compact(const Json::Value value, const std::string_view where) {
    std::string text;
    Json::Writer writer(text);
    Json::WriteValue(writer, value);
    if (auto finished = writer.Finish(); !finished) {
        return Base::Err(Fail(CatalogFailureCode::WriteFailed, "could not keep a member as written",
                              Base::CauseDetail(finished.error(), where)));
    }
    return text;
}

CatalogResult<void> ReadVersion(const Json::ObjectView root, const std::int64_t known, const std::string_view source) {
    const std::optional<Json::Value> version = root.Find("version");
    if (!version) return Base::Err(Fail(CatalogFailureCode::BadVersion, "\"version\" is missing", std::string(source)));
    auto number = version->AsInt64(); // integer literals only: "1.0" is WrongKind
    if (!number) {
        return Base::Err(Fail(CatalogFailureCode::BadVersion, "\"version\" is not an integer",
                              Base::CauseDetail(number.error(), source)));
    }
    if (*number != known) {
        return Base::Err(Fail(CatalogFailureCode::UnknownVersion,
                              "\"version\" " + std::to_string(*number) + " is not known (this version reads " +
                                  std::to_string(known) + ")",
                              std::string(source)));
    }
    return {};
}

template <std::size_t N>
CatalogResult<std::vector<PreservedMember>> Preserve(const Json::ObjectView object,
                                                      const std::array<std::string_view, N>& known,
                                                      const std::string_view where) {
    std::vector<PreservedMember> kept;
    for (const Json::Member member : object) {
        if (IsKnown(known, member.key)) continue;
        auto text = Compact(member.value, At(where, member.key));
        if (!text) return Base::Err(std::move(text).error());
        kept.push_back(PreservedMember{std::string(member.key), std::move(*text)});
    }
    return kept;
}

CatalogResult<Json::Document> ParseText(const std::string_view text, const std::string_view source) {
    auto document = Json::Parse(text);
    if (!document) {
        return Base::Err(Fail(CatalogFailureCode::NotJson, "the text is not JSON",
                              Base::CauseDetail(document.error(), source)));
    }
    return std::move(*document);
}

CatalogResult<std::string> RequiredString(const Json::ObjectView entry, const std::string_view key,
                                          const std::string_view source, const std::size_t index) {
    const std::optional<Json::Value> value = entry.Find(key);
    if (!value) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "\"" + std::string(key) + "\" is missing",
                              EntryAt(source, index)));
    }
    auto text = value->AsString();
    if (!text || text->empty()) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "\"" + std::string(key) + "\" is not a non-empty string",
                              EntryAt(source, index, key)));
    }
    return std::string(*text);
}

CatalogResult<void> ValidateEntry(const CatalogEntry& entry, const std::string_view source, const std::size_t index) {
    if (entry.id.Empty() || entry.type.empty() || entry.path.empty()) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "\"id\", \"type\" and \"path\" must not be empty",
                              EntryAt(source, index)));
    }
    if (!IsValidAssetId(entry.id.Text())) {
        return Base::Err(Fail(CatalogFailureCode::BadId,
                              "id \"" + entry.id.Text() + "\" is not [a-z0-9._-] of 1 to 128 characters",
                              EntryAt(source, index, "id")));
    }
    if (!IsRelativeContentPath(entry.path) || entry.path == kContentManifestName) {
        return Base::Err(Fail(CatalogFailureCode::BadPath,
                              "path \"" + entry.path + "\" is not a relative content path",
                              EntryAt(source, index, "path")));
    }
    if (entry.format) {
        if (!IsValidFormat(*entry.format)) {
            return Base::Err(Fail(CatalogFailureCode::BadFormat, "format \"" + *entry.format + "\" is not [a-z0-9]+",
                                  EntryAt(source, index, "format")));
        }
    } else if (PathExtension(entry.path).empty()) {
        return Base::Err(Fail(CatalogFailureCode::NoFormat,
                              "path \"" + entry.path + "\" has no extension and no format is written",
                              EntryAt(source, index)));
    }
    if (entry.settings) {
        auto settings = Json::Parse(*entry.settings);
        if (!settings || settings->Root().GetKind() != Json::Kind::Object) {
            return Base::Err(Fail(CatalogFailureCode::SettingsNotObject, "settings is not a JSON object",
                                  EntryAt(source, index, "settings")));
        }
    }
    return {};
}

// Writes a preserved member's value; false when its text is not JSON.
bool WriteRaw(Json::Writer& writer, const std::string_view json) {
    auto document = Json::Parse(json);
    if (!document) return false;
    Json::WriteValue(writer, document->Root());
    return true;
}

// Preserved members must not repeat each other or a known key, or the written
// text would not read back.
template <std::size_t N>
CatalogResult<void> CheckPreserved(const std::vector<PreservedMember>& members,
                                   const std::array<std::string_view, N>& known, const std::string_view where) {
    std::unordered_set<std::string_view> seen;
    for (const PreservedMember& member : members) {
        if (IsKnown(known, member.key) || !seen.insert(member.key).second) {
            return Base::Err(Fail(CatalogFailureCode::WriteFailed,
                                  "preserved member \"" + member.key + "\" repeats a key", std::string(where)));
        }
        if (!Json::Parse(member.json)) {
            return Base::Err(Fail(CatalogFailureCode::WriteFailed,
                                  "preserved member \"" + member.key + "\" is not JSON", std::string(where)));
        }
    }
    return {};
}

void WritePreserved(Json::Writer& writer, const std::vector<PreservedMember>& members) {
    for (const PreservedMember& member : members) {
        writer.Key(member.key);
        (void)WriteRaw(writer, member.json); // checked by CheckPreserved
    }
}

CatalogResult<std::string> FinishWriting(Json::Writer& writer, std::string& text) {
    if (auto finished = writer.Finish(); !finished) {
        return Base::Err(Fail(CatalogFailureCode::WriteFailed, "could not write the JSON",
                              Base::CauseDetail(finished.error())));
    }
    return std::move(text);
}

CatalogResult<std::string> ReadText(const std::string_view root, const std::string_view relative) {
    // A catalog is content the product wrote; it has no byte limit of its own.
    auto bytes = ReadWholeFile(JoinContentRoot(root, relative), std::numeric_limits<std::size_t>::max());
    if (!bytes) {
        return Base::Err(Fail(CatalogFailureCode::Unreadable, "\"" + std::string(relative) + "\" cannot be read",
                              Base::CauseDetail(bytes.error(), relative)));
    }
    return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

} // namespace

bool IsValidFormat(const std::string_view format) noexcept {
    if (format.empty()) return false;
    for (const char character : format) {
        if (!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9'))) return false;
    }
    return true;
}

std::string EffectiveFormat(const CatalogEntry& entry) {
    return entry.format ? *entry.format : PathExtension(entry.path);
}

CatalogResult<ContentManifest> ParseContentManifest(const std::string_view text, const std::string_view sourceName) {
    auto document = ParseText(text, sourceName);
    if (!document) return Base::Err(std::move(document).error());
    auto root = document->Root().AsObject();
    if (!root) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "the top level is not an object", std::string(sourceName)));
    }
    if (auto version = ReadVersion(*root, kContentManifestVersion, sourceName); !version) {
        return Base::Err(std::move(version).error());
    }
    ContentManifest manifest;
    const std::optional<Json::Value> catalogs = root->Find("catalogs");
    if (!catalogs || catalogs->GetKind() != Json::Kind::Array) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "\"catalogs\" is not an array", std::string(sourceName)));
    }
    std::size_t index = 0;
    for (const Json::Value value : *catalogs->AsArray()) {
        auto path = value.AsString();
        if (!path) {
            return Base::Err(Fail(CatalogFailureCode::BadShape, "a catalog is not named by a string",
                                  At(sourceName, "catalogs[" + std::to_string(index) + "]")));
        }
        manifest.catalogs.emplace_back(*path);
        ++index;
    }
    if (const std::optional<Json::Value> bundles = root->Find("shader_bundles")) {
        auto bundleText = Compact(*bundles, At(sourceName, "shader_bundles"));
        if (!bundleText) return Base::Err(std::move(bundleText).error());
        manifest.shaderBundles = std::move(*bundleText);
    }
    auto extra = Preserve(*root, kManifestKeys, sourceName);
    if (!extra) return Base::Err(std::move(extra).error());
    manifest.extra = std::move(*extra);
    if (auto valid = ValidateContentManifest(manifest); !valid) return Base::Err(std::move(valid).error());
    return manifest;
}

CatalogResult<Catalog> ParseCatalog(const std::string_view text, const std::string_view sourceName) {
    auto document = ParseText(text, sourceName);
    if (!document) return Base::Err(std::move(document).error());
    auto root = document->Root().AsObject();
    if (!root) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "the top level is not an object", std::string(sourceName)));
    }
    if (auto version = ReadVersion(*root, kCatalogVersion, sourceName); !version) {
        return Base::Err(std::move(version).error());
    }
    const std::optional<Json::Value> assets = root->Find("assets");
    if (!assets || assets->GetKind() != Json::Kind::Array) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "\"assets\" is not an array", std::string(sourceName)));
    }
    Catalog catalog;
    std::size_t index = 0;
    for (const Json::Value value : *assets->AsArray()) {
        auto object = value.AsObject();
        if (!object) {
            return Base::Err(Fail(CatalogFailureCode::BadShape, "an entry is not an object", EntryAt(sourceName, index)));
        }
        CatalogEntry entry;
        auto id = RequiredString(*object, "id", sourceName, index);
        if (!id) return Base::Err(std::move(id).error());
        auto type = RequiredString(*object, "type", sourceName, index);
        if (!type) return Base::Err(std::move(type).error());
        auto path = RequiredString(*object, "path", sourceName, index);
        if (!path) return Base::Err(std::move(path).error());
        entry.id = AssetId(std::move(*id));
        entry.type = std::move(*type);
        entry.path = std::move(*path);
        if (const std::optional<Json::Value> format = object->Find("format")) {
            auto formatText = format->AsString();
            if (!formatText) {
                return Base::Err(Fail(CatalogFailureCode::BadFormat, "format is not a string",
                                      EntryAt(sourceName, index, "format")));
            }
            entry.format = std::string(*formatText);
        }
        if (const std::optional<Json::Value> settings = object->Find("settings")) {
            auto settingsText = Compact(*settings, EntryAt(sourceName, index, "settings"));
            if (!settingsText) return Base::Err(std::move(settingsText).error());
            entry.settings = std::move(*settingsText);
        }
        auto extra = Preserve(*object, kEntryKeys, EntryAt(sourceName, index));
        if (!extra) return Base::Err(std::move(extra).error());
        entry.extra = std::move(*extra);
        catalog.assets.push_back(std::move(entry));
        ++index;
    }
    auto extra = Preserve(*root, kCatalogKeys, sourceName);
    if (!extra) return Base::Err(std::move(extra).error());
    catalog.extra = std::move(*extra);
    if (auto valid = ValidateCatalog(catalog, sourceName); !valid) return Base::Err(std::move(valid).error());
    return catalog;
}

CatalogResult<void> ValidateContentManifest(const ContentManifest& manifest) {
    std::unordered_set<std::string_view> listed;
    for (std::size_t index = 0; index < manifest.catalogs.size(); ++index) {
        const std::string& path = manifest.catalogs[index];
        const std::string where = At(kContentManifestName, "catalogs[" + std::to_string(index) + "]");
        if (!IsRelativeContentPath(path) || path == kContentManifestName) {
            return Base::Err(Fail(CatalogFailureCode::BadCatalogPath,
                                  "catalog \"" + path + "\" is not a relative content path", where));
        }
        if (!listed.insert(path).second) {
            return Base::Err(Fail(CatalogFailureCode::DuplicateCatalog, "catalog \"" + path + "\" is listed twice",
                                  where));
        }
    }
    return {};
}

CatalogResult<void> ValidateCatalog(const Catalog& catalog, const std::string_view sourceName) {
    std::unordered_map<std::string_view, std::size_t> seen;
    for (std::size_t index = 0; index < catalog.assets.size(); ++index) {
        const CatalogEntry& entry = catalog.assets[index];
        if (auto valid = ValidateEntry(entry, sourceName, index); !valid) return Base::Err(std::move(valid).error());
        const auto [found, inserted] = seen.emplace(entry.id.Text(), index);
        if (!inserted) {
            return Base::Err(Fail(CatalogFailureCode::DuplicateId,
                                  "id \"" + entry.id.Text() + "\" appears twice",
                                  EntryAt(sourceName, found->second) + " and " + EntryAt({}, index)));
        }
    }
    return {};
}

CatalogResult<void> ValidateContent(const Content& content) {
    if (auto valid = ValidateContentManifest(content.manifest); !valid) return Base::Err(std::move(valid).error());
    if (content.catalogs.size() != content.manifest.catalogs.size()) {
        return Base::Err(Fail(CatalogFailureCode::BadShape, "one catalog per listed path is needed",
                              std::to_string(content.manifest.catalogs.size()) + " listed, " +
                                  std::to_string(content.catalogs.size()) + " given"));
    }
    std::unordered_map<std::string_view, std::size_t> owner;
    for (std::size_t file = 0; file < content.catalogs.size(); ++file) {
        const std::string& source = content.manifest.catalogs[file];
        if (auto valid = ValidateCatalog(content.catalogs[file], source); !valid) {
            return Base::Err(std::move(valid).error());
        }
        for (const CatalogEntry& entry : content.catalogs[file].assets) {
            const auto [found, inserted] = owner.emplace(entry.id.Text(), file);
            if (!inserted) {
                return Base::Err(Fail(CatalogFailureCode::DuplicateId,
                                      "id \"" + entry.id.Text() + "\" appears in two catalogs",
                                      content.manifest.catalogs[found->second] + " and " + source));
            }
        }
    }
    return {};
}

CatalogResult<std::string> WriteContentManifest(const ContentManifest& manifest) {
    if (auto valid = ValidateContentManifest(manifest); !valid) return Base::Err(std::move(valid).error());
    if (auto preserved = CheckPreserved(manifest.extra, kManifestKeys, kContentManifestName); !preserved) {
        return Base::Err(std::move(preserved).error());
    }
    if (manifest.shaderBundles && !Json::Parse(*manifest.shaderBundles)) {
        return Base::Err(Fail(CatalogFailureCode::WriteFailed, "shader_bundles is not JSON",
                              std::string(kContentManifestName)));
    }
    std::string text;
    Json::Writer writer(text);
    writer.BeginObject();
    writer.Key("version");
    writer.Int(kContentManifestVersion);
    writer.Key("catalogs");
    writer.BeginArray();
    for (const std::string& path : manifest.catalogs) writer.String(path);
    writer.EndArray();
    if (manifest.shaderBundles) {
        writer.Key("shader_bundles");
        (void)WriteRaw(writer, *manifest.shaderBundles); // checked above
    }
    WritePreserved(writer, manifest.extra);
    writer.EndObject();
    return FinishWriting(writer, text);
}

CatalogResult<std::string> WriteCatalog(const Catalog& catalog) {
    if (auto valid = ValidateCatalog(catalog); !valid) return Base::Err(std::move(valid).error());
    if (auto preserved = CheckPreserved(catalog.extra, kCatalogKeys, "catalog"); !preserved) {
        return Base::Err(std::move(preserved).error());
    }
    for (std::size_t index = 0; index < catalog.assets.size(); ++index) {
        if (auto preserved = CheckPreserved(catalog.assets[index].extra, kEntryKeys, EntryAt({}, index)); !preserved) {
            return Base::Err(std::move(preserved).error());
        }
    }
    std::string text;
    Json::Writer writer(text);
    writer.BeginObject();
    writer.Key("version");
    writer.Int(kCatalogVersion);
    writer.Key("assets");
    writer.BeginArray();
    for (const CatalogEntry& entry : catalog.assets) {
        writer.BeginObject();
        writer.Key("id");
        writer.String(entry.id.Text());
        writer.Key("type");
        writer.String(entry.type);
        writer.Key("path");
        writer.String(entry.path);
        if (entry.format) {
            writer.Key("format");
            writer.String(*entry.format);
        }
        if (entry.settings) {
            writer.Key("settings");
            (void)WriteRaw(writer, *entry.settings); // an object, checked by ValidateCatalog
        }
        WritePreserved(writer, entry.extra);
        writer.EndObject();
    }
    writer.EndArray();
    WritePreserved(writer, catalog.extra);
    writer.EndObject();
    return FinishWriting(writer, text);
}

CatalogResult<Content> ReadContent(const std::string_view contentRoot) {
    auto manifestText = ReadText(contentRoot, kContentManifestName);
    if (!manifestText) return Base::Err(std::move(manifestText).error());
    auto manifest = ParseContentManifest(*manifestText, kContentManifestName);
    if (!manifest) return Base::Err(std::move(manifest).error());
    Content content;
    content.manifest = std::move(*manifest);
    for (const std::string& path : content.manifest.catalogs) {
        auto text = ReadText(contentRoot, path);
        if (!text) return Base::Err(std::move(text).error());
        auto catalog = ParseCatalog(*text, path);
        if (!catalog) return Base::Err(std::move(catalog).error());
        content.catalogs.push_back(std::move(*catalog));
    }
    if (auto valid = ValidateContent(content); !valid) return Base::Err(std::move(valid).error());
    return content;
}

} // namespace GYO::Engine::Asset
