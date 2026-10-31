#include <doctest/doctest.h>

#include "RetroFPS/Pvp/SnapshotTimeline.hpp"
#include "RetroFPS/Pvp/SoundCues.hpp"
#include "engine/model/AnimationEvents.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace {
using namespace fps::pvp;
using Engine::Math::Vec3;

std::string ProductText() {
    std::ifstream file(PVP_SOUND_SETTINGS_PATH);
    REQUIRE(file);
    std::stringstream text;
    text << file.rdbuf();
    return text.str();
}

SoundSettings ProductSettings() {
    std::string error;
    const auto settings = ParseSoundSettings(ProductText(), error);
    REQUIRE_MESSAGE(settings, error);
    return *settings;
}

WorldSnapshot Snapshot(std::uint64_t tick, ActionId remoteShot, std::uint64_t remoteLife = 1) {
    WorldSnapshot snapshot;
    snapshot.tick = tick;
    snapshot.players.push_back({.playerId = 1, .position = {0, 0, 0}});
    snapshot.players.push_back({.playerId = 2, .position = {3, 0, 4}, .lifeGeneration = remoteLife});
    CombatState self;
    self.playerId = 1;
    self.lastShotActionId = 99; // the local player's own shots never sound as remote
    self.lastShotTick = tick;
    CombatState remote;
    remote.playerId = 2;
    remote.lifeGeneration = remoteLife;
    remote.lastShotActionId = remoteShot;
    remote.lastShotTick = remoteShot ? tick - 1 : 0;
    snapshot.combat = {self, remote};
    return snapshot;
}
} // namespace

TEST_CASE("PvP sound settings parse the product content and reject bad values") {
    const auto settings = ProductSettings();
    CHECK(settings.cues[static_cast<std::size_t>(SoundCue::LocalShot)].clip == "object_fps_pvp.audio.gun_shoot");
    CHECK(settings.cues[static_cast<std::size_t>(SoundCue::HitConfirmed)].clip == "object_fps_pvp.audio.hit_flesh");
    CHECK(settings.strideDistance > 0);
    std::string error;
    CHECK_FALSE(ParseSoundSettings("{\"version\":2}", error));
    CHECK_FALSE(error.empty());
    const auto product = nlohmann::json::parse(ProductText());
    const auto rejects = [&error, &product](const char* what, const auto& change) {
        auto changed = product;
        change(changed);
        CHECK_FALSE_MESSAGE(ParseSoundSettings(changed.dump(), error), what);
    };
    rejects("version 1", [](nlohmann::json& j) { j["version"] = 1; });
    rejects("no animation_events", [](nlohmann::json& j) { j.erase("animation_events"); });
    rejects("an unknown cue", [](nlohmann::json& j) { j["animation_events"]["magazine_in"]["cue"] = "reloading"; });
    rejects("a negative sync point", [](nlohmann::json& j) { j["animation_events"]["magazine_in"]["sync_seconds"] = -0.1; });
    rejects("an empty event name", [](nlohmann::json& j) { j["animation_events"][""] = {{"cue", "reload"}, {"sync_seconds", 0}}; });
    std::ifstream file(PVP_SOUND_SETTINGS_PATH);
    std::stringstream text;
    text << file.rdbuf();
    std::string bad = text.str();
    bad.replace(bad.find("\"volume\": 0.8"), 13, "\"volume\": 9.0");
    CHECK_FALSE(ParseSoundSettings(bad, error));
}

