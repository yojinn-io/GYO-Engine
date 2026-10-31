#include "doctest/doctest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Mixer.hpp"

using namespace Engine::Audio;
using namespace std::chrono_literals;
using Engine::Time::Duration;
using Engine::Time::TimePoint;

namespace {

constexpr std::int16_t kLevel = 1000;
const TimePoint kAnchor = TimePoint{} + std::chrono::duration_cast<Duration>(10s);

Duration Ns(const std::int64_t ns) { return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(ns)); }
Duration Ms(const std::int64_t ms) { return Ns(ms * 1'000'000); }

ClipId AddConstant(ClipBank& bank, const std::uint32_t frames, const std::int16_t level = kLevel) {
    const std::vector<std::int16_t> samples(frames, level);
    auto clip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 1, kSampleRate);
    REQUIRE(clip.has_value());
    return *clip;
}

struct Rig final {
    Mixer mixer;
    Submitter submitter;
    ClipId clip;  // 4800 frames of kLevel
    ClipId burst; // 10 frames of kLevel
};

Rig MakeRig(const Duration delay = Duration::zero(), const std::uint32_t voices = 8,
            const std::uint32_t capacity = 64) {
    ClipBank bank;
    const ClipId clip = AddConstant(bank, 4800);
    const ClipId burst = AddConstant(bank, 10);
    MixerConfig config;
    config.scheduleDelay = delay;
    config.maxVoices = voices;
    config.queueCapacity = capacity;
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    return Rig{std::move(mixer).value(), std::move(submitter).value(), clip, burst};
}

std::vector<std::int16_t> RenderBlock(Mixer& mixer, const std::uint32_t frames, const TimePoint anchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * kChannels, 0x5555);
    mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    return out;
}

// Frames (counted from firstFrame) whose left sample is not silent.
std::vector<std::int64_t> Sounding(const std::vector<std::int16_t>& out, const std::int64_t firstFrame = 0) {
    std::vector<std::int64_t> frames;
    for (std::size_t f = 0; f * 2 < out.size(); ++f) {
        if (out[f * 2] != 0) frames.push_back(firstFrame + static_cast<std::int64_t>(f));
    }
    return frames;
}

std::vector<std::int16_t> RenderInBlocks(Mixer& mixer, const std::uint32_t total, const std::uint32_t block) {
    std::vector<std::int16_t> all;
    for (std::uint32_t done = 0; done < total; done += block) {
        const auto out = RenderBlock(mixer, std::min(block, total - done), kAnchor);
        all.insert(all.end(), out.begin(), out.end());
    }
    return all;
}

void CheckStopBalance(const MixerStats& stats) {
    CHECK(stats.stopsSubmitted == stats.stopsMatched + stats.stopsUnmatched + stats.stopsDroppedQueueFull);
    CHECK(stats.submitted == stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
}

} // namespace

TEST_CASE("Audio Mixer stops a sound at the frame its time maps to") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    REQUIRE(sound);
    REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(10))); // frame 480
    const auto frames = Sounding(RenderBlock(rig.mixer, 2048, kAnchor));
    REQUIRE_FALSE(frames.empty());
    CHECK(frames.front() == 0);
    CHECK(frames.back() == 479);
    CHECK(frames.size() == 480);
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.stopsMatched == 1);
    CHECK(stats.stopsLate == 0);
    CheckStopBalance(stats);

    // The stopped voice is free again, and a second stop finds nothing.
    Rig one = MakeRig(Duration::zero(), 1);
    const SoundHandle first = one.submitter.Play({one.clip, kAnchor});
    REQUIRE(one.submitter.Stop(first, kAnchor + Ms(10)));
    static_cast<void>(RenderBlock(one.mixer, 960, kAnchor));
    REQUIRE(one.submitter.Play({one.burst, kAnchor + Ms(20)}));
    REQUIRE(one.submitter.Stop(first, kAnchor + Ms(20)));
    CHECK(Sounding(RenderBlock(one.mixer, 960, kAnchor + Ms(20)), 960).size() == 10);
    CHECK(one.mixer.Stats().droppedNoVoice == 0);
    CHECK(one.mixer.Stats().stopsUnmatched == 1);
}

