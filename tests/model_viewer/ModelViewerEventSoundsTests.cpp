// -event-sounds judgments S1-S3 (docs/tools/plans/v8/model-viewer.md): the
// viewer writes gyo.event_sounds text that the framework reads back as the
// same set, and refuses what the framework refuses; a sound starts when the
// framework's EventSoundOffsetSeconds says; a file of another events file is
// not shown.
#include <doctest/doctest.h>

#include "gyo/model_viewer/EventSoundFiles.hpp"

#include "gyo/framework/EventSounds.hpp"
#include "gyo/framework/json/EventSoundsText.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace Gyo::Tools::ModelViewer;
namespace Framework = Gyo::Tools::Framework;
namespace Model = Engine::Model;

namespace {

constexpr const char* kEventsId = "game.anim_events.rifle";

Model::AnimationEventSet Events() {
    return {"game.model.rifle", {{"Armature|Reload", {{0.5, "bolt_pull"}, {1.25, "magazine_in"}}}}};
}

// Every sound is one second long; an unknown id has no length.
std::optional<double> OneSecond(std::string_view id) {
    if (id.rfind("game.audio.", 0) != 0) return std::nullopt;
    return 1.0;
}

// The layout the writer gives, written by hand: it reads back byte for byte.
constexpr const char* kHandWritten = R"({
  "schema": "gyo.event_sounds",
  "version": 1,
  "animation_events_asset_id": "game.anim_events.rifle",
  "events": {
    "bolt_pull": { "sound_asset_id": "game.audio.bolt", "sync_seconds": 0.1, "gain": 0.8 },
    "magazine_in": { "sound_asset_id": "game.audio.magazine", "sync_seconds": 0.33, "gain": 1.25 }
  }
}
)";

std::filesystem::path TemporaryDirectory(const std::string& name) {
    static const auto run = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / ("gyo_model_viewer_sounds_" + run + "_" + name);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteFile(const std::filesystem::path& path, const std::string& text) { std::ofstream(path, std::ios::binary) << text; }

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

class Recorder final : public SoundSink {
public:
    void Play(const ScheduledSound& sound, const Engine::Time::TimePoint at) override { played.push_back({sound, at}); }
    void StopAll() override {
        ++stops;
        played.clear();
    }
    std::vector<std::pair<ScheduledSound, Engine::Time::TimePoint>> played;
    int stops{};
};

} // namespace

TEST_CASE("S1: an opened file saved unchanged is byte for byte the same; the gain reads back as the same float") {
    const auto root = TemporaryDirectory("roundtrip");
    const auto file = root / "rifle.event_sounds.json";
    WriteFile(file, kHandWritten);
    auto opened = OpenEventSounds(Utf8(file), kEventsId, Events(), OneSecond);
    REQUIRE(opened);
    const auto text = CheckedEventSoundsText(opened->set, Events(), OneSecond);
    REQUIRE(text);
    CHECK_MESSAGE(*text == kHandWritten, "the written text is not the file's own");

    // A gain typed as a double is held as the float the format has.
    auto edited = opened->set;
    edited.sounds[0].gain = static_cast<float>(0.7);
    const auto saved = SaveEventSounds(Utf8(file), opened->stamp, edited, Events(), OneSecond);
    REQUIRE(saved);
    auto again = OpenEventSounds(Utf8(file), kEventsId, Events(), OneSecond);
    REQUIRE(again);
    CHECK(std::bit_cast<std::uint32_t>(again->set.sounds[0].gain) == std::bit_cast<std::uint32_t>(static_cast<float>(0.7)));
    CHECK(SameEventSounds(again->set, edited));
}

TEST_CASE("S1: the framework decides what is refused, and a refused or changed file is not written") {
    const auto root = TemporaryDirectory("refused");
    const auto file = root / "rifle.event_sounds.json";
    WriteFile(file, kHandWritten);
    auto opened = OpenEventSounds(Utf8(file), kEventsId, Events(), OneSecond);
    REQUIRE(opened);

    auto late = opened->set;
    late.sounds[1].syncSeconds = 1.5; // after the end of its one-second sound
    const auto refused = SaveEventSounds(Utf8(file), opened->stamp, late, Events(), OneSecond);
    REQUIRE_MESSAGE(!refused, "a sync point after its sound's end was saved");
    CHECK(refused.error().code == ModelViewerErrorCode::InvalidEventSounds);
    auto unknownEvent = opened->set;
    unknownEvent.sounds[0].event = "not_an_event";
    CHECK_FALSE(CheckedEventSoundsText(unknownEvent, Events(), OneSecond));
    auto loud = opened->set;
    loud.sounds[0].gain = 9.0F;
    CHECK_FALSE(CheckedEventSoundsText(loud, Events(), OneSecond));
    CHECK(ReadFile(file) == kHandWritten);

    WriteFile(file, std::string(kHandWritten) + " ");
    const auto changed = SaveEventSounds(Utf8(file), opened->stamp, opened->set, Events(), OneSecond);
    REQUIRE_FALSE(changed);
    CHECK(changed.error().code == ModelViewerErrorCode::ChangedOnDisk);
}

