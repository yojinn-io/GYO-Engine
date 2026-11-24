#include "object_connect/runtime/GameSound.hpp"

#include "object_connect/runtime/GameData.hpp"
#include "object_connect/runtime/RuntimeWorld.hpp"
#include "object_connect/world/Messages.hpp"

#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Clip.hpp"
#include "engine/base/Log.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/WorldGeneration.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace object_connect::runtime {
namespace {

namespace AU = GYO::Engine::Audio;
namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace T = GYO::Engine::Time;

[[nodiscard]] T::Duration Seconds(const double seconds) {
    return std::chrono::duration_cast<T::Duration>(std::chrono::duration<double>(seconds));
}

// A clip's length, rounded up to the nanosecond (the audio Module's rule for
// an intro followed by a loop, Mixer.hpp PlayRequest::loop).
[[nodiscard]] T::Duration LengthOf(const AU::Clip& clip) {
    const std::uint64_t frames = clip.View().frames;
    const std::uint64_t nanoseconds = (frames * 1'000'000'000ull + AU::kSampleRate - 1) / AU::kSampleRate;
    return std::chrono::duration_cast<T::Duration>(std::chrono::nanoseconds(nanoseconds));
}

// The mixer's schedule delay as sdl_audio makes its mixer (AudioOutputOptions::mixer, the default MixerConfig):
// every request starts that much after its `at`.
[[nodiscard]] T::Duration ScheduleDelay() { return AU::MixerConfig{}.scheduleDelay; }

AU::SoundHandle Play(AU::Submitter& submitter, const AU::Clip& clip, const T::TimePoint at, const float gain,
                     const bool loop = false) {
    if (!clip) {
        return {};
    }
    return submitter.Play(AU::PlayRequest{clip, at, gain, gain, loop});
}

void Stop(AU::Submitter& submitter, AU::SoundHandle& sound, const T::TimePoint at) {
    // A full queue drops the stop (counted by the mixer); the sound then plays on, as a failed StopWave did.
    static_cast<void>(submitter.Stop(sound, at));
    sound = {};
}

void SetGain(AU::Submitter& submitter, const AU::SoundHandle sound, const float gain, const T::TimePoint at,
             const T::Duration ramp) {
    static_cast<void>(submitter.SetGain(sound, gain, gain, at, ramp));
}

// The music from its start: the intro, then the loop from the intro's end
// (GameAudio::StartMusicSequence; the loop no longer waits for IsPlaying).
void StartMusic(GameSound& sound, const GameContent& content, const T::TimePoint now) {
    AU::Submitter& submitter = *sound.submitter;
    Stop(submitter, sound.intro, now);
    Stop(submitter, sound.loop, now);
    const AU::Clip& intro = content.clips[static_cast<std::size_t>(SoundId::Intro)];
    const AU::Clip& loop = content.clips[static_cast<std::size_t>(SoundId::Loop)];
    // The new voices start at now + delay with the duck the mixer has then; a duck still ramping goes on in them
    // (the original set the music's volume every frame from its duck).
    const T::TimePoint startsAt = now + ScheduleDelay();
    const float duck = sound.duck.At(startsAt);
    sound.intro = Play(submitter, intro, now, kIntroGain * duck);
    const T::TimePoint loopAt = intro ? now + LengthOf(intro) : now;
    sound.loop = Play(submitter, loop, loopAt, kLoopGain * duck, true);
    if (sound.duck.End() > startsAt) {
        const T::Duration remaining = sound.duck.End() - startsAt;
        SetGain(submitter, sound.intro, kIntroGain * sound.duck.to, now, remaining);
        SetGain(submitter, sound.loop, kLoopGain * sound.duck.to, now, remaining);
    }
    sound.musicStarted = true;
}

void Duck(GameSound& sound, const float target, const T::TimePoint now) {
    sound.duck = PlanDuck(sound.duck, target, now, ScheduleDelay());
    SetGain(*sound.submitter, sound.intro, kIntroGain * target, now, sound.duck.length);
    SetGain(*sound.submitter, sound.loop, kLoopGain * target, now, sound.duck.length);
}

// Plays what the frame's Messages ask (GameAudio's BeginLineHold,
// EndLineHold, RestartMusicSequence) and ducks the music (GameAudio::Update).
K::SystemResult PlaySounds(K::Res<GameContent> content, K::ResMut<GameSound> state,
                           K::MessageReader<world::LineHoldStarted> started,
                           K::MessageReader<world::LineHoldEnded> ended,
                           K::MessageReader<world::MusicRestarted> restarted) {
    if (state.Read().submitter == nullptr) {
        return {};
    }
    GameSound& sound = state.Get();
    AU::Submitter& submitter = *sound.submitter;
    const T::TimePoint now = T::Now();
    if (!sound.musicStarted || !restarted.Read().empty()) {
        StartMusic(sound, *content, now);
    }

    const bool wasHolding = sound.holding;
    for ([[maybe_unused]] const world::LineHoldStarted* message : started.Read()) {
        if (sound.holding) {
            continue;
        }
        sound.holding = true;
        // The new hold cuts the last one, fading or not (BeginLineHold's StopVoice).
        Stop(submitter, sound.fadingHold, now);
        Stop(submitter, sound.hold, now);
        sound.hold = Play(submitter, content->clips[static_cast<std::size_t>(SoundId::Hold)], now, kHoldGain);
    }
    for (const world::LineHoldEnded* message : ended.Read()) {
        if (!sound.holding) {
            continue;
        }
        sound.holding = false;
        // Faded out over 0.05 s, then stopped (GameAudio::UpdateHoldFade).
        Stop(submitter, sound.fadingHold, now);
        SetGain(submitter, sound.hold, 0.0f, now, Seconds(kHoldFadeSeconds));
        static_cast<void>(submitter.Stop(sound.hold, now + Seconds(kHoldFadeSeconds)));
        sound.fadingHold = std::exchange(sound.hold, {});
        if (message->reason == world::LineHoldEnd::Released) {
            Stop(submitter, sound.relax, now);
            sound.relax = Play(submitter, content->clips[static_cast<std::size_t>(SoundId::Relax)], now, kRelaxGain);
        }
    }

    if (sound.holding != wasHolding) {
        Duck(sound, sound.holding ? kDuckGain : 1.0f, now);
    }
    return {};
}

} // namespace

float GainRamp::At(const T::TimePoint time) const noexcept {
    if (time <= start) return from;
    if (time >= End() || length <= T::Duration::zero()) return to;
    const double progress = std::chrono::duration<double>(time - start) / std::chrono::duration<double>(length);
    return from + static_cast<float>(progress) * (to - from);
}

GainRamp PlanDuck(const GainRamp& current, const float target, const T::TimePoint now,
                  const T::Duration scheduleDelay) {
    const float from = current.At(now);
    const float duration = target < from ? kDuckAttackSeconds : kDuckReleaseSeconds;
    const T::Duration length = Seconds(static_cast<double>(std::abs(target - from) / (1.0f - kDuckGain) * duration));
    return GainRamp{now + scheduleDelay, from, target, length};
}

void ConfigureProduct(O::ProductSetup& setup) {
    const auto* registry = setup.Buffer().Find<GYO::Engine::Asset::AssetLoaderRegistry>();
    if (registry == nullptr) {
        setup.Report("NoAssetStore", "the asset store is not there (world_asset is not carried)");
        return;
    }
    if (std::optional<std::string> refused = RegisterLoaders(*registry)) {
        setup.Report("LoaderRefused", std::move(*refused));
        return;
    }
    AU::Mixer* mixer = setup.Buffer().Find<AU::Mixer>();
    if (mixer == nullptr) {
        GYO::Engine::Base::Log(GYO::Engine::Base::LogLevel::Info, "object_connect",
                               "no mixer in the Product (sdl_audio is not carried): the game is silent");
        return;
    }
    auto submitter = mixer->CreateSubmitter();
    if (!submitter) {
        setup.Report("NoSubmitter", "the mixer gave no submitter: " + GYO::Engine::Base::Describe(submitter.error()));
        return;
    }
    setup.Buffer().Create<SoundPort>(std::move(submitter).value());
}

void BuildSound(O::WorldSetup& setup) {
    K::Registrar& r = setup.Registrar();
    r.Resource<GameSound>({.name = "object_connect.GameSound"});
    K::SystemOptions options;
    options.name = "object_connect.PlaySounds";
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::string(O::DefaultSchedules::update);
    mount.after = {"object_connect.PresentMenus"};
    mount.conditions = {O::AfterGenerating()};
    options.mounts.push_back(std::move(mount));
    r.AddSystem<&PlaySounds>(std::move(options));
    SoundPort* port = setup.ProductBuffer().Find<SoundPort>();
    setup.Prepare([port](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        GameSound sound;
        sound.submitter = port != nullptr ? &port->submitter : nullptr;
        world.SetResource(std::move(sound));
        return {};
    });
}

} // namespace object_connect::runtime
