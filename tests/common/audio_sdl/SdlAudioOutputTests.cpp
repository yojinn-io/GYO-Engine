#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"

using namespace Engine::Audio;
using Engine::Audio::Sdl::SdlAudioOutput;

namespace {

constexpr std::int16_t kImpulse = 1000;

struct MixerAndClip final {
    Mixer mixer;
    ClipId impulse;
};

MixerAndClip MakeMixer() {
    ClipBank bank;
    const std::vector<std::int16_t> one{kImpulse};
    auto clip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(one.data(), one.size()), 1, kSampleRate);
    REQUIRE(clip.has_value());
    auto mixer = Mixer::Create(MixerConfig{}, std::move(bank));
    REQUIRE(mixer.has_value());
    return MixerAndClip{std::move(mixer).value(), *clip};
}

SdlAudioOutput OpenOutput(Mixer&& mixer, const Engine::Audio::Sdl::SdlAudioOutputConfig& config = {}) {
    auto output = SdlAudioOutput::Open(std::move(mixer), config);
    REQUIRE_MESSAGE(output.has_value(), (output ? std::string() : Describe(output.error())));
    return std::move(output).value();
}

// Waits until the condition holds. The limit is a hang guard, not a timing
// requirement: the drivers render far faster than it (structure only).
bool WaitFor(const std::function<bool()>& condition) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!condition()) {
        if (std::chrono::steady_clock::now() > limit) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

bool Advances(SdlAudioOutput& output) {
    const std::uint64_t start = output.OwnedMixer().Stats().renderedFrames;
    return WaitFor([&] { return output.OwnedMixer().Stats().renderedFrames > start + 4800; });
}

} // namespace

TEST_CASE("Audio SDL output on the dummy driver renders the mixer from SDL's callback") {
    auto [mixer, clip] = MakeMixer();
    SdlAudioOutput output = OpenOutput(std::move(mixer));
    CHECK(output.DriverName() == "dummy");
    CHECK(output.DeviceSampleFrames() > 0);
    CHECK(Advances(output));
    const auto stats = output.Stats();
    CHECK(stats.callbacks > 0);
    CHECK(stats.frames >= output.OwnedMixer().Stats().renderedFrames - stats.maxCallbackFrames);
    CHECK(stats.minCallbackFrames > 0);
    CHECK(stats.maxCallbackFrames >= stats.minCallbackFrames);
}

TEST_CASE("Audio SDL output on the dummy driver logs each callback's anchor in order") {
    auto [mixer, clip] = MakeMixer();
    Engine::Audio::Sdl::SdlAudioOutputConfig config;
    config.anchorLogCapacity = 4096;
    SdlAudioOutput output = OpenOutput(std::move(mixer), config);
    REQUIRE(Advances(output));
    std::vector<Engine::Audio::Sdl::OutputAnchor> anchors;
    const std::size_t first = output.TakeAnchors(anchors);
    REQUIRE(first >= 2);
    CHECK(anchors.size() == first);
    CHECK(anchors[0].firstFrame == 0);
    for (std::size_t i = 1; i < anchors.size(); ++i) {
        CHECK(anchors[i].anchor >= anchors[i - 1].anchor);
        CHECK(anchors[i].firstFrame - anchors[i - 1].firstFrame >= output.Stats().minCallbackFrames);
    }
    REQUIRE(Advances(output));
    const std::size_t second = output.TakeAnchors(anchors);
    REQUIRE(second >= 1);
    CHECK(anchors[first].firstFrame > anchors[first - 1].firstFrame); // no anchor is handed out twice
    CHECK(output.Stats().anchorsDropped == 0);
}

TEST_CASE("Audio SDL output on the dummy driver drops and counts anchors when its log is full") {
    auto [mixer, clip] = MakeMixer();
    Engine::Audio::Sdl::SdlAudioOutputConfig config;
    config.anchorLogCapacity = 2;
    SdlAudioOutput output = OpenOutput(std::move(mixer), config);
    REQUIRE(Advances(output));
    CHECK(output.Stats().anchorsDropped > 0);
    std::vector<Engine::Audio::Sdl::OutputAnchor> anchors;
    CHECK(output.TakeAnchors(anchors) == 2);
    CHECK(anchors[0].firstFrame == 0);

    auto other = MakeMixer();
    SdlAudioOutput silent = OpenOutput(std::move(other.mixer)); // no log by default
    REQUIRE(Advances(silent));
    CHECK(silent.TakeAnchors(anchors) == 0);
}

