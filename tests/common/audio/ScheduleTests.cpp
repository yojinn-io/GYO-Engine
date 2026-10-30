#include "doctest/doctest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
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

constexpr std::int16_t kImpulse = 1000;
// An arbitrary anchor well inside the clock's range.
const TimePoint kAnchor = TimePoint{} + std::chrono::duration_cast<Duration>(10s);

struct Rig final {
    Mixer mixer;
    Submitter submitter;
    ClipId impulse;
};

Rig MakeRig(const Duration delay = Duration::zero(), const std::uint32_t capacity = 256,
            const std::uint32_t voices = 64) {
    ClipBank bank;
    const std::vector<std::int16_t> one{kImpulse};
    auto clip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(one.data(), one.size()), 1, kSampleRate);
    REQUIRE(clip.has_value());
    MixerConfig config;
    config.scheduleDelay = delay;
    config.queueCapacity = capacity;
    config.maxVoices = voices;
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    return Rig{std::move(mixer).value(), std::move(submitter).value(), *clip};
}

std::vector<std::int16_t> RenderBlock(Mixer& mixer, const std::uint32_t frames, const TimePoint anchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * kChannels, 0x5555);
    mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    return out;
}

// Frames (within the rendered output) whose left sample is not silent.
std::vector<std::int64_t> Onsets(const std::vector<std::int16_t>& out, const std::int64_t firstFrame = 0) {
    std::vector<std::int64_t> frames;
    for (std::size_t f = 0; f * 2 < out.size(); ++f) {
        if (out[f * 2] != 0) frames.push_back(firstFrame + static_cast<std::int64_t>(f));
    }
    return frames;
}

Duration Ns(const std::int64_t ns) { return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(ns)); }

// The expected start, from the contract's wording rather than the mixer's
// arithmetic: offset * 48000 / 10^9 rounded to the nearest frame, halves up.
std::int64_t ExpectedFrames(const std::int64_t offsetNs) {
    const std::int64_t scaled = offsetNs * 48000 + 500'000'000;
    const std::int64_t q = scaled / 1'000'000'000;
    return (scaled % 1'000'000'000 != 0 && scaled < 0) ? q - 1 : q;
}

std::int64_t OnsetOf(const std::int64_t offsetNs, const Duration delay) {
    Rig rig = MakeRig(delay);
    REQUIRE(rig.submitter.Play({rig.impulse, kAnchor + Ns(offsetNs)}));
    const auto onsets = Onsets(RenderBlock(rig.mixer, 480, kAnchor));
    REQUIRE(onsets.size() == 1);
    return onsets[0];
}

} // namespace

TEST_CASE("Audio Mixer delays every start by 60 ms by default") {
    CHECK(MixerConfig{}.scheduleDelay == std::chrono::duration_cast<Duration>(60ms));
}

TEST_CASE("Audio Mixer rejects a configuration outside its limits") {
    const auto expectInvalid = [](const MixerConfig& config) {
        auto mixer = Mixer::Create(config, ClipBank{});
        REQUIRE_FALSE(mixer.has_value());
        CHECK(mixer.error().code == AudioErrorCode::InvalidConfig);
    };
    MixerConfig config;
    config.maxSubmitters = 0;
    expectInvalid(config);
    config.maxSubmitters = 65;
    expectInvalid(config);
    config = {};
    config.queueCapacity = 0;
    expectInvalid(config);
    config.queueCapacity = 65537;
    expectInvalid(config);
    config = {};
    config.scheduleDelay = Ns(-1);
    expectInvalid(config);
    config.scheduleDelay = Ns(1'000'000'001);
    expectInvalid(config);
    config = {};
    config.maxVoices = 0;
    expectInvalid(config);

    config = {};
    config.maxSubmitters = 64;
    config.queueCapacity = 65536;
    config.scheduleDelay = std::chrono::duration_cast<Duration>(1s);
    CHECK(Mixer::Create(config, ClipBank{}).has_value());
}

TEST_CASE("Audio Mixer hands out each submitter once, also to concurrent callers") {
    MixerConfig config;
    config.maxSubmitters = 8;
    auto mixer = Mixer::Create(config, ClipBank{});
    REQUIRE(mixer.has_value());
    std::atomic<int> created{0};
    std::atomic<int> refused{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 4; ++i) {
                auto submitter = mixer->CreateSubmitter();
                if (submitter) {
                    ++created;
                } else if (submitter.error().code == AudioErrorCode::TooManySubmitters) {
                    ++refused;
                }
            }
        });
    }
    for (auto& thread : threads) thread.join();
    CHECK(created == 8);
    CHECK(refused == 8);
}

