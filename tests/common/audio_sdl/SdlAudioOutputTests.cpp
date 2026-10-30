#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

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

SdlAudioOutput OpenOutput(Mixer&& mixer) {
    auto output = SdlAudioOutput::Open(std::move(mixer));
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
    {
        auto [mixer, clip] = MakeMixer();
        auto submitter = mixer.CreateSubmitter();
        REQUIRE(submitter.has_value());
        SdlAudioOutput output = OpenOutput(std::move(mixer));
        CHECK(output.DriverName() == "disk");
        CHECK(output.DeviceSampleFrames() == 480);
        // A time already passed: the impulse starts at once, counted late.
        REQUIRE(submitter->Play({clip, Engine::Time::Now() - std::chrono::seconds(1), 1.0f, 0.5f}));
        REQUIRE(WaitFor([&] { return output.OwnedMixer().Stats().renderedFrames > 48000; }));
        const auto stats = output.OwnedMixer().Stats();
        CHECK(stats.submitted == 1);
        CHECK(stats.playedLate == 1);
        CHECK(stats.submitted == stats.playedOnTime + stats.playedLate + stats.droppedQueueFull + stats.droppedNoVoice);
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
    CHECK(sounding.size() == 1);
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