TEST_CASE("S2: a sound starts at EventSoundOffsetSeconds, bit for bit, at speed 1, 0.5 and 2") {
    const auto set = Framework::ParseEventSounds(kHandWritten);
    REQUIRE(set);
    const auto events = Events();
    const auto bound = Framework::BindEventSounds(set.value(), events, OneSecond);
    REQUIRE(bound);
    const auto& track = events.tracks[0];
    constexpr double clip = 2.5;
    for (const double speed : {1.0, 0.5, 2.0}) {
        CAPTURE(speed);
        const auto schedule = ScheduleSounds(bound.value(), track, clip, speed);
        REQUIRE(schedule.size() == 2);
        Recorder recorder;
        const auto start = Engine::Time::TimePoint{} + std::chrono::seconds(100);
        CHECK(PlayClipSounds(recorder, schedule, start, Engine::Time::TimePoint{}) == 2);
        REQUIRE(recorder.played.size() == 2);
        for (std::size_t index = 0; index < 2; ++index) {
            const auto& event = track.events[index];
            const auto* sound = bound->Find(event.name);
            const auto expected = Framework::EventSoundOffsetSeconds(event.seconds, clip, clip / speed, sound->syncSeconds);
            REQUIRE(expected);
            const auto& [scheduled, at] = recorder.played[index];
            CHECK_MESSAGE(std::bit_cast<std::uint64_t>(scheduled.offsetSeconds) == std::bit_cast<std::uint64_t>(*expected),
                          "the sound's start is not the framework's");
            CHECK(at == start + std::chrono::duration_cast<Engine::Time::Duration>(std::chrono::duration<double>(*expected)));
            CHECK(scheduled.soundAssetId == sound->soundAssetId);
            CHECK(std::bit_cast<std::uint32_t>(scheduled.gain) == std::bit_cast<std::uint32_t>(sound->gain));
        }
    }
    // A sound that would start before now is not sent late.
    const auto schedule = ScheduleSounds(bound.value(), track, clip, 1.0);
    Recorder recorder;
    const auto start = Engine::Time::TimePoint{} + std::chrono::seconds(100);
    CHECK(PlayClipSounds(recorder, schedule, start, start + std::chrono::milliseconds(500)) == 1); // 0.4 s is past, 0.92 s is not
    CHECK(recorder.played.at(0).first.event == 1);
}

TEST_CASE("S3: a file of another events file is not shown, and says which it names") {
    const auto root = TemporaryDirectory("other");
    const auto file = root / "other.event_sounds.json";
    std::string text = kHandWritten;
    text.replace(text.find(kEventsId), std::string(kEventsId).size(), "game.anim_events.pistol");
    WriteFile(file, text);
    const auto opened = OpenEventSounds(Utf8(file), kEventsId, Events(), OneSecond);
    REQUIRE_MESSAGE(!opened, "a file of another events file was opened");
    CHECK(opened.error().code == ModelViewerErrorCode::OtherEvents);
    CHECK(opened.error().detail.find("game.anim_events.pistol") != std::string::npos);
    CHECK(opened.error().detail.find(kEventsId) != std::string::npos);
}

TEST_CASE("S1: a new event sounds file is written and registered only when the framework accepts it") {
    const auto root = TemporaryDirectory("create");
    WriteFile(root / "asset_catalog.json", "{ \"version\": 1, \"assets\": [] }\n");
    NewEventsRequest request{Utf8(root), Utf8(root / "asset_catalog.json"), "game.event_sounds.rifle", "rifle.event_sounds.json"};
    Framework::EventSoundSet set{kEventsId, {{"bolt_pull", "game.audio.bolt", 0.1, 0.8F}}};

    auto other = set;
    other.animationEventsAssetId = "game.anim_events.pistol";
    CHECK(CreateEventSounds(request, kEventsId, other, Events(), OneSecond).error().code == ModelViewerErrorCode::OtherEvents);
    auto refused = set;
    refused.sounds[0].soundAssetId = "unknown";
    CHECK(CreateEventSounds(request, kEventsId, refused, Events(), OneSecond).error().code ==
          ModelViewerErrorCode::InvalidEventSounds);
    CHECK_FALSE(std::filesystem::exists(root / "rifle.event_sounds.json"));
    CHECK(ReadFile(root / "asset_catalog.json") == "{ \"version\": 1, \"assets\": [] }\n");

    const auto created = CreateEventSounds(request, kEventsId, set, Events(), OneSecond);
    REQUIRE(created);
    auto opened = OpenEventSounds(created->outputPath, kEventsId, Events(), OneSecond);
    REQUIRE(opened);
    CHECK(SameEventSounds(opened->set, set));
    CHECK(ReadFile(root / "asset_catalog.json").find("game.event_sounds.rifle") != std::string::npos);
}

