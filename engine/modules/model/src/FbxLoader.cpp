#include "engine/model/FbxLoader.hpp"

#include <string>
#include <utility>

#include "adapters/ufbx/UfbxImport.hpp"
#include "engine/json/Document.hpp"

namespace GYO::Engine::Model {
namespace {

using Failure = Asset::LoaderFailure<FbxLoaderCode>;

Base::Err<Failure> Fail(const FbxLoaderCode code, const char* message, const Asset::LoadContext& context,
                        const std::string& detail) {
    return Base::Err(Failure::Make(code, message, context.Id().Text() + " (" + context.Path() + "): " + detail));
}

Base::Err<Failure> BadSettings(const std::string& detail) {
    return Base::Err(
        Failure::Make(FbxLoaderCode::BadSettings, "the settings of an fbx model are {\"meshes\": true | false}", detail));
}

FbxLoaderCode CodeOf(const Detail::ImportProblem problem) noexcept {
    switch (problem) {
    case Detail::ImportProblem::NotFbx: return FbxLoaderCode::NotFbx;
    case Detail::ImportProblem::Undecodable: return FbxLoaderCode::Undecodable;
    case Detail::ImportProblem::Unsupported: return FbxLoaderCode::Unsupported;
    }
    return FbxLoaderCode::Undecodable;
}

} // namespace

Base::Result<FbxSettings, Asset::LoaderFailure<FbxLoaderCode>> FbxLoader::ParseSettings(
    const std::optional<std::string_view> settingsJson) const {
    FbxSettings settings;
    if (!settingsJson) return settings;
    const auto document = Json::Parse(*settingsJson);
    if (!document) return BadSettings("not JSON: " + document.error().message);
    const auto object = document->Root().AsObject();
    if (!object) return BadSettings("not an object");
    for (const Json::Member member : *object) {
        if (member.key != "meshes") return BadSettings("unknown member \"" + std::string(member.key) + "\"");
        const auto value = member.value.AsBool();
        if (!value) return BadSettings("meshes is not true or false");
        settings.meshes = *value;
    }
    return settings;
}

Base::Result<ModelData, Asset::LoaderFailure<FbxLoaderCode>> FbxLoader::Load(const std::span<const std::byte> bytes,
                                                                            const FbxSettings& settings,
                                                                            Asset::LoadContext& context) const {
    // 1 to 3
    auto imported = Detail::ImportFbx(bytes, Detail::ImportOptions{settings.meshes, kBakeRate});
    if (!imported) {
        const Detail::ImportFailure& failure = imported.error();
        return Fail(CodeOf(failure.code), failure.message.c_str(), context, failure.detail);
    }
    // 4
    if (auto valid = Validate(*imported); !valid) {
        return Fail(FbxLoaderCode::BadData, "the file's content breaks a rule of model data", context,
                    std::string(ToString(valid.error().code)) + ": " + valid.error().message + " (" +
                        valid.error().detail + ")");
    }
    return std::move(*imported);
}

} // namespace GYO::Engine::Model
