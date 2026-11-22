#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <utility>
#include <vector>

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Clip.hpp"
#include "engine/audio/Mixer.hpp"

// This binary replaces the global allocation functions to count what the
// calling thread allocates and frees while counting is on (Design Invariant 7:
// Render allocates and frees nothing). It is its own executable because the
// replacement applies to the whole program.

namespace {

thread_local bool tCounting = false;
thread_local std::uint64_t tAllocations = 0;
thread_local std::uint64_t tFrees = 0;

void* Allocate(const std::size_t size) {
    if (tCounting) ++tAllocations;
    if (void* p = std::malloc(size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}

void Free(void* p) noexcept {
    if (p == nullptr) return;
    if (tCounting) ++tFrees;
    std::free(p);
}

struct Counted final {
    std::uint64_t allocations = 0;
    std::uint64_t frees = 0;
};

template <typename F>
Counted Count(F&& body) {
    tAllocations = 0;
    tFrees = 0;
    tCounting = true;
    body();
    tCounting = false;
    return Counted{tAllocations, tFrees};
}

} // namespace

void* operator new(const std::size_t size) { return Allocate(size); }
void* operator new[](const std::size_t size) { return Allocate(size); }
void operator delete(void* p) noexcept { Free(p); }
void operator delete[](void* p) noexcept { Free(p); }
void operator delete(void* p, std::size_t) noexcept { Free(p); }
void operator delete[](void* p, std::size_t) noexcept { Free(p); }

using namespace GYO::Engine::Audio;
using GYO::Engine::Time::Duration;
using GYO::Engine::Time::TimePoint;

TEST_CASE("Audio allocation guard counts what the thread allocates and frees") {
    const Counted counted = Count([] {
        std::vector<int> temporary(4);
        static_cast<void>(temporary);
    });
    CHECK(counted.allocations == 1);
    CHECK(counted.frees == 1);
}

TEST_CASE("Audio Mixer Render allocates and frees nothing") {
    const std::vector<std::int16_t> samples(48000, 20000); // 24000 stereo frames, longer than the test
    auto clip = Clip::Make(GYO::Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 2, kSampleRate);
    REQUIRE(clip.has_value());
    MixerConfig config;
    config.maxVoices = 3;
    config.queueCapacity = 16;
    auto mixer = Mixer::Create(config);
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());

    const TimePoint anchor = TimePoint{} + std::chrono::duration_cast<Duration>(std::chrono::seconds(5));
    std::vector<std::int16_t> out(std::size_t{1024} * kChannels);
    const auto render = [&] {
        mixer->Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    };
    // Plays on time, late, beyond the free voices, with saturation; then the
    // voices are stopped or end. Every Render path runs while counting is on.
    SoundHandle last;
    for (int i = 0; i < 6; ++i) {
        const auto offset = std::chrono::duration_cast<Duration>(std::chrono::milliseconds(i % 2 == 0 ? -100 : 5));
        last = submitter->Play({*clip, anchor + offset, 1.5f, 1.5f});
        REQUIRE(last);
    }
    // After two blocks, stops too: a stop and a late stop-all. Render and the
    // submitter's calls are counted apart (declaration 10.3): the test holds the
    // clip, so the calls let go of nothing either.
    bool stopsQueued = false;
    Counted counted;
    Counted calls;
    for (int block = 0; block < 8; ++block) {
        const Counted one = Count(render);
        counted.allocations += one.allocations;
        counted.frees += one.frees;
        if (block == 1) {
            calls = Count([&] {
                stopsQueued = submitter->Stop(last, anchor + std::chrono::duration_cast<Duration>(std::chrono::milliseconds(30)));
                stopsQueued = submitter->StopAll(anchor - std::chrono::duration_cast<Duration>(std::chrono::milliseconds(100))) &&
                              stopsQueued;
            });
        }
    }
    CHECK(stopsQueued);
    CHECK(calls.allocations == 0);
    CHECK(calls.frees == 0);
    const MixerStats stats = mixer->Stats();
    CHECK(stats.playedLate > 0);
    CHECK(stats.playedOnTime > 0);
    CHECK(stats.droppedNoVoice > 0);
    CHECK(stats.clippedSamples > 0);
    CHECK(stats.stopsMatched > 0);
    CHECK(stats.stopsUnmatched > 0); // the last play had no voice
    CHECK(stats.stopsLate > 0);
    CHECK(counted.allocations == 0);
    CHECK(counted.frees == 0);
}

namespace {

const TimePoint kAnchor = TimePoint{} + std::chrono::duration_cast<Duration>(std::chrono::seconds(5));

Duration Ms(const std::int64_t ms) { return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(ms)); }

Clip Constant(const std::uint32_t frames, const std::int16_t level) {
    auto clip = Clip::Make(std::vector<std::int16_t>(frames, level), 1, kSampleRate);
    REQUIRE(clip.has_value());
    return *clip;
}

// What letting go of the last reference to one clip frees on this thread.
std::uint64_t FreesOfOneClip() {
    Clip clip = Constant(10, 1);
    const Counted counted = Count([&] { clip = Clip{}; });
    REQUIRE(counted.frees > 0);
    return counted.frees;
}

struct Rig final {
    Mixer mixer;
    Submitter submitter;
};

Rig MakeRig(const std::uint32_t voices, const std::uint32_t capacity, const std::uint32_t submitters = 1) {
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    config.maxVoices = voices;
    config.queueCapacity = capacity;
    config.maxSubmitters = submitters;
    auto mixer = Mixer::Create(config);
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    return Rig{std::move(mixer).value(), std::move(submitter).value()};
}

// Renders one block of 480 frames while counting; returns its left channel.
std::vector<std::int16_t> RenderCounted(Mixer& mixer, Counted& counted, const TimePoint anchor = kAnchor) {
    std::vector<std::int16_t> out(std::size_t{480} * kChannels);
    std::vector<std::int16_t> left(480);
    counted = Count([&] { mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor); });
    for (std::size_t f = 0; f < 480; ++f) left[f] = out[f * 2];
    return left;
}

} // namespace

