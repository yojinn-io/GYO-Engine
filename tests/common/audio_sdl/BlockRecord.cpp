// Records how the default playback device asks for audio: its block size, the
// frames per callback and the longest gap between callbacks, over a few seconds
// of silence. Not a CTest (it needs a real device); its line is the evidence
// for the mixer's default schedule delay (docs/architecture/plans/audio).
//
// Usage: gyo_audio_block_record [seconds] [deviceSampleFrames]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "engine/audio/Mixer.hpp"
#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"

int main(int argc, char** argv) {
    using namespace Engine::Audio;
    const int seconds = argc > 1 ? std::atoi(argv[1]) : 5;
    Sdl::SdlAudioOutputConfig config;
    if (argc > 2) config.deviceSampleFrames = static_cast<std::uint32_t>(std::atoi(argv[2]));

    auto mixer = Mixer::Create(MixerConfig{}, ClipBank{});
    if (!mixer) return 2;
    auto output = Sdl::SdlAudioOutput::Open(std::move(mixer).value(), config);
    if (!output) {
        std::printf("audio_block_record status=unavailable error=\"%s\"\n", Engine::Base::Describe(output.error()).c_str());
        return 1;
    }
    const auto start = Engine::Time::Now();
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    const auto elapsed = std::chrono::duration<double>(Engine::Time::Now() - start).count();
    const auto stats = output->Stats();
    std::printf("audio_block_record status=ok driver=%s device_frames=%u seconds=%.3f callbacks=%llu frames=%llu "
                "frames_per_second=%.1f callback_frames_min=%u callback_frames_max=%u callback_gap_max_ms=%.3f\n",
                output->DriverName().c_str(), output->DeviceSampleFrames(), elapsed,
                static_cast<unsigned long long>(stats.callbacks), static_cast<unsigned long long>(stats.frames),
                static_cast<double>(stats.frames) / elapsed, stats.minCallbackFrames, stats.maxCallbackFrames,
                std::chrono::duration<double, std::milli>(stats.maxCallbackGap).count());
    return 0;
}