TEST_CASE("Audio SDL outputs on the dummy driver share SDL's audio subsystem by reference count") {
    auto first = MakeMixer();
    auto second = MakeMixer();
    SdlAudioOutput a = OpenOutput(std::move(first.mixer));
    {
        SdlAudioOutput b = OpenOutput(std::move(second.mixer));
        CHECK(Advances(b));
    }
    // Closing one output must not stop audio for the other.
    CHECK(Advances(a));
    {
        SdlAudioOutput moved = std::move(a);
        CHECK(Advances(moved));
    }
    // After every output closed, audio starts again.
    auto third = MakeMixer();
    SdlAudioOutput c = OpenOutput(std::move(third.mixer));
    CHECK(Advances(c));
    GYO_CHECK_ASSERTS(a.OwnedMixer());
}

TEST_CASE("Audio SDL output on the disk driver writes the mixer's samples where they were scheduled") {
    const char* path = std::getenv("SDL_AUDIO_DISK_OUTPUT_FILE");
    REQUIRE(path != nullptr);
    std::vector<Engine::Audio::Sdl::OutputAnchor> anchors;
    {
        auto [mixer, clip] = MakeMixer();
        auto submitter = mixer.CreateSubmitter();
        REQUIRE(submitter.has_value());
        Engine::Audio::Sdl::SdlAudioOutputConfig config;
        config.anchorLogCapacity = 4096;
        SdlAudioOutput output = OpenOutput(std::move(mixer), config);
        CHECK(output.DriverName() == "disk");
        CHECK(output.DeviceSampleFrames() == 480);
        // A time already passed: the impulse starts at once, counted late.
        REQUIRE(submitter->Play({clip, Engine::Time::Now() - std::chrono::seconds(1), 1.0f, 0.5f}));
        REQUIRE(WaitFor([&] { return output.OwnedMixer().Stats().renderedFrames > 48000; }));
        const auto stats = output.OwnedMixer().Stats();
        CHECK(stats.submitted == 1);
        CHECK(stats.playedLate == 1);
        CHECK(stats.submitted == stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
        // A stalled test thread, as on a busy CI machine, must not fill the anchor log
        // before it drains it: the driver's pace (timescale 0.1) bounds the callbacks.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        static_cast<void>(output.TakeAnchors(anchors));
        CHECK(output.Stats().anchorsDropped == 0);
    }
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    REQUIRE(bytes.size() % 4 == 0);
    REQUIRE(bytes.size() > 4 * 4800);
    std::vector<std::size_t> sounding;
    for (std::size_t frame = 0; frame * 4 < bytes.size(); ++frame) {
        const auto sample = [&](const std::size_t offset) {
            const auto lo = static_cast<std::uint8_t>(bytes[frame * 4 + offset]);
            const auto hi = static_cast<std::uint8_t>(bytes[frame * 4 + offset + 1]);
            return static_cast<std::int16_t>(lo | (hi << 8)); // S16 little-endian on every CI host
        };
        if (sample(0) != 0 || sample(2) != 0) {
            CHECK(sample(0) == kImpulse);
            CHECK(sample(2) == kImpulse / 2);
            sounding.push_back(frame);
        }
    }
    REQUIRE(sounding.size() == 1);
    // The file's frames are the mixer's frames: a late request starts at the
    // first frame of the block that took it, which is a logged anchor's frame.
    bool atAnchor = false;
    for (const auto& anchor : anchors) atAnchor = atAnchor || anchor.firstFrame == sounding[0];
    CHECK(atAnchor);
}

TEST_CASE("Audio SDL output on the dummy driver stops, swaps and reports sounds while it runs") {
    MixerConfig config;
    config.clipSlotsPerSubmitter = 1;
    auto created = Mixer::Create(config, ClipBank{});
    REQUIRE(created.has_value());
    auto submitter = created->CreateSubmitter();
    REQUIRE(submitter.has_value());
    SdlAudioOutput output = OpenOutput(std::move(created).value());
    Mixer& mixer = output.OwnedMixer();
    const std::vector<std::int16_t> longClip(480000, 300); // ten seconds
    const auto load = [&](const std::vector<std::int16_t>& samples) {
        return submitter->LoadClip(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 1,
                                   kSampleRate);
    };
    auto first = load(longClip);
    REQUIRE(first.has_value());
    const SoundHandle sound = submitter->Play({*first, Engine::Time::Now()});
    REQUIRE(sound);
    CHECK(WaitFor([&] { return mixer.PlayheadOf(sound).status == Playhead::Status::Playing; }));
    REQUIRE(submitter->Stop(sound, Engine::Time::Now()));
    CHECK(WaitFor([&] { return mixer.PlayheadOf(sound).status == Playhead::Status::NotPlaying; }));
    CHECK(mixer.Stats().stopsMatched == 1);
    REQUIRE(submitter->UnloadClip(*first));
    // The single slot comes back once the audio thread has taken the unload.
    Engine::Base::Result<ClipId, AudioError> second = load(longClip);
    CHECK(WaitFor([&] {
        if (!second) second = load(longClip);
        return second.has_value();
    }));
    REQUIRE(second.has_value());
    const SoundHandle next = submitter->Play({*second, Engine::Time::Now()});
    CHECK(WaitFor([&] { return mixer.PlayheadOf(next).status == Playhead::Status::Playing; }));
    const MixerStats stats = mixer.Stats();
    CHECK(stats.unloadsTaken == 1);
    CHECK(stats.stopsSubmitted == stats.stopsMatched + stats.stopsUnmatched + stats.stopsDroppedQueueFull);
}

namespace {

// One play and stop on the disk driver, with what the judgment needs.
// The driver's file is not the mixer's frame count: while the device stream is
// paused, between SDL opening it and SdlAudioOutput::Open resuming it, SDL's
// device thread writes whole blocks of silence to the file when it runs first.
// A marker played from the mixer's frame 0 finds that offset.
struct DiskStopRun final {
    std::vector<Engine::Audio::Sdl::OutputAnchor> anchors;
    Engine::Time::TimePoint stopAt{};
    Engine::Time::TimePoint sentAt{}; // read before Stop is called
    Engine::Time::TimePoint doneAt{}; // read after Stop returned: the stop is queued
    std::uint64_t stopsLate = 0;
    std::vector<char> bytes; // the file the driver wrote
};

constexpr std::int16_t kDiskLevel = 1000;
constexpr std::int16_t kDiskMarker = 500;
constexpr std::size_t kDiskMarkerFrames = 48;

// pause: how long the test thread waits between reading sentAt and calling
// Stop, standing in for a runner that deschedules it there.
DiskStopRun PlayAndStopOnDisk(const std::chrono::microseconds pause) {
    const char* path = std::getenv("SDL_AUDIO_DISK_OUTPUT_FILE");
    REQUIRE(path != nullptr);
    DiskStopRun run;
    {
        MixerConfig config;
        config.clipSlotsPerSubmitter = 1;
        config.scheduleDelay = Engine::Time::Duration::zero();
        ClipBank bank;
        const std::vector<std::int16_t> marker(kDiskMarkerFrames, kDiskMarker);
        auto markerClip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(marker.data(), marker.size()), 1, kSampleRate);
        REQUIRE(markerClip.has_value());
        auto created = Mixer::Create(config, std::move(bank));
        REQUIRE(created.has_value());
        auto submitter = created->CreateSubmitter();
        REQUIRE(submitter.has_value());
        // Queued before the output opens, so the first block takes it late and
        // it starts at the mixer's frame 0.
        REQUIRE(submitter->Play({*markerClip, Engine::Time::Now()}));
        Engine::Audio::Sdl::SdlAudioOutputConfig outputConfig;
        outputConfig.anchorLogCapacity = 4096;
        SdlAudioOutput output = OpenOutput(std::move(created).value(), outputConfig);
        Mixer& mixer = output.OwnedMixer();
        REQUIRE(WaitFor([&] { return mixer.Stats().renderedFrames > 4800; })); // output runs
        const std::vector<std::int16_t> clip(480000, kDiskLevel);
        auto loaded =
            submitter->LoadClip(Engine::Base::ConstSpan<std::int16_t>(clip.data(), clip.size()), 1, kSampleRate);
        REQUIRE(loaded.has_value());
        const SoundHandle sound = submitter->Play({*loaded, Engine::Time::Now()});
        REQUIRE(sound);
        REQUIRE(WaitFor([&] { return mixer.PlayheadOf(sound).status == Playhead::Status::Playing; }));
        run.stopAt = Engine::Time::Now() + std::chrono::milliseconds(2);
        run.sentAt = Engine::Time::Now();
        if (pause.count() > 0) std::this_thread::sleep_for(pause);
        REQUIRE(submitter->Stop(sound, run.stopAt));
        run.doneAt = Engine::Time::Now();
        REQUIRE(WaitFor([&] { return mixer.PlayheadOf(sound).status == Playhead::Status::NotPlaying; }));
        CHECK(mixer.Stats().stopsMatched == 1);
        run.stopsLate = mixer.Stats().stopsLate;
        static_cast<void>(output.TakeAnchors(run.anchors));
        CHECK(output.Stats().anchorsDropped == 0);
    }
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.good());
    run.bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return run;
}

