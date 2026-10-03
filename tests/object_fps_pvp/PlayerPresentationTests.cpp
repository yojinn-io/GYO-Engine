#include <doctest/doctest.h>

#include "PresentationLegacyMath.hpp"
#include "RetroFPS/App/WeaponPresentationDefinition.hpp"
#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "gyo/AppConfig.hpp"
#include "model/Animation.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"

#include <SDL3/SDL_filesystem.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace {
using namespace fps::pvp;
using namespace Engine::Model;
using Engine::Math::Matrix4;
using Engine::Math::Vec3;

Engine::Asset::AssetCatalog LoadCatalog() {
    const char* base = SDL_GetBasePath();
    if (!base) throw std::runtime_error("Cannot locate the PvP test product directory");
    const auto root = std::filesystem::path(base) / Gyo::AppConfig::Assets;
    const auto manifest = Engine::Asset::ContentManifest::Load(root);
    if (!manifest) throw std::runtime_error(manifest.error().message + " " + manifest.error().detail);
    auto catalog = manifest.value().LoadCatalogs(root);
    if (!catalog) throw std::runtime_error(catalog.error().message + " " + catalog.error().detail);
    return std::move(catalog).value();
}

// A malformed definition is isolated to this fixture. All other data comes
// from the selected owner's assembled product, never from v2 or a source fallback.
class DefinitionSource final : public Engine::Asset::Loading::IAssetSource {
public:
    std::unordered_map<std::string, std::string> overrides;

    Engine::Base::Result<Engine::Asset::Loading::ByteBuffer, Engine::Asset::AssetError>
    ReadAll(std::string_view path) override {
        const auto found = overrides.find(std::string(path));
        if (found != overrides.end()) {
            const auto bytes = std::as_bytes(std::span(found->second.data(), found->second.size()));
            return Engine::Base::Result<Engine::Asset::Loading::ByteBuffer, Engine::Asset::AssetError>::Ok(
                {bytes.begin(), bytes.end()});
        }
        Engine::Asset::Loading::NativeFileAssetSource native;
        return native.ReadAll(path);
    }
};

struct PresentationAssets final {
    Engine::Asset::AssetCatalog catalog{LoadCatalog()};
    Engine::Asset::Loading::LoaderRegistry loaders;
    DefinitionSource source;
    Engine::Asset::Loading::AssetPipeline pipeline{source, loaders};
    Engine::Asset::Core::AssetStorage storage;
    Engine::Asset::Core::AssetLifetime lifetime;
    Engine::Asset::Core::AssetCachePolicy policy{{}};
    Engine::Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};

    PresentationAssets() {
        if (!loaders.Register(std::make_unique<Engine::Asset::Loaders::TextLoader>()) ||
            !loaders.Register(std::make_unique<Engine::Asset::Loaders::SdlImage::SdlImageTextureLoader>()) ||
            !loaders.Register(std::make_unique<Engine::Model::Ufbx::UfbxModelLoader>()))
            throw std::runtime_error("PvP presentation CPU loaders failed to register");
    }

    template<class Edit>
    void Override(std::string_view id, Edit edit) {
        const auto* entry = catalog.Find(Engine::Asset::AssetId::FromString(id));
        if (!entry) throw std::runtime_error("Missing PvP definition " + std::string(id));
        const auto bytes = source.ReadAll(entry->resolvedPath);
        if (!bytes) throw std::runtime_error(bytes.error().message);
        auto json = nlohmann::json::parse(std::string_view(
            reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size()));
        edit(json);
        source.overrides[entry->resolvedPath] = json.dump();
    }
};

const PlayerPresentationDefinition& ProductionDefinition() {
    static PresentationAssets fixture;
    static const auto definition = [&] {
        std::string error;
        auto loaded = LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error);
        if (!loaded) throw std::runtime_error(error);
        return loaded;
    }();
    return *definition;
}

// Samples come from an authority moving at the 3 m/s reference speed.
PlayerPresentationFrame Frame(double seconds, double delta, float x = 0, float z = 0) {
    return {.playerId = 1, .movementEpoch = 1, .position = {x, 0, z}, .yaw = 0,
        .presentationSeconds = seconds, .deltaSeconds = delta, .planarSpeed = 3};
}

double ReferenceCycle() {
    const auto& definition = ProductionDefinition();
    return PlayerCycleDistance(definition, PlayerJogWeight(definition, 3));
}

void Advance(PlayerLocomotionState& state, const PlayerPresentationFrame& frame,
             const PlayerPresentationDefinition& definition = ProductionDefinition()) {
    std::string error;
    REQUIRE_MESSAGE(AdvancePlayerLocomotion(state, frame, definition, error), error);
}

void CheckMatrix(const Matrix4& actual, const Matrix4& expected) {
    for (std::size_t i = 0; i < actual.values.size(); ++i)
        CHECK(actual.values[i] == doctest::Approx(expected.values[i]).epsilon(1e-5).scale(1));
}

double MatrixDifference(const Matrix4& left, const Matrix4& right) {
    double difference{};
    for (std::size_t i = 0; i < left.values.size(); ++i)
        difference += std::abs(left.values[i] - right.values[i]);
    return difference;
}

PlayerLocomotionState Travel(int fps, float xDirection, float zDirection, double seconds = 1) {
    PlayerLocomotionState state;
    Advance(state, Frame(0, 0));
    for (int step = 1; step <= static_cast<int>(fps * seconds); ++step) {
        const double time = static_cast<double>(step) / fps;
        Advance(state, Frame(time, 1.0 / fps, static_cast<float>(3 * time * xDirection),
            static_cast<float>(3 * time * zDirection)));
    }
    return state;
}
} // namespace

