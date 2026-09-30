#include <doctest/doctest.h>

#include "RetroFPS/Pvp/PlayerPresentation.hpp"
#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/ContentManifest.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "gyo/AppConfig.hpp"
#include "model/backend/ufbx/UfbxModelLoader.hpp"

#include <SDL3/SDL_filesystem.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {
using namespace fps::pvp;
using namespace Engine::Model;

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

PlayerPresentationFrame Frame(double seconds, double delta, float x = 0, float z = 0) {
    return {.playerId = 1, .movementEpoch = 1, .position = {x, 0, z}, .yaw = 0,
        .presentationSeconds = seconds, .deltaSeconds = delta};
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
        state.phaseSeconds = phase;
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

TEST_CASE("PvP scaled Jog foot travel provides measured stride calibration evidence") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    const double duration = model.clips.at(definition.jogClip).durationSeconds;
    constexpr std::size_t samples = 120;
    // This is an explicit bone-height approximation of stance, not a claim
    // that a rendered shoe sole has exact contact or zero foot sliding.
    constexpr double contactBandMeters = 0.025;
    for (const std::string_view name : {"foot_l", "foot_r", "ball_l", "ball_r"}) {
        const auto node = model.FindNode(name);
        if (!node && name.starts_with("ball_")) continue;
        REQUIRE(node);
        std::array<Vec3, samples> points;
        double minimumY = std::numeric_limits<double>::max();
        double maximumY = -minimumY, minimumZ = minimumY, maximumZ = -minimumY;
        for (std::size_t frame = 0; frame < samples; ++frame) {
            Pose pose;
            REQUIRE(SamplePose(model, definition.jogClip,
                duration * frame / samples, PlaybackMode::Loop, pose));
            const auto point = TransformPoint(pose.globalTransforms[*node], {});
            points[frame] = {(point.x - definition.anchor.x) * definition.scale,
                (point.y - definition.anchor.y) * definition.scale,
                (point.z - definition.anchor.z) * definition.scale};
            minimumY = std::min(minimumY, static_cast<double>(points[frame].y));
            maximumY = std::max(maximumY, static_cast<double>(points[frame].y));
            minimumZ = std::min(minimumZ, static_cast<double>(points[frame].z));
            maximumZ = std::max(maximumZ, static_cast<double>(points[frame].z));
        }
        double backwardContactDistance{}, backwardContactSeconds{};
        std::array<bool, samples> contact;
        std::array<int, samples> forwardDirection, verticalDirection;
        const auto direction = [](double delta) { return delta > 1e-5 ? 1 : delta < -1e-5 ? -1 : 0; };
        for (std::size_t frame = 0; frame < samples; ++frame) {
            const auto& from = points[frame];
            const auto& to = points[(frame + 1) % samples];
            forwardDirection[frame] = direction(to.z - from.z);
            verticalDirection[frame] = direction(to.y - from.y);
            contact[frame] = std::max(from.y, to.y) <= minimumY + contactBandMeters && to.z < from.z;
            if (contact[frame]) {
                backwardContactDistance += from.z - to.z;
                backwardContactSeconds += duration / samples;
            }
        }
        std::size_t contactRuns{}, forwardPeaks{}, backwardPeaks{}, lowPoints{};
        std::string contactStarts, forwardPeakPhases;
        for (std::size_t frame = 0; frame < samples; ++frame) {
            const auto previous = (frame + samples - 1) % samples;
            if (contact[frame] && !contact[previous]) {
                ++contactRuns;
                contactStarts += std::to_string(static_cast<double>(frame) / samples) + ",";
            }
            if (forwardDirection[previous] > 0 && forwardDirection[frame] < 0) {
                ++forwardPeaks;
                forwardPeakPhases += std::to_string(static_cast<double>(frame) / samples) + ",";
            }
            backwardPeaks += forwardDirection[previous] < 0 && forwardDirection[frame] > 0;
            lowPoints += verticalDirection[previous] < 0 && verticalDirection[frame] > 0;
        }
        const double contactSpeed = backwardContactSeconds > 0 ?
            backwardContactDistance / backwardContactSeconds : 0;
        INFO("node=", name, " reference_height=", definition.bodyHeight,
            " model_scale=", definition.scale, " jog_duration=", duration,
            " fore_aft_swing_m=", maximumZ - minimumZ,
            " vertical_swing_m=", maximumY - minimumY,
            " backward_contact_distance_m=", backwardContactDistance,
            " backward_contact_seconds=", backwardContactSeconds,
            " backward_contact_speed_mps=", contactSpeed,
            " contact_band_m=", contactBandMeters,
            " contact_runs=", contactRuns, " contact_start_phases=", contactStarts,
            " forward_peaks=", forwardPeaks, " forward_peak_phases=", forwardPeakPhases,
            " backward_peaks=", backwardPeaks, " low_points=", lowPoints,
            " stride_scale=", definition.strideScale,
            " configured_reference_speed_mps=", definition.referenceSpeed,
            " calibrated_contact_speed_mps=", contactSpeed / definition.strideScale);
        CHECK(maximumZ - minimumZ > 0.1);
        CHECK(backwardContactSeconds > 0);
        CHECK(std::isfinite(contactSpeed));
        // A 2.5cm band is approximate; keep a 10% tolerance for stance speed,
        // while catching an uncalibrated full-speed clip at this body height.
        if (name.starts_with("ball_"))
            CHECK(contactSpeed / definition.strideScale == doctest::Approx(definition.referenceSpeed).epsilon(0.1));
    }
}