// The clip sounds in one contiguous run that ends at the stop frame of the
// block that took the stop (audio.md Invariants 3 and 10). That block is the
// first whose callback drained the queue after the stop was queued: at the
// earliest the last one whose anchor was read before sentAt (a callback in
// progress), at the latest the first one whose anchor was read after doneAt.
// Blocks in between may have started before the stop was queued, so any of
// them may be the one. Its stop frame is the stop time mapped from its anchor,
// or its first frame when that is already rendered, counted late. Each block
// here is one pass (480 frames, under the backend's 4096 per pass).
void CheckDiskStopFrame(const DiskStopRun& run) {
    std::vector<std::uint64_t> marked;
    std::vector<std::uint64_t> sounding;
    for (std::size_t frame = 0; frame * 4 < run.bytes.size(); ++frame) {
        const auto lo = static_cast<std::uint8_t>(run.bytes[frame * 4]);
        const auto hi = static_cast<std::uint8_t>(run.bytes[frame * 4 + 1]);
        const auto sample = static_cast<std::int16_t>(lo | (hi << 8));
        if (sample == kDiskMarker) marked.push_back(frame);
        if (sample == kDiskLevel) sounding.push_back(frame);
    }
    // The marker: one contiguous run of its length; its first frame in the file
    // is the mixer's frame 0.
    REQUIRE(marked.size() == kDiskMarkerFrames);
    REQUIRE(marked.back() - marked.front() + 1 == marked.size());
    const auto silence = static_cast<std::int64_t>(marked.front());
    CAPTURE(silence);
    REQUIRE_FALSE(sounding.empty());
    CHECK(sounding.back() - sounding.front() + 1 == sounding.size()); // one contiguous run
    // The stop frame from a block's anchor (S16 little-endian above).
    const auto mapped = [&](const Engine::Audio::Sdl::OutputAnchor& anchor) {
        const std::int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(run.stopAt - anchor.anchor).count();
        const std::int64_t scaled = ns * 6 + 62500; // floor(ns * 48000 / 10^9 + 1/2)
        const std::int64_t offset = scaled / 125000 - ((scaled % 125000 != 0 && scaled < 0) ? 1 : 0);
        return static_cast<std::int64_t>(anchor.firstFrame) + offset;
    };
    REQUIRE_FALSE(run.anchors.empty());
    std::size_t first = 0; // the last anchor read before sentAt, or the first one
    while (first + 1 < run.anchors.size() && run.anchors[first + 1].anchor <= run.sentAt) ++first;
    std::size_t last = first; // the first anchor read after doneAt, or the last one
    while (last + 1 < run.anchors.size() && run.anchors[last].anchor <= run.doneAt) ++last;
    const auto end = static_cast<std::int64_t>(sounding.back()) + 1 - silence; // in the mixer's frames
    bool found = false;
    std::string blocks;
    const auto us = [&](const Engine::Time::TimePoint at) {
        return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(at - run.sentAt).count());
    };
    for (std::size_t i = first; i <= last; ++i) {
        const auto start = static_cast<std::int64_t>(run.anchors[i].firstFrame);
        const std::int64_t at = mapped(run.anchors[i]);
        const bool late = at < start;
        if ((late ? start : at) == end && late == (run.stopsLate == 1)) found = true;
        blocks += "\n  block " + std::to_string(i) + ": anchor " + us(run.anchors[i].anchor) + " us after sentAt, first " +
                  std::to_string(start) + ", mapped " + std::to_string(at) + (late ? " (late)" : "");
    }
    INFO("stop ends at frame " << end << ", stopsLate " << run.stopsLate << ", stopAt " << us(run.stopAt)
                               << " us and doneAt " << us(run.doneAt) << " us after sentAt; candidate blocks:" << blocks);
    CHECK(found);
}

} // namespace