TEST_CASE("PvP spatial gains pan by bearing with constant power and fall off with distance") {
    SoundSettings settings = ProductSettings();
    settings.referenceDistance = 2;
    settings.minimumGain = 0.1F;
    // Yaw 0 looks along +Z; +X is to the right.
    const auto ahead = SpatialGains({0, 0, 0}, 0, {0, 0, 1}, 1, settings);
    CHECK(ahead.left == doctest::Approx(1));
    CHECK(ahead.right == doctest::Approx(1));
    const auto right = SpatialGains({0, 0, 0}, 0, {1, 0, 0}, 1, settings);
    CHECK(right.left == doctest::Approx(0).epsilon(1e-6));
    CHECK(right.right == doctest::Approx(std::sqrt(2.0F)));
    const auto left = SpatialGains({0, 0, 0}, 0, {-1, 0, 0}, 1, settings);
    CHECK(left.left > left.right);
    // Turning the listener right puts a source ahead on its left.
    const auto turned = SpatialGains({0, 0, 0}, 1.5707964F, {0, 0, 1}, 1, settings);
    CHECK(turned.left > turned.right);
    // Equal power on both sides: left^2 + right^2 is constant around the listener.
    for (const float angle : {0.0F, 0.5F, 1.0F, 2.0F, 3.0F}) {
        const auto g = SpatialGains({0, 0, 0}, 0, {std::sin(angle), 0, std::cos(angle)}, 1, settings);
        CHECK(g.left * g.left + g.right * g.right == doctest::Approx(2));
    }
    const auto far = SpatialGains({0, 0, 0}, 0, {0, 0, 8}, 1, settings);
    CHECK(far.left == doctest::Approx(0.25)); // reference 2 over distance 8
    const auto farther = SpatialGains({0, 0, 0}, 0, {0, 0, 100}, 1, settings);
    CHECK(farther.left == doctest::Approx(0.1)); // never below the minimum gain
    const auto here = SpatialGains({1, 0, 1}, 0, {1, 2, 1}, 0.5F, settings);
    CHECK(here.left == doctest::Approx(here.right));
}

TEST_CASE("PvP remote shot tracker plays each new remote shot once and never replays a life's first state") {
    RemoteShotTracker tracker;
    CHECK(tracker.Observe(Snapshot(10, 5), 1).empty()); // first state seen: an old shot, not replayed
    CHECK(tracker.Observe(Snapshot(11, 5), 1).empty()); // the same shot again
    const auto shots = tracker.Observe(Snapshot(12, 6), 1);
    REQUIRE(shots.size() == 1);
    CHECK(shots[0].playerId == 2);
    CHECK(shots[0].tick == 11);
    CHECK(shots[0].position.x == doctest::Approx(3));
    CHECK(tracker.Observe(Snapshot(13, 6), 1).empty());
    CHECK(tracker.Observe(Snapshot(14, 7, 2), 1).empty()); // a new life: its first state does not sound
    CHECK(tracker.Observe(Snapshot(15, 8, 2), 1).size() == 1);
    tracker.Reset();
    CHECK(tracker.Observe(Snapshot(16, 9, 2), 1).empty());
}

TEST_CASE("PvP footstep tracker steps every stride on the ground and lands once") {
    SoundSettings settings = ProductSettings();
    settings.strideDistance = 1;
    settings.teleportDistance = 2;
    FootstepTracker steps;
    using Step = FootstepTracker::Step;
    CHECK(steps.Observe({0, 0, 0}, true, 1, settings) == Step::None); // the first sample only starts
    CHECK(steps.Observe({0.6F, 0, 0}, true, 1, settings) == Step::None);
    CHECK(steps.Observe({1.2F, 0, 0}, true, 1, settings) == Step::Footstep);
    CHECK(steps.Observe({1.8F, 0, 0}, true, 1, settings) == Step::None);
    CHECK(steps.Observe({2.4F, 0, 0}, true, 1, settings) == Step::Footstep);
    // In the air no steps, however far; touching down is one landing.
    CHECK(steps.Observe({3.4F, 1, 0}, false, 1, settings) == Step::None);
    CHECK(steps.Observe({4.4F, 1, 0}, false, 1, settings) == Step::None);
    CHECK(steps.Observe({4.5F, 0, 0}, true, 1, settings) == Step::Landing);
    CHECK(steps.Observe({4.6F, 0, 0}, true, 1, settings) == Step::None);
    // Standing still never steps.
    CHECK(steps.Observe({4.6F, 0, 0}, true, 1, settings) == Step::None);
    // A teleport or a new life restarts silently.
    CHECK(steps.Observe({20, 0, 0}, true, 1, settings) == Step::None);
    CHECK(steps.Observe({20.9F, 0, 0}, false, 1, settings) == Step::None);
    CHECK(steps.Observe({20.9F, 0, 0}, true, 2, settings) == Step::None); // new life: no landing replay
}

