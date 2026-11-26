#pragma once

#include "object_fps_pvp/present/HitFeedback.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace object_fps_pvp::present {

// When this game makes a sound, and how loud on each side. Presentation only:
// it reads the game state, never changes it, and knows nothing of how sound is
// mixed or played.

enum class SoundCue : std::uint8_t { LocalShot, RemoteShot, HitTaken, HitConfirmed, Footstep, Landing };
inline constexpr std::size_t SoundCueCount = 6;

// audio/sfx.json (version 4): the clip each cue plays, its volume, how far a
// spatial cue carries, and the spatial and footstep tuning. Content, so it is
// edited without code. The sounds of animation events are the weapon's own
// data, not here.
struct SoundSettings final {
    struct Cue final {
        std::string clip; // an asset id of type sound
        float volume{};   // 0 to 4
        // A spatial cue farther than this from the listener is not played
        // (maximum_distance; absent: heard at any distance).
        float maximumDistance = std::numeric_limits<float>::infinity();
    };
    Cue cues[SoundCueCount]{};
    // Spatial cues are full volume within the reference distance, then fall
    // off as reference / distance, never below the minimum gain.
    float referenceDistance{};
    float minimumGain{};
    // A spatial cue the map stands between it and the listener sounds at this
    // fraction of its gain (occluded_gain, 0 to 1): gain only, no filter.
    float occludedGain{};
    // Planar distance between two footsteps while grounded, and the planar jump
    // beyond which a position change is a teleport (respawn, correction), not a step.
    float strideDistance{};
    float teleportDistance{};
};

[[nodiscard]] std::optional<SoundSettings> ParseSoundSettings(const std::string& text, std::string& error);
[[nodiscard]] const char* SoundCueName(SoundCue cue) noexcept;

struct StereoGains final {
    float left{}, right{};
};

// A non-spatial cue: the volume on both sides.
[[nodiscard]] StereoGains CentredGains(float volume) noexcept;
// A cue at a world position heard from the listener: constant-power panning by
// the bearing relative to the listener's yaw (yaw turns +Z toward +X, as in
// HitFeedback), and the settings' distance falloff.
[[nodiscard]] StereoGains SpatialGains(GYO::Engine::Math::Vec3 listener, float listenerYaw,
                                       GYO::Engine::Math::Vec3 source, float volume,
                                       const SoundSettings& settings) noexcept;
// One spatial cue as the listener hears it: nullopt beyond the cue's maximum
// distance (not played); else SpatialGains at the cue's volume, times the
// occluded gain when the map stands between them (occluded: the caller's
// query of the map).
[[nodiscard]] std::optional<StereoGains> HeardGains(SoundCue cue, GYO::Engine::Math::Vec3 listener, float listenerYaw,
                                                    GYO::Engine::Math::Vec3 source, bool occluded,
                                                    const SoundSettings& settings) noexcept;

// Whether a hit the local player took sounds: a new hit does, also the one
// that kills (unlike the flash, which death suppresses); the first state of a
// life (a join or a respawn carrying old hits) never does.
[[nodiscard]] constexpr bool HitTakenSounds(const HitFeedbackOutcome outcome) noexcept {
    return outcome == HitFeedbackOutcome::Played || outcome == HitFeedbackOutcome::SuppressedDead;
}

// Footsteps of one presented body: a step for every stride of planar distance
// covered while grounded and standing (a crouched walk is silent, and the
// distance it covers does not count toward the next step), and a landing when
// it touches the ground again. A teleport, a new life or losing the body
// restarts it without a sound.
class FootstepTracker final {
public:
    enum class Step : std::uint8_t { None, Footstep, Landing };
    void Reset() noexcept { *this = {}; }
    [[nodiscard]] Step Observe(GYO::Engine::Math::Vec3 position, bool grounded, bool crouched, std::uint64_t lifeGeneration,
                               const SoundSettings& settings) noexcept;

private:
    bool started_{};
    GYO::Engine::Math::Vec3 last_{};
    bool grounded_{};
    std::uint64_t life_{};
    float travelled_{};
};

} // namespace object_fps_pvp::present
