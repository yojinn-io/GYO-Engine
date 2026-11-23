#include "plugins/sdl_audio/SDLAudioPlugin.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "AudioDevice.hpp"
#include "engine/asset/AssetStore.hpp"
#include "engine/audio/WavLoader.hpp"
#include "engine/orchestration/Setup.hpp"
#include "plugins/sdl_audio/AudioOutput.hpp"

namespace GYO::Plugins::SDLAudio {

namespace Audio = Engine::Audio;

void SDLAudioPlugin::Configure(Engine::Orchestration::ProductSetup& setup) {
    const AudioOutputOptions defaults{};
    const AudioOutputOptions* given = setup.Options<AudioOutputOptions>();
    const AudioOutputOptions& options = given != nullptr ? *given : defaults;

    // 1. The mixer: the product's, or one this unit creates and owns in the Product's buffer.
    Audio::Mixer* mixer = nullptr;
    if (const AudioOutputMixer* own = setup.Options<AudioOutputMixer>(); own != nullptr && own->mixer != nullptr) {
        mixer = own->mixer.get();
    } else {
        auto made = Audio::Mixer::Create(options.mixer);
        if (!made) {
            setup.Report("SA-E1", "the mixer configuration of AudioOutputOptions is refused: " +
                                      std::string(Audio::ToString(made.error().code)) + ": " + made.error().message);
            return;
        }
        mixer = &setup.Buffer().Create<Audio::Mixer>(std::move(made).value());
    }

    // 2. The sound loader (audio page section 1): registered here when the Product has an asset store.
    if (const auto* registry = std::as_const(setup.Buffer()).Find<Engine::Asset::AssetLoaderRegistry>()) {
        if (auto registered = registry->Register<Audio::WavLoader>(); !registered) {
            setup.Report("SA-E2", "registering the sound loader failed: " +
                                      std::string(Engine::Asset::ToString(registered.error().code)) + ": " +
                                      registered.error().message);
        }
    }

    // 3. The device. Not opening is told, not a startup error: the Product runs without sound.
    auto device = AudioDevice::Open(*mixer, AudioDeviceSettings{options.deviceSampleFrames});
    std::unique_ptr<AudioDevice> playing;
    std::optional<AudioOutputFailure> failure;
    if (device) {
        playing = std::move(device).value();
    } else {
        failure = std::move(device).error();
    }
    AudioOutput& output = setup.Buffer().Create<AudioOutput>(std::move(playing), std::move(failure));

    // 4. The device stops before the Product's buffer (the mixer this unit created) and the options (the
    //    product's mixer) go: OnDestroy runs after every World and before the buffer (O-15). The buffer's
    //    reverse order would stop it first today as well; stopping here does not rest on the order of the
    //    Creates above (SDL declaration section 7, audio page section 4).
    setup.OnDestroy([&output] { output.Stop(); });
}

void SDLAudioPlugin::Build(Engine::Orchestration::WorldSetup&) {}

} // namespace GYO::Plugins::SDLAudio