TEST_CASE("Audio Mixer lets go of a clip on the submitter's next call after its play ended, never in Render") {
    // Declaration 10.3. The caller lets go of the clip while its play is queued
    // or sounding: (a) a call before the play ends lets go of nothing and the
    // output stays exactly the clip's samples; (b) the first call after it ended
    // lets go of exactly one clip. Every Render allocates and frees nothing.
    const std::uint64_t one = FreesOfOneClip();
    constexpr std::int16_t kLevel = 300;
    enum class Outcome { OnTime, Late, NoVoice, StopBeforeStart, StopLater, LateStop };
    for (const Outcome outcome : {Outcome::OnTime, Outcome::Late, Outcome::NoVoice, Outcome::StopBeforeStart,
                                  Outcome::StopLater, Outcome::LateStop}) {
        CAPTURE(static_cast<int>(outcome));
        Rig rig = MakeRig(1, 4);
        const Clip busy = Constant(48000, 7); // the test holds it: occupies the voice for NoVoice
        SoundHandle sound;
        {
            Clip clip = Constant(1440, kLevel); // 3 blocks
            if (outcome == Outcome::NoVoice) {
                REQUIRE(rig.submitter.Play({busy, kAnchor}));
            }
            const TimePoint at = outcome == Outcome::Late ? kAnchor - Ms(1) : kAnchor;
            sound = rig.submitter.Play({std::move(clip), at});
            REQUIRE(sound);
            if (outcome == Outcome::StopBeforeStart) REQUIRE(rig.submitter.Stop(sound, kAnchor - Ms(1)));
        } // the caller holds no reference any more
        Counted counted;
        auto left = RenderCounted(rig.mixer, counted);
        CHECK(counted.allocations == 0);
        CHECK(counted.frees == 0);
        const bool sounding = outcome == Outcome::OnTime || outcome == Outcome::Late || outcome == Outcome::StopLater ||
                              outcome == Outcome::LateStop;
        if (sounding) {
            CHECK(std::count(left.begin(), left.end(), kLevel) == 480);
            // (a) a call while it sounds lets go of nothing
            const Counted early = Count([&] { CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor)); });
            CHECK(early.frees == 0);
            // A stop at frame 1200, taken in the next block (anchor 10 ms on: 15 ms = 720 frames after frame 480).
            if (outcome == Outcome::StopLater) REQUIRE(rig.submitter.Stop(sound, kAnchor + Ms(25)));
            if (outcome == Outcome::LateStop) REQUIRE(rig.submitter.Stop(sound, kAnchor - Ms(1)));
            // Each block reads the time base 10 ms (480 frames) later.
            left = RenderCounted(rig.mixer, counted, kAnchor + Ms(10));
            CHECK(counted.allocations == 0);
            CHECK(counted.frees == 0);
            const auto expected = outcome == Outcome::LateStop ? 0 : 480;
            CHECK(std::count(left.begin(), left.end(), kLevel) == expected);
            if (outcome != Outcome::LateStop) {
                // (a) again while it still sounds (also after a stop at a later frame was taken)
                const Counted again = Count([&] { CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor)); });
                CHECK(again.frees == 0);
            }
            left = RenderCounted(rig.mixer, counted, kAnchor + Ms(20));
            CHECK(counted.allocations == 0);
            CHECK(counted.frees == 0);
            const auto third = outcome == Outcome::LateStop ? 0 : outcome == Outcome::StopLater ? 240 : 480;
            CHECK(std::count(left.begin(), left.end(), kLevel) == third);
            static_cast<void>(RenderCounted(rig.mixer, counted, kAnchor + Ms(30))); // past the end
            CHECK(counted.frees == 0);
        } else {
            CHECK(std::count(left.begin(), left.end(), kLevel) == 0);
        }
        // (b) the next call lets go of exactly that clip
        const Counted next = Count([&] { CHECK(rig.submitter.StopAll(kAnchor + Ms(1000))); });
        CHECK(next.allocations == 0);
        CHECK(next.frees == one);
    }
}

