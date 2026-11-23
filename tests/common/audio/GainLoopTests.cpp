// Gain changes (Submitter::SetGain) and looping plays (PlayRequest::loop): the
// two playback features v8-arch-gyo2-sdl-a added for Object_Connect (G1, G2).
// Replacement Criterion 6 asks every new playback feature to verify again the
// mapping, the late rule, the ring order, the clip's release and real-time
// safety; the last two are in RenderAllocTests.cpp, the rest here.
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

using namespace GYO::Engine::Audio;
using namespace std::chrono_literals;
using GYO::Engine::Time::Duration;
using GYO::Engine::Time::TimePoint;

namespace {

constexpr std::int16_t kLevel = 1000;
const TimePoint kAnchor = TimePoint{} + std::chrono::duration_cast<Duration>(10s);

Duration Ns(const std::int64_t ns) { return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(ns)); }
Duration Ms(const std::int64_t ms) { return Ns(ms * 1'000'000); }

Clip Constant(const std::uint32_t frames, const std::int16_t level = kLevel) {
    auto clip = Clip::Make(std::vector<std::int16_t>(frames, level), 1, kSampleRate);
    REQUIRE(clip.has_value());
    return *clip;
}

// 0, 1, 2, ... frames - 1: each frame tells where in the clip it came from.
Clip Ramp(const std::uint32_t frames) {
    std::vector<std::int16_t> samples(frames);
    for (std::uint32_t i = 0; i < frames; ++i) samples[i] = static_cast<std::int16_t>(i + 1);
    auto clip = Clip::Make(std::move(samples), 1, kSampleRate);
    REQUIRE(clip.has_value());
    return *clip;
}

struct Rig final {
    Mixer mixer;
    Submitter submitter;
};

Rig MakeRig(const Duration delay = Duration::zero(), const std::uint32_t voices = 8, const std::uint32_t capacity = 64) {
    MixerConfig config;
    config.scheduleDelay = delay;
    config.maxVoices = voices;
    config.queueCapacity = capacity;
    auto mixer = Mixer::Create(config);
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    return Rig{std::move(mixer).value(), std::move(submitter).value()};
}

// The left channel of one block.
std::vector<std::int16_t> RenderLeft(Mixer& mixer, const std::uint32_t frames, const TimePoint anchor = kAnchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * kChannels, 0x5555);
    mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    std::vector<std::int16_t> left(frames);
    for (std::size_t f = 0; f < frames; ++f) left[f] = out[f * 2];
    return left;
}

std::vector<std::int16_t> RenderLeftInBlocks(Mixer& mixer, const std::uint32_t total, const std::uint32_t block) {
    std::vector<std::int16_t> all;
    for (std::uint32_t done = 0; done < total; done += block) {
        const auto one = RenderLeft(mixer, std::min(block, total - done));
        all.insert(all.end(), one.begin(), one.end());
    }
    return all;
}

void CheckGainBalance(const MixerStats& stats) {
    CHECK(stats.gainsSubmitted == stats.gainsMatched + stats.gainsUnmatched + stats.gainsDroppedQueueFull);
    CHECK(stats.submitted == stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
}

} // namespace

// --- SetGain -------------------------------------------------------------------------------------------------

TEST_CASE("Audio Mixer changes a sound's gain at the frame its time maps to") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({Constant(4800), kAnchor});
    REQUIRE(sound);
    REQUIRE(rig.submitter.SetGain(sound, 0.5f, 0.25f, kAnchor + Ms(10))); // frame 480, at once
    std::vector<std::int16_t> out(std::size_t{960} * kChannels);
    rig.mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), kAnchor);
    CHECK(out[479 * 2] == kLevel);
    CHECK(out[479 * 2 + 1] == kLevel);
    CHECK(out[480 * 2] == kLevel / 2);
    CHECK(out[480 * 2 + 1] == kLevel / 4);
    CHECK(out[959 * 2] == kLevel / 2);
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.gainsMatched == 1);
    CHECK(stats.gainsLate == 0);
    CheckGainBalance(stats);
}

TEST_CASE("Audio Mixer ramps a gain linearly over the ramp's frames") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({Constant(4800), kAnchor});
    REQUIRE(sound);
    // From 1 to 0 over 100 frames, starting at frame 100: frame 100 + k is 1000 * (100 - k) / 100.
    REQUIRE(rig.submitter.SetGain(sound, 0.0f, 0.0f, kAnchor + Ns(2'083'334), Ns(2'083'334)));
    const auto left = RenderLeft(rig.mixer, 300);
    CHECK(left[99] == kLevel);
    for (int k = 0; k < 100; ++k) {
        CAPTURE(k);
        CHECK(left[static_cast<std::size_t>(100 + k)] == kLevel * (100 - k) / 100);
    }
    CHECK(left[200] == 0);
    CHECK(left[299] == 0);
}

