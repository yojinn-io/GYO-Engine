#pragma once

#include "RetroFPS/Pvp/Combat.hpp"
#include "RetroFPS/Pvp/Movement.hpp"
#include "RetroFPS/Pvp/PvpMatch.hpp"
#include "engine/math/linear/Vec3.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace fps::pvp {

// When this product makes a sound, and how loud on each side. Presentation
// only: it reads snapshots and the presented movement, never changes them, and
// knows nothing of how sound is mixed or played (v8-product-sfx).

enum class SoundCue : std::uint8_t { LocalShot, RemoteShot, HitTaken, HitConfirmed, Reload, Footstep, Landing };
inline constexpr std::size_t SoundCueCount = 7;

// audio/sfx.json (version 1): the clip each cue plays, its volume, and the
// spatial and footstep tuning. Content, so it is edited without code.
struct SoundSettings final {
    struct Cue final {
        std::string clip; // an asset id of type sound
        float volume{};   // 0 to 4
    };
    Cue cues[SoundCueCount]{};
    // Spatial cues are full volume within the reference distance, then fall
    // off as reference / distance, never below the minimum gain.
    float referenceDistance{};
    float minimumGain{};
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
[[nodiscard]] StereoGains SpatialGains(Engine::Math::Vec3 listener, float listenerYaw, Engine::Math::Vec3 source,
                                       float volume, const SoundSettings& settings) noexcept;

// New remote shots in snapshots, in arrival order: a shot is a combat state
// whose last shot action differs from the one seen before in the same life.
// The first state seen of a player's life never plays (a join or a respawn
// would replay an old shot).
class RemoteShotTracker final {
public:
    struct Shot final {
        PlayerId playerId{};
        std::uint64_t tick{};
        Engine::Math::Vec3 position{};
    };
    void Reset() noexcept { seen_.clear(); }
    [[nodiscard]] std::vector<Shot> Observe(const WorldSnapshot& snapshot, PlayerId self);

private:
    struct Seen final {
        std::uint64_t lifeGeneration{};
        ActionId lastShot{};
    };
    std::unordered_map<PlayerId, Seen> seen_;
};

// Footsteps of one presented body: a step for every stride of planar distance
// covered while grounded, and a landing when it touches the ground again. A
// teleport, a new life or losing the body restarts it without a sound.
class FootstepTracker final {
public:
    enum class Step : std::uint8_t { None, Footstep, Landing };
    void Reset() noexcept { *this = {}; }
    [[nodiscard]] Step Observe(Engine::Math::Vec3 position, bool grounded, std::uint64_t lifeGeneration,
                               const SoundSettings& settings) noexcept;

private:
    bool started_{};
    Engine::Math::Vec3 last_{};
    bool grounded_{};
    std::uint64_t life_{};
    float travelled_{};
};

} // namespace fps::pvp