TEST_CASE("PvP player content resolves owner-local character clips materials and explicit upper body mask") {
    const auto& definition = ProductionDefinition();
    REQUIRE(definition.character);
    REQUIRE(definition.character->model);
    REQUIRE(definition.character->animationSet);
    const auto& character = *definition.character;
    const auto& model = *character.model;
    CHECK(character.modelAssetId.debugName == "object_fps_pvp.model.character.superhero_female");
    REQUIRE(definition.idleClip < model.clips.size());
    REQUIRE(definition.jogClip < model.clips.size());
    CHECK(model.clips[definition.idleClip].name == "Armature|Pistol_Idle_Loop");
    CHECK(model.clips[definition.jogClip].name == "Armature|Jog_Fwd_Loop");
    REQUIRE(definition.walkClip < model.clips.size());
    CHECK(model.clips[definition.walkClip].name == "Armature|Walk_Loop");
    for (const auto& [name, clip] : character.animationSet->clips) {
        CAPTURE(name);
        CHECK(clip.model == character.model);
        CHECK(clip.modelAssetId == character.modelAssetId);
    }
    REQUIRE(character.materials.size() == model.materials.size());
    REQUIRE(character.materials.size() == 3);
    for (const auto& material : character.materials) {
        REQUIRE(material.textureAssetId);
        CHECK(material.textureAssetId->debugName.starts_with("object_fps_pvp.texture.character.female."));
    }
    REQUIRE(character.accessories.size() == 1);
    const auto& hair = character.accessories.front();
    REQUIRE(hair.presentation);
    CHECK(hair.presentation->modelAssetId.debugName == "object_fps_pvp.model.hair.buns");
    CHECK(model.nodes.at(hair.targetNode).name == "Head");
    CHECK(hair.presentation->model->nodes.at(hair.sourceNode).name == "Head");
    REQUIRE(definition.weapon);
    CHECK(definition.weapon->modelAssetId.debugName == "object_fps_pvp.model.weapon.ultimate_pistol_1.world");
    CHECK(model.nodes.at(definition.weaponNode).name == "hand_r");
    CHECK(model.nodes.at(definition.upperBodyRoot).name == "spine_01");
    REQUIRE(definition.upperBodyMask.size() == model.nodes.size());
    for (const std::string_view name : {"spine_01", "spine_03", "Head", "hand_l", "hand_r"}) {
        const auto node = model.FindNode(name);
        REQUIRE(node);
        CHECK(definition.upperBodyMask[*node]);
    }
    for (const std::string_view name : {"pelvis", "thigh_l", "calf_l", "foot_l", "thigh_r", "foot_r"}) {
        const auto node = model.FindNode(name);
        REQUIRE(node);
        CHECK_FALSE(definition.upperBodyMask[*node]);
    }
    CHECK(definition.referenceSpeed == doctest::Approx(3));
}

TEST_CASE("PvP player anchor and body scale come from the immutable reference geometry") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    Pose reference;
    REQUIRE(MakeDefaultPose(model, reference));
    float minimum = std::numeric_limits<float>::max();
    float maximum = -minimum;
    std::vector<SkinnedVertex> vertices;
    for (std::size_t mesh = 0; mesh < model.meshes.size(); ++mesh) {
        REQUIRE(SkinMesh(model, mesh, reference, vertices));
        for (const auto& vertex : vertices) {
            const float height = (vertex.position.y - definition.anchor.y) * definition.scale;
            minimum = std::min(minimum, height);
            maximum = std::max(maximum, height);
        }
    }
    CHECK(minimum == doctest::Approx(0).epsilon(1e-5).scale(1));
    CHECK(maximum == doctest::Approx(1.8).epsilon(1e-5));
    CHECK(definition.bodyHeight == doctest::Approx(1.8));
    const auto originalAnchor = definition.anchor;
    const auto originalScale = definition.scale;
    for (const double phase : {0.0, 0.23, 0.71}) {
        PlayerLocomotionState state;
        state.initialized = true;
        state.phaseCycles = phase;
        state.jogWeight = 0.4;
        state.idleSeconds = phase;
        state.moveWeight = 1;
        PlayerPresentationPose pose;
        std::string error;
        REQUIRE_MESSAGE(SamplePlayerPresentationPose(definition, state, pose, error), error);
        CHECK(definition.anchor.x == originalAnchor.x);
        CHECK(definition.anchor.y == originalAnchor.y);
        CHECK(definition.anchor.z == originalAnchor.z);
        CHECK(definition.scale == originalScale);
    }
}

namespace {
struct StanceMeasure final {
    double contactSpeed{}, contactSeconds{}, foreAftSwing{}, verticalSwing{};
    std::size_t contactRuns{};
    std::string contactStarts;
};

// Stance is approximated by a bone within 2.5cm of its lowest point while it
// moves backwards: an explicit bone-height model, not a claim of exact sole
// contact or zero sliding. Phase is the gait cycle fraction.
StanceMeasure MeasureStance(const PlayerPresentationDefinition& definition, std::string_view bone,
                            const std::function<void(double, Pose&)>& sample, double cycleSeconds) {
    constexpr std::size_t samples = 240;
    constexpr double contactBandMeters = 0.025;
    const auto& model = *definition.character->model;
    const auto node = model.FindNode(bone);
    REQUIRE(node);
    std::array<Vec3, samples> points;
    double minimumY = std::numeric_limits<double>::max();
    double maximumY = -minimumY, minimumZ = minimumY, maximumZ = -minimumY;
    for (std::size_t frame = 0; frame < samples; ++frame) {
        Pose pose;
        sample(static_cast<double>(frame) / samples, pose);
        const auto point = TransformPoint(pose.globalTransforms[*node], {});
        points[frame] = {(point.x - definition.anchor.x) * definition.scale,
            (point.y - definition.anchor.y) * definition.scale, (point.z - definition.anchor.z) * definition.scale};
        minimumY = std::min(minimumY, static_cast<double>(points[frame].y));
        maximumY = std::max(maximumY, static_cast<double>(points[frame].y));
        minimumZ = std::min(minimumZ, static_cast<double>(points[frame].z));
        maximumZ = std::max(maximumZ, static_cast<double>(points[frame].z));
    }
    StanceMeasure measure;
    double distance{};
    std::array<bool, samples> contact{};
    for (std::size_t frame = 0; frame < samples; ++frame) {
        const auto& from = points[frame];
        const auto& to = points[(frame + 1) % samples];
        contact[frame] = std::max(from.y, to.y) <= minimumY + contactBandMeters && to.z < from.z;
        if (contact[frame]) { distance += from.z - to.z; measure.contactSeconds += cycleSeconds / samples; }
    }
    // The cycle loops: a stance running through phase 1 continues at phase 0.
    for (std::size_t frame = 0; frame < samples; ++frame) {
        if (contact[frame] && !contact[(frame + samples - 1) % samples]) {
            ++measure.contactRuns;
            measure.contactStarts += std::to_string(static_cast<double>(frame) / samples) + ",";
        }
    }
    measure.contactSpeed = measure.contactSeconds > 0 ? distance / measure.contactSeconds : 0;
    measure.foreAftSwing = maximumZ - minimumZ;
    measure.verticalSwing = maximumY - minimumY;
    return measure;
}
} // namespace

