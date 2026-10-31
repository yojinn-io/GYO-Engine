#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/model/ModelAsset.hpp"
#include "engine/model/backend/ufbx/UfbxModelLoader.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "gyo/AppConfig.hpp"

#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

// v8-product-hitbox-data D3: the scope of AnimationContentDigest (H5,
// v8-product-hitbox-digest-scope (a), protocol-v9 §4.6).
namespace {
using namespace fps::pvp;

std::filesystem::path ContentRoot() {
    const auto base = Engine::Platform::Sdl::BaseDirectory();
    if (!base) throw std::runtime_error("Cannot locate the PvP test product directory");
    return base.value() / Gyo::AppConfig::Assets;
}

struct Assets final {
    Engine::Asset::AssetCatalog catalog{[] {
        const auto root = ContentRoot();
        const auto manifest = Engine::Asset::ContentManifest::Load(root);
        if (!manifest) throw std::runtime_error(manifest.error().message);
        auto catalog = manifest.value().LoadCatalogs(root);
        if (!catalog) throw std::runtime_error(catalog.error().message);
        return std::move(catalog).value();
    }()};
    Engine::Asset::Loading::LoaderRegistry loaders;
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};
    Assets() {
        loaders.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>());
        loaders.Register(std::make_unique<Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader>());
        loaders.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>());
    }
};

// The Client's definition: texture, accessories and weapon loaded.
const PlayerPresentationDefinition& ClientDefinition() {
    static Assets fixture;
    static const auto definition = [&] {
        std::string error;
        auto loaded = LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error);
        if (!loaded) throw std::runtime_error(error);
        return loaded;
    }();
    return *definition;
}

std::string Hex(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

// The digest of a copy of the Client's definition, its character and model
// changed by edit.
std::uint64_t EditedDigest(const std::function<void(PlayerPresentationDefinition&, Engine::Model::ModelAsset&)>& edit) {
    auto definition = ClientDefinition();
    auto model = std::make_shared<Engine::Model::ModelAsset>(*definition.character->model);
    auto character = std::make_shared<fps::CharacterPresentationDefinition>(*definition.character);
    edit(definition, *model);
    character->model = std::move(model);
    definition.character = std::move(character);
    return AnimationContentDigest(definition);
}

std::size_t NodeNamed(const Engine::Model::ModelAsset& model, const std::string& name) {
    const auto found = model.FindNode(name);
    REQUIRE(found);
    return *found;
}

// A track of the clip with at least two translation and rotation keys.
Engine::Model::NodeTrack& KeyedTrack(Engine::Model::AnimationClip& clip) {
    for (auto& track : clip.tracks)
        if (track.translations.size() > 1 && track.rotations.size() > 1) return track;
    throw std::runtime_error("no keyed track in " + clip.name);
}
} // namespace

TEST_CASE("PvP animation digest is the same on the Client and Match paths") {
    const auto client = AnimationContentDigest(ClientDefinition());
    std::string error;
    const auto match = LoadPlayerPoseDefinition(ContentRoot(), 1.8F, error);
    REQUIRE_MESSAGE(match, error);
    CHECK(AnimationContentDigest(*match) == client);
    CHECK(client != 0);
    // Repeatable within a process.
    CHECK(AnimationContentDigest(ClientDefinition()) == client);
    // H6 reads this line from every platform's L1 log.
    std::cout << "animation content digest: animation_digest=" << Hex(client) << '\n';
}

TEST_CASE("PvP animation digest reads the first eight SHA-256 bytes little-endian and never yields zero") {
    CHECK(AnimationDigestFromSha256("0102030405060708ffffffffffffffffffffffffffffffffffffffffffffffff") ==
          0x0807060504030201ULL);
    CHECK(AnimationDigestFromSha256("0000000000000000ffffffffffffffffffffffffffffffffffffffffffffffff") == 1);
    CHECK(AnimationDigestFromSha256("0100000000000000ffffffffffffffffffffffffffffffffffffffffffffffff") == 1);
    CHECK(AnimationDigestFromSha256("0200000000000000ffffffffffffffffffffffffffffffffffffffffffffffff") == 2);
}

