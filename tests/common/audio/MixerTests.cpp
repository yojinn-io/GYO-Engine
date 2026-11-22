#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "AssertTestSupport.hpp"
#include "VoiceMixer.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Clip.hpp"

using namespace GYO::Engine::Audio;
using GYO::Engine::Audio::Detail::ScheduledPlay;
using GYO::Engine::Audio::Detail::StartOutcome;
using GYO::Engine::Audio::Detail::VoiceMixer;

namespace {

// Makes a clip, keeps it in `bank` (which outlives the mixer) and returns the
// view a ScheduledPlay carries.
ClipView AddClip(std::vector<Clip>& bank, const std::vector<std::int16_t>& samples, const std::uint16_t channels) {
    auto clip = Clip::Make(GYO::Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), channels, kSampleRate);
    REQUIRE(clip.has_value());
    bank.push_back(*clip);
    return bank.back().View();
}

VoiceMixer MakeMixer(const std::uint32_t voices = 8) {
    auto mixer = VoiceMixer::Create(voices);
    REQUIRE(mixer.has_value());
    return std::move(mixer).value();
}

// Renders frames in blocks of the given sizes (cycled) and returns the output.
std::vector<std::int16_t> RenderInBlocks(VoiceMixer& mixer, const std::uint64_t frames,
                                         const std::vector<std::uint32_t>& blocks) {
    std::vector<std::int16_t> out(frames * kChannels, 0x7777);
    std::uint64_t done = 0;
    for (std::size_t i = 0; done < frames; ++i) {
        const std::uint64_t n = std::min<std::uint64_t>(blocks[i % blocks.size()], frames - done);
        mixer.Render(GYO::Engine::Base::Span<std::int16_t>(out.data() + done * kChannels, n * kChannels));
        done += n;
    }
    return out;
}

std::int16_t Left(const std::vector<std::int16_t>& out, const std::size_t frame) { return out[frame * 2]; }
std::int16_t Right(const std::vector<std::int16_t>& out, const std::size_t frame) { return out[frame * 2 + 1]; }

} // namespace

TEST_CASE("Audio Clip rejects clips the mixer cannot play") {
    const std::vector<std::int16_t> three{1, 2, 3};
    const GYO::Engine::Base::ConstSpan<std::int16_t> span(three.data(), three.size());
    const auto expectInvalid = [](const GYO::Engine::Base::Result<Clip, AudioError>& result) {
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().code == AudioErrorCode::InvalidClip);
    };
    expectInvalid(Clip::Make(span, 0, kSampleRate));
    expectInvalid(Clip::Make(span, 3, kSampleRate));
    expectInvalid(Clip::Make(span, 1, 44100));
    expectInvalid(Clip::Make(GYO::Engine::Base::ConstSpan<std::int16_t>(), 1, kSampleRate));
    expectInvalid(Clip::Make(span, 2, kSampleRate)); // 3 samples are not whole stereo frames
    // The same rules for samples taken without a copy.
    expectInvalid(Clip::Make(std::vector<std::int16_t>{1, 2, 3}, 1, 44100));
    expectInvalid(Clip::Make(std::vector<std::int16_t>{1, 2, 3}, 2, kSampleRate));
    expectInvalid(Clip::Make(std::vector<std::int16_t>{}, 1, kSampleRate));
}

TEST_CASE("Audio Clip copies samples given as a span once and shares them from then on") {
    std::vector<std::int16_t> mono{10, 20, 30};
    auto made = Clip::Make(GYO::Engine::Base::ConstSpan<std::int16_t>(mono.data(), mono.size()), 1, kSampleRate);
    REQUIRE(made.has_value());
    const Clip first = *made;
    mono[0] = 99; // the clip keeps its own copy
    CHECK(first.View().channels == 1);
    CHECK(first.View().frames == 3);
    CHECK(first.View().samples[0] == 10);
    CHECK(first.View().samples.data() != mono.data());

    // A copy shares the samples.
    const Clip copy = first; // NOLINT(performance-unnecessary-copy-initialization)
    CHECK(copy.View().samples.data() == first.View().samples.data());
    CHECK(static_cast<bool>(copy));
    CHECK_FALSE(static_cast<bool>(Clip{}));

    // Samples given as a vector are taken without a copy.
    std::vector<std::int16_t> stereo{1, 2, 3, 4};
    const std::int16_t* const data = stereo.data();
    auto taken = Clip::Make(std::move(stereo), 2, kSampleRate);
    REQUIRE(taken.has_value());
    CHECK(taken->View().samples.data() == data);
    CHECK(taken->View().frames == 2);
}

TEST_CASE("Audio VoiceMixer rejects a voice count outside 1 to 1024") {
    for (const std::uint32_t voices : {0u, 1025u}) {
        auto mixer = VoiceMixer::Create(voices);
        REQUIRE_FALSE(mixer.has_value());
        CHECK(mixer.error().code == AudioErrorCode::InvalidConfig);
    }
    CHECK(VoiceMixer::Create(1024).has_value());
}

