#include "RetroFPS/Pvp/SoundCues.hpp"

#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace fps::pvp {
namespace {

constexpr const char* CueNames[SoundCueCount] = {"local_shot", "remote_shot", "hit_taken", "hit_confirmed",
                                                 "reload", "footstep", "landing"};

float Bounded(const nlohmann::json& value, const float low, const float high, const char* what) {
    const auto number = value.get<float>();
    if (!std::isfinite(number) || number < low || number > high) throw std::runtime_error(what);
    return number;
}

float PlanarDistance(const Engine::Math::Vec3 a, const Engine::Math::Vec3 b) noexcept {
    const float dx = b.x - a.x, dz = b.z - a.z;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

const char* SoundCueName(const SoundCue cue) noexcept {
    const auto index = static_cast<std::size_t>(cue);
    return index < SoundCueCount ? CueNames[index] : "unknown";
}

std::optional<SoundSettings> ParseSoundSettings(const std::string& text, std::string& error) {
    error.clear();
    try {
        const auto json = nlohmann::json::parse(text);
        if (json.at("version").get<int>() != 1) throw std::runtime_error("unsupported version");
        SoundSettings settings;
        const auto& cues = json.at("cues");
        for (std::size_t i = 0; i < SoundCueCount; ++i) {
            const auto& cue = cues.at(CueNames[i]);
            settings.cues[i].clip = cue.at("clip").get<std::string>();
            if (settings.cues[i].clip.empty()) throw std::runtime_error("a cue names its clip");
            settings.cues[i].volume = Bounded(cue.at("volume"), 0, 4, "volumes are in [0, 4]");
        }
        const auto& spatial = json.at("spatial");
        settings.referenceDistance = Bounded(spatial.at("reference_distance"), 0.01F, 1000, "the reference distance is positive");
        settings.minimumGain = Bounded(spatial.at("minimum_gain"), 0, 1, "the minimum gain is in [0, 1]");
        const auto& footsteps = json.at("footsteps");
        settings.strideDistance = Bounded(footsteps.at("stride_distance"), 0.01F, 100, "the stride is positive");
        settings.teleportDistance = Bounded(footsteps.at("teleport_distance"), 0.01F, 1000, "the teleport distance is positive");
        return settings;
    } catch (const std::exception& failure) {
        error = std::string("sound settings: ") + failure.what();
        return std::nullopt;
    }
}

StereoGains CentredGains(const float volume) noexcept { return {volume, volume}; }

StereoGains SpatialGains(const Engine::Math::Vec3 listener, const float listenerYaw, const Engine::Math::Vec3 source,
                         const float volume, const SoundSettings& settings) noexcept {
    const float dx = source.x - listener.x, dz = source.z - listener.z;
    const float distance = std::sqrt(dx * dx + dz * dz + (source.y - listener.y) * (source.y - listener.y));
    const float falloff = Engine::Math::Max(settings.minimumGain,
        distance <= settings.referenceDistance ? 1.0F : settings.referenceDistance / distance);
    // At the listener, or straight above or below, the cue is centred.
    float pan = 0;
    if (dx * dx + dz * dz > 1e-6F) pan = std::sin(Engine::Math::WrapRadians(std::atan2(dx, dz) - listenerYaw));
    // Constant power: left = cos, right = sin of (pan + 1) * pi / 4, equal at the centre.
    const float angle = (pan + 1.0F) * std::numbers::pi_v<float> * 0.25F;
    const float scale = volume * falloff * std::numbers::sqrt2_v<float>;
    return {Engine::Math::Min(4.0F, scale * std::cos(angle)), Engine::Math::Min(4.0F, scale * std::sin(angle))};
}

double ReloadSoundOffsetSeconds(const double reloadSeconds, const double clipSeconds) noexcept {
    if (!std::isfinite(reloadSeconds) || !std::isfinite(clipSeconds)) return 0;
    return Engine::Math::Max(0.0, reloadSeconds - clipSeconds);
}

std::vector<RemoteShotTracker::Shot> RemoteShotTracker::Observe(const WorldSnapshot& snapshot, const PlayerId self) {
    std::vector<Shot> shots;
    for (const auto& combat : snapshot.combat) {
        if (combat.playerId == self) continue;
        const auto found = seen_.find(combat.playerId);
        const bool newLife = found == seen_.end() || found->second.lifeGeneration != combat.lifeGeneration;
        if (!newLife && combat.lastShotActionId != 0 && combat.lastShotActionId != found->second.lastShot) {
            for (const auto& player : snapshot.players)
                if (player.playerId == combat.playerId) shots.push_back({combat.playerId, combat.lastShotTick, player.position});
        }
        seen_[combat.playerId] = {combat.lifeGeneration, combat.lastShotActionId};
    }
    return shots;
}

FootstepTracker::Step FootstepTracker::Observe(const Engine::Math::Vec3 position, const bool grounded,
                                               const std::uint64_t lifeGeneration,
                                               const SoundSettings& settings) noexcept {
    if (!started_ || life_ != lifeGeneration || PlanarDistance(last_, position) > settings.teleportDistance) {
        started_ = true;
        life_ = lifeGeneration;
        last_ = position;
        grounded_ = grounded;
        travelled_ = 0;
        return Step::None;
    }
    const float moved = PlanarDistance(last_, position);
    last_ = position;
    const bool landed = grounded && !grounded_;
    grounded_ = grounded;
    if (landed) {
        travelled_ = 0;
        return Step::Landing;
    }
    if (!grounded) return Step::None;
    travelled_ += moved;
    if (travelled_ < settings.strideDistance) return Step::None;
    travelled_ = std::fmod(travelled_, settings.strideDistance);
    return Step::Footstep;
}

} // namespace fps::pvp
