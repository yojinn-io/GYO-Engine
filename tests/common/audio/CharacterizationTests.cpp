#include "doctest/doctest.h"

#include <chrono>
#include <cstdint>
#include <vector>

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Mixer.hpp"

using namespace Engine::Audio;
using Engine::Time::Duration;
using Engine::Time::TimePoint;

namespace {

// A characterization of the mixer as a one-shot consumer uses it (several clips,
// two producers, plays across anchors, late plays, a full queue, no free voice,
// saturation and a master gain change), recorded on the code before the control
// layer rebuild (v8-engine-audio-control-baseline). The rebuild must keep it
// bit-identical. If it differs, stop and report (AGENTS §14); never re-record it.
constexpr std::uint64_t kRecordedHash = 12184034990701221815ULL;

Duration Ns(const std::int64_t ns) { return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(ns)); }

std::vector<std::int16_t> MakeClip(const std::uint32_t frames, const std::uint16_t channels, const int seed) {
    std::vector<std::int16_t> samples(std::size_t{frames} * channels);
    std::uint32_t state = 2463534242u + static_cast<std::uint32_t>(seed);
    for (auto& sample : samples) {
        state ^= state << 13; // xorshift32: deterministic and platform-independent
        state ^= state >> 17;
        state ^= state << 5;
        sample = static_cast<std::int16_t>(static_cast<std::int32_t>(state & 0xFFFF) - 32768);
    }
    return samples;
}

struct Fnv1a final {
    std::uint64_t value = 1469598103934665603ULL;
    void Add(const std::uint64_t word) {
        for (int i = 0; i < 8; ++i) {
            value ^= (word >> (i * 8)) & 0xFF;
            value *= 1099511628211ULL;
        }
    }
};

} // namespace

TEST_CASE("Audio Mixer renders a recorded one-shot sequence bit-identically") {
    ClipBank bank;
    const auto shortMono = MakeClip(37, 1, 1);
    const auto stereo = MakeClip(500, 2, 2);
    const auto longMono = MakeClip(2000, 1, 3);
    auto a = bank.Add(Engine::Base::ConstSpan<std::int16_t>(shortMono.data(), shortMono.size()), 1, kSampleRate);
    auto b = bank.Add(Engine::Base::ConstSpan<std::int16_t>(stereo.data(), stereo.size()), 2, kSampleRate);
    auto c = bank.Add(Engine::Base::ConstSpan<std::int16_t>(longMono.data(), longMono.size()), 1, kSampleRate);
    REQUIRE((a && b && c));
    const ClipId clips[] = {*a, *b, *c};

    MixerConfig config; // the default 60 ms delay
    config.maxVoices = 6;
    config.maxSubmitters = 2;
    config.queueCapacity = 8;
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto first = mixer->CreateSubmitter();
    auto second = mixer->CreateSubmitter();
    REQUIRE((first && second));

    const TimePoint start = TimePoint{} + Ns(5'000'000'000);
    const std::uint32_t blockFrames[] = {480, 441, 1024, 7, 256, 300};
    const float gains[] = {0.37f, 1.5f, 0.8f, 1.0f, 2.0f, 0.05f};
    Fnv1a hash;
    TimePoint anchor = start;
    std::int64_t frames = 0;
    for (int block = 0; block < 60; ++block) {
        // Plays: some in the past (late), some ahead, a burst that fills the
        // queue every 10th block, and loud stacks that saturate.
        const int plays = (block % 10 == 3) ? 20 : (block % 3);
        for (int p = 0; p < plays; ++p) {
            const int k = block * 7 + p;
            const std::int64_t offsetNs = (k % 5 == 0) ? -80'000'000 : (k % 4) * 3'333'333 + p * 1'000'001;
            const PlayRequest request{clips[k % 3], anchor + Ns(offsetNs), gains[k % 6], gains[(k + 2) % 6]};
            static_cast<void>(((k % 2 == 0) ? *first : *second).Play(request));
        }
        if (block == 30) mixer->SetMasterGain(0.9f);
        const std::uint32_t n = blockFrames[block % 6];
        std::vector<std::int16_t> out(std::size_t{n} * kChannels, 0x5555);
        mixer->Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
        for (const std::int16_t sample : out) hash.Add(static_cast<std::uint16_t>(sample));
        frames += n;
        // The device runs 0.03% fast against the Engine clock, with jitter.
        anchor = start + Ns(frames * 1'000'000'000 / 48000 * 10003 / 10000 + (block % 4) * 250'000);
    }

    const MixerStats stats = mixer->Stats();
    hash.Add(stats.submitted);
    hash.Add(stats.playedOnTime);
    hash.Add(stats.playedLate);
    hash.Add(stats.droppedQueueFull);
    hash.Add(stats.droppedNoVoice);
    hash.Add(static_cast<std::uint64_t>(stats.maxLateness.count()));
    hash.Add(stats.renderedFrames);
    hash.Add(stats.clippedSamples);

    // The sequence reaches every outcome, so the hash covers each of them.
    CHECK(stats.playedOnTime > 0);
    CHECK(stats.playedLate > 0);
    CHECK(stats.droppedQueueFull > 0);
    CHECK(stats.droppedNoVoice > 0);
    CHECK(stats.clippedSamples > 0);
    CHECK(stats.submitted ==
          stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
    INFO("hash=" << hash.value);
    CHECK(hash.value == kRecordedHash);
}
