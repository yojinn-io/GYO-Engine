#include "doctest/doctest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Clip.hpp"
#include "engine/audio/Mixer.hpp"

// Plays of shared clips (Design Invariant 7, audio-clip-loader declaration 4.1):
// what the output shows. What is let go where is counted in RenderAllocTests.cpp,
// whose executable counts each thread's allocations and frees.

using namespace GYO::Engine::Audio;
using namespace std::chrono_literals;
using GYO::Engine::Time::Duration;
using GYO::Engine::Time::TimePoint;

// Error values are not renumbered when one is removed (4 was DeviceUnavailable,
// 5 NoClipSlot; v8-arch-gyo2-audio-review, v8-arch-gyo2-audio-clip-loader).
static_assert(static_cast<int>(AudioErrorCode::TooManySubmitters) == 3);

namespace {

const TimePoint kAnchor = TimePoint{} + std::chrono::duration_cast<Duration>(10s);

Duration Ms(const std::int64_t ms) { return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(ms)); }

struct Rig final {
    Mixer mixer;
    Submitter submitter;
};

Rig MakeRig(const std::uint32_t voices = 8, const std::uint32_t capacity = 64) {
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    config.maxVoices = voices;
    config.queueCapacity = capacity;
    auto mixer = Mixer::Create(config);
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    return Rig{std::move(mixer).value(), std::move(submitter).value()};
}

Clip Constant(const std::uint32_t frames, const std::int16_t level, const std::uint16_t channels = 1) {
    auto clip = Clip::Make(std::vector<std::int16_t>(std::size_t{frames} * channels, level), channels, kSampleRate);
    REQUIRE(clip.has_value());
    return *clip;
}

std::vector<std::int16_t> RenderBlock(Mixer& mixer, const std::uint32_t frames, const TimePoint anchor = kAnchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * kChannels, 0x5555);
    mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    return out;
}

std::size_t CountLeft(const std::vector<std::int16_t>& out, const std::int16_t value) {
    std::size_t count = 0;
    for (std::size_t f = 0; f * 2 < out.size(); ++f) count += out[f * 2] == value ? 1 : 0;
    return count;
}

void CheckBalance(const MixerStats& stats) {
    CHECK(stats.submitted == stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
    CHECK(stats.stopsSubmitted == stats.stopsMatched + stats.stopsUnmatched + stats.stopsDroppedQueueFull);
}

} // namespace

TEST_CASE("Audio Mixer plays a clip made while output runs") {
    Rig rig = MakeRig();
    static_cast<void>(RenderBlock(rig.mixer, 480)); // output is running
    const Clip mono = Constant(100, 300);
    auto both = Clip::Make(std::vector<std::int16_t>{11, -22, 11, -22, 11, -22}, 2, kSampleRate);
    REQUIRE(both.has_value());
    REQUIRE(rig.submitter.Play({mono, kAnchor + Ms(10)}));
    REQUIRE(rig.submitter.Play({*both, kAnchor + Ms(15)}));
    const auto out = RenderBlock(rig.mixer, 960, kAnchor + Ms(10));
    CHECK(CountLeft(out, 300) == 100);
    CHECK(out[240 * 2] == 11);
    CHECK(out[240 * 2 + 1] == -22);
    CHECK(out[243 * 2] == 0);
}

TEST_CASE("Audio Mixer plays every queued play fully after every other holder let go") {
    Rig rig = MakeRig(4);
    {
        const Clip clip = Constant(4800, 300);
        REQUIRE(rig.submitter.Play({clip, kAnchor}));
        REQUIRE(rig.submitter.Play({clip, kAnchor + Ms(10)}));
    } // the caller lets go before the audio thread has taken either play
    std::size_t heard = 0;
    for (int block = 0; block < 12; ++block) {
        const auto out = RenderBlock(rig.mixer, 480, kAnchor + Ms(10 * block));
        for (std::size_t f = 0; f < 480; ++f) {
            const std::int16_t v = out[f * 2];
            CHECK((v == 0 || v == 300 || v == 600)); // never anything but the two plays' samples
            heard += static_cast<std::size_t>(v / 300);
        }
    }
    CHECK(heard == 2 * 4800);
    CheckBalance(rig.mixer.Stats());
}