TEST_CASE("Audio VoiceMixer starts a clip exactly at its start frame") {
    std::vector<Clip> bank;
    const ClipView clip = AddClip(bank, {1000, -2000, 3000}, 1);
    VoiceMixer mixer = MakeMixer();
    CHECK(mixer.Schedule({clip, 5, 1.0f, 1.0f}) == StartOutcome::OnTime);

    const auto out = RenderInBlocks(mixer, 10, {10});
    for (std::size_t f = 0; f < 5; ++f) {
        CHECK(Left(out, f) == 0);
        CHECK(Right(out, f) == 0);
    }
    CHECK(Left(out, 5) == 1000);
    CHECK(Right(out, 5) == 1000);
    CHECK(Left(out, 6) == -2000);
    CHECK(Left(out, 7) == 3000);
    CHECK(Right(out, 7) == 3000);
    CHECK(Left(out, 8) == 0);
    CHECK(mixer.RenderedFrames() == 10);
}

TEST_CASE("Audio VoiceMixer starts a clip scheduled in a later block at its frame") {
    std::vector<Clip> bank;
    const ClipView clip = AddClip(bank, {500}, 1);
    VoiceMixer mixer = MakeMixer();
    REQUIRE(mixer.Schedule({clip, 300, 1.0f, 1.0f}) == StartOutcome::OnTime);
    const auto out = RenderInBlocks(mixer, 400, {100});
    for (std::size_t f = 0; f < 400; ++f) CHECK(Left(out, f) == (f == 300 ? 500 : 0));
}

TEST_CASE("Audio VoiceMixer applies left and right gains with rounding") {
    std::vector<Clip> bank;
    const ClipView stereo = AddClip(bank, {1001, -1001, 3, -3}, 2);
    VoiceMixer mixer = MakeMixer();
    REQUIRE(mixer.Schedule({stereo, 0, 0.5f, 2.0f}) == StartOutcome::OnTime);
    const auto out = RenderInBlocks(mixer, 2, {2});
    CHECK(Left(out, 0) == 501);   // 500.5 rounds half up
    CHECK(Right(out, 0) == -2002);
    CHECK(Left(out, 1) == 2);     // 1.5 rounds half up
    CHECK(Right(out, 1) == -6);
}

TEST_CASE("Audio VoiceMixer output does not depend on how blocks split it") {
    std::vector<std::int16_t> ramp;
    for (int i = 0; i < 700; ++i) ramp.push_back(static_cast<std::int16_t>((i * 37) % 4000 - 2000));
    std::vector<std::int16_t> stereo;
    for (int i = 0; i < 400; ++i) {
        stereo.push_back(static_cast<std::int16_t>(i * 11 - 2000));
        stereo.push_back(static_cast<std::int16_t>(1500 - i * 7));
    }
    std::vector<Clip> bank; // outlives every mixer built below: they read its samples
    const auto build = [&] {
        const ClipView a = AddClip(bank, ramp, 1);
        const ClipView b = AddClip(bank, stereo, 2);
        VoiceMixer mixer = MakeMixer();
        // Starts on and around the internal 256-frame chunk and the block edges.
        for (const ScheduledPlay play : {ScheduledPlay{a, 0, 0.75f, 0.25f}, ScheduledPlay{b, 255, 1.0f, 1.0f},
                                         ScheduledPlay{a, 256, 1.5f, 0.5f}, ScheduledPlay{b, 999, 0.3f, 0.9f},
                                         ScheduledPlay{a, 1300, 1.0f, 1.0f}}) {
            REQUIRE(mixer.Schedule(play) == StartOutcome::OnTime);
        }
        return mixer;
    };

    VoiceMixer whole = build();
    const auto reference = RenderInBlocks(whole, 2200, {2200});
    for (const std::vector<std::uint32_t>& blocks :
         {std::vector<std::uint32_t>{1}, {7}, {256}, {255, 257}, {480}, {1024}, {13, 600, 1}}) {
        VoiceMixer split = build();
        CHECK(RenderInBlocks(split, 2200, blocks) == reference);
        CHECK(split.Stats().renderedFrames == 2200);
    }
    bool sound = false;
    for (const std::int16_t sample : reference) sound = sound || sample != 0;
    CHECK(sound);
}

TEST_CASE("Audio VoiceMixer saturates the mix to int16 and counts it") {
    std::vector<Clip> bank;
    const ClipView loud = AddClip(bank, {30000, -30000, 100}, 1);
    VoiceMixer mixer = MakeMixer();
    REQUIRE(mixer.Schedule({loud, 0, 1.0f, 1.0f}) == StartOutcome::OnTime);
    REQUIRE(mixer.Schedule({loud, 0, 1.0f, 1.0f}) == StartOutcome::OnTime);
    const auto out = RenderInBlocks(mixer, 3, {3});
    CHECK(Left(out, 0) == std::numeric_limits<std::int16_t>::max());
    CHECK(Right(out, 1) == std::numeric_limits<std::int16_t>::min());
    CHECK(Left(out, 2) == 200);
    CHECK(mixer.Stats().clippedSamples == 4);
}