TEST_CASE("Audio Mixer gives each submitter its own queue") {
    ClipBank bank;
    const std::vector<std::int16_t> one{kImpulse};
    const auto clip = *bank.Add(Engine::Base::ConstSpan<std::int16_t>(one.data(), one.size()), 1, kSampleRate);
    MixerConfig config;
    config.maxSubmitters = 3;
    config.queueCapacity = 1;
    config.scheduleDelay = Duration::zero();
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    std::vector<Submitter> submitters;
    for (int i = 0; i < 3; ++i) submitters.push_back(std::move(mixer->CreateSubmitter()).value());
    // Each queue holds one request, so a shared queue would refuse the second.
    for (Submitter& submitter : submitters) CHECK(submitter.Play({clip, kAnchor}));
    CHECK_FALSE(submitters[0].Play({clip, kAnchor}));
    const auto out = RenderBlock(*mixer, 1, kAnchor);
    CHECK(out[0] == 3 * kImpulse);
}

TEST_CASE("Audio Mixer starts a request at its offset from the block's anchor") {
    CHECK(OnsetOf(0, Duration::zero()) == 0);
    CHECK(OnsetOf(1'000'000, Duration::zero()) == 48);       // 1 ms
    CHECK(OnsetOf(9'979'167, Duration::zero()) == 479);      // the block's last frame
    CHECK(OnsetOf(0, std::chrono::duration_cast<Duration>(1ms)) == 48); // the delay adds 48 frames
}

