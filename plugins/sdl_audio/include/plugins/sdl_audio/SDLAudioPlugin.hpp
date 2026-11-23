#pragma once

#include "engine/orchestration/Plugin.hpp"

namespace GYO::Plugins::SDLAudio {

// The registration unit of sdl_audio (config/plugins.csv): a unit that only
// configures, carried by the product and put into no World (SDL declaration
// section 3.3). Its Configure, in this order:
//   1. the mixer: the product's (AudioOutputMixer) when given; otherwise it
//      creates one from AudioOutputOptions::mixer and puts it in the Product's
//      buffer (Find<Engine::Audio::Mixer>). A configuration outside the mixer's
//      limits is a startup error (SA-E1);
//   2. the sound loader: WavLoader, when the Product's buffer holds the asset
//      loader registry (world_asset); a refusal is a startup error (SA-E2);
//   3. the device: opens the default playback device on the mixer and puts an
//      AudioOutput in the Product's buffer, playing or saying why not (not a
//      startup error: the Product runs without sound);
//   4. OnDestroy stops the device, before the Product's buffer and the options.
class SDLAudioPlugin final : public Engine::Orchestration::Plugin {
public:
    void Configure(Engine::Orchestration::ProductSetup& setup) override;
    // In no World: there is nothing to register.
    void Build(Engine::Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Plugins::SDLAudio