TEST_CASE("PvP walk and jog native speeds are measured stance evidence and share one gait phase") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    for (const auto& [label, clip, native] : {std::tuple{"walk", definition.walkClip, definition.walkNativeSpeed},
                                              std::tuple{"jog", definition.jogClip, definition.jogNativeSpeed}}) {
        const double duration = model.clips.at(clip).durationSeconds;
        for (const std::string_view bone : {"ball_l", "ball_r"}) {
            const auto measure = MeasureStance(definition, bone, [&](double phase, Pose& pose) {
                REQUIRE(SamplePose(model, clip, phase * duration, PlaybackMode::Loop, pose));
            }, duration);
            INFO("clip=", label, " bone=", bone, " duration=", duration, " contact_speed_mps=", measure.contactSpeed,
                " contact_seconds=", measure.contactSeconds, " contact_starts=", measure.contactStarts,
                " fore_aft_swing_m=", measure.foreAftSwing, " configured_native_speed_mps=", native);
            CHECK(measure.foreAftSwing > 0.1);
            CHECK(measure.contactRuns >= 1);
            // A 2.5cm band is approximate: 10% tolerance for stance speed.
            CHECK(measure.contactSpeed == doctest::Approx(native).epsilon(0.1));
            // Left stance starts near phase 0 and right near 0.5 in both clips,
            // so one gait phase drives both without an offset.
            const double start = std::stod(measure.contactStarts);
            const double expected = bone == "ball_l" ? 0.0 : 0.5;
            CHECK(std::abs(std::remainder(start - expected, 1.0)) < 0.1);
        }
    }
}

TEST_CASE("PvP speed blend weights walk and jog from the authority speed and keeps blended stance at that speed") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    CHECK(PlayerJogWeight(definition, 0) == 0);
    CHECK(PlayerJogWeight(definition, definition.walkNativeSpeed) == doctest::Approx(0));
    CHECK(PlayerJogWeight(definition, definition.jogNativeSpeed) == doctest::Approx(1));
    CHECK(PlayerJogWeight(definition, 20) == 1);
    const double weight = PlayerJogWeight(definition, 3);
    CHECK(weight == doctest::Approx((3 - definition.walkNativeSpeed) /
        (definition.jogNativeSpeed - definition.walkNativeSpeed)));
    const double walkCycle = definition.walkNativeSpeed * model.clips[definition.walkClip].durationSeconds;
    const double jogCycle = definition.jogNativeSpeed * model.clips[definition.jogClip].durationSeconds;
    CHECK(PlayerCycleDistance(definition, 0) == doctest::Approx(walkCycle));
    CHECK(PlayerCycleDistance(definition, 1) == doctest::Approx(jogCycle));
    const double cycle = PlayerCycleDistance(definition, weight);
    // At 3 m/s the gait runs about one cycle (two steps) per second instead of
    // the half-speed jog that read as slow long strides.
    CHECK(3 / cycle > 0.9);
    CHECK(3 / cycle < 1.1);
    for (const std::string_view bone : {"ball_l", "ball_r"}) {
        const auto measure = MeasureStance(definition, bone, [&](double phase, Pose& pose) {
            Pose walk, jog;
            REQUIRE(SamplePose(model, definition.walkClip, phase * model.clips[definition.walkClip].durationSeconds,
                PlaybackMode::Loop, walk));
            REQUIRE(SamplePose(model, definition.jogClip, phase * model.clips[definition.jogClip].durationSeconds,
                PlaybackMode::Loop, jog));
            REQUIRE(BlendPoses(model, walk, jog, static_cast<float>(weight), pose));
        }, cycle / 3);
        INFO("blended bone=", bone, " jog_weight=", weight, " cycle_m=", cycle, " contact_speed_mps=", measure.contactSpeed,
            " contact_seconds=", measure.contactSeconds, " contact_starts=", measure.contactStarts);
        // Linear blending of two gaits is not exact: allow 15% stance error.
        CHECK(measure.contactSpeed == doctest::Approx(3).epsilon(0.15));
    }
}

TEST_CASE("PvP gait phase uses the authority speed sample so render rate and partial frames cannot change it") {
    const auto& definition = ProductionDefinition();
    // The same route, with a first frame that moved only part of a tick and a
    // held sample without speed: the phase depends only on distance and the
    // authority speed, never on this frame's own displacement rate.
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        PlayerLocomotionState state;
        Advance(state, Frame(0, 0));
        Advance(state, Frame(0.25 / fps, 0.25 / fps, 0, 0.01F));
        double now = 0.25 / fps, z = 0.01;
        while (z < 1.5) {
            now += 1.0 / fps;
            z = std::min(1.5, z + 3.0 / fps);
            Advance(state, Frame(now, 1.0 / fps, 0, static_cast<float>(z)));
        }
        CHECK(state.unwrappedPhaseCycles == doctest::Approx(1.5 / ReferenceCycle()).epsilon(1e-5));
        const double weight = state.jogWeight;
        auto held = Frame(now + 1.0 / fps, 1.0 / fps, 0, static_cast<float>(z));
        held.planarSpeed = 0;
        held.holding = true;
        Advance(state, held);
        CHECK(state.jogWeight == weight); // A held or stopped sample keeps the blend.
    }
}