TEST_CASE("PvP player pose retains jogging legs and aiming upper body with consistent attachments") {
    const auto& definition = ProductionDefinition();
    const auto& model = *definition.character->model;
    PlayerLocomotionState state;
    state.initialized = true;
    state.phaseSeconds = 0.31;
    state.idleSeconds = 0.23;
    state.moveWeight = 1;
    Pose idle, jog;
    REQUIRE(SamplePose(model, definition.idleClip, state.idleSeconds, PlaybackMode::Loop, idle));
    REQUIRE(SamplePose(model, definition.jogClip, state.phaseSeconds, PlaybackMode::Loop, jog));
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
    const auto duration = definition.character->model->clips[definition.jogClip].durationSeconds;
    const double expectedPhase = 3.0 / (definition.referenceSpeed * definition.strideScale);
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
            CHECK(state.unwrappedPhaseSeconds == doctest::Approx(expectedPhase).epsilon(1e-5));
            CHECK(state.phaseSeconds == doctest::Approx(std::fmod(expectedPhase, duration)).epsilon(1e-5));
            CHECK(state.playbackRate == doctest::Approx(1.0 / definition.strideScale).epsilon(1e-4));
            CHECK(state.moveWeight > 0.99F);
            CHECK_FALSE(state.backward);
        }
    }
}