TEST_CASE("Audio Mixer maps a gain change like a play from the block's anchor, delay included") {
    // Delay 10 ms (480 frames). The change is taken in the second block, whose
    // anchor is 5 ms after the first: at - anchor = 5 ms, so it starts at frame
    // 960 (block start) + 240 + 480 = 1680.
    Rig rig = MakeRig(Ms(10));
    const SoundHandle sound = rig.submitter.Play({Constant(9600), kAnchor - Ms(10)}); // frame 0
    REQUIRE(sound);
    static_cast<void>(RenderLeft(rig.mixer, 960, kAnchor));
    REQUIRE(rig.submitter.SetGain(sound, 0.5f, 0.5f, kAnchor + Ms(10)));
    const auto left = RenderLeft(rig.mixer, 1440, kAnchor + Ms(5));
    CHECK(left[1680 - 960 - 1] == kLevel);
    CHECK(left[1680 - 960] == kLevel / 2);
    CHECK(rig.mixer.Stats().gainsLate == 0);
}

TEST_CASE("Audio Mixer applies a late gain change at the block's first frame") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({Constant(4800), kAnchor});
    REQUIRE(sound);
    static_cast<void>(RenderLeft(rig.mixer, 480));
    REQUIRE(rig.submitter.SetGain(sound, 0.5f, 0.5f, kAnchor - Ms(5), Ns(2'083'334))); // 100 frames
    const auto left = RenderLeft(rig.mixer, 480, kAnchor);
    // The ramp starts at the block's first frame (480) from the gain it had there.
    CHECK(left[0] == kLevel);
    CHECK(left[50] == kLevel - kLevel / 2 * 50 / 100);
    CHECK(left[100] == kLevel / 2);
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.gainsLate == 1);
    CHECK(stats.gainsMatched == 1);
    CheckGainBalance(stats);
}

TEST_CASE("Audio Mixer starts a new gain change from the gain the sound has when it is taken") {
    // A ramp from 1 to 0 over 960 frames is half done when the next block takes a
    // change to 1 at frame 1440: the gain stays at 0.5 from frame 480 to 1439
    // (the unfinished ramp is replaced), then is 1.
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({Constant(4800), kAnchor});
    REQUIRE(sound);
    REQUIRE(rig.submitter.SetGain(sound, 0.0f, 0.0f, kAnchor, Ms(20)));
    const auto first = RenderLeft(rig.mixer, 480);
    CHECK(first[0] == kLevel);
    CHECK(first[479] == kLevel * (960 - 479) / 960);
    REQUIRE(rig.submitter.SetGain(sound, 1.0f, 1.0f, kAnchor + Ms(20))); // 480 + 960
    const auto second = RenderLeft(rig.mixer, 1440, kAnchor);
    CHECK(second[0] == kLevel / 2);
    CHECK(second[959] == kLevel / 2);
    CHECK(second[960] == kLevel);
    // A change sent before the sound starts applies from its start.
    Rig later = MakeRig();
    const SoundHandle pending = later.submitter.Play({Constant(4800), kAnchor + Ms(10)});
    REQUIRE(pending);
    REQUIRE(later.submitter.SetGain(pending, 0.25f, 0.25f, kAnchor));
    const auto left = RenderLeft(later.mixer, 960);
    CHECK(left[479] == 0);
    CHECK(left[480] == kLevel / 4);
}

TEST_CASE("Audio Mixer output with gain changes does not depend on how blocks split it") {
    // Every command is queued before the first render and taken there, so each
    // change's start frame is fixed (Design Invariant 8 (a)).
    const auto render = [](const std::uint32_t block) {
        Rig rig = MakeRig();
        const SoundHandle a = rig.submitter.Play({Constant(4400), kAnchor + Ns(1'234'567)});
        const SoundHandle b = rig.submitter.Play({Constant(4400), kAnchor + Ms(20), 0.5f, 0.25f, true});
        REQUIRE((a && b));
        REQUIRE(rig.submitter.SetGain(a, 0.1f, 2.0f, kAnchor + Ms(5), Ns(7'777'777)));
        REQUIRE(rig.submitter.SetGain(b, 1.5f, 0.0f, kAnchor + Ms(40), Ms(13)));
        std::vector<std::int16_t> all;
        for (std::uint32_t done = 0; done < 4400; done += block) {
            const std::uint32_t frames = std::min(block, 4400 - done);
            std::vector<std::int16_t> out(std::size_t{frames} * kChannels);
            rig.mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), kAnchor);
            all.insert(all.end(), out.begin(), out.end());
        }
        return all;
    };
    const auto reference = render(4400);
    for (const std::uint32_t block : {1u, 7u, 255u, 256u, 257u, 441u, 2200u}) {
        CAPTURE(block);
        CHECK(render(block) == reference);
    }
}