TEST_CASE("Audio Mixer hands back the entry of every voice a block frees") {
    // One voice, reused within one block: each stop frees it before the next
    // play takes it, so the entry of every freed voice must come back before
    // the next; with one entry more than the queue and the voices, a lost entry
    // shows as a Play that fails (asserted) within a few rounds.
    Rig rig = MakeRig(1, 4);
    const Clip clip = Constant(4800, 300);
    for (int round = 0; round < 20; ++round) {
        // Two plays, each stopped before its start, taken in one block.
        const SoundHandle a = rig.submitter.Play({clip, kAnchor + Ms(20)});
        REQUIRE(a);
        REQUIRE(rig.submitter.Stop(a, kAnchor + Ms(10)));
        const SoundHandle b = rig.submitter.Play({clip, kAnchor + Ms(20)});
        REQUIRE(b);
        REQUIRE(rig.submitter.Stop(b, kAnchor + Ms(10)));
        CHECK(CountLeft(RenderBlock(rig.mixer, 480), 300) == 0);
    }
    const auto stats = rig.mixer.Stats();
    CHECK(stats.droppedNoVoice == 0);
    CHECK(stats.droppedQueueFull == 0);
    CheckBalance(stats);
}

TEST_CASE("Audio Submitter plays on one thread and lets go on another after a hand-over") {
    // One user at a time; the join orders the first thread's calls before the
    // second's. The second side's Play takes back the entry the first side's
    // play held and lets go of that clip on the second side's thread.
    Rig rig = MakeRig(1, 1);
    auto use = [&](const std::int16_t level) {
        const Clip clip = Constant(10, level);
        CHECK(rig.submitter.Play({clip, kAnchor}));
        CHECK(CountLeft(RenderBlock(rig.mixer, 48), level) == 10); // the voice ends and its entry is handed back
    };
    std::thread other(use, std::int16_t{1});
    other.join();
    use(2);
    use(3);
    const auto stats = rig.mixer.Stats();
    CHECK(stats.submitted == 3);
    CHECK(stats.droppedQueueFull == 0);
    CheckBalance(stats);
}

TEST_CASE("Audio Mixer plays and lets go of clips while another thread renders") {
    // Structural stress for ThreadSanitizer and AddressSanitizer (declared: 200
    // rounds, no time bound). One voice, so every rendered sample is silence or
    // exactly one round's constant; the caller lets go of each clip at once, so
    // a read of samples let go too early shows another value (or a sanitizer
    // report).
    constexpr int kRounds = 200;
    Rig rig = MakeRig(1, 16);
    std::atomic<bool> done{false};
    std::atomic<std::uint64_t> badSamples{0};
    std::thread audio([&] {
        // Rounds play in order through one voice, so the sounding values only
        // grow; a value below one already heard came from samples let go.
        std::int16_t highest = 0;
        std::vector<std::int16_t> out(std::size_t{64} * kChannels);
        while (!done.load(std::memory_order_acquire)) {
            rig.mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), kAnchor);
            for (const std::int16_t v : out) {
                if (v == 0) continue;
                if (v < 100 || v >= 100 + kRounds || v < highest) badSamples.fetch_add(1, std::memory_order_relaxed);
                highest = std::max(highest, v);
            }
        }
    });
    for (int round = 0; round < kRounds; ++round) {
        PlayRequest request{Constant(200, static_cast<std::int16_t>(100 + round)), kAnchor};
        SoundHandle sound = rig.submitter.Play(request);
        while (!sound) {
            std::this_thread::yield();
            sound = rig.submitter.Play(request);
        }
        if (round % 3 == 0) {
            while (!rig.submitter.Stop(sound, kAnchor)) std::this_thread::yield();
        }
    } // each request, the last holder outside the mixer, goes here
    done.store(true, std::memory_order_release);
    audio.join();
    static_cast<void>(RenderBlock(rig.mixer, 64)); // take what is left (this thread renders now)
    CHECK(badSamples.load() == 0);
    CheckBalance(rig.mixer.Stats());
}