TEST_CASE("PvP stop wall contact turning and timeline hold cannot accumulate phantom footsteps") {
    auto state = Travel(60, 0, 1, 0.5);
    const double phase = state.unwrappedPhaseSeconds;
    const double distance = state.totalDistance;
    for (int step = 1; step <= 30; ++step) {
        auto frame = Frame(0.5 + step / 60.0, 1.0 / 60, 0, 1.5F);
        frame.yaw = static_cast<float>(step) / 10;
        Advance(state, frame);
    }
    CHECK(state.unwrappedPhaseSeconds == phase);
    CHECK(state.totalDistance == distance);
    CHECK(state.distanceDelta == 0);
    CHECK(state.speed == 0);
    CHECK(state.moveWeight < 0.01F);
    // The first hold-labelled sample can still contain the final actual step.
    auto finalStep = Frame(1 + 1.0 / 60, 1.0 / 60, 0, 1.55F);
    finalStep.holding = true;
    Advance(state, finalStep);
    CHECK(state.totalDistance == doctest::Approx(distance + 0.05).epsilon(1e-5));
    const auto heldPhase = state.unwrappedPhaseSeconds;
    const auto resets = state.resetCount;
    for (int step = 0; step < 30; ++step) Advance(state, finalStep);
    CHECK(state.unwrappedPhaseSeconds == heldPhase);
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
    CHECK(backward.unwrappedPhaseSeconds == doctest::Approx(-1.0 / definition.strideScale));
    CHECK(backward.playbackRate == doctest::Approx(-1.0 / definition.strideScale).epsilon(1e-4));
    CHECK(backward.backward);
    auto state = Travel(60, 0, 1, 0.5);
    for (int step = 1; step <= 30; ++step)
        Advance(state, Frame(0.5 + step / 60.0, 1.0 / 60, 0, 1.5F - step * 0.05F));
    CHECK(state.totalDistance == doctest::Approx(3));
    CHECK(state.signedDistance == doctest::Approx(0).epsilon(1e-5).scale(1));
    CHECK(state.unwrappedPhaseSeconds == doctest::Approx(0).epsilon(1e-5).scale(1));
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
        CHECK(state.unwrappedPhaseSeconds == 0);
        CHECK(state.distanceDelta == 0);
        frame.continuous = true;
        frame.deltaSeconds = 1.0 / 60;
        frame.presentationSeconds += frame.deltaSeconds;
        frame.position.z += 0.05F;
        Advance(state, frame);
        CHECK_FALSE(state.phaseReset);
        CHECK(state.distanceDelta == doctest::Approx(0.05).epsilon(1e-4));
        CHECK(state.unwrappedPhaseSeconds == doctest::Approx(1.0 / (60 * definition.strideScale)).epsilon(1e-4));
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
        CHECK(state.unwrappedPhaseSeconds == 0);
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
        CHECK(state.unwrappedPhaseSeconds == 0);
    }
}

TEST_CASE("PvP independent players and rejoined identities cannot inherit another locomotion history") {
    auto first = Travel(60, 0, 1, 0.5);
    PlayerLocomotionState second;
    auto other = Frame(0, 0, 4, 5);
    other.playerId = 2;
    Advance(second, other);
    const auto firstPhase = first.unwrappedPhaseSeconds;
    other.presentationSeconds = 0.05;
    other.deltaSeconds = 0.05;
    other.position.x += 0.15F;
    Advance(second, other);
    CHECK(second.unwrappedPhaseSeconds == doctest::Approx(0.05 / ProductionDefinition().strideScale).epsilon(1e-5));
    CHECK(first.unwrappedPhaseSeconds == firstPhase);
    const auto secondPhase = second.unwrappedPhaseSeconds;
    auto rejoin = Frame(0.5 + 1.0 / 60, 1.0 / 60, 9, 9);
    rejoin.playerId = 3;
    Advance(first, rejoin);
    CHECK(first.playerId == 3);
    CHECK(first.phaseReset);
    CHECK(first.resetReason == "player");
    CHECK(first.unwrappedPhaseSeconds == 0);
    CHECK(second.unwrappedPhaseSeconds == secondPhase);
    CHECK(second.playerId == 2);
}

TEST_CASE("PvP player definitions reject missing clips bones and invalid stride calibration") {
    for (const std::string_view fault : {"mask", "leg_mask", "weapon", "clip", "speed", "stride"}) {
        CAPTURE(fault);
        PresentationAssets fixture;
        if (fault == "clip") {
            fixture.Override("object_fps_pvp.player.animations", [](auto& json) {
                json["clips"]["jog"]["clip"] = "Armature|MissingJog";
            });
        } else {
            fixture.Override("object_fps_pvp.player.presentation", [&](auto& json) {
                if (fault == "mask") json["upper_body_root"] = "MissingSpine";
                if (fault == "leg_mask") json["upper_body_root"] = "pelvis";
                if (fault == "weapon") json["weapon"]["node"] = "MissingHand";
                if (fault == "speed") json["reference_speed"] = 0;
                if (fault == "stride") json["jog_stride_scale"] = 0;
            });
        }
        std::string error;
        CHECK_FALSE(LoadPlayerPresentationDefinition(fixture.assets, 1.8F, error));
        CHECK_FALSE(error.empty());
    }
}
