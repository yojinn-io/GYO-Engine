#include "doctest/doctest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <set>
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

const TimePoint kAnchor = TimePoint{} + std::chrono::duration_cast<Duration>(10s);

Duration Ms(const std::int64_t ms) { return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(ms)); }

struct Rig final {
    Mixer mixer;
    Submitter submitter;
    ClipId bankLong; // a bank clip of 4800 frames of 7
};

Rig MakeRig(const std::uint32_t slots, const std::uint32_t voices = 8, const std::uint32_t capacity = 64) {
    ClipBank bank;
    const std::vector<std::int16_t> samples(4800, 7);
    auto clip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 1, kSampleRate);
    REQUIRE(clip.has_value());
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    config.maxVoices = voices;
    config.queueCapacity = capacity;
    config.clipSlotsPerSubmitter = slots;
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    return Rig{std::move(mixer).value(), std::move(submitter).value(), *clip};
}

Engine::Base::Result<ClipId, AudioError> Load(Submitter& submitter, const std::uint32_t frames, const std::int16_t level,
                                               const std::uint16_t channels = 1) {
    const std::vector<std::int16_t> samples(std::size_t{frames} * channels, level);
    return submitter.LoadClip(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), channels,
                              kSampleRate);
}

std::vector<std::int16_t> RenderBlock(Mixer& mixer, const std::uint32_t frames, const TimePoint anchor = kAnchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * kChannels, 0x5555);
    mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    return out;
}

std::size_t CountLeft(const std::vector<std::int16_t>& out, const std::int16_t value) {
    std::size_t count = 0;
    for (std::size_t f = 0; f * 2 < out.size(); ++f) count += out[f * 2] == value ? 1 : 0;
    return count;
}

void CheckUnloadBalance(const MixerStats& stats) {
    CHECK(stats.unloadsSubmitted == stats.unloadsTaken + stats.unloadsDroppedQueueFull);
    CHECK(stats.submitted == stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
}

} // namespace

TEST_CASE("Audio Mixer plays a clip loaded while output runs") {
    Rig rig = MakeRig(2);
    static_cast<void>(RenderBlock(rig.mixer, 480)); // output is running
    auto mono = Load(rig.submitter, 100, 300);
    REQUIRE(mono.has_value());
    const std::vector<std::int16_t> stereo{11, -22, 11, -22, 11, -22};
    auto both = rig.submitter.LoadClip(Engine::Base::ConstSpan<std::int16_t>(stereo.data(), stereo.size()), 2,
                                       kSampleRate);
    REQUIRE(both.has_value());
    CHECK(*mono != *both);
    REQUIRE(rig.submitter.Play({*mono, kAnchor + Ms(10)}));
    REQUIRE(rig.submitter.Play({*both, kAnchor + Ms(15)}));
    const auto out = RenderBlock(rig.mixer, 960, kAnchor + Ms(10));
    CHECK(CountLeft(out, 300) == 100);
    CHECK(out[240 * 2] == 11);
    CHECK(out[240 * 2 + 1] == -22);
    CHECK(out[243 * 2] == 0);
    // Both voices ended in that Render: both slots come back.
    REQUIRE(rig.submitter.UnloadClip(*mono));
    REQUIRE(rig.submitter.UnloadClip(*both));
    static_cast<void>(RenderBlock(rig.mixer, 48, kAnchor + Ms(30)));
    CHECK(Load(rig.submitter, 10, 1).has_value());
    CHECK(Load(rig.submitter, 10, 1).has_value());
}

TEST_CASE("Audio Mixer keeps a clip until the last of its voices ends") {
    Rig rig = MakeRig(1, 4);
    auto clip = Load(rig.submitter, 4800, 300);
    REQUIRE(clip.has_value());
    const SoundHandle first = rig.submitter.Play({*clip, kAnchor});
    const SoundHandle second = rig.submitter.Play({*clip, kAnchor + Ms(10)});
    REQUIRE((first && second));
    REQUIRE(rig.submitter.UnloadClip(*clip));
    REQUIRE(rig.submitter.Stop(first, kAnchor + Ms(5)));
    static_cast<void>(RenderBlock(rig.mixer, 480)); // the first voice ends, the second has not started
    CHECK_FALSE(Load(rig.submitter, 10, 1).has_value());
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10))); // the second plays
    CHECK_FALSE(Load(rig.submitter, 10, 1).has_value());
    for (int block = 2; block < 12; ++block) static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10 * block)));
    CHECK(Load(rig.submitter, 10, 1).has_value()); // the second ended at frame 5280
}

