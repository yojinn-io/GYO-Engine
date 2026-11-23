// sdl_audio through a Product, the way a product carries it. No SDL here: the
// tests pick SDL's audio driver through the environment (SDL reads its hints
// from there; tests/CMakeLists.txt), so they run headless on every CI row.
#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <ostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "engine/asset/AssetStore.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Clip.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/WavLoader.hpp"
#include "engine/orchestration/Product.hpp"
#include "engine/os/Files.hpp"
#include "engine/orchestration/RegisteredUnits.hpp"
#include "plugins/sdl_audio/AudioOutput.hpp"

// In the Plugin's namespace: a Plugin's tests live in its directory (naming rule N17).
namespace GYO::Plugins::SDLAudio {
namespace {

namespace Audio = Engine::Audio;
namespace Orchestration = Engine::Orchestration;
namespace Time = Engine::Time;

// One World with world_asset (the asset loader registry); sdl_audio goes into no World.
Orchestration::ProductDescription Description() {
    Orchestration::ProductDescription description;
    description.name = "sdl_audio_test";
    Orchestration::WorldDescription world;
    world.name = "main";
    world.units = {"world_asset"};
    description.worlds.push_back(std::move(world));
    return description;
}

std::unique_ptr<Orchestration::Product> Build(Orchestration::ProductDescription description) {
    auto product = Orchestration::Product::Build(std::move(description), Orchestration::MakeRegisteredUnits());
    REQUIRE_MESSAGE(product.has_value(), (product ? std::string() : product.error().detail));
    return std::move(product).value();
}

// Waits until the condition holds. The limit is a hang guard, not a timing
// requirement: the drivers render far faster than it.
bool WaitFor(const std::function<bool()>& condition) {
    const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!condition()) {
        if (std::chrono::steady_clock::now() > limit) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

bool Advances(const Audio::Mixer& mixer) {
    const std::uint64_t start = mixer.Stats().renderedFrames;
    return WaitFor([&] { return mixer.Stats().renderedFrames > start + 4800; });
}

Audio::Clip MakeClip(const std::size_t frames, const std::int16_t level) {
    auto clip = Audio::Clip::Make(std::vector<std::int16_t>(frames, level), 1, Audio::kSampleRate);
    REQUIRE(clip.has_value());
    return std::move(clip).value();
}

// A loader of another type for the same (type, format): refused while WavLoader holds the pair.
struct AnotherWavLoader final {
    using Output = Audio::Clip;
    using Settings = Audio::WavSettings;
    using Code = Audio::WavLoaderCode;
    static constexpr std::string_view kTypeName = Audio::WavLoader::kTypeName;
    static constexpr auto kFormats = Audio::WavLoader::kFormats;
    static constexpr std::size_t kDefaultMaxBytes = Audio::WavLoader::kDefaultMaxBytes;
    [[nodiscard]] auto ParseSettings(const std::optional<std::string_view> json) const {
        return Audio::WavLoader{}.ParseSettings(json);
    }
    [[nodiscard]] auto Load(const std::span<const std::byte> bytes, const Settings& settings,
                            Engine::Asset::LoadContext& context) const {
        return Audio::WavLoader{}.Load(bytes, settings, context);
    }
};

} // namespace

TEST_CASE("dummy: creates the mixer, registers the sound loader and plays the mixer on the device") {
    auto product = Build(Description());
    Audio::Mixer* mixer = product->Buffer().Find<Audio::Mixer>();
    REQUIRE(mixer != nullptr);
    const AudioOutput* output = std::as_const(product->Buffer()).Find<AudioOutput>();
    REQUIRE(output != nullptr);
    REQUIRE_MESSAGE(output->Playing(), (output->Failure() ? output->Failure()->detail : std::string()));
    CHECK(output->Failure() == nullptr);
    CHECK(output->DriverName() == "dummy");
    CHECK(output->DeviceSampleFrames() > 0);
    CHECK(Advances(*mixer));
    const auto stats = output->Stats();
    CHECK(stats.callbacks > 0);
    CHECK(stats.minCallbackFrames > 0);
    CHECK(stats.maxCallbackFrames >= stats.minCallbackFrames);
    CHECK(stats.frames + stats.maxCallbackFrames >= mixer->Stats().renderedFrames);

    const auto* registry = std::as_const(product->Buffer()).Find<Engine::Asset::AssetLoaderRegistry>();
    REQUIRE(registry != nullptr);
    const auto other = registry->Register<AnotherWavLoader>(); // WavLoader holds ("sound", "wav") already
    REQUIRE_FALSE(other.has_value());
    CHECK(other.error().code == Engine::Asset::LoaderRegistrationFailureCode::FormatTaken);
    CHECK(registry->Register<Audio::WavLoader>().has_value()); // the same loader again: no change
}

TEST_CASE("dummy: creates the mixer from the options") {
    auto description = Description();
    AudioOutputOptions options;
    options.mixer.maxSubmitters = 1;
    description.options.Set(options);
    auto product = Build(std::move(description));
    Audio::Mixer* mixer = product->Buffer().Find<Audio::Mixer>();
    REQUIRE(mixer != nullptr);
    REQUIRE(mixer->CreateSubmitter().has_value());
    const auto second = mixer->CreateSubmitter();
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code == Audio::AudioErrorCode::TooManySubmitters);
}

TEST_CASE("dummy: a mixer configuration outside the limits is a startup error") {
    auto description = Description();
    AudioOutputOptions options;
    options.mixer.maxVoices = 0;
    description.options.Set(options);
    auto product = Orchestration::Product::Build(std::move(description), Orchestration::MakeRegisteredUnits());
    REQUIRE_FALSE(product.has_value());
    REQUIRE(product.error().unitErrors.size() == 1);
    CHECK(product.error().unitErrors[0].unit == "sdl_audio");
    CHECK(product.error().unitErrors[0].code == "SA-E1");
}

TEST_CASE("dummy: plays the product's mixer instead of creating one, and stops when the Product goes") {
    auto made = Audio::Mixer::Create(Audio::MixerConfig{});
    REQUIRE(made.has_value());
    auto mixer = std::make_shared<Audio::Mixer>(std::move(made).value());
    auto description = Description();
    description.options.Set(AudioOutputMixer{mixer});
    auto product = Build(std::move(description));
    CHECK(product->Buffer().Find<Audio::Mixer>() == nullptr);
    const AudioOutput* output = std::as_const(product->Buffer()).Find<AudioOutput>();
    REQUIRE(output != nullptr);
    REQUIRE(output->Playing());
    CHECK(Advances(*mixer));

    product.reset(); // OnDestroy stops the device; this test still holds the mixer
    const std::uint64_t stopped = mixer->Stats().renderedFrames;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    CHECK(mixer->Stats().renderedFrames == stopped);
    CHECK(mixer.use_count() == 1); // the output kept no share
}

TEST_CASE("dummy: two Products play side by side, and one going leaves the other playing") {
    auto first = Build(Description());
    {
        auto second = Build(Description());
        CHECK(Advances(*second->Buffer().Find<Audio::Mixer>()));
    }
    CHECK(Advances(*first->Buffer().Find<Audio::Mixer>()));
    first.reset();
    auto third = Build(Description()); // the subsystem starts again after every output stopped
    CHECK(Advances(*third->Buffer().Find<Audio::Mixer>()));
}

TEST_CASE("dummy: maps a play from the anchor of the block that takes it") {
    // A play 100 ms ahead, sent while the device has played for a while: mapped
    // from the anchor of the block that takes it, it starts about 4800 frames
    // after the frame the mixer was at when it was sent. Mapped from an older
    // anchor (one read at opening), it would start later by all the time since.
    auto description = Description();
    AudioOutputOptions options;
    options.mixer.scheduleDelay = Time::Duration::zero();
    description.options.Set(options);
    auto product = Build(std::move(description));
    Audio::Mixer& mixer = *product->Buffer().Find<Audio::Mixer>();
    auto submitter = mixer.CreateSubmitter();
    REQUIRE(submitter.has_value());
    REQUIRE(WaitFor([&] { return mixer.Stats().renderedFrames > 48000; })); // one second played

    const std::uint64_t sentFrame = mixer.Stats().renderedFrames;
    const Audio::SoundHandle sound =
        submitter->Play({MakeClip(5 * Audio::kSampleRate, 100), Time::Now() + std::chrono::milliseconds(100)});
    REQUIRE(sound);
    REQUIRE(WaitFor([&] { return mixer.PlayheadOf(sound).status == Audio::Playhead::Status::Playing; }));
    const Audio::Playhead playhead = mixer.PlayheadOf(sound);
    const std::uint64_t rendered = mixer.Stats().renderedFrames; // at or after the playhead's block
    REQUIRE(rendered >= playhead.clipFrame);
    const auto start = static_cast<std::int64_t>(rendered - playhead.clipFrame);
    const auto offset = start - static_cast<std::int64_t>(sentFrame);
    INFO("started " << offset << " frames after the frame at sending");
    CHECK(mixer.Stats().playedOnTime == 1);
    // 4800 frames, give or take the device blocks on either side (the dummy
    // driver's are a few thousand frames at most); the old anchor would add 48000.
    CHECK(offset > 0);
    CHECK(offset < 4800 + 16000);
}

TEST_CASE("disk: writes the mixer's samples to the device") {
    const char* path = std::getenv("SDL_AUDIO_DISK_OUTPUT_FILE");
    REQUIRE(path != nullptr);
    {
        auto product = Build(Description());
        const AudioOutput* output = std::as_const(product->Buffer()).Find<AudioOutput>();
        REQUIRE(output != nullptr);
        REQUIRE_MESSAGE(output->Playing(), (output->Failure() ? output->Failure()->detail : std::string()));
        CHECK(output->DriverName() == "disk");
        CHECK(output->DeviceSampleFrames() == 480);
        Audio::Mixer& mixer = *product->Buffer().Find<Audio::Mixer>();
        auto submitter = mixer.CreateSubmitter();
        REQUIRE(submitter.has_value());
        // A time already passed: the one-frame clip starts at once, counted late.
        REQUIRE(submitter->Play({MakeClip(1, 1000), Time::Now() - std::chrono::seconds(1), 1.0f, 0.5f}));
        REQUIRE(WaitFor([&] { return mixer.Stats().renderedFrames > 48000; }));
        CHECK(mixer.Stats().playedLate == 1);
    }
    auto file = Engine::OS::Files::FileReader::Open(path); // plugins/ reads files through the OS layer (R4)
    REQUIRE(file.has_value());
    const auto size = file->Size();
    REQUIRE(size.has_value());
    std::vector<std::byte> bytes(static_cast<std::size_t>(*size));
    std::size_t filled = 0;
    while (filled < bytes.size()) {
        const auto read = file->Read(std::span<std::byte>(bytes).subspan(filled));
        REQUIRE(read.has_value());
        REQUIRE(*read > 0);
        filled += *read;
    }
    REQUIRE(bytes.size() % 4 == 0);
    REQUIRE(bytes.size() > 4 * 48000);
    std::size_t sounding = 0;
    for (std::size_t frame = 0; frame * 4 < bytes.size(); ++frame) {
        const auto sample = [&](const std::size_t offset) {
            const auto lo = static_cast<std::uint8_t>(bytes[frame * 4 + offset]);
            const auto hi = static_cast<std::uint8_t>(bytes[frame * 4 + offset + 1]);
            return static_cast<std::int16_t>(lo | (hi << 8)); // S16 little-endian on every CI host
        };
        if (sample(0) != 0 || sample(2) != 0) {
            CHECK(sample(0) == 1000);
            CHECK(sample(2) == 500);
            ++sounding;
        }
    }
    CHECK(sounding == 1);
}

TEST_CASE("unavailable: tells why the device does not play, and the Product runs without sound") {
    auto product = Build(Description());
    const AudioOutput* output = std::as_const(product->Buffer()).Find<AudioOutput>();
    REQUIRE(output != nullptr);
    CHECK_FALSE(output->Playing());
    REQUIRE(output->Failure() != nullptr);
    CHECK(output->Failure()->code == AudioOutputFailureCode::SubsystemUnavailable);
    CHECK_FALSE(output->Failure()->detail.empty());
    CHECK(output->DriverName().empty());
    CHECK(output->Stats().callbacks == 0);
    Audio::Mixer* mixer = product->Buffer().Find<Audio::Mixer>();
    REQUIRE(mixer != nullptr);
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    CHECK(submitter->Play({MakeClip(1, 1000), Time::Now()}));
    CHECK(product->RunFrame().has_value());
}

} // namespace GYO::Plugins::SDLAudio