TEST_CASE("PvP animation digest changes with every presentation value that moves the pose") {
    const auto base = AnimationContentDigest(ClientDefinition());
    using Edit = std::pair<const char*, std::function<void(PlayerPresentationDefinition&, Engine::Model::ModelAsset&)>>;
    const Edit edits[] = {
        {"reference_speed", [](auto& d, auto&) { d.referenceSpeed *= 1.5F; }},
        {"walk_native_speed", [](auto& d, auto&) { d.walkNativeSpeed *= 1.5; }},
        {"jog_native_speed", [](auto& d, auto&) { d.jogNativeSpeed *= 1.5; }},
        {"transition_seconds", [](auto& d, auto&) { d.transitionSeconds *= 1.5; }},
        {"max_frame_delta_seconds", [](auto& d, auto&) { d.maxFrameDeltaSeconds *= 1.5; }},
        {"shot_seconds", [](auto& d, auto&) { d.shotSeconds *= 1.5; }},
        {"jump_start_seconds", [](auto& d, auto&) { d.jumpStartSeconds *= 1.5; }},
        {"jump_land_seconds", [](auto& d, auto&) { d.jumpLandSeconds *= 1.5; }},
        {"hit_seconds", [](auto& d, auto&) { d.hitSeconds *= 1.5; }},
        {"up_pitch_radians", [](auto& d, auto&) { d.aimUpRadians *= 0.5; }},
        {"down_pitch_radians", [](auto& d, auto&) { d.aimDownRadians *= 0.5; }},
        {"upper_body_root", [](auto& d, auto& m) { d.upperBodyRoot = NodeNamed(m, "Head"); }},
    };
    for (const auto& [name, edit] : edits) {
        INFO(std::string(name));
        CHECK(EditedDigest(edit) != base);
    }
}

TEST_CASE("PvP animation digest changes with one key, the clip order or one parent") {
    const auto base = AnimationContentDigest(ClientDefinition());
    using Edit = std::pair<const char*, std::function<void(PlayerPresentationDefinition&, Engine::Model::ModelAsset&)>>;
    const Edit edits[] = {
        // One key of one clip; every clip of the 15 is in the digest.
        {"translation value", [](auto& d, auto& m) { KeyedTrack(m.clips.at(d.walkClip)).translations[1].value.y += 0.001F; }},
        {"rotation value", [](auto& d, auto& m) { KeyedTrack(m.clips.at(d.hitClip)).rotations[1].value.x += 0.001F; }},
        {"key time", [](auto& d, auto& m) { KeyedTrack(m.clips.at(d.jumpLandClip)).translations[1].timeSeconds += 0.001; }},
        {"key removed", [](auto& d, auto& m) { KeyedTrack(m.clips.at(d.crouchMoveClip)).rotations.pop_back(); }},
        {"track node", [](auto& d, auto& m) {
             auto& track = KeyedTrack(m.clips.at(d.idleClip));
             track.nodeIndex = track.nodeIndex == 0 ? 1 : 0;
         }},
        {"clip duration", [](auto& d, auto& m) { m.clips.at(d.deathClip).durationSeconds += 0.001; }},
        {"clip name", [](auto& d, auto& m) { m.clips.at(d.aimUpClip).name += "_v2"; }},
        // The order: idle and walk exchanged.
        {"clip order", [](auto& d, auto&) { std::swap(d.idleClip, d.walkClip); }},
        // The skeleton.
        {"parent", [](auto&, auto& m) { m.nodes.at(NodeNamed(m, "hand_r")).parentIndex = NodeNamed(m, "Head"); }},
        {"reference pose", [](auto&, auto& m) { m.nodes.at(NodeNamed(m, "thigh_l")).localTransform.translation.x += 0.001F; }},
        {"node name", [](auto&, auto& m) { m.nodes.at(NodeNamed(m, "Head")).name = "head_v2"; }},
    };
    for (const auto& [name, edit] : edits) {
        INFO(std::string(name));
        CHECK(EditedDigest(edit) != base);
    }
}

TEST_CASE("PvP animation digest ignores the weapon and the accessories") {
    const auto base = AnimationContentDigest(ClientDefinition());
    CHECK(EditedDigest([](auto& d, auto&) {
        d.weapon.reset();
        d.weaponMount.translation.x += 0.1F;
        d.weaponReferencePose = {};
    }) == base);
    auto definition = ClientDefinition();
    auto character = std::make_shared<fps::CharacterPresentationDefinition>(*definition.character);
    // An accessory added and the material bindings dropped: presentation only.
    character->accessories.push_back({.presentation = definition.weapon, .targetNode = 1, .sourceNode = 0});
    character->materials.clear();
    definition.character = std::move(character);
    CHECK(AnimationContentDigest(definition) == base);
}
