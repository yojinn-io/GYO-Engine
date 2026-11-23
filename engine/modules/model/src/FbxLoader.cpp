#include "engine/model/FbxLoader.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
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

// What ufbx may allocate for one file, for its work and for the scene each:
// at least 256 MiB, and 8 times the entry's byte limit, so a product that
// raises the limit for a larger file raises this with it.
constexpr std::size_t kMinMemoryLimit = std::size_t{256} * 1024u * 1024u;
constexpr std::size_t kMemoryPerFileByte = 8;

std::size_t MemoryLimit(const std::size_t maxBytes) noexcept {
    const std::size_t scaled = maxBytes > std::numeric_limits<std::size_t>::max() / kMemoryPerFileByte
                                   ? std::numeric_limits<std::size_t>::max()
                                   : maxBytes * kMemoryPerFileByte;
    return std::max(kMinMemoryLimit, scaled);
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
    auto imported =
        Detail::ImportFbx(bytes, Detail::ImportOptions{settings.meshes, kBakeRate, MemoryLimit(context.MaxBytes())});
    if (!imported) {
        const Detail::ImportFailure& failure = imported.error();
        return Fail(failure.code, failure.message.c_str(), context, failure.detail);
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