TEST_CASE("Audio Mixer releases the lease of every voice a block frees") {
    // One voice, reused within one block: each stop frees it before the next
    // play takes it, so every freed voice must be released before the next.
    Rig rig = MakeRig(1, 1);
    auto clip = Load(rig.submitter, 4800, 300);
    REQUIRE(clip.has_value());
    const SoundHandle a = rig.submitter.Play({*clip, kAnchor + Ms(20)});
    REQUIRE(rig.submitter.Stop(a, kAnchor + Ms(10)));
    const SoundHandle b = rig.submitter.Play({*clip, kAnchor + Ms(20)});
    REQUIRE(rig.submitter.Stop(b, kAnchor + Ms(10)));
    REQUIRE(rig.submitter.UnloadClip(*clip));
    CHECK(CountLeft(RenderBlock(rig.mixer, 480), 300) == 0);
    CHECK(rig.mixer.Stats().droppedNoVoice == 0);
    CHECK(Load(rig.submitter, 10, 1).has_value());
}

TEST_CASE("Audio Submitter LoadClip rejects what ClipBank rejects and reports a missing slot") {
    Rig rig = MakeRig(1);
    const std::vector<std::int16_t> three{1, 2, 3};
    const auto span = Engine::Base::ConstSpan<std::int16_t>(three.data(), three.size());
    const auto expect = [](const Engine::Base::Result<ClipId, AudioError>& result, const AudioErrorCode code) {
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == code);
    };
    expect(rig.submitter.LoadClip(span, 3, kSampleRate), AudioErrorCode::InvalidClip);
    expect(rig.submitter.LoadClip(span, 2, kSampleRate), AudioErrorCode::InvalidClip);
    expect(rig.submitter.LoadClip(span, 1, 44100), AudioErrorCode::InvalidClip);
    expect(rig.submitter.LoadClip(Engine::Base::ConstSpan<std::int16_t>(), 1, kSampleRate),
           AudioErrorCode::InvalidClip);
    REQUIRE(rig.submitter.LoadClip(span, 1, kSampleRate).has_value());
    expect(rig.submitter.LoadClip(span, 1, kSampleRate), AudioErrorCode::NoClipSlot);

    Rig none = MakeRig(0); // the default: no slots
    expect(none.submitter.LoadClip(span, 1, kSampleRate), AudioErrorCode::NoClipSlot);
}

TEST_CASE("Audio Mixer does not hand back a clip while a voice reads it") {
    Rig rig = MakeRig(1);
    auto clip = Load(rig.submitter, 960, 300);
    REQUIRE(clip.has_value());
    REQUIRE(rig.submitter.Play({*clip, kAnchor}));
    REQUIRE(rig.submitter.UnloadClip(*clip));
    CHECK(CountLeft(RenderBlock(rig.mixer, 480), 300) == 480);
    auto early = Load(rig.submitter, 10, 1);
    REQUIRE_FALSE(early.has_value());
    CHECK(early.error().code == AudioErrorCode::NoClipSlot);
    CHECK(CountLeft(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)), 300) == 480); // the clip plays to its end
    CHECK(Load(rig.submitter, 10, 1).has_value());
    CheckUnloadBalance(rig.mixer.Stats());
}

TEST_CASE("Audio Mixer plays a queued play of an unloaded clip fully") {
    Rig rig = MakeRig(1);
    auto clip = Load(rig.submitter, 200, 300);
    REQUIRE(clip.has_value());
    REQUIRE(rig.submitter.Play({*clip, kAnchor + Ms(5)}));
    REQUIRE(rig.submitter.UnloadClip(*clip)); // queued behind the play
    CHECK(CountLeft(RenderBlock(rig.mixer, 960), 300) == 200);
    CHECK(Load(rig.submitter, 10, 1).has_value());
}

TEST_CASE("Audio Mixer hands back a stopped clip once its voice ends") {
    SUBCASE("at its stop frame") {
        Rig rig = MakeRig(1);
        auto clip = Load(rig.submitter, 4800, 300);
        REQUIRE(clip.has_value());
        const SoundHandle sound = rig.submitter.Play({*clip, kAnchor});
        REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(10)));
        REQUIRE(rig.submitter.UnloadClip(*clip));
        CHECK(CountLeft(RenderBlock(rig.mixer, 480), 300) == 480);
        CHECK(Load(rig.submitter, 10, 1).has_value());
    }
    SUBCASE("stopped before it started") {
        Rig rig = MakeRig(1);
        auto clip = Load(rig.submitter, 4800, 300);
        REQUIRE(clip.has_value());
        const SoundHandle sound = rig.submitter.Play({*clip, kAnchor + Ms(20)});
        REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(10)));
        REQUIRE(rig.submitter.UnloadClip(*clip));
        CHECK(CountLeft(RenderBlock(rig.mixer, 480), 300) == 0);
        CHECK(Load(rig.submitter, 10, 1).has_value());
    }
    SUBCASE("stopped late after it started") {
        Rig rig = MakeRig(1);
        auto clip = Load(rig.submitter, 4800, 300);
        REQUIRE(clip.has_value());
        const SoundHandle sound = rig.submitter.Play({*clip, kAnchor});
        static_cast<void>(RenderBlock(rig.mixer, 480));
        REQUIRE(rig.submitter.Stop(sound, kAnchor));
        REQUIRE(rig.submitter.UnloadClip(*clip));
        CHECK(CountLeft(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)), 300) == 0);
        CHECK(Load(rig.submitter, 10, 1).has_value());
    }
}