TEST_CASE("PvP player pose retains jogging legs and aiming upper body with consistent attachments") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    PlayerLocomotionState state;
    state.initialized = true;
    state.phaseCycles = 0.33;
    state.jogWeight = 0.4;
    state.idleSeconds = 0.23;
    state.moveWeight = 1;
    Pose idle, walk, gaitJog, jog;
    REQUIRE(SamplePose(model, definition.idleClip, state.idleSeconds, PlaybackMode::Loop, idle));
    REQUIRE(SamplePose(model, definition.walkClip, state.phaseCycles * model.clips[definition.walkClip].durationSeconds,
        PlaybackMode::Loop, walk));
    REQUIRE(SamplePose(model, definition.jogClip, state.phaseCycles * model.clips[definition.jogClip].durationSeconds,
        PlaybackMode::Loop, gaitJog));
    REQUIRE(BlendPoses(model, walk, gaitJog, static_cast<float>(state.jogWeight), jog));
    PlayerPresentationPose combined;
    std::string error;
    REQUIRE_MESSAGE(SamplePlayerPresentationPose(definition, state, combined, error), error);
    REQUIRE(combined.body.localTransforms.size() == model.nodes.size());
    REQUIRE(combined.body.globalTransforms.size() == model.nodes.size());
    double legMotion{};
    for (std::size_t node = 0; node < model.nodes.size(); ++node) {
        CAPTURE(model.nodes[node].name);
        const auto local = ToMatrix(combined.body.localTransforms[node]);
        const auto expected = ToMatrix((definition.upperBodyMask[node] ? idle : jog).localTransforms[node]);
        CheckMatrix(local, expected);
        const auto parent = model.nodes[node].parentIndex;
        CheckMatrix(combined.body.globalTransforms[node], parent ?
            Multiply(combined.body.globalTransforms[*parent], local) : local);
        if (model.nodes[node].name == "thigh_l" || model.nodes[node].name == "calf_l")
            legMotion += MatrixDifference(local, ToMatrix(idle.localTransforms[node]));
    }
    CHECK(legMotion > 0.01);
    const auto mount = Multiply(combined.body.globalTransforms.at(definition.weaponNode),
        ToMatrix(definition.weaponMount));
    REQUIRE(combined.weapon.globalTransforms.size() == definition.weaponReferencePose.globalTransforms.size());
    for (std::size_t node = 0; node < combined.weapon.globalTransforms.size(); ++node)
        CheckMatrix(combined.weapon.globalTransforms[node],
            Multiply(mount, definition.weaponReferencePose.globalTransforms[node]));
    REQUIRE(combined.accessories.size() == definition.character->accessories.size());
    const auto& hair = definition.character->accessories.front();
    CheckMatrix(combined.accessories.front().globalTransforms.at(hair.sourceNode),
        Multiply(combined.body.globalTransforms.at(hair.targetNode), ToMatrix(hair.placement)));
}

TEST_CASE("PvP jogging phase follows actual distance at 30 60 and 144 FPS in every planar direction") {
    const auto& definition = ProductionDefinition();
    const double expectedPhase = 3.0 / ReferenceCycle();
    const float diagonal = 1.0F / std::sqrt(2.0F);
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        for (const auto direction : std::array{std::array{0.0F, 1.0F}, std::array{1.0F, 0.0F},
                 std::array{diagonal, diagonal}}) {
            CAPTURE(direction[0]);
            CAPTURE(direction[1]);
            const auto state = Travel(fps, direction[0], direction[1]);
            CHECK(state.totalDistance == doctest::Approx(3).epsilon(1e-5));
            CHECK(state.signedDistance == doctest::Approx(3).epsilon(1e-5));
            CHECK(state.unwrappedPhaseCycles == doctest::Approx(expectedPhase).epsilon(1e-5));
            CHECK(state.phaseCycles == doctest::Approx(expectedPhase - std::floor(expectedPhase)).epsilon(1e-5));
            CHECK(state.playbackRate == doctest::Approx(3.0 / ReferenceCycle()).epsilon(1e-4));
            CHECK(state.jogWeight == doctest::Approx(PlayerJogWeight(definition, 3)));
            CHECK(state.moveWeight > 0.99F);
            CHECK_FALSE(state.backward);
        }
    }
}

TEST_CASE("PvP stop wall contact turning and timeline hold cannot accumulate phantom footsteps") {
    auto state = Travel(60, 0, 1, 0.5);
    const double phase = state.unwrappedPhaseCycles;
    const double distance = state.totalDistance;
    for (int step = 1; step <= 30; ++step) {
        auto frame = Frame(0.5 + step / 60.0, 1.0 / 60, 0, 1.5F);
        frame.yaw = static_cast<float>(step) / 10;
        Advance(state, frame);
    }
    CHECK(state.unwrappedPhaseCycles == phase);
    CHECK(state.totalDistance == distance);
    CHECK(state.distanceDelta == 0);
    CHECK(state.speed == 0);
    CHECK(state.moveWeight < 0.01F);
    // The first hold-labelled sample can still contain the final actual step.
    auto finalStep = Frame(1 + 1.0 / 60, 1.0 / 60, 0, 1.55F);
    finalStep.holding = true;
    Advance(state, finalStep);
    CHECK(state.totalDistance == doctest::Approx(distance + 0.05).epsilon(1e-5));
    const auto heldPhase = state.unwrappedPhaseCycles;
    const auto resets = state.resetCount;
    for (int step = 0; step < 30; ++step) Advance(state, finalStep);
    CHECK(state.unwrappedPhaseCycles == heldPhase);
    CHECK(state.resetCount == resets);
    CHECK(state.holding);
    auto resume = Frame(finalStep.presentationSeconds + 1.0 / 60, 1.0 / 60, 0, 1.6F);
    Advance(state, resume);
    CHECK(state.totalDistance == doctest::Approx(distance + 0.1).epsilon(1e-5));
    CHECK_FALSE(state.holding);
}

TEST_CASE("PvP backward motion reverses jog phase and returning along the same route cancels signed travel") {
    const auto& definition = ProductionDefinition();
    const auto backward = Travel(60, 0, -1);
    CHECK(backward.totalDistance == doctest::Approx(3));
    CHECK(backward.signedDistance == doctest::Approx(-3));
    CHECK(backward.unwrappedPhaseCycles == doctest::Approx(-3.0 / ReferenceCycle()));
    CHECK(backward.playbackRate == doctest::Approx(-3.0 / ReferenceCycle()).epsilon(1e-4));
    CHECK(backward.backward);
    auto state = Travel(60, 0, 1, 0.5);
    for (int step = 1; step <= 30; ++step)
        Advance(state, Frame(0.5 + step / 60.0, 1.0 / 60, 0, 1.5F - step * 0.05F));
    CHECK(state.totalDistance == doctest::Approx(3));
    CHECK(state.signedDistance == doctest::Approx(0).epsilon(1e-5).scale(1));
    CHECK(state.unwrappedPhaseCycles == doctest::Approx(0).epsilon(1e-5).scale(1));
}

