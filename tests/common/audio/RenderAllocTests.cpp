#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <vector>

#include "engine/audio/AudioFormat.hpp"
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

using namespace Engine::Audio;
using Engine::Time::Duration;
using Engine::Time::TimePoint;

TEST_CASE("Audio allocation guard counts what the thread allocates and frees") {
    const Counted counted = Count([] {
        std::vector<int> temporary(4);
        static_cast<void>(temporary);
    });
    CHECK(counted.allocations == 1);
    CHECK(counted.frees == 1);
}

TEST_CASE("Audio Mixer Render allocates and frees nothing") {
    ClipBank bank;
    const std::vector<std::int16_t> samples(48000, 20000); // 24000 stereo frames, longer than the test
    auto clip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 2, kSampleRate);
    REQUIRE(clip.has_value());
    MixerConfig config;
    config.maxVoices = 3;
    config.queueCapacity = 16;
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());

    const TimePoint anchor = TimePoint{} + std::chrono::duration_cast<Duration>(std::chrono::seconds(5));
    std::vector<std::int16_t> out(std::size_t{1024} * kChannels);
    const auto render = [&] {
        mixer->Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    };
    // Plays on time, late, beyond the free voices, with saturation; then the
    // voices are stopped or end. Every Render path runs while counting is on.
    SoundHandle last;
    for (int i = 0; i < 6; ++i) {
        const auto offset = std::chrono::duration_cast<Duration>(std::chrono::milliseconds(i % 2 == 0 ? -100 : 5));
        last = submitter->Play({*clip, anchor + offset, 1.5f, 1.5f});
        REQUIRE(last);
    }
    // After two blocks, stops too: a stop and a late stop-all. Queueing them
    // allocates nothing either, so they are sent while counting.
    bool stopsQueued = false;
    const Counted counted = Count([&] {
        for (int block = 0; block < 8; ++block) {
            render();
            if (block == 1) {
                stopsQueued = submitter->Stop(last, anchor + std::chrono::duration_cast<Duration>(std::chrono::milliseconds(30)));
                stopsQueued = submitter->StopAll(anchor - std::chrono::duration_cast<Duration>(std::chrono::milliseconds(100))) &&
                              stopsQueued;
            }
        }
    });
    CHECK(stopsQueued);
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
