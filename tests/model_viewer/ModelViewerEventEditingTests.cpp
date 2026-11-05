// -events judgments E1-E4 (docs/tools/plans/v8/model-viewer.md): edits keep a
// clip's events in order, Undo and Redo give every earlier and later state,
// a dragged marker stops at the clip's ends while a typed moment is Engine's
// to refuse, and a marker lights when playback passes it (Crossed only).
#include <doctest/doctest.h>

#include "gyo/model_viewer/EventDocument.hpp"
#include "gyo/model_viewer/EventHistory.hpp"
#include "gyo/model_viewer/Playback.hpp"

#include "engine/model/Animation.hpp"

#include <optional>
#include <string>
#include <vector>

using namespace Gyo::Tools::ModelViewer;
namespace Model = Engine::Model;

namespace {

constexpr const char* kClip = "Armature|Reload";

// One node and one 2.5 s clip, enough for Engine to bind events to it.
Model::ModelAsset MakeModel() {
    Model::ModelAsset model;
    model.nodes.push_back({"root", std::nullopt, {{0, 0, 0}, {}, {1, 1, 1}}});
    Model::AnimationClip clip;
    clip.name = kClip;
    clip.durationSeconds = 2.5;
    model.clips.push_back(clip);
    REQUIRE(Model::ValidateModel(model));
    return model;
}

Model::AnimationEventSet ThreeEvents() {
    return {"game.model.rifle", {{kClip, {{0.5, "a"}, {1.0, "b"}, {2.0, "c"}}}}};
}

std::vector<std::string> Names(const EventHistory& history) {
    std::vector<std::string> names;
    for (const auto& event : history.Document().Track(kClip)->events) names.push_back(event.name);
    return names;
}

bool InOrder(const Model::AnimationEventTrack& track) {
    for (std::size_t index = 1; index < track.events.size(); ++index) {
        if (track.events[index].seconds < track.events[index - 1].seconds) return false;
    }
    return true;
}

} // namespace

TEST_CASE("E1: a move past other events reorders them, and Engine accepts every edit") {
    const auto model = MakeModel();
    EventHistory history(ThreeEvents());
    CHECK(history.Move(kClip, 0, 1.5) == 1);
    CHECK_MESSAGE(Names(history) == (std::vector<std::string>{"b", "a", "c"}), "the move did not reorder the clip");
    CHECK(history.Move(kClip, 2, 0.1) == 0);
    CHECK(Names(history) == (std::vector<std::string>{"c", "b", "a"}));
    CHECK(history.Add(kClip, {1.5, "d"}) == 3); // after the event at the same moment
    CHECK(Names(history) == (std::vector<std::string>{"c", "b", "a", "d"}));
    history.Rename(kClip, 0, "c2");
    history.Remove(kClip, 1);
    CHECK(Names(history) == (std::vector<std::string>{"c2", "a", "d"}));
    CHECK(InOrder(*history.Document().Track(kClip)));
    CHECK(CheckedText(history.Document().Set(), model));
}

TEST_CASE("E2: Undo and Redo give the events of every step, field by field, and saving is tracked") {
    EventHistory history(ThreeEvents());
    std::vector<Model::AnimationEventSet> states{history.Document().Set()};
    history.Move(kClip, 0, 1.5);
    states.push_back(history.Document().Set());
    history.Rename(kClip, 1, "renamed");
    states.push_back(history.Document().Set());
    history.Remove(kClip, 2);
    states.push_back(history.Document().Set());
    history.Add(kClip, {0.25, "new"});
    states.push_back(history.Document().Set());
    CHECK(history.Unsaved());

    for (std::size_t step = states.size() - 1; step > 0; --step) {
        REQUIRE(history.Undo());
        CAPTURE(step);
        CHECK_MESSAGE(SameEvents(history.Document().Set(), states[step - 1]), "Undo did not give the earlier events");
    }
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.Unsaved());
    for (std::size_t step = 1; step < states.size(); ++step) {
        REQUIRE(history.Redo());
        CHECK(SameEvents(history.Document().Set(), states[step]));
    }
    CHECK_FALSE(history.CanRedo());

    // Saved, then one step back: unsaved again; forward again: saved.
    history.MarkSaved();
    CHECK_FALSE(history.Unsaved());
    REQUIRE(history.Undo());
    CHECK_MESSAGE(history.Unsaved(), "Undo after saving does not show the document as unsaved");
    REQUIRE(history.Redo());
    CHECK_FALSE(history.Unsaved());

    // A new edit after Undo drops the redo steps; the saved state is gone with them.
    REQUIRE(history.Undo());
    history.Rename(kClip, 0, "other");
    CHECK_FALSE(history.CanRedo());
    CHECK(history.Unsaved());
    // A rename to the same name is no step.
    REQUIRE(history.CanUndo());
    history.Rename(kClip, 0, "other");
    REQUIRE(history.Undo());
    CHECK(history.Document().Track(kClip)->events[0].name != "other");
}

