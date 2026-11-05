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

// Every field of version 1, in the order the file has them.
constexpr std::array<std::string_view, 10> kFields{
    "format", "version", "source", "asset_root", "asset_catalog", "id", "path", "trim_start", "trim_end", "gain_db"};

AudioEditorError Invalid(std::string message) {
    return AudioEditorError::Make(AudioEditorErrorCode::InvalidProject, std::move(message));
}

} // namespace

bool operator==(const AudioProject& left, const AudioProject& right) {
    const ExportRequest& a = left.request;
    const ExportRequest& b = right.request;
    return a.sourcePath == b.sourcePath && a.assetRoot == b.assetRoot && a.catalogPath == b.catalogPath &&
           a.registration.id == b.registration.id && a.registration.path == b.registration.path &&
           a.settings == b.settings;
}

std::string ProjectText(const AudioProject& project) {
    const ExportRequest& request = project.request;
    Json json;
    json["format"] = kProjectFormat;
    json["version"] = kProjectVersion;
    json["source"] = request.sourcePath;
    json["asset_root"] = request.assetRoot;
    json["asset_catalog"] = request.catalogPath;
    json["id"] = request.registration.id;
    json["path"] = request.registration.path;
    json["trim_start"] = request.settings.trimStart;
    json["trim_end"] = request.settings.trimEnd.has_value() ? Json(*request.settings.trimEnd) : Json(nullptr);
    json["gain_db"] = request.settings.gainDb;
    // nlohmann writes a double with as many digits as reading it back needs.
    return json.dump(2) + "\n";
}

Result<AudioProject, AudioEditorError> ParseProject(const std::string_view text) {
    const Json json = Json::parse(text, nullptr, false);
    if (json.is_discarded() || !json.is_object()) return Engine::Base::Err(Invalid("not a JSON object"));
    if (const auto format = json.find("format");
        format == json.end() || !format->is_string() || format->get<std::string>() != kProjectFormat) {
        return Engine::Base::Err(Invalid("not an audio editor project"));
    }
    if (const auto version = json.find("version");
        version == json.end() || !version->is_number_integer() || version->get<std::int64_t>() != kProjectVersion) {
        return Engine::Base::Err(Invalid("a project version this editor does not read"));
    }
    if (json.size() != kFields.size()) return Engine::Base::Err(Invalid("not the fields of version 1"));
    for (const std::string_view field : kFields) {
        if (!json.contains(field)) return Engine::Base::Err(Invalid("no field " + std::string(field)));
    }
    AudioProject project;
    ExportRequest& request = project.request;
    const auto textField = [&json](const char* name, std::string& out) {
        const Json& value = json.at(name);
        if (!value.is_string()) return false;
        out = value.get<std::string>();
        return true;
    };
    if (!textField("source", request.sourcePath) || !textField("asset_root", request.assetRoot) ||
        !textField("asset_catalog", request.catalogPath) || !textField("id", request.registration.id) ||
        !textField("path", request.registration.path)) {
        return Engine::Base::Err(Invalid("a path or the id is not a string"));
    }
    const Json& start = json.at("trim_start");
    const Json& end = json.at("trim_end");
    const Json& gain = json.at("gain_db");
    if (!start.is_number_unsigned() || !(end.is_null() || end.is_number_unsigned()) || !gain.is_number()) {
        return Engine::Base::Err(Invalid("the trim or the gain is not a number"));
    }
    request.settings.trimStart = start.get<std::size_t>();
    if (!end.is_null()) request.settings.trimEnd = end.get<std::size_t>();
    request.settings.gainDb = gain.get<double>();
    return project;
}

Result<void, AudioEditorError> SaveProject(const std::string& path, const AudioProject& project) {
    if (!project.request.assetRoot.empty()) {
        const auto within = Engine::IO::FS::IsNativePathWithin(path, project.request.assetRoot);
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