TEST_CASE("Audio Mixer stops a voice exactly at its stop frame whatever the block split") {
    // Every command is queued before the first render and on time, so each
    // voice's start and stop frames are fixed (Design Invariant 8 (a)).
    const auto render = [](const std::uint32_t block) {
        Rig rig = MakeRig();
        const SoundHandle a = rig.submitter.Play({rig.clip, kAnchor + Ns(1'234'567)});
        const SoundHandle b = rig.submitter.Play({rig.clip, kAnchor + Ms(20), 0.5f, 0.25f});
        REQUIRE((a && b));
        REQUIRE(rig.submitter.Stop(a, kAnchor + Ns(7'777'777)));
        REQUIRE(rig.submitter.Stop(b, kAnchor + Ms(30)));
        return RenderInBlocks(rig.mixer, 4400, block);
    };
    const auto reference = render(4400);
    const auto frames = Sounding(reference);
    REQUIRE_FALSE(frames.empty());
    CHECK(frames.front() == 59);  // 1.234567 ms
    CHECK(frames.back() == 1439); // the stop of b at 30 ms is frame 1440
    for (const std::uint32_t block : {1u, 7u, 255u, 256u, 257u, 441u, 2200u}) {
        CAPTURE(block);
        CHECK(render(block) == reference);
    }
}

TEST_CASE("Audio Mixer silences a sound stopped before its start and frees its voice") {
    Rig rig = MakeRig(Duration::zero(), 1);
    const SoundHandle early = rig.submitter.Play({rig.clip, kAnchor + Ms(20)});
    const SoundHandle same = rig.submitter.Play({rig.clip, kAnchor + Ms(20)}); // no voice left
    REQUIRE((early && same));
    REQUIRE(rig.submitter.Stop(early, kAnchor + Ms(10)));
    const SoundHandle next = rig.submitter.Play({rig.burst, kAnchor + Ms(40)});
    REQUIRE(next);
    const auto frames = Sounding(RenderBlock(rig.mixer, 4800, kAnchor));
    // Only the burst sounds: the stopped voice was freed for it.
    REQUIRE(frames.size() == 10);
    CHECK(frames.front() == 1920);
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.droppedNoVoice == 1);
    CHECK(stats.stopsMatched == 1);
    CheckStopBalance(stats);

    // A stop exactly at the start frame is before it too.
    Rig at = MakeRig(Duration::zero(), 1);
    const SoundHandle sound = at.submitter.Play({at.clip, kAnchor + Ms(20)});
    REQUIRE(at.submitter.Stop(sound, kAnchor + Ms(20)));
    REQUIRE(at.submitter.Play({at.burst, kAnchor}));
    CHECK(Sounding(RenderBlock(at.mixer, 4800, kAnchor)).size() == 10);
}

TEST_CASE("Audio Mixer applies a late stop of a started sound at the block's first frame") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    REQUIRE(sound);
    std::vector<std::int64_t> heard;
    for (int block = 0; block < 3; ++block) {
        const auto frames = Sounding(RenderBlock(rig.mixer, 480, kAnchor + Ms(10 * block)), 480 * block);
        heard.insert(heard.end(), frames.begin(), frames.end());
    }
    CHECK(heard.size() == 1440); // the frames before the stop stay
    // Stopped at a time before the sound even started: it has started, so the
    // stop is late and applies at frame 1440, the first frame of this block.
    REQUIRE(rig.submitter.Stop(sound, kAnchor - Ms(1)));
    CHECK(Sounding(RenderBlock(rig.mixer, 480, kAnchor + Ms(30))).empty());
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.stopsMatched == 1);
    CHECK(stats.stopsLate == 1);
    CheckStopBalance(stats);
}

TEST_CASE("Audio Mixer keeps the earliest of several stops") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    REQUIRE(sound);
    REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(10)));
    REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(20)));
    const auto frames = Sounding(RenderBlock(rig.mixer, 2048, kAnchor));
    REQUIRE_FALSE(frames.empty());
    CHECK(frames.back() == 479);
    CHECK(rig.mixer.Stats().stopsMatched == 2);
}