TEST_CASE("E2: a move to the same moment is no step; a new document is unsaved in every state") {
    EventHistory history(ThreeEvents());
    CHECK(history.Move(kClip, 1, 1.0) == 1);
    CHECK_FALSE(history.CanUndo());
    CHECK_FALSE(history.Unsaved());

    EventHistory fresh({"game.model.rifle", {}}, false);
    CHECK(fresh.Unsaved());
    fresh.Add(kClip, {1.0, "a"});
    REQUIRE(fresh.Undo());
    CHECK_MESSAGE(fresh.Unsaved(), "a document never written shows as saved");
}

TEST_CASE("E2: Undo gives back a renamed event's name") {
    EventHistory history(ThreeEvents());
    history.Rename(kClip, 1, "renamed");
    REQUIRE(history.Undo());
    CHECK_MESSAGE(history.Document().Track(kClip)->events[1].name == "b", "Undo did not give the name back");
}

TEST_CASE("E3: a dragged marker stops at the clip's ends; a typed moment outside is Engine's to refuse") {
    CHECK(DragSeconds(-0.4, 2.5) == 0.0);
    CHECK(DragSeconds(3.0, 2.5) == 2.5);
    CHECK(DragSeconds(1.25, 2.5) == 1.25);

    const auto model = MakeModel();
    EventHistory history(ThreeEvents());
    history.Move(kClip, 2, 3.0); // typed, not clamped
    CHECK(history.Document().Track(kClip)->events[2].seconds == 3.0);
    const auto written = CheckedText(history.Document().Set(), model);
    REQUIRE_FALSE(written);
    CHECK(written.error().code == ModelViewerErrorCode::InvalidEvents);
}

TEST_CASE("E4: a marker lights once when playback passes it, also across the loop's end") {
    const Model::AnimationEventTrack track{kClip, {{0.5, "a"}, {1.0, "b"}, {2.0, "c"}}};
    PlaybackClock clock;
    clock.durationSeconds = 2.5;
    clock.playing = true;
    clock.loop = true;

    // 0 -> 0.75: passes a only.
    auto step = clock.Advance(0.75);
    REQUIRE(step);
    CHECK(CrossedEvents(track, *step) == (std::vector<std::size_t>{0}));
    // 0.75 -> 0.9: passes nothing, although a lies behind (not "now >= event").
    step = clock.Advance(0.15);
    REQUIRE(step);
    CHECK_MESSAGE(CrossedEvents(track, *step).empty(), "an event behind the playhead lit again");
    // 0.9 -> 2.9, wrapping to 0.4: passes b and c, not a.
    step = clock.Advance(2.0);
    REQUIRE(step);
    CHECK(clock.seconds == doctest::Approx(0.4));
    CHECK(CrossedEvents(track, *step) == (std::vector<std::size_t>{1, 2}));
    // 0.4 -> 0.6 after the wrap: a lights again, once.
    step = clock.Advance(0.2);
    REQUIRE(step);
    CHECK_MESSAGE(CrossedEvents(track, *step) == (std::vector<std::size_t>{0}), "the event after the loop's end did not light");
    // 2.2 -> 3.1: across the end, a (0.5) lights in the next loop; a clamped clock would light nothing.
    clock.Seek(2.2);
    step = clock.Advance(0.9);
    REQUIRE(step);
    CHECK_MESSAGE(CrossedEvents(track, *step) == (std::vector<std::size_t>{0}), "a looping clock's step is not a loop's");

    // A paused clock goes nowhere: nothing lights.
    clock.playing = false;
    CHECK_FALSE(clock.Advance(1.0));
    // A clamped clock stops at the end and lights what lies before it.
    clock.loop = false;
    clock.playing = true;
    clock.Seek(1.5);
    step = clock.Advance(5.0);
    REQUIRE(step);
    CHECK(step->currentSeconds == 2.5);
    CHECK(CrossedEvents(track, *step) == (std::vector<std::size_t>{2}));
}
