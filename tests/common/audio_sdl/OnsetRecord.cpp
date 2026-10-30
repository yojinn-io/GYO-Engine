// Engine L2 of the audio plan (v8-engine-audio-record; record only, never a
// pass condition). A 30 FPS frame loop on the Engine Waiter plays a one-sample
// impulse for events on a 60 Hz tick grid, spaced 6, 7 and 5 ticks in turn.
// Each event is sent in the first frame after it happened, stamped with its
// tick time, as a game would. Run through onset_record.py, which picks SDL's
// driver through the environment and finds the onsets in the disk driver's
// file; this program calls no SDL.
//
// Usage: gyo_audio_onset_record <seconds> <output directory>
// Writes events.csv (index,tick,at_ns,sent_ns,accepted), rate.csv
// (now_ns,rendered_frames, once per frame), anchors.csv (anchor_ns,first_frame,
// one per output callback) and summary.txt (key=value lines).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Mixer.hpp"
#include "engine/audio/backend/sdl/SdlAudioOutput.hpp"
#include "engine/time/Waiter.hpp"

namespace {

using Engine::Time::Duration;
using Engine::Time::TimePoint;

constexpr std::int16_t kImpulse = 1000;
constexpr int kGaps[] = {6, 7, 5};

Duration Ticks(const std::int64_t ticks) {
    // 60 Hz without accumulating rounding: tick k is k * 10^9 / 60 ns.
    return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(ticks * 1'000'000'000 / 60));
}

Duration Frames(const std::int64_t frames) {
    return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(frames * 1'000'000'000 / 30));
}

std::int64_t Ns(const TimePoint point, const TimePoint origin) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(point - origin).count();
}

} // namespace

int main(int argc, char** argv) {
    using namespace Engine::Audio;
    if (argc != 3) {
        std::fprintf(stderr, "usage: gyo_audio_onset_record <seconds> <output directory>\n");
        return 2;
    }
    const int seconds = std::atoi(argv[1]);
    const std::filesystem::path out = argv[2];
    std::filesystem::create_directories(out);

    ClipBank bank;
    const std::vector<std::int16_t> one{kImpulse};
    const auto clip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(one.data(), one.size()), 1, kSampleRate);
    auto mixer = Mixer::Create(MixerConfig{}, std::move(bank));
    if (!clip || !mixer) return 2;
    auto submitter = mixer->CreateSubmitter();
    if (!submitter) return 2;
    Sdl::SdlAudioOutputConfig config;
    config.anchorLogCapacity = 8192; // drained every frame, far above one frame's callbacks
    auto output = Sdl::SdlAudioOutput::Open(std::move(mixer).value(), config);
    std::ofstream summary(out / "summary.txt");
    if (!output) {
        summary << "status=unavailable\nerror=" << Engine::Base::Describe(output.error()) << "\n";
        return 1;
    }
    auto waiter = Engine::Time::Waiter::Create();
    if (!waiter) return 2;

    std::ofstream events(out / "events.csv");
    std::ofstream rate(out / "rate.csv");
    std::ofstream anchorFile(out / "anchors.csv");
    events << "index,tick,at_ns,sent_ns,accepted\n";
    rate << "now_ns,rendered_frames\n";
    anchorFile << "anchor_ns,first_frame\n";
    std::vector<Sdl::OutputAnchor> anchors;
    anchors.reserve(8192);

    // Start a little ahead so the device is running before the first event.
    const TimePoint origin = Engine::Time::Now() + std::chrono::duration_cast<Duration>(std::chrono::milliseconds(200));
    const std::int64_t frameCount = static_cast<std::int64_t>(seconds) * 30;
    std::int64_t nextTick = 6;
    std::size_t gap = 0;
    std::int64_t index = 0;
    std::uint64_t lateFrames = 0;
    for (std::int64_t frame = 1; frame <= frameCount; ++frame) {
        const auto wake = waiter->WaitUntil(origin + Frames(frame));
        if (wake.late > Frames(1)) ++lateFrames;
        const TimePoint now = Engine::Time::Now();
        // Every event that happened since the previous frame, stamped with its tick.
        while (origin + Ticks(nextTick) <= now) {
            const TimePoint at = origin + Ticks(nextTick);
            const bool accepted = submitter->Play({*clip, at, 1.0f, 1.0f});
            events << index << ',' << nextTick << ',' << Ns(at, origin) << ',' << Ns(now, origin) << ','
                   << (accepted ? 1 : 0) << '\n';
            ++index;
            nextTick += kGaps[gap];
            gap = (gap + 1) % std::size(kGaps);
        }
        rate << Ns(Engine::Time::Now(), origin) << ',' << output->OwnedMixer().Stats().renderedFrames << '\n';
        anchors.clear();
        output->TakeAnchors(anchors);
        for (const auto& anchor : anchors) anchorFile << Ns(anchor.anchor, origin) << ',' << anchor.firstFrame << '\n';
    }
    // Let the last events play out before closing the device.
    static_cast<void>(waiter->WaitUntil(Engine::Time::Now() + std::chrono::duration_cast<Duration>(std::chrono::milliseconds(300))));

    anchors.clear();
    output->TakeAnchors(anchors);
    for (const auto& anchor : anchors) anchorFile << Ns(anchor.anchor, origin) << ',' << anchor.firstFrame << '\n';
    const auto stats = output->OwnedMixer().Stats();
    const auto device = output->Stats();
    summary << "status=ok\n"
            << "driver=" << output->DriverName() << "\n"
            << "device_frames=" << output->DeviceSampleFrames() << "\n"
            << "wait_backend=" << Engine::Time::ToString(waiter->Backend()) << "\n"
            << "frames_late_over_one_frame=" << lateFrames << "\n"
            << "submitted=" << stats.submitted << "\n"
            << "played_on_time=" << stats.playedOnTime << "\n"
            << "played_late=" << stats.playedLate << "\n"
            << "dropped_queue_full=" << stats.droppedQueueFull << "\n"
            << "dropped_no_voice=" << stats.droppedNoVoice << "\n"
            << "max_lateness_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(stats.maxLateness).count() << "\n"
            << "rendered_frames=" << stats.renderedFrames << "\n"
            << "clipped_samples=" << stats.clippedSamples << "\n"
            << "callbacks=" << device.callbacks << "\n"
            << "callback_frames_min=" << device.minCallbackFrames << "\n"
            << "callback_frames_max=" << device.maxCallbackFrames << "\n"
            << "anchors_dropped=" << device.anchorsDropped << "\n"
            << "schedule_delay_ns="
            << std::chrono::duration_cast<std::chrono::nanoseconds>(MixerConfig{}.scheduleDelay).count() << "\n"
            << "callback_gap_max_ns="
            << std::chrono::duration_cast<std::chrono::nanoseconds>(device.maxCallbackGap).count() << "\n";
    return 0;
}