TEST_CASE("PvP epoch discontinuity teleport and long frame reanchor instead of replaying unseen travel") {
    const auto& definition = ProductionDefinition();
    for (const std::string_view reason : {"epoch", "discontinuity", "teleport", "long_frame"}) {
        CAPTURE(reason);
        auto state = Travel(60, 0, 1, 0.5);
        const auto resets = state.resetCount;
        auto frame = Frame(0.5 + 1.0 / 60, 1.0 / 60, 0, 1.55F);
        if (reason == "epoch") frame.movementEpoch = 2;
        if (reason == "discontinuity") frame.continuous = false;
        if (reason == "teleport") frame.position.z = 10;
        if (reason == "long_frame") {
            frame.deltaSeconds = definition.maxFrameDeltaSeconds + 0.1;
            frame.presentationSeconds = 0.5 + frame.deltaSeconds;
            frame.position.z = 2.5F;
        }
        Advance(state, frame);
        CHECK(state.phaseReset);
        CHECK(state.resetCount == resets + 1);
        CHECK(state.resetReason == reason);
        CHECK(state.unwrappedPhaseCycles == 0);
        CHECK(state.distanceDelta == 0);
        frame.continuous = true;
        frame.deltaSeconds = 1.0 / 60;
        frame.presentationSeconds += frame.deltaSeconds;
        frame.position.z += 0.05F;
        Advance(state, frame);
        CHECK_FALSE(state.phaseReset);
        CHECK(state.distanceDelta == doctest::Approx(0.05).epsilon(1e-4));
        CHECK(state.unwrappedPhaseCycles == doctest::Approx(0.05 / ReferenceCycle()).epsilon(1e-4));
    }
}

TEST_CASE("PvP life and death boundaries clear old locomotion even at the same movement epoch") {
    for (bool death : {false, true}) {
        PlayerLocomotionState state;
        Advance(state, Frame(0, 0));
        Advance(state, Frame(1.0 / 60, 1.0 / 60, 0, .05F));
        REQUIRE(state.totalDistance > 0);
        auto frame = Frame(2.0 / 60, 1.0 / 60, 0, .1F);
        frame.lifeGeneration = death ? 1 : 2;
        frame.dead = death;
        Advance(state, frame);
        CHECK(state.phaseReset);
        CHECK(state.resetReason == "life");
        CHECK(state.totalDistance == 0);
        CHECK(state.unwrappedPhaseCycles == 0);
        CHECK(state.lifeGeneration == frame.lifeGeneration);
        CHECK(state.dead == frame.dead);
    }
}

TEST_CASE("PvP 100ms and 108ms rendering gaps reset locomotion at the existing runtime boundary") {
    for (const double gap : {0.1, 0.108}) {
        CAPTURE(gap);
        auto state = Travel(60, 0, 1, 0.5);
        const auto resets = state.resetCount;
        Advance(state, Frame(0.5 + gap, gap, 0, static_cast<float>(1.5 + 3 * gap)));
        CHECK(state.phaseReset);
        CHECK(state.resetReason == "long_frame");
        CHECK(state.resetCount == resets + 1);
        CHECK(state.distanceDelta == 0);
        CHECK(state.unwrappedPhaseCycles == 0);
    }
}

TEST_CASE("PvP independent players and rejoined identities cannot inherit another locomotion history") {
    auto first = Travel(60, 0, 1, 0.5);
    PlayerLocomotionState second;
    auto other = Frame(0, 0, 4, 5);
    other.playerId = 2;
    Advance(second, other);
    const auto firstPhase = first.unwrappedPhaseCycles;
    other.presentationSeconds = 0.05;
    other.deltaSeconds = 0.05;
    other.position.x += 0.15F;
    Advance(second, other);
    CHECK(second.unwrappedPhaseCycles == doctest::Approx(0.15 / ReferenceCycle()).epsilon(1e-5));
    CHECK(first.unwrappedPhaseCycles == firstPhase);
    const auto secondPhase = second.unwrappedPhaseCycles;
    auto rejoin = Frame(0.5 + 1.0 / 60, 1.0 / 60, 9, 9);
    rejoin.playerId = 3;
    Advance(first, rejoin);
    CHECK(first.playerId == 3);
    CHECK(first.phaseReset);
    CHECK(first.resetReason == "player");
    CHECK(first.unwrappedPhaseCycles == 0);
    CHECK(second.unwrappedPhaseCycles == secondPhase);
    CHECK(second.playerId == 2);
}

TEST_CASE("PvP player definitions reject missing clips bones and invalid stride calibration") {
    for (const std::string_view fault : {"mask", "leg_mask", "weapon", "clip", "action_clip", "speed", "stride",
                                         "action_span"}) {
        CAPTURE(fault);
        PresentationAssets fixture;
        if (fault == "clip") {
            fixture.Override("object_fps_pvp.player.animations", [](auto& json) {
                json["clips"]["jog"]["clip"] = "Armature|MissingJog";
            });
        } else if (fault == "action_clip") {
            fixture.Override("object_fps_pvp.player.animations", [](auto& json) {
                json["clips"].erase("death");
            });
        } else if (fault == "action_span") {
            fixture.Override("object_fps_pvp.player.presentation", [](auto& json) {
                json["actions"]["shot_seconds"] = 0;
            });
        } else {
            fixture.Override("object_fps_pvp.player.presentation", [&](auto& json) {
                if (fault == "mask") json["upper_body_root"] = "MissingSpine";
                if (fault == "leg_mask") json["upper_body_root"] = "pelvis";
                if (fault == "weapon") json["weapon"]["node"] = "MissingHand";
                if (fault == "speed") json["reference_speed"] = 0;
                if (fault == "stride") json["locomotion"]["jog_native_speed"] = 0.5; // Not faster than walk.
            });
        }
        std::string error;
        CHECK_FALSE(LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error));
        CHECK_FALSE(error.empty());
    }
}

namespace {
PlayerActionPose Resolve(const PlayerLocomotionState& state, const PlayerPresentationFrame& frame) {
    PlayerActionPose actions;
    std::string error;
    REQUIRE_MESSAGE(ResolvePlayerActions(ProductionDefinition(), state, frame, actions, error), error);
    return actions;
}

double ClipSeconds(const std::size_t clip) {
    return ProductionDefinition().character->model->clips.at(clip).durationSeconds;
}

PlayerPresentationFrame Air(const double seconds, const bool grounded, const float verticalVelocity) {
    auto frame = Frame(seconds, 1.0 / 60);
    frame.grounded = grounded;
    frame.verticalVelocity = verticalVelocity;
    return frame;
}

// Steps at 60 FPS so no sampled gap reaches the long-frame reanchor.
double AdvanceTo(PlayerLocomotionState& state, double from, const double to, PlayerPresentationFrame frame) {
    while (from < to - 1e-9) {
        from = (std::min)(to, from + 1.0 / 60);
        frame.presentationSeconds = from;
        Advance(state, frame);
    }
    return from;
}
} // namespace