TEST_CASE("Audio SDL output on the disk driver plays a clip loaded after opening and stops it at its mapped frame") {
    CheckDiskStopFrame(PlayAndStopOnDisk(std::chrono::microseconds::zero()));
}

TEST_CASE("Audio SDL output on the disk driver stops at the frame of the block that takes a stop queued blocks later") {
    // The test thread waits after sentAt, so blocks start before the stop is
    // queued (about one per millisecond at this timescale); after 3 ms the
    // block that takes it is past the stop time, so the stop is late.
    for (const auto pause : {std::chrono::microseconds(1000), std::chrono::microseconds(3000)}) {
        CAPTURE(pause.count());
        CheckDiskStopFrame(PlayAndStopOnDisk(pause));
    }
}

TEST_CASE("Audio SDL output reports an unavailable driver and leaves submitters usable") {
    auto [mixer, clip] = MakeMixer();
    auto submitter = mixer.CreateSubmitter();
    REQUIRE(submitter.has_value());
    auto output = SdlAudioOutput::Open(std::move(mixer));
    REQUIRE_FALSE(output.has_value());
    CHECK(output.error().code == AudioErrorCode::DeviceUnavailable);
    CHECK_FALSE(output.error().detail.empty());
    CHECK(submitter->Play({clip, Engine::Time::Now()}));
}
