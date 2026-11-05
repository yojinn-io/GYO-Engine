// -preview judgments V1 and V3 (docs/tools/plans/v8/model-viewer.md): the
// preview's pose is Engine's SamplePose at the clip seconds the clock gives,
// and the window's model is the last one opened or none.
#include <doctest/doctest.h>

#include "gyo/model_viewer/ModelSession.hpp"
#include "gyo/model_viewer/Playback.hpp"

#include "engine/model/Animation.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

using namespace Gyo::Tools::ModelViewer;
namespace Model = Engine::Model;

namespace {

// Two nodes and one clip moving the child up by 2 over 2.5 s; built in code.
Model::ModelAsset MakeAnimated() {
    Model::ModelAsset model;
    model.nodes.push_back({"root", std::nullopt, {{0, 0, 0}, {}, {1, 1, 1}}});
    model.nodes.push_back({"joint", 0, {{0, 1, 0}, {}, {1, 1, 1}}});
    Model::AnimationClip clip;
    clip.name = "Lift";
    clip.durationSeconds = 2.5;
    Model::NodeTrack track;
    track.nodeIndex = 1;
    track.translations = {{0, {0, 1, 0}}, {2.5, {0, 3, 0}}};
    clip.tracks.push_back(track);
    model.clips.push_back(clip);
    REQUIRE(Model::ValidateModel(model));
    return model;
}

void CheckSamePose(const Model::Pose& preview, const Model::Pose& engine) {
    REQUIRE(preview.globalTransforms.size() == engine.globalTransforms.size());
    for (std::size_t node = 0; node < engine.globalTransforms.size(); ++node) {
        for (std::size_t element = 0; element < 16; ++element) {
            CAPTURE(node);
            CAPTURE(element);
            CHECK(preview.globalTransforms[node].values[element] == engine.globalTransforms[node].values[element]);
        }
    }
}

Model::Pose Sampled(const Model::ModelAsset& model, const double seconds, const Model::PlaybackMode mode) {
    Model::Pose pose;
    REQUIRE(Model::SamplePose(model, 0, seconds, mode, pose));
    return pose;
}

std::filesystem::path TemporaryDirectory(const std::string& name) {
    static const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / ("gyo_model_viewer_playback_" + run + "_" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::ofstream(path, std::ios::binary) << text;
}

} // namespace

TEST_CASE("V1: the preview's pose is SamplePose at the clip's start, middle and end") {
    const auto model = MakeAnimated();
    for (const double seconds : {0.0, 1.25, 2.5}) {
        CAPTURE(seconds);
        PlaybackClock clock;
        clock.durationSeconds = 2.5;
        clock.loop = false;
        clock.Seek(seconds);
        CHECK(clock.seconds == seconds);
        Model::Pose preview;
        REQUIRE(PreviewPose(model, 0, clock, preview));
        CheckSamePose(preview, Sampled(model, seconds, Model::PlaybackMode::Clamp));
    }
    // The middle is not the rest pose: a preview of the bind pose would differ.
    PlaybackClock middle;
    middle.durationSeconds = 2.5;
    middle.Seek(1.25);
    Model::Pose preview, rest;
    REQUIRE(PreviewPose(model, 0, middle, preview));
    REQUIRE(Model::MakeDefaultPose(model, rest));
    CHECK_MESSAGE(preview.globalTransforms[1].values[13] != rest.globalTransforms[1].values[13],
                  "the preview drew the rest pose, not the sampled one");
}

TEST_CASE("V1: at speed 0.5 and 2.0 the same wall time gives speed times as many clip seconds") {
    const auto model = MakeAnimated();
    for (const double speed : {0.5, 2.0}) {
        CAPTURE(speed);
        PlaybackClock clock;
        clock.durationSeconds = 2.5;
        clock.speed = speed;
        clock.playing = true;
        clock.Advance(0.25);
        clock.Advance(0.25);
        // Two steps of 0.25 s of wall time: 0.5 * speed clip seconds.
        CHECK_MESSAGE(clock.seconds == 0.5 * speed, "the speed is applied other than once");
        Model::Pose preview;
        REQUIRE(PreviewPose(model, 0, clock, preview));
        CheckSamePose(preview, Sampled(model, clock.seconds, Model::PlaybackMode::Loop));
    }
}

TEST_CASE("V1: a looping clock wraps, a clamped one stops at the end, a paused one stays") {
    PlaybackClock clock;
    clock.durationSeconds = 2.5;
    clock.playing = true;
    clock.Seek(2.0);
    clock.Advance(1.0);
    CHECK(clock.seconds == doctest::Approx(0.5));
    CHECK(clock.playing);

    clock.loop = false;
    clock.Seek(2.0);
    clock.Advance(1.0);
    CHECK(clock.seconds == 2.5);
    CHECK_FALSE(clock.playing);

    clock.Seek(1.0);
    clock.Advance(1.0);
    CHECK(clock.seconds == 1.0);
    clock.playing = true;
    clock.Advance(-1.0);
    CHECK(clock.seconds == 1.0);
    clock.Seek(9.0);
    CHECK(clock.seconds == 2.5);
    clock.Seek(-1.0);
    CHECK(clock.seconds == 0.0);
}

TEST_CASE("V3: a model without clips shows its default pose; one without meshes has a pose too") {
    auto model = MakeAnimated();
    model.clips.clear();
    PlaybackClock clock;
    Model::Pose preview, rest;
    REQUIRE(PreviewPose(model, std::nullopt, clock, preview));
    REQUIRE(Model::MakeDefaultPose(model, rest));
    CheckSamePose(preview, rest);
    CHECK(model.meshes.empty());
}

TEST_CASE("V3: a failed open shows Engine's reason and nothing of the model opened before") {
    const auto root = TemporaryDirectory("session");
    std::filesystem::copy_file(std::filesystem::path(GYO_MODEL_VIEWER_FIXTURES) / "animated_triangle.fbx",
                               root / "triangle.fbx");
    WriteFile(root / "broken.fbx", "not a model");
    WriteFile(root / "asset_catalog.json",
              "{ \"version\": 1, \"assets\": [ "
              "{ \"id\": \"game.model.triangle\", \"type\": \"model\", \"path\": \"triangle.fbx\" }, "
              "{ \"id\": \"game.model.broken\", \"type\": \"model\", \"path\": \"broken.fbx\" } ] }\n");
    auto content = ModelContent::Mount(root, root / "asset_catalog.json");
    REQUIRE(content);

    ModelSession session;
    REQUIRE(session.Open(*content, "game.model.triangle"));
    REQUIRE(session.Model() != nullptr);
    CHECK(session.Clip() == std::optional<std::size_t>(0));
    CHECK(session.clock.durationSeconds == doctest::Approx(2.0 / 30));
    CHECK(session.Error().empty());

    CHECK_FALSE(session.Open(*content, "game.model.broken"));
    CHECK_MESSAGE(session.Model() == nullptr, "the model opened before is still shown");
    CHECK_FALSE(session.Clip().has_value());
    CHECK(session.Error().find("Engine refuses the model") != std::string::npos);

    REQUIRE(session.Open(*content, "game.model.triangle"));
    CHECK(session.Error().empty());
    CHECK_FALSE(session.Open(*content, "game.model.missing"));
    CHECK(session.Model() == nullptr);
    CHECK_FALSE(session.Error().empty());
    CHECK_FALSE(session.SelectClip(0));
}