TEST_CASE("Audio Submitter lets go of the clips handed back on each of its calls") {
    // Declaration 10.3 (b): Play, Stop with a valid handle, Stop with an invalid
    // one, StopAll and the destructor each first let go of what came back.
    const std::uint64_t one = FreesOfOneClip();
    for (int kind = 0; kind < 5; ++kind) {
        CAPTURE(kind);
        Rig rig = MakeRig(2, 4);
        const Clip other = Constant(4800, 2);
        const SoundHandle otherSound = rig.submitter.Play({other, kAnchor + Ms(500)});
        REQUIRE(otherSound);
        REQUIRE(rig.submitter.Play({Constant(10, 1), kAnchor}));
        Counted counted;
        static_cast<void>(RenderCounted(rig.mixer, counted)); // the short play ends; its entry comes back
        CHECK(counted.frees == 0);
        Counted call;
        switch (kind) {
        case 0: call = Count([&] { CHECK(rig.submitter.Play({other, kAnchor + Ms(500)})); }); break;
        case 1: call = Count([&] { CHECK(rig.submitter.Stop(otherSound, kAnchor + Ms(600))); }); break;
        case 2: call = Count([&] { CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor)); }); break;
        case 3: call = Count([&] { CHECK(rig.submitter.StopAll(kAnchor + Ms(600))); }); break;
        default: {
            Submitter moved(std::move(rig.submitter));
            call = Count([&] { Submitter gone(std::move(moved)); });
        }
        }
        CHECK(call.allocations == 0);
        CHECK(call.frees == one);
    }
}

TEST_CASE("Audio Mixer hands back every voice that ends in one block") {
    // Two voices end in the same Render; the next call lets go of both clips.
    const std::uint64_t one = FreesOfOneClip();
    Rig rig = MakeRig(2, 4);
    REQUIRE(rig.submitter.Play({Constant(10, 1), kAnchor}));
    REQUIRE(rig.submitter.Play({Constant(20, 2), kAnchor}));
    Counted counted;
    static_cast<void>(RenderCounted(rig.mixer, counted));
    CHECK(counted.frees == 0);
    const Counted call = Count([&] { CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor)); });
    CHECK(call.frees == 2 * one);
}

TEST_CASE("Audio Mixer does not hand a reused entry back early") {
    // Declaration 10.3 (a) with an entry reused: the second play takes the
    // entry the first play gave back (the free entries are a stack); nothing
    // about the first play may bring that entry back again while the second
    // still sounds.
    const std::uint64_t one = FreesOfOneClip();
    Rig rig = MakeRig(1, 1);
    REQUIRE(rig.submitter.Play({Constant(10, 1), kAnchor}));
    Counted counted;
    static_cast<void>(RenderCounted(rig.mixer, counted)); // the first play ends; its entry comes back
    Clip second = Constant(1440, 300);                    // 3 blocks
    const Counted take = Count([&] { CHECK(rig.submitter.Play(PlayRequest{std::move(second), kAnchor + Ms(10)})); });
    CHECK(take.frees == one); // the first play's clip, taken back first
    for (int block = 1; block <= 2; ++block) {
        CAPTURE(block);
        const auto left = RenderCounted(rig.mixer, counted, kAnchor + Ms(10 * block));
        CHECK(counted.frees == 0);
        CHECK(std::count(left.begin(), left.end(), std::int16_t{300}) == 480);
        const Counted early = Count([&] { CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor)); });
        CHECK(early.frees == 0);
    }
}