TEST_CASE("Audio VoiceMixer drops a play when every voice is busy and reuses finished voices") {
    std::vector<Clip> bank;
    const ClipView clip = AddClip(bank, {100, 100}, 1);
    VoiceMixer mixer = MakeMixer(2);
    CHECK(mixer.Schedule({clip, 0, 1.0f, 1.0f}) == StartOutcome::OnTime);
    CHECK(mixer.Schedule({clip, 100, 1.0f, 1.0f}) == StartOutcome::OnTime); // a scheduled voice is busy too
    CHECK(mixer.Schedule({clip, 1, 1.0f, 1.0f}) == StartOutcome::NoVoice);

    auto out = RenderInBlocks(mixer, 2, {2});
    CHECK(Left(out, 1) == 100); // the dropped play adds nothing
    CHECK(mixer.Schedule({clip, 2, 1.0f, 1.0f}) == StartOutcome::OnTime);

    const auto stats = mixer.Stats();
    CHECK(stats.noVoice == 1);
    CHECK(stats.accepted == 4);
    CHECK(stats.accepted == stats.onTime + stats.late + stats.noVoice);
}

TEST_CASE("Audio VoiceMixer starts a late play at the next frame and records how late") {
    std::vector<Clip> bank;
    const ClipView clip = AddClip(bank, {700, 800}, 1);
    VoiceMixer mixer = MakeMixer();
    static_cast<void>(RenderInBlocks(mixer, 10, {10}));
    CHECK(mixer.Schedule({clip, 4, 1.0f, 1.0f}) == StartOutcome::Late);
    CHECK(mixer.Schedule({clip, 9, 1.0f, 1.0f}) == StartOutcome::Late);  // one frame late
    CHECK(mixer.Schedule({clip, 10, 1.0f, 1.0f}) == StartOutcome::OnTime); // exactly the next frame

    const auto out = RenderInBlocks(mixer, 3, {3});
    CHECK(Left(out, 0) == 2100);
    CHECK(Left(out, 1) == 2400);
    CHECK(Left(out, 2) == 0);

    const auto stats = mixer.Stats();
    CHECK(stats.late == 2);
    CHECK(stats.onTime == 1);
    CHECK(stats.maxLateFrames == 6);
    CHECK(stats.accepted == stats.onTime + stats.late + stats.noVoice);
}

TEST_CASE("Audio VoiceMixer master gain scales and silences the mix") {
    std::vector<Clip> bank;
    const ClipView clip = AddClip(bank, {1000, 1000, 1000}, 1);
    VoiceMixer mixer = MakeMixer();
    REQUIRE(mixer.Schedule({clip, 0, 1.0f, 1.0f}) == StartOutcome::OnTime);
    mixer.SetMasterGain(0.5f);
    auto out = RenderInBlocks(mixer, 1, {1});
    CHECK(Left(out, 0) == 500);
    mixer.SetMasterGain(0.0f);
    out = RenderInBlocks(mixer, 1, {1});
    CHECK(Left(out, 0) == 0);
    mixer.SetMasterGain(kMaxGain);
    out = RenderInBlocks(mixer, 1, {1});
    CHECK(Left(out, 0) == 8000);
}

TEST_CASE("Audio VoiceMixer treats an empty clip and a bad gain as Programmer Errors") {
    std::vector<Clip> bank;
    const ClipView clip = AddClip(bank, {1}, 1);
    VoiceMixer mixer = MakeMixer();
    GYO_CHECK_ASSERTS(mixer.Schedule({ClipView{}, 0, 1.0f, 1.0f}));
    GYO_CHECK_ASSERTS(mixer.Schedule({clip, 0, -0.1f, 1.0f}));
    GYO_CHECK_ASSERTS(mixer.Schedule({clip, 0, 1.0f, std::nanf("")}));
    GYO_CHECK_ASSERTS(mixer.Schedule({clip, 0, kMaxGain * 2.0f, 1.0f}));
    GYO_CHECK_ASSERTS(mixer.SetMasterGain(std::numeric_limits<float>::infinity()));
    CHECK(mixer.Stats().accepted == 0);
}

TEST_CASE("Audio VoiceMixer keeps playing voices across a move") {
    std::vector<Clip> bank;
    const ClipView clip = AddClip(bank, {5, 6, 7, 8}, 1);
    VoiceMixer first = MakeMixer();
    REQUIRE(first.Schedule({clip, 0, 1.0f, 1.0f}) == StartOutcome::OnTime);
    static_cast<void>(RenderInBlocks(first, 2, {2}));
    VoiceMixer moved(std::move(first));
    const auto out = RenderInBlocks(moved, 2, {2});
    CHECK(Left(out, 0) == 7);
    CHECK(Left(out, 1) == 8);
    CHECK(moved.Stats().renderedFrames == 4);
}