TEST_CASE("PvP player action clips and contract spans resolve from owner-local content") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    const std::array<std::tuple<std::size_t, std::string_view, double>, 6> clips{{
        {definition.shootClip, "Armature|Pistol_Shoot", 0.633},
        {definition.reloadClip, "Armature|Pistol_Reload", 1.667},
        {definition.jumpStartClip, "Armature|Jump_Start", 1.333},
        {definition.jumpLoopClip, "Armature|Jump_Loop", 2.5},
        {definition.jumpLandClip, "Armature|Jump_Land", 1.267},
        {definition.deathClip, "Armature|Death01", 2.4}}};
    for (const auto& [clip, name, seconds] : clips) {
        CAPTURE(name);
        REQUIRE(clip < model.clips.size());
        CHECK(model.clips[clip].name == name);
        CHECK(model.clips[clip].durationSeconds == doctest::Approx(seconds).epsilon(0.01));
    }
    // The contract spans, not the authored lengths, time the presentation.
    CHECK(definition.shotSeconds == doctest::Approx(10.0 / 60).epsilon(1e-4));
    CHECK(definition.jumpStartSeconds == doctest::Approx(0.1));
    CHECK(definition.jumpLandSeconds == doctest::Approx(0.1));
}

TEST_CASE("PvP remote shot plays over its contract span as a pure function of presentation time") {
    const auto& definition = ProductionDefinition();
    PlayerLocomotionState state;
    auto frame = Frame(10, 1.0 / 60);
    Advance(state, frame);
    frame.shotActionId = 7;
    frame.shotSeconds = 10;
    const auto first = Resolve(state, frame);
    CHECK(first.upper == PlayerUpperAction::Shoot);
    CHECK(first.shotActionId == 7);
    CHECK(first.upperClipSeconds == doctest::Approx(0));
    CHECK(first.lower == PlayerLowerAction::Locomotion);
    frame.presentationSeconds = 10 + definition.shotSeconds / 2;
    const auto middle = Resolve(state, frame);
    CHECK(middle.upper == PlayerUpperAction::Shoot);
    CHECK(middle.upperClipSeconds == doctest::Approx(ClipSeconds(definition.shootClip) / 2));
    // A held timeline, a duplicate snapshot, a resend or an ACK presents the
    // same input again and must select the same point, never a restart.
    for (int repeat = 0; repeat < 3; ++repeat) {
        const auto again = Resolve(state, frame);
        CHECK(again.upper == middle.upper);
        CHECK(again.upperClipSeconds == middle.upperClipSeconds);
        CHECK(again.shotActionId == 7);
    }
    // The span is half-open, and resuming past it shows no catch-up replay.
    frame.presentationSeconds = 10 + definition.shotSeconds;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Hold);
    frame.presentationSeconds = 10.8;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Hold);
    // A later accepted shot restarts from its own authority tick.
    frame.shotActionId = 8;
    frame.shotSeconds = 10.5;
    frame.presentationSeconds = 10.5;
    const auto next = Resolve(state, frame);
    CHECK(next.upper == PlayerUpperAction::Shoot);
    CHECK(next.shotActionId == 8);
    CHECK(next.upperClipSeconds == doctest::Approx(0));
}

TEST_CASE("PvP remote reload follows the authoritative interval and outranks a shot") {
    const auto& definition = ProductionDefinition();
    PlayerLocomotionState state;
    auto frame = Frame(20, 1.0 / 60);
    Advance(state, frame);
    frame.reloadActionId = 9;
    frame.reloadStartSeconds = 20;
    frame.reloadEndSeconds = 21.5;
    frame.presentationSeconds = 20.75;
    const auto reload = Resolve(state, frame);
    CHECK(reload.upper == PlayerUpperAction::Reload);
    CHECK(reload.reloadActionId == 9);
    CHECK(reload.upperClipSeconds == doctest::Approx(ClipSeconds(definition.reloadClip) / 2));
    frame.shotActionId = 8;
    frame.shotSeconds = 20.7;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Reload);
    frame.presentationSeconds = 21.5;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Hold);
    frame.presentationSeconds = 20.75;
    frame.shotActionId = 0;
    frame.reloadEndSeconds = frame.reloadStartSeconds;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Hold);
}

TEST_CASE("PvP actions from before the current life began cannot appear in it") {
    PlayerLocomotionState state;
    auto frame = Frame(33, 1.0 / 60);
    frame.lifeGeneration = 2;
    frame.lifeStateSeconds = 33;
    Advance(state, frame);
    frame.presentationSeconds = 33.05;
    frame.shotActionId = 3;
    frame.shotSeconds = 32.99;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Hold);
    frame.shotActionId = 0;
    frame.reloadActionId = 4;
    frame.reloadStartSeconds = 32;
    frame.reloadEndSeconds = 33.5;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Hold);
    frame.reloadStartSeconds = 33;
    frame.reloadEndSeconds = 34.5;
    CHECK(Resolve(state, frame).upper == PlayerUpperAction::Reload);
}

