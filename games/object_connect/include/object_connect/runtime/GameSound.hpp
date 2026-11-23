#pragma once

#include "engine/audio/Mixer.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/time/MonotonicClock.hpp"

#include <optional>
#include <string>

// The game's sound (the original GameAudio, Object_Connect ca4482c,
// src/ObjectConnect/Audio/GameAudio.cpp) on the audio Module's mixer: the
// music (an intro, then a loop), ducked while a vessel is held; the hold
// sound, faded out when the hold ends; the relax sound when it is released.
//
// The mixer is the one sdl_audio puts in the Product's buffer. The game's
// submitter to it lives there too (SoundPort, made once in the product's
// configure), so it outlives the World as the mixer asks. Without a mixer the
// game runs silent, as the original did without its audio backend.

namespace object_connect::runtime {

// The product's own submitter to the mixer, in the Product's buffer.
struct SoundPort final {
    explicit SoundPort(GYO::Engine::Audio::Submitter submitter) : submitter(std::move(submitter)) {}
    GYO::Engine::Audio::Submitter submitter;
};

// The music's duck gain as the mixer applies it: a linear ramp from `from` to
// `to` over `length`, starting at `start` (Engine time). Before `start` the
// gain is `from`.
struct GainRamp final {
    GYO::Engine::Time::TimePoint start{};
    float from = 1.0f;
    float to = 1.0f;
    GYO::Engine::Time::Duration length{};

    [[nodiscard]] float At(GYO::Engine::Time::TimePoint time) const noexcept;
    [[nodiscard]] GYO::Engine::Time::TimePoint End() const noexcept { return start + length; }
};

// The ramp the mixer will run for a duck change to `target` sent at `now`:
// the mixer takes the change about at `now` and freezes the running ramp
// there, holds that gain until its start (`now` plus the mixer's schedule
// delay), then ramps at the original's rate (the full swing 1.0 to 0.4 over
// 0.05 s down, 0.25 s up), so the remaining distance takes its share of that.
[[nodiscard]] GainRamp PlanDuck(const GainRamp& current, float target, GYO::Engine::Time::TimePoint now,
                                GYO::Engine::Time::Duration scheduleDelay);

// What the sound System keeps (the original GameAudio's members).
struct GameSound final {
    // Null: no mixer, the game is silent.
    GYO::Engine::Audio::Submitter* submitter = nullptr;
    bool musicStarted = false;
    GYO::Engine::Audio::SoundHandle intro;
    GYO::Engine::Audio::SoundHandle loop;
    GYO::Engine::Audio::SoundHandle hold;
    // The hold sound of the last hold, fading out until it stops.
    GYO::Engine::Audio::SoundHandle fadingHold;
    GYO::Engine::Audio::SoundHandle relax;
    bool holding = false;
    GainRamp duck;
};

inline void HashValue(GYO::Kernel::Hasher&, const GameSound&) {}

// The product's configure: registers the game's loaders and the Engine loaders
// its content needs, and makes the SoundPort when a mixer is there. A refusal
// is reported (ProductSetup::Report); a missing mixer is not an error.
void ConfigureProduct(GYO::Engine::Orchestration::ProductSetup& setup);

// Registers GameSound and the sound System (Main/Update after the game's
// Systems and PresentMenus, once generation has ended). Called by BuildRuntime.
void BuildSound(GYO::Engine::Orchestration::WorldSetup& setup);

// The original's sound constants (GameAudio.cpp:20-33).
inline constexpr float kIntroGain = 0.9f;
inline constexpr float kLoopGain = 0.4f;
inline constexpr float kHoldGain = 0.65f;
inline constexpr float kRelaxGain = 2.0f;
inline constexpr float kDuckGain = 0.4f;
inline constexpr float kDuckAttackSeconds = 0.05f;
inline constexpr float kDuckReleaseSeconds = 0.25f;
inline constexpr float kHoldFadeSeconds = 0.05f;

} // namespace object_connect::runtime