namespace {

// A clip of 2 s with one sound at 1.0 s; lead > 0 makes it start before its event.
std::vector<ScheduledSound> OneSound(const double offset) { return {{0, "game.audio.bolt", 1.0F, offset}}; }

Engine::Time::TimePoint At(const double seconds) {
    return Engine::Time::TimePoint{} + std::chrono::duration_cast<Engine::Time::Duration>(std::chrono::duration<double>(seconds));
}

double SecondsOf(const Engine::Time::TimePoint at) {
    return std::chrono::duration<double>(at.time_since_epoch()).count();
}

} // namespace

TEST_CASE("S2: a looping clip sends each round once, a round ahead, from the round's start at its speed") {
    Recorder sink;
    SoundPlayback playback;
    const auto schedule = OneSound(0.5); // 0.5 s into the round
    // Playing at speed 2 from clip second 0.5: the round started 0.25 s ago and lasts 1 s.
    SoundClock clock{true, true, 2.0, 0.5, 2.0, false};
    playback.Update(sink, schedule, 1, clock, At(100.0), {});
    REQUIRE(sink.played.size() == 2);
    CHECK_MESSAGE(SecondsOf(sink.played[0].second) == doctest::Approx(100.25), "the round's start is not now less seconds over speed");
    CHECK(SecondsOf(sink.played[1].second) == doctest::Approx(101.25));
    // The next frames on course send nothing; the wrap sends the round after the next.
    clock.seconds = 1.0;
    playback.Update(sink, schedule, 1, clock, At(100.25), {});
    CHECK(sink.played.size() == 2);
    clock.seconds = 0.1;
    clock.wrapped = true;
    playback.Update(sink, schedule, 1, clock, At(100.8), {});
    REQUIRE(sink.played.size() == 3);
    CHECK(SecondsOf(sink.played[2].second) == doctest::Approx(102.25));
    CHECK(sink.stops == 0);
}

TEST_CASE("S2: an edit, a seek, a speed or loop change stops what was sent and starts over; a pause stops") {
    Recorder sink;
    SoundPlayback playback;
    const auto schedule = OneSound(0.5);
    SoundClock clock{true, true, 1.0, 0.0, 2.0, false};
    playback.Update(sink, schedule, 1, clock, At(100.0), {});
    REQUIRE(sink.played.size() == 2);
    // A new schedule version (an edit): stopped once, then sent again, not on top.
    playback.Update(sink, schedule, 2, clock, At(100.0), {});
    CHECK_MESSAGE(sink.stops == 1, "an edit while playing did not stop the sounds sent before it");
    CHECK(sink.played.size() == 2);
    // A seek.
    clock.seconds = 1.5;
    playback.Update(sink, schedule, 2, clock, At(100.1), {});
    CHECK(sink.stops == 2);
    // Loop off: no round ahead.
    clock.loop = false;
    playback.Update(sink, schedule, 2, clock, At(100.1), {});
    CHECK(sink.stops == 3);
    CHECK(sink.played.empty()); // 0.5 s into a round that started 1.5 s ago lies behind
    // Paused: stopped.
    clock.loop = true;
    playback.Update(sink, schedule, 2, clock, At(100.2), {});
    clock.playing = false;
    playback.Update(sink, schedule, 2, clock, At(100.3), {});
    CHECK(sink.stops == 5);
    // A clamped clip that reached its end lets its sounds ring out.
    clock = {true, false, 1.0, 0.0, 2.0, false};
    playback.Update(sink, schedule, 2, clock, At(200.0), {});
    const int before = sink.stops;
    clock = {false, false, 1.0, 2.0, 2.0, false};
    playback.Update(sink, schedule, 2, clock, At(202.0), {});
    CHECK(sink.stops == before);
}

TEST_CASE("S2: a sound that leads by more than a round, or is late by less than the mixer's delay, still sounds") {
    Recorder sink;
    SoundPlayback playback;
    // Starts 2.5 s before its round's start: rounds of 1 s need three rounds ahead.
    const auto schedule = OneSound(-2.5);
    SoundClock clock{true, true, 1.0, 0.0, 1.0, false};
    playback.Update(sink, schedule, 1, clock, At(100.0), {});
    // Round 4 (starting at 104) has its sound at 101.5, the first one not yet past.
    const bool sent = std::any_of(sink.played.begin(), sink.played.end(),
                                  [](const auto& play) { return SecondsOf(play.second) == doctest::Approx(101.5); });
    CHECK_MESSAGE(sent, "a long lead was not sent rounds ahead");
    // 30 ms late with a 60 ms delay: sent.
    Recorder late;
    SoundPlayback again;
    SoundClock now{true, false, 1.0, 0.53, 2.0, false};
    again.Update(late, OneSound(0.5), 1, now, At(100.53), std::chrono::milliseconds(60));
    CHECK(late.played.size() == 1);
}