TEST_CASE("PvP jump phases follow the sampled grounded state within their contract spans") {
    const auto& definition = ProductionDefinition();
    SUBCASE("rising liftoff plays Start Loop and Land") {
        PlayerLocomotionState state;
        Advance(state, Air(1, true, 0));
        CHECK(state.jumpPhase == PlayerJumpPhase::Grounded);
        const double liftoff = 1 + 1.0 / 60;
        Advance(state, Air(liftoff, false, 4));
        CHECK(state.jumpPhase == PlayerJumpPhase::Start);
        auto actions = Resolve(state, Air(liftoff, false, 4));
        CHECK(actions.lower == PlayerLowerAction::JumpStart);
        CHECK(actions.lowerClipSeconds == doctest::Approx(0));
        double now = AdvanceTo(state, liftoff, liftoff + definition.jumpStartSeconds / 2, Air(0, false, 3));
        actions = Resolve(state, Air(now, false, 3));
        CHECK(actions.lower == PlayerLowerAction::JumpStart);
        CHECK(actions.lowerClipSeconds == doctest::Approx(ClipSeconds(definition.jumpStartClip) / 2));
        now = AdvanceTo(state, now, liftoff + definition.jumpStartSeconds + 0.05, Air(0, false, 1));
        CHECK(state.jumpPhase == PlayerJumpPhase::Airborne);
        actions = Resolve(state, Air(now, false, 1));
        CHECK(actions.lower == PlayerLowerAction::JumpLoop);
        CHECK(actions.lowerClipSeconds == doctest::Approx(0.05));
        now = AdvanceTo(state, now, 1.6, Air(0, false, -2));
        Advance(state, Air(now + 1.0 / 60, true, 0));
        now += 1.0 / 60;
        CHECK(state.jumpPhase == PlayerJumpPhase::Land);
        const double landed = now;
        now = AdvanceTo(state, now, landed + definition.jumpLandSeconds / 2, Air(0, true, 0));
        actions = Resolve(state, Air(now, true, 0));
        CHECK(actions.lower == PlayerLowerAction::JumpLand);
        CHECK(actions.lowerClipSeconds == doctest::Approx(ClipSeconds(definition.jumpLandClip) / 2));
        now = AdvanceTo(state, now, landed + definition.jumpLandSeconds + 1.0 / 60, Air(0, true, 0));
        CHECK(state.jumpPhase == PlayerJumpPhase::Grounded);
        CHECK(Resolve(state, Air(now, true, 0)).lower == PlayerLowerAction::Locomotion);
    }
    SUBCASE("leaving the ground while falling skips Start") {
        PlayerLocomotionState state;
        Advance(state, Air(1, true, 0));
        Advance(state, Air(1 + 1.0 / 60, false, -1));
        CHECK(state.jumpPhase == PlayerJumpPhase::Airborne);
    }
    SUBCASE("a short hop lands during Start") {
        PlayerLocomotionState state;
        Advance(state, Air(1, true, 0));
        Advance(state, Air(1 + 1.0 / 60, false, 2));
        Advance(state, Air(1 + 2.0 / 60, true, 0));
        CHECK(state.jumpPhase == PlayerJumpPhase::Land);
    }
    SUBCASE("a reanchor in the air never invents a liftoff") {
        PlayerLocomotionState state;
        Advance(state, Air(1, true, 0));
        auto discontinuous = Air(1 + 1.0 / 60, false, 4);
        discontinuous.continuous = false;
        Advance(state, discontinuous);
        CHECK(state.phaseReset);
        CHECK(state.jumpPhase == PlayerJumpPhase::Airborne);
        auto spawned = Air(2, false, 4);
        PlayerLocomotionState fresh;
        Advance(fresh, spawned);
        CHECK(fresh.jumpPhase == PlayerJumpPhase::Airborne);
    }
}

TEST_CASE("PvP death plays Death01 once over the whole body and holds its last pose until the new life") {
    const auto& definition = ProductionDefinition();
    PlayerLocomotionState state;
    Advance(state, Frame(29.9, 1.0 / 60));
    auto dead = Frame(30, 1.0 / 60);
    dead.dead = true;
    dead.lifeStateSeconds = 30;
    dead.shotActionId = 5;
    dead.shotSeconds = 29.95;
    Advance(state, dead);
    CHECK(state.phaseReset);
    CHECK(state.resetReason == "life");
    auto actions = Resolve(state, dead);
    CHECK(actions.lower == PlayerLowerAction::Death);
    CHECK(actions.lowerClipSeconds == doctest::Approx(0));
    CHECK(actions.upper == PlayerUpperAction::Hold); // Death owns the weapon hand.
    double now = AdvanceTo(state, 30, 30.5, dead);
    dead.presentationSeconds = now;
    CHECK(Resolve(state, dead).lowerClipSeconds == doctest::Approx(0.5));
    std::string error;
    PlayerPresentationPose held, later;
    now = AdvanceTo(state, now, 33, dead);
    dead.presentationSeconds = now;
    actions = Resolve(state, dead);
    CHECK(actions.lowerClipSeconds == doctest::Approx(ClipSeconds(definition.deathClip)));
    REQUIRE_MESSAGE(SamplePlayerPresentationPose(definition, state, actions, held, error), error);
    now = AdvanceTo(state, now, 35, dead);
    dead.presentationSeconds = now;
    REQUIRE_MESSAGE(SamplePlayerPresentationPose(definition, state, Resolve(state, dead), later, error), error);
    for (std::size_t node = 0; node < held.body.globalTransforms.size(); ++node)
        CheckMatrix(later.body.globalTransforms[node], held.body.globalTransforms[node]);
    // The new life starts clean: no death pose and no previous-life shot.
    auto alive = Frame(now + 1.0 / 60, 1.0 / 60);
    alive.lifeGeneration = 2;
    alive.lifeStateSeconds = alive.presentationSeconds;
    alive.shotActionId = 5;
    alive.shotSeconds = 29.95;
    Advance(state, alive);
    CHECK(state.phaseReset);
    CHECK(state.resetReason == "life");
    actions = Resolve(state, alive);
    CHECK(actions.lower == PlayerLowerAction::Locomotion);
    CHECK(actions.upper == PlayerUpperAction::Hold);
}

TEST_CASE("PvP composed action pose takes action upper body over locomotion or jump legs with attached weapon") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    PlayerLocomotionState state;
    state.initialized = true;
    state.phaseCycles = 0.33;
    state.jogWeight = 0.4;
    state.idleSeconds = 0.23;
    state.moveWeight = 1;
    Pose idle, walk, jog, gait, legs, shoot, loop, death;
    REQUIRE(SamplePose(model, definition.idleClip, state.idleSeconds, PlaybackMode::Loop, idle));
    REQUIRE(SamplePose(model, definition.walkClip, state.phaseCycles * model.clips[definition.walkClip].durationSeconds,
        PlaybackMode::Loop, walk));
    REQUIRE(SamplePose(model, definition.jogClip, state.phaseCycles * model.clips[definition.jogClip].durationSeconds,
        PlaybackMode::Loop, jog));
    REQUIRE(BlendPoses(model, walk, jog, static_cast<float>(state.jogWeight), gait));
    REQUIRE(BlendPoses(model, idle, gait, state.moveWeight, legs));
    REQUIRE(SamplePose(model, definition.shootClip, 0.2, PlaybackMode::Clamp, shoot));
    REQUIRE(SamplePose(model, definition.jumpLoopClip, 0.4, PlaybackMode::Loop, loop));
    REQUIRE(SamplePose(model, definition.deathClip, 1.0, PlaybackMode::Clamp, death));
    const auto check = [&](const PlayerActionPose& actions, const Pose& upper, const Pose& lower) {
        PlayerPresentationPose combined;
        std::string error;
        REQUIRE_MESSAGE(SamplePlayerPresentationPose(definition, state, actions, combined, error), error);
        for (std::size_t node = 0; node < model.nodes.size(); ++node) {
            CAPTURE(model.nodes[node].name);
            const auto local = ToMatrix(combined.body.localTransforms[node]);
            CheckMatrix(local, ToMatrix((definition.upperBodyMask[node] ? upper : lower).localTransforms[node]));
            const auto parent = model.nodes[node].parentIndex;
            CheckMatrix(combined.body.globalTransforms[node], parent ?
                Multiply(combined.body.globalTransforms[*parent], local) : local);
        }
        const auto mount = Multiply(combined.body.globalTransforms.at(definition.weaponNode),
            ToMatrix(definition.weaponMount));
        for (std::size_t node = 0; node < combined.weapon.globalTransforms.size(); ++node)
            CheckMatrix(combined.weapon.globalTransforms[node],
                Multiply(mount, definition.weaponReferencePose.globalTransforms[node]));
    };
    PlayerActionPose actions;
    actions.upper = PlayerUpperAction::Shoot;
    actions.upperClipSeconds = 0.2;
    check(actions, shoot, legs);
    actions.lower = PlayerLowerAction::JumpLoop;
    actions.lowerClipSeconds = 0.4;
    check(actions, shoot, loop);
    actions.lower = PlayerLowerAction::Death;
    actions.lowerClipSeconds = 1.0;
    check(actions, death, death); // The upper action is ignored while dead.
}