TEST_CASE("Audio Mixer gives a play dropped for want of a voice no lease") {
    Rig rig = MakeRig(1, 1);
    REQUIRE(rig.submitter.Play({rig.bankLong, kAnchor})); // takes the only voice
    auto clip = Load(rig.submitter, 100, 300);
    REQUIRE(clip.has_value());
    REQUIRE(rig.submitter.Play({*clip, kAnchor}));
    REQUIRE(rig.submitter.UnloadClip(*clip));
    static_cast<void>(RenderBlock(rig.mixer, 480));
    CHECK(rig.mixer.Stats().droppedNoVoice == 1);
    CHECK(Load(rig.submitter, 10, 1).has_value()); // no voice ever read it
}

TEST_CASE("Audio Mixer does not hand back a reused slot early") {
    // After a slot's first clip was unloaded and handed back, a voice of the
    // next clip in that slot ends while that clip is still loaded: the slot
    // must stay loaded (the lease is reset when it is handed back).
    Rig rig = MakeRig(1);
    auto first = Load(rig.submitter, 100, 300);
    REQUIRE(first.has_value());
    REQUIRE(rig.submitter.Play({*first, kAnchor}));
    REQUIRE(rig.submitter.UnloadClip(*first));
    static_cast<void>(RenderBlock(rig.mixer, 480));
    auto second = Load(rig.submitter, 100, 500);
    REQUIRE(second.has_value());
    REQUIRE(rig.submitter.Play({*second, kAnchor + Ms(10)}));
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10))); // its voice ends
    auto third = Load(rig.submitter, 10, 1);
    REQUIRE_FALSE(third.has_value());
    CHECK(third.error().code == AudioErrorCode::NoClipSlot);
    // Unloading it now must not let the next load free it before the audio
    // thread has taken the unload and the queued play has ended.
    REQUIRE(rig.submitter.Play({*second, kAnchor + Ms(20)}));
    REQUIRE(rig.submitter.UnloadClip(*second));
    auto fourth = Load(rig.submitter, 10, 1);
    REQUIRE_FALSE(fourth.has_value());
    CHECK(CountLeft(RenderBlock(rig.mixer, 480, kAnchor + Ms(20)), 500) == 100);
    CHECK(Load(rig.submitter, 10, 1).has_value());
}

TEST_CASE("Audio Submitter may unload again after its queue was full") {
    Rig rig = MakeRig(1, 8, 1);
    auto clip = Load(rig.submitter, 100, 300);
    REQUIRE(clip.has_value());
    REQUIRE(rig.submitter.Play({*clip, kAnchor}));
    CHECK_FALSE(rig.submitter.UnloadClip(*clip));
    static_cast<void>(RenderBlock(rig.mixer, 480));
    CHECK(rig.submitter.Play({*clip, kAnchor + Ms(10)})); // still loaded
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)));
    REQUIRE(rig.submitter.UnloadClip(*clip));
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(20)));
    const MixerStats stats = rig.mixer.Stats();
    CHECK(stats.unloadsSubmitted == 2);
    CHECK(stats.unloadsDroppedQueueFull == 1);
    CHECK(stats.unloadsTaken == 1);
    CheckUnloadBalance(stats);
    CHECK(Load(rig.submitter, 10, 1).has_value());
}

