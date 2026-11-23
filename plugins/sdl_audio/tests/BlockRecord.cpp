// Records the default playback device's blocks for some seconds and prints one
// JSON line: what the mixer's scheduleDelay has to cover besides the consumer's
// frame time (docs/architecture/target/v2/sdl.zh-Hant.md, sdl_audio). Not a
// test: it needs a real audio device. Usage:
//   gyo_sdl_audio_block_record [seconds = 10] [device frames = 0, the platform's choice]
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <utility>

#include "engine/audio/Mixer.hpp"
#include "engine/orchestration/Product.hpp"
#include "engine/orchestration/RegisteredUnits.hpp"
#include "plugins/sdl_audio/AudioOutput.hpp"

using GYO::Engine::Orchestration::MakeRegisteredUnits;
using GYO::Engine::Orchestration::Product;
using GYO::Engine::Orchestration::ProductDescription;
using GYO::Engine::Orchestration::WorldDescription;
using GYO::Plugins::SDLAudio::AudioOutput;
using GYO::Plugins::SDLAudio::AudioOutputOptions;

int main(int argc, char** argv) {
    const int seconds = argc > 1 ? std::atoi(argv[1]) : 10;
    AudioOutputOptions options;
    options.deviceSampleFrames = argc > 2 ? static_cast<std::uint32_t>(std::strtoul(argv[2], nullptr, 10)) : 0;

    ProductDescription description;
    description.name = "sdl_audio_block_record";
    WorldDescription world;
    world.name = "main";
    world.units = {"world_time"};
    description.worlds.push_back(std::move(world));
    description.options.Set(options);
    auto product = Product::Build(std::move(description), MakeRegisteredUnits());
    if (!product) {
        std::fprintf(stderr, "the Product did not build: %s\n", product.error().detail.c_str());
        return 2;
    }
    const AudioOutput& output = *std::as_const((*product)->Buffer()).Find<AudioOutput>();
    if (!output.Playing()) {
        std::fprintf(stderr, "the device does not play: %s: %s (%s)\n", ToString(output.Failure()->code),
                     output.Failure()->message.c_str(), output.Failure()->detail.c_str());
        return 3;
    }
    const auto begin = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    const auto stats = output.Stats();
    const double gapMs = std::chrono::duration<double, std::milli>(stats.maxCallbackGap).count();
    std::printf("{\"driver\":\"%s\",\"requested_frames\":%u,\"device_frames\":%u,\"seconds\":%.3f,\"callbacks\":%llu,"
                "\"min_callback_frames\":%u,\"max_callback_frames\":%u,\"max_callback_gap_ms\":%.3f,"
                "\"frames_per_second\":%.1f}\n",
                output.DriverName().c_str(), options.deviceSampleFrames, output.DeviceSampleFrames(), elapsed,
                static_cast<unsigned long long>(stats.callbacks), stats.minCallbackFrames, stats.maxCallbackFrames,
                gapMs, static_cast<double>(stats.frames) / elapsed);
    return 0;
}