// Math B6b characterization with production content; random inputs are in
// PresentationMathCharacterizationTests.cpp.
TEST_CASE("characterization: production weapon mount rotation normalizes as the legacy double reciprocal did") {
    PresentationAssets fixture;
    const auto* entry = fixture.catalog.Find(Engine::Asset::AssetId::FromString("object_fps_pvp.player.presentation"));
    REQUIRE(entry);
    const auto bytes = fixture.source.ReadAll(entry->resolvedPath);
    REQUIRE(bytes);
    const auto json = nlohmann::json::parse(std::string_view(
        reinterpret_cast<const char*>(bytes.value().data()), bytes.value().size()));
    const auto& r = json.at("weapon").at("rotation_xyzw");
    const Engine::Math::Quaternion authored{r[0].get<float>(), r[1].get<float>(), r[2].get<float>(), r[3].get<float>()};
    const auto legacy = PresentationLegacy::NormalizeMountRotation(authored);
    const auto& loaded = ProductionDefinition().weaponMount.rotation;
    CHECK(loaded.x == legacy.x);
    CHECK(loaded.y == legacy.y);
    CHECK(loaded.z == legacy.z);
    CHECK(loaded.w == legacy.w);
}

TEST_CASE("characterization: production weapon muzzle keeps its load-time value and drifts only under recoil") {
    PresentationAssets fixture;
    std::string error;
    const auto definition = fps::LoadWeaponPresentationDefinition(
        fixture.assets, Engine::Asset::AssetId::FromString("object_fps_pvp.weapon.mark23"), error);
    REQUIRE_MESSAGE(definition, error);
    const auto muzzlePoint = [&](const Pose& pose) {
        return Engine::Math::TransformPoint(
            pose.globalTransforms[definition->muzzleNodeIndex], definition->muzzleLocalPosition);
    };
    Pose pose;
    REQUIRE(static_cast<bool>(SamplePose(*definition->model, definition->clips[1], 0, PlaybackMode::Clamp, pose)));
    const auto legacyShot = PresentationLegacy::MuzzleViewCameraPosition(
        muzzlePoint(pose), definition->idleAnchor, definition->placement);
    const auto& shot = definition->shotGeometry.muzzleViewCameraPosition;
    CHECK(shot.x == legacyShot.x);
    CHECK(shot.y == legacyShot.y);
    CHECK(shot.z == legacyShot.z);

    // The viewmodel adds the shot recoil to the placement's X rotation.
    float maximumError = 0;
    std::size_t samples = 0;
    std::size_t recoilFreeDiffering = 0;
    for (const auto clip : definition->clips) {
        const double duration = definition->model->clips[clip].durationSeconds;
        for (double time = 0; time <= duration; time += 1.0 / 30.0) {
            REQUIRE(static_cast<bool>(SamplePose(*definition->model, clip, time, PlaybackMode::Clamp, pose)));
            const auto point = muzzlePoint(pose);
            for (const float recoil : {0.0F, 0.005F, 0.02F, 0.04F}) {
                auto placement = definition->placement;
                placement.rotationRadians.x += recoil;
                const auto legacy = PresentationLegacy::MuzzleViewCameraPosition(point, definition->idleAnchor, placement);
                const auto current = fps::EvaluateWeaponMuzzleViewCameraPosition(*definition, pose, placement);
                const float difference = (std::max)({std::abs(legacy.x - current.x), std::abs(legacy.y - current.y),
                                                     std::abs(legacy.z - current.z)});
                if (recoil == 0.0F && difference != 0.0F) ++recoilFreeDiffering;
                maximumError = (std::max)(maximumError, difference);
                CHECK(current.z > definition->camera.nearClip);
                ++samples;
            }
        }
    }
    MESSAGE("production muzzle: max |delta| " << maximumError << " m over " << samples << " samples");
    CHECK(recoilFreeDiffering == 0);
    // 32 u M (PresentationMathCharacterizationTests.cpp) with M < 2 m here.
    CHECK(maximumError <= 32.0F * 0x1p-24F * 2.0F);
}

TEST_CASE("PvP player weapon mount rejects degenerate and overflowing rotations") {
    const auto rejects = [](const nlohmann::json& rotation) {
        PresentationAssets fixture;
        fixture.Override("object_fps_pvp.player.presentation", [&](auto& json) {
            json["weapon"]["rotation_xyzw"] = rotation;
        });
        std::string error;
        return !LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error) &&
            error.find("weapon mount") != std::string::npos;
    };
    CHECK(rejects(nlohmann::json::array({0, 0, 0, 0})));
    CHECK(rejects(nlohmann::json::array({1e-7, 0, 0, 0})));
    // The squared length must stay finite in float: content this large was
    // accepted before B6b (the length was summed in double) and is now rejected.
    CHECK(rejects(nlohmann::json::array({1e20, 0, 0, 1e20})));
    // A rotation just above the threshold still loads.
    CHECK_FALSE(rejects(nlohmann::json::array({0, 0, 0, 1e-5})));
}