TEST_CASE("Audio Mixer gives a reloaded slot a new ClipId") {
    Rig rig = MakeRig(1);
    std::set<std::uint64_t> seen;
    ClipId last{};
    for (int i = 0; i < 4; ++i) {
        auto clip = Load(rig.submitter, 10, 1);
        REQUIRE(clip.has_value());
        CHECK(seen.insert(clip->value).second);
        CHECK(clip->value > 0xFFFF'FFFFu); // above every bank id
        REQUIRE(rig.submitter.UnloadClip(*clip));
        static_cast<void>(RenderBlock(rig.mixer, 48));
        last = *clip;
    }
    auto current = Load(rig.submitter, 10, 1);
    REQUIRE(current.has_value());
    GYO_CHECK_ASSERTS(rig.submitter.Play({last, kAnchor})); // the old id answers no more
    CHECK(rig.submitter.Play({*current, kAnchor}));
}

TEST_CASE("Audio Submitter treats misused runtime clips as Programmer Errors") {
    ClipBank bank;
    const std::vector<std::int16_t> one{1};
    auto bankClip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(one.data(), one.size()), 1, kSampleRate);
    REQUIRE(bankClip.has_value());
    MixerConfig config;
    config.clipSlotsPerSubmitter = 2;
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto a = mixer->CreateSubmitter();
    auto b = mixer->CreateSubmitter();
    REQUIRE((a && b));
    auto clip = Load(*a, 10, 1);
    REQUIRE(clip.has_value());
    GYO_CHECK_ASSERTS(b->Play({*clip, kAnchor}));  // another submitter's clip
    GYO_CHECK_ASSERTS(b->UnloadClip(*clip));
    GYO_CHECK_ASSERTS(a->UnloadClip(*bankClip)); // a bank clip
    REQUIRE(a->UnloadClip(*clip));
    GYO_CHECK_ASSERTS(a->Play({*clip, kAnchor})); // unloaded
    GYO_CHECK_ASSERTS(a->UnloadClip(*clip));      // unloaded twice
    GYO_CHECK_ASSERTS(a->Play({ClipId{clip->value + 1}, kAnchor})); // a slot never loaded
}

TEST_CASE("Audio Submitter's clip calls belong to the thread that owns it") {
    Rig rig = MakeRig(2);
    std::thread other([&] { CHECK(Load(rig.submitter, 10, 1).has_value()); });
    other.join();
    GYO_CHECK_ASSERTS(Load(rig.submitter, 10, 1));

    Rig owned = MakeRig(2);
    auto clip = Load(owned.submitter, 10, 1);
    REQUIRE(clip.has_value());
    std::thread intruder([&] { GYO_CHECK_ASSERTS(owned.submitter.UnloadClip(*clip)); });
    intruder.join();
    CHECK(owned.mixer.Stats().unloadsSubmitted == 0);
}

TEST_CASE("Audio Mixer rejects more than 256 clip slots per submitter") {
    MixerConfig config;
    config.clipSlotsPerSubmitter = 257;
    auto rejected = Mixer::Create(config, ClipBank{});
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code == AudioErrorCode::InvalidConfig);
    config.clipSlotsPerSubmitter = 256;
    CHECK(Mixer::Create(config, ClipBank{}).has_value());
}

TEST_CASE("Audio Mixer loads, plays, stops and unloads while another thread renders") {
    // Structural stress for ThreadSanitizer and AddressSanitizer (declared: 200
    // rounds, run 3 times; no time bound). One voice, so every rendered sample
    // is silence or exactly one generation's constant: a read of freed or
    // reused samples shows another value (or a sanitizer report).
    constexpr int kRounds = 200;
    Rig rig = MakeRig(4, 1, 16);
    std::atomic<bool> done{false};
    std::atomic<std::uint64_t> badSamples{0};
    std::thread audio([&] {
        // Rounds play in order through one voice, so the sounding values only
        // grow; a value below one already heard came from freed samples.
        std::int16_t highest = 0;
        std::vector<std::int16_t> out(std::size_t{64} * kChannels);
        while (!done.load(std::memory_order_acquire)) {
            rig.mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), kAnchor);
            for (std::size_t i = 0; i < out.size(); ++i) {
                const std::int16_t v = out[i];
                if (v == 0) continue;
                if (v < 100 || v >= 100 + kRounds || v < highest) badSamples.fetch_add(1, std::memory_order_relaxed);
                highest = std::max(highest, v);
            }
        }
    });
    int loaded = 0;
    for (int round = 0; round < kRounds; ++round) {
        auto clip = Load(rig.submitter, 200, static_cast<std::int16_t>(100 + round));
        while (!clip && clip.error().code == AudioErrorCode::NoClipSlot) {
            std::this_thread::yield();
            clip = Load(rig.submitter, 200, static_cast<std::int16_t>(100 + round));
        }
        if (!clip) break; // reported below, after the audio thread is joined
        ++loaded;
        const SoundHandle sound = rig.submitter.Play({*clip, kAnchor});
        if (round % 3 == 0) static_cast<void>(rig.submitter.Stop(sound, kAnchor));
        while (!rig.submitter.UnloadClip(*clip)) std::this_thread::yield();
    }
    done.store(true, std::memory_order_release);
    audio.join();
    static_cast<void>(RenderBlock(rig.mixer, 64)); // take what is left (this thread renders now)
    CHECK(loaded == kRounds);
    CHECK(badSamples.load() == 0);
    CheckUnloadBalance(rig.mixer.Stats());
}