TEST_CASE("Audio Mixer treats a late play clamped to the block start as not started") {
    // A play late enough to start at the first frame of the block that takes it
    // has not started yet when a stop in the same block names an earlier frame:
    // it is silenced and frees its voice, and the stop is not late.
    Rig rig = MakeRig(Duration::zero(), 1);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    const SoundHandle late = rig.submitter.Play({rig.clip, kAnchor - Ms(10)}); // clamped to frame 480
    REQUIRE(late);
    REQUIRE(rig.submitter.Stop(late, kAnchor - Ms(5)));
    REQUIRE(rig.submitter.Play({rig.burst, kAnchor + Ms(10)}));
    CHECK(Sounding(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)), 480).size() == 10);
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.playedLate == 1);
    CHECK(stats.droppedNoVoice == 0);
    CHECK(stats.stopsLate == 0);
    CheckStopBalance(stats);
}

TEST_CASE("Audio Mixer frees a started sound stopped at the block's first frame, on time") {
    Rig rig = MakeRig(Duration::zero(), 1);
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    REQUIRE(sound);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(10))); // frame 480, this block's first
    REQUIRE(rig.submitter.Play({rig.burst, kAnchor + Ms(10)}));
    REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(10))); // already stopped
    const auto frames = Sounding(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)), 480);
    CHECK(frames.size() == 10); // only the burst, in the freed voice
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.droppedNoVoice == 0);
    CHECK(stats.stopsLate == 0);
    CHECK(stats.stopsMatched == 1);
    CHECK(stats.stopsUnmatched == 1);
}

TEST_CASE("Audio Mixer frees a voice whose stop frame ends the block") {
    Rig rig = MakeRig(Duration::zero(), 1);
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(10))); // frame 480
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    REQUIRE(rig.submitter.Play({rig.burst, kAnchor + Ms(10)}));
    CHECK(Sounding(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)), 480).size() == 10);
    CHECK(rig.mixer.Stats().droppedNoVoice == 0);
}

TEST_CASE("Audio Mixer maps a stop like a play from the block's anchor") {
    // A 5 ms delay (240 frames) applies to the stop as to the play.
    Rig rig = MakeRig(Ms(5));
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    REQUIRE(sound);
    const auto first = Sounding(RenderBlock(rig.mixer, 480, kAnchor));
    REQUIRE_FALSE(first.empty());
    CHECK(first.front() == 240);
    // The next block's anchor is not 10 ms later: the stop maps from it anyway.
    const TimePoint anchor = kAnchor + Ns(10'500'000);
    REQUIRE(rig.submitter.Stop(sound, anchor + Ms(2)));
    const auto frames = Sounding(RenderBlock(rig.mixer, 960, anchor), 480);
    REQUIRE_FALSE(frames.empty());
    CHECK(frames.back() == 480 + 96 + 240 - 1);
}

TEST_CASE("Audio Mixer StopAll stops only its own submitter's sounds") {
    ClipBank bank;
    const ClipId loud = AddConstant(bank, 4800, kLevel);
    const ClipId soft = AddConstant(bank, 4800, 3);
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto a = mixer->CreateSubmitter();
    auto b = mixer->CreateSubmitter();
    REQUIRE((a && b));
    REQUIRE(a->Play({loud, kAnchor}));
    REQUIRE(a->Play({loud, kAnchor + Ms(1)}));
    REQUIRE(b->Play({soft, kAnchor}));
    // Every voice is scheduled before the stop is taken: rings are taken in
    // index order, so a stop in the same block would run before b's play.
    static_cast<void>(RenderBlock(*mixer, 120, kAnchor));
    REQUIRE(a->StopAll(kAnchor + Ms(5))); // frame 240, mapped from the next anchor
    const auto out = RenderBlock(*mixer, 840, kAnchor + Ns(2'500'000)); // frames 120 to 959
    CHECK(out[(239 - 120) * 2] == 2 * kLevel + 3);
    CHECK(out[(240 - 120) * 2] == 3);
    CHECK(out[(959 - 120) * 2] == 3);
    const MixerStats stats = mixer->Stats();
    CHECK(stats.stopsSubmitted == 1); // one request, two voices
    CHECK(stats.stopsMatched == 1);
    CheckStopBalance(stats);
}