TEST_CASE("Audio Submitter lets go of a play the full queue dropped in that call, and keeps every entry") {
    // Declaration 10.3: the request is the last holder; the dropped play gives
    // its entry back at once, so more drops than entries leave the table whole.
    const std::uint64_t one = FreesOfOneClip();
    Rig rig = MakeRig(1, 1); // 3 entries
    const Clip held = Constant(4800, 2);
    REQUIRE(rig.submitter.Play({held, kAnchor})); // fills the queue
    SoundHandle dropped{1};
    Clip clip = Constant(10, 1);
    const Counted call = Count([&] { dropped = rig.submitter.Play(PlayRequest{std::move(clip), kAnchor}); });
    CHECK_FALSE(dropped);
    CHECK(call.allocations == 0);
    CHECK(call.frees == one); // the request was the last holder: let go in that call
    for (int i = 0; i < 10; ++i) CHECK_FALSE(rig.submitter.Play({held, kAnchor}));
    Counted counted;
    static_cast<void>(RenderCounted(rig.mixer, counted));
    CHECK(rig.submitter.Play({held, kAnchor}));
    CHECK(rig.mixer.Stats().droppedQueueFull == 11);
}

TEST_CASE("Audio Mixer takes its table and both rings around twice") {
    // Declaration 10.4: one entry in the queue, one voice, three entries; 12
    // rounds take the table and both rings around more than twice. Rounds take
    // back only through Play, and every third round's play gets no voice.
    // Clips are made outside the counts, so a count's frees are exactly the
    // clips let go there.
    const std::uint64_t one = FreesOfOneClip();
    Rig rig = MakeRig(1, 1);
    const Clip busy = Constant(48000, 7); // the test holds it
    Counted counted;
    bool pending = false; // a clip the caller let go of has come back and waits for the next call
    for (int round = 0; round < 12; ++round) {
        CAPTURE(round);
        const bool noVoice = round % 3 == 2;
        if (noVoice) {
            const Counted call = Count([&] { CHECK(rig.submitter.Play({busy, kAnchor})); });
            CHECK(call.frees == (pending ? one : 0));
            pending = false;
            static_cast<void>(RenderCounted(rig.mixer, counted)); // busy takes the voice
            CHECK(counted.frees == 0);
        }
        Clip clip = Constant(10, 1);
        const Counted call = Count([&] { CHECK(rig.submitter.Play(PlayRequest{std::move(clip), kAnchor})); });
        CHECK(call.allocations == 0);
        CHECK(call.frees == (pending ? one : 0));
        pending = false;
        static_cast<void>(RenderCounted(rig.mixer, counted)); // it ends, or gets no voice
        CHECK(counted.allocations == 0);
        CHECK(counted.frees == 0);
        pending = true;
        if (noVoice) {
            const Counted stop = Count([&] { CHECK(rig.submitter.StopAll(kAnchor)); });
            CHECK(stop.frees == one);
            pending = false;
            static_cast<void>(RenderCounted(rig.mixer, counted)); // busy stops; the test still holds it
            CHECK(counted.frees == 0);
        }
    }
    const auto stats = rig.mixer.Stats();
    CHECK(stats.droppedQueueFull == 0);
    CHECK(stats.droppedNoVoice == 4);
    CHECK(stats.submitted == 16);
    const Counted last = Count([&] { CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor)); });
    CHECK(last.frees == (pending ? one : 0));
}

TEST_CASE("Audio Mixer hands an entry above 65535 back to its own submitter") {
    // Declaration 10.4: the lease mark keeps the table entry in 32 bits, so an
    // entry above 65535 does not spill into the submitter's bits.
    const std::uint64_t one = FreesOfOneClip();
    Rig rig = MakeRig(1, 65536, 2);
    auto second = rig.mixer.CreateSubmitter();
    REQUIRE(second.has_value());
    const Clip held = Constant(48000, 2);
    REQUIRE(rig.submitter.Play({held, kAnchor})); // entry 0
    Counted counted;
    static_cast<void>(RenderCounted(rig.mixer, counted)); // it takes the voice
    for (int i = 1; i < 65536; ++i) REQUIRE(rig.submitter.Play({held, kAnchor})); // entries 1 to 65535
    REQUIRE(rig.submitter.Play({Constant(10, 1), kAnchor}));                     // entry 65536, its own clip
    static_cast<void>(RenderCounted(rig.mixer, counted)); // every queued play gets no voice
    CHECK(counted.frees == 0);
    const Counted call = Count([&] { CHECK(rig.submitter.Stop(SoundHandle{}, kAnchor)); });
    CHECK(call.frees == one);
    const Counted otherCall = Count([&] { CHECK(second->Stop(SoundHandle{}, kAnchor)); });
    CHECK(otherCall.frees == 0);
}