TEST_CASE("Audio Mixer counts a gain change of a finished or dropped play as unmatched") {
    Rig rig = MakeRig(Duration::zero(), 1);
    const SoundHandle done = rig.submitter.Play({Constant(10), kAnchor});
    const SoundHandle dropped = rig.submitter.Play({Constant(10), kAnchor}); // the only voice is taken
    REQUIRE((done && dropped));
    static_cast<void>(RenderLeft(rig.mixer, 480));
    REQUIRE(rig.submitter.SetGain(done, 0.5f, 0.5f, kAnchor));
    REQUIRE(rig.submitter.SetGain(dropped, 0.5f, 0.5f, kAnchor));
    static_cast<void>(RenderLeft(rig.mixer, 480));
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.gainsUnmatched == 2);
    CHECK(stats.gainsMatched == 0);
    CHECK(stats.gainsLate == 0);
    CheckGainBalance(stats);
}

TEST_CASE("Audio Mixer drops and counts a gain change when its queue is full") {
    Rig rig = MakeRig(Duration::zero(), 8, 1);
    const SoundHandle sound = rig.submitter.Play({Constant(4800), kAnchor});
    REQUIRE(sound);
    CHECK_FALSE(rig.submitter.SetGain(sound, 0.5f, 0.5f, kAnchor));
    const auto left = RenderLeft(rig.mixer, 480);
    CHECK(left[0] == kLevel); // the gain stays
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.gainsDroppedQueueFull == 1);
    CHECK(stats.gainsSubmitted == 1);
    CheckGainBalance(stats);
}

TEST_CASE("Audio Mixer takes a gain change after the play queued before it, in queue order") {
    // Play, change, play again on one ring in one block: the change reaches only
    // the first sound, and only after it was scheduled.
    Rig rig = MakeRig();
    const SoundHandle first = rig.submitter.Play({Constant(4800), kAnchor});
    REQUIRE(rig.submitter.SetGain(first, 0.5f, 0.5f, kAnchor));
    const SoundHandle second = rig.submitter.Play({Constant(4800, 100), kAnchor});
    REQUIRE((first && second));
    const auto left = RenderLeft(rig.mixer, 480);
    CHECK(left[0] == kLevel / 2 + 100);
    CHECK(rig.mixer.Stats().gainsMatched == 1);
}

TEST_CASE("Audio Mixer ignores a gain change of an invalid handle and asserts a foreign one") {
    const Clip clip = Constant(10);
    auto mixer = Mixer::Create(MixerConfig{});
    REQUIRE(mixer.has_value());
    auto a = mixer->CreateSubmitter();
    auto b = mixer->CreateSubmitter();
    REQUIRE((a && b));
    CHECK(a->SetGain(SoundHandle{}, 0.5f, 0.5f, kAnchor));
    const SoundHandle mine = a->Play({clip, kAnchor});
    REQUIRE(mine);
    GYO_CHECK_ASSERTS(b->SetGain(mine, 0.5f, 0.5f, kAnchor));
    GYO_CHECK_ASSERTS(a->SetGain(SoundHandle{mine.value + 1}, 0.5f, 0.5f, kAnchor));
    GYO_CHECK_ASSERTS(a->SetGain(mine, -0.1f, 0.5f, kAnchor));
    GYO_CHECK_ASSERTS(a->SetGain(mine, 0.5f, kMaxGain * 2, kAnchor));
    GYO_CHECK_ASSERTS(a->SetGain(mine, 0.5f, 0.5f, kAnchor, -Ms(1)));
    CHECK(a->SetGain(mine, 0.5f, 0.5f, kAnchor));
    // The Programmer Errors are asserted before anything is counted.
    CHECK(mixer->Stats().gainsSubmitted == 1);
}