TEST_CASE("Audio Mixer counts a stop of a finished or dropped play as unmatched") {
    Rig rig = MakeRig(Duration::zero(), 1);
    const SoundHandle finished = rig.submitter.Play({rig.burst, kAnchor});
    REQUIRE(finished);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    REQUIRE(rig.submitter.Stop(finished, kAnchor + Ms(20)));
    const SoundHandle playing = rig.submitter.Play({rig.clip, kAnchor + Ms(20)});
    const SoundHandle dropped = rig.submitter.Play({rig.clip, kAnchor + Ms(20)}); // no voice left
    REQUIRE((playing && dropped));
    REQUIRE(rig.submitter.Stop(dropped, kAnchor + Ms(30)));
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)));
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.droppedNoVoice == 1);
    CHECK(stats.stopsMatched == 0);
    CHECK(stats.stopsUnmatched == 2);
    CheckStopBalance(stats);
}

TEST_CASE("Audio Mixer never lets a stale handle stop a newer sound in the same voice") {
    Rig rig = MakeRig(Duration::zero(), 1);
    const SoundHandle old = rig.submitter.Play({rig.burst, kAnchor});
    REQUIRE(old);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    const SoundHandle newer = rig.submitter.Play({rig.clip, kAnchor + Ms(10)});
    REQUIRE(newer);
    REQUIRE(rig.submitter.Stop(old, kAnchor + Ms(10)));
    CHECK(Sounding(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)), 480).size() == 480);
    CHECK(rig.mixer.Stats().stopsUnmatched == 1);
}

TEST_CASE("Audio Mixer drops and counts a stop when its queue is full") {
    Rig rig = MakeRig(Duration::zero(), 8, 2);
    const SoundHandle a = rig.submitter.Play({rig.clip, kAnchor});
    REQUIRE(a);
    REQUIRE(rig.submitter.Play({rig.clip, kAnchor}));
    CHECK_FALSE(rig.submitter.Stop(a, kAnchor));
    CHECK_FALSE(rig.submitter.StopAll(kAnchor));
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.stopsSubmitted == 2);
    CHECK(stats.stopsDroppedQueueFull == 2);
    CheckStopBalance(stats);
}

TEST_CASE("Audio Mixer returns a unique handle per queued play and none for a dropped one") {
    Rig rig = MakeRig(Duration::zero(), 8, 2);
    const SoundHandle a = rig.submitter.Play({rig.burst, kAnchor});
    const SoundHandle b = rig.submitter.Play({rig.burst, kAnchor});
    const SoundHandle full = rig.submitter.Play({rig.burst, kAnchor});
    REQUIRE((a && b));
    CHECK(a != b);
    CHECK_FALSE(full);
    CHECK(full.value == 0);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    const SoundHandle c = rig.submitter.Play({rig.burst, kAnchor});
    REQUIRE(c);
    CHECK(c.value == b.value + 1); // the dropped play used no sequence
    CHECK((a.value >> 57) == 1);   // the first submitter
}

TEST_CASE("Audio Mixer ignores a stop of an invalid handle") {
    Rig rig = MakeRig();
    CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor));
    CHECK(rig.mixer.Stats().stopsSubmitted == 0);
}

TEST_CASE("Audio Mixer treats a foreign or never-issued handle as a Programmer Error") {
    ClipBank bank;
    const ClipId clip = AddConstant(bank, 10);
    auto mixer = Mixer::Create(MixerConfig{}, std::move(bank));
    REQUIRE(mixer.has_value());
    auto a = mixer->CreateSubmitter();
    auto b = mixer->CreateSubmitter();
    REQUIRE((a && b));
    const SoundHandle mine = a->Play({clip, kAnchor});
    REQUIRE(mine);
    GYO_CHECK_ASSERTS(b->Stop(mine, kAnchor));
    GYO_CHECK_ASSERTS(a->Stop(SoundHandle{mine.value + 1}, kAnchor));
    GYO_CHECK_ASSERTS(a->Stop(SoundHandle{(mine.value >> 57) << 57}, kAnchor)); // sequence 0
    CHECK(a->Stop(mine, kAnchor));
    Submitter moved(std::move(*a));
    GYO_CHECK_ASSERTS(a->Stop(mine, kAnchor));
    GYO_CHECK_ASSERTS(a->StopAll(kAnchor));
    CHECK(moved.Stop(mine, kAnchor));
}