TEST_CASE("Audio Mixer rounds an offset to the nearest frame, halves up") {
    // One frame is 20833.33 ns; half a frame is 10416.67 ns.
    CHECK(OnsetOf(10'416, Duration::zero()) == 0);
    CHECK(OnsetOf(10'417, Duration::zero()) == 1);
    CHECK(OnsetOf(31'249, Duration::zero()) == 1);
    CHECK(OnsetOf(31'250, Duration::zero()) == 2); // exactly 1.5 frames rounds up
    const Duration delay = std::chrono::duration_cast<Duration>(1ms);
    CHECK(OnsetOf(-10'416, delay) == 48);
    CHECK(OnsetOf(-10'417, delay) == 47);
    CHECK(OnsetOf(-31'250, delay) == 47);           // exactly -1.5 frames: floor(x + 1/2), toward +infinity
    CHECK(OnsetOf(-10'418 - 20'833, delay) == 46); // just past -1.5 frames
    CHECK(OnsetOf(-1'000'000, delay) == 0);
}

TEST_CASE("Audio Mixer rounds the schedule delay to the nearest frame") {
    CHECK(OnsetOf(0, Ns(10'416)) == 0);
    CHECK(OnsetOf(0, Ns(10'417)) == 1);
    CHECK(OnsetOf(0, Ns(1'010'417)) == 49);
}

TEST_CASE("Audio Mixer starts each request at its own rounded offset under one anchor") {
    // Design Invariant 5: each start is its offset from the anchor rounded to the
    // nearest frame, so two starts differ from the timestamp difference times
    // 48000 by less than one frame.
    // Offsets 0 and 7000 ns share frame 0, so that frame must carry both impulses.
    const std::vector<std::int64_t> offsets{0, 7'000, 15'000, 1'000'000, 1'010'417, 2'345'678, 3'999'999, 9'000'000};
    Rig rig = MakeRig();
    for (const std::int64_t offset : offsets) REQUIRE(rig.submitter.Play({rig.impulse, kAnchor + Ns(offset)}));
    const auto out = RenderBlock(rig.mixer, 480, kAnchor);
    std::vector<std::int16_t> expected(480, 0);
    for (const std::int64_t offset : offsets) {
        const std::int64_t frame = ExpectedFrames(offset);
        REQUIRE(frame >= 0);
        REQUIRE(frame < 480);
        expected[static_cast<std::size_t>(frame)] = static_cast<std::int16_t>(expected[static_cast<std::size_t>(frame)] + kImpulse);
    }
    for (std::size_t f = 0; f < 480; ++f) CHECK(out[f * 2] == expected[f]);
    CHECK(expected[0] == 2 * kImpulse);
}

TEST_CASE("Audio Mixer maps a request in a later block from that block's anchor") {
    Rig rig = MakeRig();
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    // The second block's anchor is not 10 ms after the first: each block is
    // mapped from its own anchor, never from an offset carried across blocks.
    const TimePoint second = kAnchor + Ns(10'300'000);
    REQUIRE(rig.submitter.Play({rig.impulse, second + Ns(500'000)}));
    const auto onsets = Onsets(RenderBlock(rig.mixer, 480, second), 480);
    REQUIRE(onsets.size() == 1);
    CHECK(onsets[0] == 480 + 24);
}

TEST_CASE("Audio Mixer starts a request before the block at its first frame and counts it late") {
    const Duration delay = std::chrono::duration_cast<Duration>(1ms);
    Rig rig = MakeRig(delay);
    // Before the first block ever rendered: a negative position is late too.
    REQUIRE(rig.submitter.Play({rig.impulse, kAnchor - Ns(2'000'000)}));
    auto onsets = Onsets(RenderBlock(rig.mixer, 480, kAnchor));
    REQUIRE(onsets.size() == 1);
    CHECK(onsets[0] == 0);
    auto stats = rig.mixer.Stats();
    CHECK(stats.playedLate == 1);
    CHECK(stats.maxLateness == std::chrono::duration_cast<Duration>(1ms)); // 48 frames

    // One frame late in a later block.
    const TimePoint second = kAnchor + Ns(10'000'000);
    REQUIRE(rig.submitter.Play({rig.impulse, second - delay - Ns(20'834)}));
    onsets = Onsets(RenderBlock(rig.mixer, 480, second), 480);
    REQUIRE(onsets.size() == 1);
    CHECK(onsets[0] == 480);
    stats = rig.mixer.Stats();
    CHECK(stats.playedLate == 2);
    CHECK(stats.playedOnTime == 0);
}

TEST_CASE("Audio Mixer survives timestamps at the ends of the clock") {
    Rig rig = MakeRig();
    REQUIRE(rig.submitter.Play({rig.impulse, TimePoint::min()}));
    REQUIRE(rig.submitter.Play({rig.impulse, TimePoint::max()}));
    REQUIRE(rig.submitter.Play({rig.impulse, TimePoint{}}));
    const auto onsets = Onsets(RenderBlock(rig.mixer, 480, kAnchor));
    CHECK(onsets == std::vector<std::int64_t>{0}); // both past ones start late at frame 0
    const auto stats = rig.mixer.Stats();
    CHECK(stats.playedLate == 2);
    CHECK(stats.playedOnTime == 1); // the far one waits an hour ahead
}

TEST_CASE("Audio Mixer keeps offsets exact up to an hour and clamps beyond") {
    Rig rig = MakeRig();
    REQUIRE(rig.submitter.Play({rig.impulse, kAnchor - std::chrono::duration_cast<Duration>(2s)}));
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor));
    CHECK(rig.mixer.Stats().maxLateness == std::chrono::duration_cast<Duration>(2s));

    Rig far = MakeRig();
    REQUIRE(far.submitter.Play({far.impulse, kAnchor - std::chrono::duration_cast<Duration>(3h)}));
    static_cast<void>(RenderBlock(far.mixer, 480, kAnchor));
    CHECK(far.mixer.Stats().maxLateness == std::chrono::duration_cast<Duration>(1h));
}

TEST_CASE("Audio Mixer does not overflow between a negative anchor and the clock's end") {
    // An anchor before the clock's epoch with a request at its far end: the raw
    // difference would overflow int64.
    const TimePoint anchor = TimePoint{} - std::chrono::duration_cast<Duration>(10s);
    Rig rig = MakeRig();
    REQUIRE(rig.submitter.Play({rig.impulse, TimePoint::max()}));
    REQUIRE(rig.submitter.Play({rig.impulse, TimePoint::min()}));
    static_cast<void>(RenderBlock(rig.mixer, 480, anchor));
    const auto stats = rig.mixer.Stats();
    CHECK(stats.playedOnTime == 1); // an hour ahead
    CHECK(stats.playedLate == 1);
    CHECK(stats.maxLateness == std::chrono::duration_cast<Duration>(1h));
}

TEST_CASE("Audio Mixer drops and counts a request when its queue is full") {
    Rig rig = MakeRig(Duration::zero(), 2);
    CHECK(rig.submitter.Play({rig.impulse, kAnchor}));
    CHECK(rig.submitter.Play({rig.impulse, kAnchor + Ns(1'000'000)}));
    CHECK_FALSE(rig.submitter.Play({rig.impulse, kAnchor + Ns(2'000'000)}));
    CHECK(Onsets(RenderBlock(rig.mixer, 480, kAnchor)) == std::vector<std::int64_t>{0, 48});
    CHECK(rig.submitter.Play({rig.impulse, kAnchor + Ns(5'000'000)})); // room again after the drain

    const auto stats = rig.mixer.Stats();
    CHECK(stats.submitted == 4);
    CHECK(stats.droppedQueueFull == 1);
    CHECK(stats.playedOnTime == 2);
}

TEST_CASE("Audio Mixer keeps order and content across ring wraparound") {
    Rig rig = MakeRig(Duration::zero(), 3);
    TimePoint anchor = kAnchor;
    for (int round = 0; round < 10; ++round) {
        REQUIRE(rig.submitter.Play({rig.impulse, anchor + Ns(1'000'000)}));
        REQUIRE(rig.submitter.Play({rig.impulse, anchor + Ns(3'000'000)}));
        const auto first = static_cast<std::int64_t>(round) * 480;
        CHECK(Onsets(RenderBlock(rig.mixer, 480, anchor), first) == std::vector<std::int64_t>{first + 48, first + 144});
        anchor += Ns(10'000'000);
    }
    const auto stats = rig.mixer.Stats();
    CHECK(stats.submitted == 20);
    CHECK(stats.playedOnTime == 20);
}

TEST_CASE("Audio Mixer balances and plays every request from concurrent producers") {
    // Producer p plays clip p (one sample of 8 * (p + 1)) with a gain that cycles
    // 0.25, 0.5, 0.75, 1.0, so the output's sum checks what the rings carried, not
    // only how many: a stale, torn or repeated slot changes it. Every request is
    // in the past of the moving anchor, so it starts in the block that takes it
    // and frees its voice there; with more voices than one block can take, no
    // play is dropped for want of a voice and the sum is exact (and far below
    // saturation).
    constexpr int kProducers = 4;
    constexpr int kEach = 5000;
    ClipBank bank;
    std::vector<ClipId> clips;
    for (int p = 0; p < kProducers; ++p) {
        const std::vector<std::int16_t> one{static_cast<std::int16_t>(8 * (p + 1))};
        clips.push_back(*bank.Add(Engine::Base::ConstSpan<std::int16_t>(one.data(), one.size()), 1, kSampleRate));
    }
    MixerConfig config;
    config.maxSubmitters = kProducers;
    config.queueCapacity = 64;
    config.maxVoices = 1024; // above kProducers * queueCapacity
    config.scheduleDelay = Duration::zero(); // every request starts in the block that takes it
    auto created = Mixer::Create(config, std::move(bank));
    REQUIRE(created.has_value());
    Mixer mixer = std::move(created).value();

    std::vector<Submitter> submitters;
    for (int p = 0; p < kProducers; ++p) submitters.push_back(std::move(mixer.CreateSubmitter()).value());
    std::atomic<int> running{kProducers};
    std::atomic<std::uint64_t> accepted{0};
    std::atomic<std::int64_t> expectedSum{0};
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            std::uint64_t mine = 0;
            std::int64_t sum = 0;
            for (int i = 0; i < kEach; ++i) {
                const int quarter = i % 4 + 1;
                const TimePoint at = kAnchor - Ns(static_cast<std::int64_t>((i * 7919 + p) % 40) * 1'000'000);
                if (submitters[static_cast<std::size_t>(p)].Play(
                        {clips[static_cast<std::size_t>(p)], at, 0.25f * quarter, 0.25f * quarter})) {
                    ++mine;
                    sum += 2 * (p + 1) * quarter; // 8 * (p + 1) * quarter / 4
                }
                if (i % 64 == 0) std::this_thread::yield();
            }
            accepted += mine;
            expectedSum += sum;
            --running;
        });
    }
    std::vector<std::int16_t> block(480 * kChannels);
    std::int64_t outputSum = 0;
    TimePoint anchor = kAnchor;
    const auto renderOnce = [&] {
        mixer.Render(Engine::Base::Span<std::int16_t>(block.data(), block.size()), anchor);
        for (std::size_t f = 0; f < 480; ++f) outputSum += block[f * 2];
        anchor += Ns(10'000'000);
    };
    // Keep rendering until every producer is done, then once more to drain.
    while (running.load() > 0) renderOnce();
    for (auto& producer : producers) producer.join();
    renderOnce();

    const auto stats = mixer.Stats();
    CHECK(stats.submitted == kProducers * kEach);
    CHECK(stats.submitted - stats.droppedQueueFull == accepted.load());
    CHECK(stats.droppedNoVoice == 0);
    CHECK(stats.clippedSamples == 0);
    CHECK(stats.submitted ==
          stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
    CHECK(outputSum == expectedSum.load());
}

TEST_CASE("Audio Submitter belongs to the first thread that plays through it") {
    Rig rig = MakeRig();
    std::thread other([&] { CHECK(rig.submitter.Play({rig.impulse, kAnchor})); });
    other.join();
    GYO_CHECK_ASSERTS(rig.submitter.Play({rig.impulse, kAnchor}));
    CHECK(rig.mixer.Stats().submitted == 1);
}

TEST_CASE("Audio Submitter treats a foreign clip, a bad gain and a moved-from handle as Programmer Errors") {
    Rig rig = MakeRig();
    GYO_CHECK_ASSERTS(rig.submitter.Play({ClipId{2}, kAnchor}));
    GYO_CHECK_ASSERTS(rig.submitter.Play({rig.impulse, kAnchor, -1.0f, 1.0f}));
    GYO_CHECK_ASSERTS(rig.submitter.Play({rig.impulse, kAnchor, 1.0f, kMaxGain + 1.0f}));
    Submitter moved(std::move(rig.submitter));
    GYO_CHECK_ASSERTS(rig.submitter.Play({rig.impulse, kAnchor}));
    CHECK(moved.Play({rig.impulse, kAnchor}));
    CHECK(rig.mixer.Stats().submitted == 1);
}

TEST_CASE("Audio Submitter may outlive its Mixer") {
    Rig rig = MakeRig();
    Submitter submitter = std::move(rig.submitter);
    { Mixer gone = std::move(rig.mixer); }
    CHECK(submitter.Play({rig.impulse, kAnchor}));
}

TEST_CASE("Audio Mixer writes silence where it has nothing to render") {
    Rig rig = MakeRig();
    REQUIRE(rig.submitter.Play({rig.impulse, kAnchor}));
    Mixer moved = std::move(rig.mixer);
    auto out = RenderBlock(rig.mixer, 4, kAnchor); // moved-from
    for (const std::int16_t sample : out) CHECK(sample == 0);

    std::vector<std::int16_t> odd(5, 0x5555);
    moved.Render(Engine::Base::Span<std::int16_t>(odd.data(), odd.size()), kAnchor);
    CHECK(odd[0] == kImpulse);
    CHECK(odd[4] == 0);
    CHECK(moved.Stats().renderedFrames == 2);
}