TEST_CASE("Audio Mixer balances plays, stops and gain changes from concurrent producers") {
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    config.maxVoices = 16;
    config.queueCapacity = 64;
    auto created = Mixer::Create(config);
    REQUIRE(created.has_value());
    Mixer mixer = std::move(created).value();
    const Clip clip = Constant(200);
    std::atomic<bool> go{false};
    std::vector<std::thread> producers;
    for (int p = 0; p < 3; ++p) {
        auto submitter = mixer.CreateSubmitter();
        REQUIRE(submitter.has_value());
        producers.emplace_back([&go, &clip, s = std::move(submitter).value()]() mutable {
            while (!go.load()) std::this_thread::yield();
            for (int i = 0; i < 2000; ++i) {
                const SoundHandle sound = s.Play({clip, kAnchor, 1.0f, 1.0f, i % 7 == 0});
                if (!sound) continue;
                static_cast<void>(s.SetGain(sound, 0.5f, 0.5f, kAnchor, Ms(1)));
                if (i % 7 == 0) static_cast<void>(s.Stop(sound, kAnchor + Ms(2)));
            }
        });
    }
    go.store(true);
    std::vector<std::int16_t> out(std::size_t{64} * kChannels);
    for (int block = 0; block < 4000; ++block) {
        mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), kAnchor);
    }
    for (auto& t : producers) t.join();
    for (int block = 0; block < 100; ++block) {
        mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), kAnchor);
    }
    const MixerStats stats = mixer.Stats();
    CHECK(stats.gainsSubmitted > 0);
    CheckGainBalance(stats);
    CHECK(stats.stopsSubmitted == stats.stopsMatched + stats.stopsUnmatched + stats.stopsDroppedQueueFull);
}

// --- loop ----------------------------------------------------------------------------------------------------

TEST_CASE("Audio Mixer plays a looping clip again from its first frame until it is stopped") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({Ramp(100), kAnchor, 1.0f, 1.0f, true});
    REQUIRE(sound);
    REQUIRE(rig.submitter.Stop(sound, kAnchor + Ns(5'208'334))); // frame 250
    const auto left = RenderLeft(rig.mixer, 480);
    for (int f = 0; f < 250; ++f) {
        CAPTURE(f);
        CHECK(left[static_cast<std::size_t>(f)] == f % 100 + 1);
    }
    CHECK(left[250] == 0);
    CHECK(rig.mixer.Stats().stopsMatched == 1);
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::NotPlaying);
}

TEST_CASE("Audio Mixer output of a looping clip does not depend on how blocks split it") {
    const auto render = [](const std::uint32_t block) {
        Rig rig = MakeRig();
        const SoundHandle sound = rig.submitter.Play({Ramp(333), kAnchor + Ns(1'234'567), 1.0f, 1.0f, true});
        REQUIRE(sound);
        REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(80)));
        return RenderLeftInBlocks(rig.mixer, 4400, block);
    };
    const auto reference = render(4400);
    CHECK(reference[59] == 1);   // 1.234567 ms: the first pass starts
    CHECK(reference[392] == 1);  // 333 frames later: the second
    CHECK(reference[3839] != 0); // the stop at 80 ms is frame 3840
    CHECK(reference[3840] == 0);
    for (const std::uint32_t block : {1u, 7u, 100u, 333u, 334u, 2200u}) {
        CAPTURE(block);
        CHECK(render(block) == reference);
    }
}

TEST_CASE("Audio Mixer reports a looping sound's position within its current pass") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({Constant(300), kAnchor, 1.0f, 1.0f, true});
    REQUIRE(sound);
    static_cast<void>(RenderLeft(rig.mixer, 200));
    CHECK(rig.mixer.PlayheadOf(sound).clipFrame == 200);
    static_cast<void>(RenderLeft(rig.mixer, 200)); // 400 frames: 100 into the second pass
    const Playhead playhead = rig.mixer.PlayheadOf(sound);
    CHECK(playhead.status == Playhead::Status::Playing);
    CHECK(playhead.clipFrame == 100);
    // It keeps its voice: a looping sound never ends by itself.
    static_cast<void>(RenderLeft(rig.mixer, 4800));
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::Playing);
}

TEST_CASE("Audio Mixer plays an intro then a loop scheduled at the intro's end") {
    // Object_Connect's music: the intro plays once, the loop starts at its last frame plus one.
    Rig rig = MakeRig();
    const Clip intro = Constant(150, 100);
    const Clip loop = Constant(50, 200);
    REQUIRE(rig.submitter.Play({intro, kAnchor}));
    const Duration introLength = Ns(150LL * 1'000'000'000 / kSampleRate);
    REQUIRE(rig.submitter.Play({loop, kAnchor + introLength, 1.0f, 1.0f, true}));
    const auto left = RenderLeft(rig.mixer, 480);
    CHECK(std::count(left.begin(), left.begin() + 150, std::int16_t{100}) == 150);
    CHECK(std::count(left.begin() + 150, left.end(), std::int16_t{200}) == 330);
}
