#include "gyo/audio_editor/ProjectFile.hpp"

#include "FileAccess.hpp"

#include "engine/io/fs/NativeFileSystem.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <span>
#include <string_view>
#include <utility>

namespace Gyo::Tools::AudioEditor {
namespace {

using Json = nlohmann::ordered_json;

// Every field of each version, in the order the file has them.
constexpr std::array<std::string_view, 10> kFieldsV1{
    "format", "version", "source", "asset_root", "asset_catalog", "id", "path", "trim_start", "trim_end", "gain_db"};
constexpr std::array<std::string_view, 6> kFieldsV2{"format", "version", "source", "asset_root", "asset_catalog",
                                                    "regions"};
constexpr std::array<std::string_view, 7> kRegionFields{"id",      "path",    "trim_start", "trim_end",
                                                        "gain_db", "fade_in", "fade_out"};

AudioEditorError Invalid(std::string message) {
    return AudioEditorError::Make(AudioEditorErrorCode::InvalidProject, std::move(message));
}

template <std::size_t Count>
bool HasExactly(const Json& json, const std::array<std::string_view, Count>& fields) {
    if (!json.is_object() || json.size() != fields.size()) return false;
    for (const std::string_view field : fields) {
        if (!json.contains(field)) return false;
    }
    return true;
}

bool TextField(const Json& json, const char* name, std::string& out) {
    const Json& value = json.at(name);
    if (!value.is_string()) return false;
    out = value.get<std::string>();
    return true;
}

// The id, path, trim and gain fields both versions share; version 2 adds the fades.
bool ReadRegion(const Json& json, const bool fades, SoundRegion& region) {
    if (!TextField(json, "id", region.registration.id) || !TextField(json, "path", region.registration.path)) {
        return false;
    }
    const Json& start = json.at("trim_start");
    const Json& end = json.at("trim_end");
    const Json& gain = json.at("gain_db");
    if (!start.is_number_unsigned() || !(end.is_null() || end.is_number_unsigned()) || !gain.is_number()) {
        return false;
    }
    region.settings.trimStart = start.get<std::size_t>();
    if (!end.is_null()) region.settings.trimEnd = end.get<std::size_t>();
    region.settings.gainDb = gain.get<double>();
    if (!fades) return true;
    const Json& fadeIn = json.at("fade_in");
    const Json& fadeOut = json.at("fade_out");
    if (!fadeIn.is_number_unsigned() || !fadeOut.is_number_unsigned()) return false;
    region.settings.fadeIn = fadeIn.get<std::size_t>();
    region.settings.fadeOut = fadeOut.get<std::size_t>();
    return true;
}

} // namespace

bool operator==(const AudioProject& left, const AudioProject& right) {
    const ExportBatch& a = left.batch;
    const ExportBatch& b = right.batch;
    return a.sourcePath == b.sourcePath && a.assetRoot == b.assetRoot && a.catalogPath == b.catalogPath &&
           a.regions == b.regions;
}

std::string ProjectText(const AudioProject& project) {
    const ExportBatch& batch = project.batch;
    Json json;
    json["format"] = kProjectFormat;
    json["version"] = kProjectVersion;
    json["source"] = batch.sourcePath;
    json["asset_root"] = batch.assetRoot;
    json["asset_catalog"] = batch.catalogPath;
    Json regions = Json::array();
    for (const SoundRegion& region : batch.regions) {
        Json entry;
        entry["id"] = region.registration.id;
        entry["path"] = region.registration.path;
        entry["trim_start"] = region.settings.trimStart;
        entry["trim_end"] = region.settings.trimEnd.has_value() ? Json(*region.settings.trimEnd) : Json(nullptr);
        // nlohmann writes a double with as many digits as reading it back needs.
        entry["gain_db"] = region.settings.gainDb;
        entry["fade_in"] = region.settings.fadeIn;
        entry["fade_out"] = region.settings.fadeOut;
        regions.push_back(std::move(entry));
    }
    json["regions"] = std::move(regions);
    return json.dump(2) + "\n";
}

Result<AudioProject, AudioEditorError> ParseProject(const std::string_view text) {
    const Json json = Json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) return Engine::Base::Err(Invalid("not a JSON object"));
    if (const auto format = json.find("format");
        format == json.end() || !format->is_string() || format->get<std::string>() != kProjectFormat) {
        return Engine::Base::Err(Invalid("not an audio editor project"));
    }
    const auto version = json.find("version");
    if (version == json.end() || !version->is_number_integer() ||
        (version->get<std::int64_t>() != 1 && version->get<std::int64_t>() != kProjectVersion)) {
        return Engine::Base::Err(Invalid("a project version this editor does not read"));
    }
    const bool first = version->get<std::int64_t>() == 1;
    if (first ? !HasExactly(json, kFieldsV1) : !HasExactly(json, kFieldsV2)) {
        return Engine::Base::Err(Invalid(first ? "not the fields of version 1" : "not the fields of version 2"));
    }
    AudioProject project;
    ExportBatch& batch = project.batch;
    if (!TextField(json, "source", batch.sourcePath) || !TextField(json, "asset_root", batch.assetRoot) ||
        !TextField(json, "asset_catalog", batch.catalogPath)) {
        return Engine::Base::Err(Invalid("a path is not a string"));
    }
    if (first) {
        // Version 1 is one region without fades.
        SoundRegion region;
        if (!ReadRegion(json, false, region)) return Engine::Base::Err(Invalid("the id, a trim or the gain is wrong"));
        batch.regions.push_back(std::move(region));
        return project;
    }
    const Json& regions = json.at("regions");
    if (!regions.is_array() || regions.empty()) return Engine::Base::Err(Invalid("the project has no region"));
    for (const Json& entry : regions) {
        SoundRegion region;
        if (!HasExactly(entry, kRegionFields) || !ReadRegion(entry, true, region)) {
            return Engine::Base::Err(Invalid("a region is not the fields of version 2"));
        }
        batch.regions.push_back(std::move(region));
    }
    return project;
}

Result<void, AudioEditorError> SaveProject(const std::string& path, const AudioProject& project) {
    // ParseProject refuses a project without a region, so none is written.
    if (project.batch.regions.empty()) {
        return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::InvalidProject,
                                                        "the project has no region", path));
    }
    if (!project.batch.assetRoot.empty()) {
        const auto within = Engine::IO::FS::IsNativePathWithin(path, project.batch.assetRoot);
        if (!within) {
            return Engine::Base::Err(AudioEditorError::Make(AudioEditorErrorCode::WriteFailed,
                                                            "cannot compare the project with the asset root",
                                                            Engine::Base::CauseDetail(within.error(), path)));
        }
        if (within.value()) {
            return Engine::Base::Err(AudioEditorError::Make(
                AudioEditorErrorCode::InvalidProject,
                "the project file may not be inside the asset root, where every file is registered", path));
        }
    }
    const std::string text = ProjectText(project);
    return Detail::WriteReplacing(path, std::as_bytes(std::span(text.data(), text.size())));
}

Result<AudioProject, AudioEditorError> LoadProject(const std::string& path) {
    auto bytes = Detail::ReadAll(path);
    if (!bytes) return Engine::Base::Err(std::move(bytes).error());
    auto project = ParseProject(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (!project) {
        auto error = std::move(project).error();
        error.detail = path;
        return Engine::Base::Err(std::move(error));
    }
    return project;
}

} // namespace Gyo::Tools::AudioEditor