TEST_CASE("PvP timeline display time is the inverse of the presentation cursor") {
    SnapshotTimeline timeline;
    const auto at = [](double seconds) {
        return SnapshotTimeline::Clock::time_point{} +
            std::chrono::duration_cast<SnapshotTimeline::Clock::duration>(std::chrono::duration<double>(seconds));
    };
    CHECK_FALSE(timeline.DisplayTime(5));
    WorldSnapshot state;
    state.players.push_back({.playerId = 2});
    for (std::uint64_t tick = 100; tick < 130; ++tick) {
        state.tick = tick;
        // Arrival jitters up to 6 ms late; the phase follows the earliest arrival.
        const double jitter = (tick % 3) * 0.003;
        REQUIRE(timeline.Push(state, at(1.0 + (tick - 100) * MovementTickSeconds + jitter)));
    }
    // Tick 100 arrived at 1.0 s and shows one tick later; each tick is 1/60 s.
    const auto shown = timeline.DisplayTime(120);
    REQUIRE(shown);
    const double seconds = std::chrono::duration<double>(shown->time_since_epoch()).count();
    CHECK(seconds == doctest::Approx(1.0 + 21 * MovementTickSeconds).epsilon(1e-9));
    // Sampling at that instant puts the cursor on that tick.
    const auto sampled = timeline.Sample(2, *shown);
    REQUIRE(sampled);
    CHECK(sampled->presentationTick == doctest::Approx(120).epsilon(1e-6));
    // Ticks a whole number apart stay exactly that far apart: spacing follows authority ticks.
    const double gap = std::chrono::duration<double>(*timeline.DisplayTime(127) - *timeline.DisplayTime(121)).count();
    CHECK(gap == doctest::Approx(6 * MovementTickSeconds).epsilon(1e-9));
    // A tick before the timeline's base lies in the past, not far ahead.
    CHECK(*timeline.DisplayTime(90) < *timeline.DisplayTime(100));
    const double old = std::chrono::duration<double>(timeline.DisplayTime(90)->time_since_epoch()).count();
    CHECK(old == doctest::Approx(1.0 - 9 * MovementTickSeconds).epsilon(1e-9)); // ten ticks before the base, shown one tick late
}

TEST_CASE("PvP animation event sounds start at the stretched moment less their sync point") {
    // A 3 s clip over a 1.5 s action: the event at 2 s of the clip is 1 s into the action.
    CHECK(*EventSoundOffsetSeconds(2, 3, 1.5, 0) == doctest::Approx(1));
    CHECK(*EventSoundOffsetSeconds(2, 3, 1.5, 0.25) == doctest::Approx(0.75));
    CHECK(*EventSoundOffsetSeconds(2, 1, 4, 0) == doctest::Approx(8)); // a slowed clip
    CHECK(*EventSoundOffsetSeconds(0.1, 1, 1, 0.3) == doctest::Approx(-0.2)); // the sound leads the action
    CHECK_FALSE(EventSoundOffsetSeconds(1, 0, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, 0, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(std::nan(""), 3, 1.5, 0));
    CHECK_FALSE(EventSoundOffsetSeconds(1, 3, 1.5, std::numeric_limits<double>::infinity()));
}

TEST_CASE("PvP animation event sounds name events of the weapon's gyo.anim_events") {
    // The mapping and the events are separate content; a renamed event must not
    // silence its sound unnoticed.
    const auto settings = ProductSettings();
    std::ifstream file(PVP_WEAPON_ANIM_EVENTS_PATH);
    REQUIRE(file);
    std::stringstream text;
    text << file.rdbuf();
    const auto events = Engine::Model::ParseAnimationEvents(text.str());
    if (!events) FAIL(Engine::Base::Describe(events.error()));
    REQUIRE_FALSE(settings.eventSounds.empty());
    for (const auto& sound : settings.eventSounds) {
        bool named = false;
        for (const auto& track : events.value().tracks)
            for (const auto& event : track.events) named = named || event.name == sound.event;
        CHECK_MESSAGE(named, sound.event);
    }
    const auto* magazine = FindEventSound(settings, "magazine_in");
    REQUIRE(magazine);
    CHECK(magazine->cue == SoundCue::Reload);
    CHECK_FALSE(FindEventSound(settings, "no_such_event"));
}

TEST_CASE("PvP hit taken sounds for a new hit and the killing one, never for a new life's first state") {
    CHECK(HitTakenSounds(HitFeedbackOutcome::Played));
    CHECK(HitTakenSounds(HitFeedbackOutcome::SuppressedDead)); // the killing blow sounds, though it does not flash
    CHECK_FALSE(HitTakenSounds(HitFeedbackOutcome::SuppressedNewLife));
    CHECK_FALSE(HitTakenSounds(HitFeedbackOutcome::None));
}
