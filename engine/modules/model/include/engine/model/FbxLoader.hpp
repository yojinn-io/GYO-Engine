#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "engine/asset/AssetLoader.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/model/ModelData.hpp"

// The model loader: type "model", format "fbx" (version 1), the Data Contract
// in docs/architecture/target/v2/model.zh-Hant.md section 3. Binary and ASCII
// FBX come out as one ModelData in model space (metres, +Y up, +Z forward,
// left-handed); animations are baked to keys. Textures are named, never read
// or decoded, and the loader declares no references. A product registers it
// with AssetLoaderRegistry::Register<FbxLoader>().

namespace GYO::Engine::Model {

// The code of the first rule broken, in the order of the Data Contract's rules
// (model.zh-Hant.md section 3: 1 to 4); BadSettings is the settings' rule.
enum class FbxLoaderCode : std::uint8_t {
    // The bytes are not an FBX file.
    NotFbx = 1,
    // The importer could not read the file (broken data, a limit of its
    // memory reached, an animation it could not bake).
    Undecodable = 2,
    // The file holds what a ModelData cannot keep: a mesh with more than one
    // skin, a skin with more than 65536 joints, or a mesh with 2^32 vertices
    // or more.
    Unsupported = 3,
    // The result breaks a rule of ModelData (model.zh-Hant.md section 2.2),
    // such as a value that is not finite.
    BadData = 4,
    // The entry's settings are not the settings of section 3.
    BadSettings = 5,
};

[[nodiscard]] constexpr const char* ToString(const FbxLoaderCode code) noexcept {
    switch (code) {
    case FbxLoaderCode::NotFbx: return "NotFbx";
    case FbxLoaderCode::Undecodable: return "Undecodable";
    case FbxLoaderCode::Unsupported: return "Unsupported";
    case FbxLoaderCode::BadData: return "BadData";
    case FbxLoaderCode::BadSettings: return "BadSettings";
    }
    return "Unknown";
}

struct FbxSettings final {
    // false: no meshes (the nodes, materials and clips only), for a user that
    // needs the skeleton and the animation but never the geometry.
    bool meshes = true;

    [[nodiscard]] friend bool operator==(const FbxSettings&, const FbxSettings&) = default;
};

class FbxLoader final {
public:
    using Output = ModelData;
    using Settings = FbxSettings;
    using Code = FbxLoaderCode;

    static constexpr std::string_view kTypeName = "model";
    static constexpr std::array<std::string_view, 1> kFormats{"fbx"};
    // The file's size; a product may override it.
    static constexpr std::size_t kDefaultMaxBytes = std::size_t{64} * 1024u * 1024u;
    // The rate animations are baked at, keys per second.
    static constexpr double kBakeRate = 60.0;

    // No settings: meshes. {"meshes": true | false}; any other member or value
    // is BadSettings.
    [[nodiscard]] Base::Result<Settings, Asset::LoaderFailure<Code>> ParseSettings(
        std::optional<std::string_view> settingsJson) const;

    [[nodiscard]] Base::Result<ModelData, Asset::LoaderFailure<Code>> Load(std::span<const std::byte> bytes,
                                                                          const Settings& settings,
                                                                          Asset::LoadContext& context) const;
};

static_assert(Asset::AssetLoader<FbxLoader>);

} // namespace GYO::Engine::Model