TEST_CASE("Audio Submitter's stops belong to the thread that owns it") {
    ClipBank bank;
    const ClipId clip = AddConstant(bank, 10);
    auto mixer = Mixer::Create(MixerConfig{}, std::move(bank));
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    SoundHandle sound;
    std::thread other([&] { sound = submitter->Play({clip, kAnchor}); });
    other.join();
    REQUIRE(sound);
    GYO_CHECK_ASSERTS(submitter->Stop(sound, kAnchor));
    GYO_CHECK_ASSERTS(submitter->StopAll(kAnchor));
    CHECK(mixer->Stats().stopsSubmitted == 0);
}

TEST_CASE("Audio Mixer gives the last of 64 submitters valid handles") {
    ClipBank bank;
    const ClipId loud = AddConstant(bank, 4800, kLevel);
    const ClipId soft = AddConstant(bank, 4800, 3);
    MixerConfig config;
    config.maxSubmitters = 64;
    config.scheduleDelay = Duration::zero();
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    std::vector<Submitter> submitters;
    for (int i = 0; i < 64; ++i) submitters.push_back(std::move(mixer->CreateSubmitter()).value());
    REQUIRE(submitters.front().Play({soft, kAnchor}));
    const SoundHandle last = submitters.back().Play({loud, kAnchor});
    REQUIRE(last);
    CHECK((last.value >> 57) == 64);
    static_cast<void>(RenderBlock(*mixer, 120, kAnchor)); // schedule both before the stops
    REQUIRE(submitters.back().Stop(last, kAnchor + Ms(5)));
    REQUIRE(submitters.back().StopAll(kAnchor + Ms(5)));
    const auto out = RenderBlock(*mixer, 360, kAnchor + Ns(2'500'000));
    CHECK(out[(239 - 120) * 2] == kLevel + 3);
    CHECK(out[(240 - 120) * 2] == 3);
    CHECK(mixer->Stats().stopsMatched == 2);
}

TEST_CASE("Audio Mixer balances plays and stops from concurrent producers") {
    // Each producer plays a sound and stops it at once or later; whether a stop
    // finds its voice depends on timing, but every request lands in exactly one
    // outcome (Design Invariant 6).
    constexpr int kProducers = 4;
    constexpr int kEach = 3000;
    ClipBank bank;
    const ClipId clip = AddConstant(bank, 64);
    MixerConfig config;
    config.maxSubmitters = kProducers;
    config.queueCapacity = 32;
    config.maxVoices = 16;
    config.scheduleDelay = Duration::zero();
    auto created = Mixer::Create(config, std::move(bank));
    REQUIRE(created.has_value());
    Mixer mixer = std::move(created).value();
    std::vector<Submitter> submitters;
    for (int p = 0; p < kProducers; ++p) submitters.push_back(std::move(mixer.CreateSubmitter()).value());

    std::atomic<int> running{kProducers};
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            Submitter& submitter = submitters[static_cast<std::size_t>(p)];
            for (int i = 0; i < kEach; ++i) {
                const SoundHandle sound = submitter.Play({clip, kAnchor + Ms(i % 3)});
                static_cast<void>(submitter.Stop(sound, kAnchor + Ms(i % 5)));
                if (i % 100 == 0) static_cast<void>(submitter.StopAll(kAnchor));
                if (i % 64 == 0) std::this_thread::yield();
            }
            --running;
        });
    }
    while (running > 0) static_cast<void>(RenderBlock(mixer, 256, kAnchor));
    for (auto& producer : producers) producer.join();
    static_cast<void>(RenderBlock(mixer, 256, kAnchor)); // take what is left
    const MixerStats stats = mixer.Stats();
    CHECK(stats.stopsSubmitted > 0);
    CHECK(stats.stopsMatched > 0);
    CheckStopBalance(stats);
}
