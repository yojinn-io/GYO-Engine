#include "object_fps_pvp/present/SoundCues.hpp"

#include "data/JsonFields.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace object_fps_pvp::present {

namespace Json = GYO::Engine::Json;
namespace Math = GYO::Engine::Math;

namespace {

constexpr const char* CueNames[SoundCueCount] = {"local_shot", "remote_shot", "hit_taken", "hit_confirmed",
                                                 "footstep", "landing"};

float Bounded(const Json::Value& value, const float low, const float high, const char* what) {
    const auto number = static_cast<float>(data::Number(value, what));
    if (!std::isfinite(number) || number < low || number > high) throw std::runtime_error(what);
    return number;
}

float PlanarDistance(const Math::Vec3 a, const Math::Vec3 b) noexcept {
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
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the settings");
        if (data::Integer(data::Member(json, "version"), "version") != 3) throw std::runtime_error("unsupported version");
        SoundSettings settings;
        const auto cues = data::AsObject(data::Member(json, "cues"), "cues");
        for (std::size_t i = 0; i < SoundCueCount; ++i) {
            const auto cue = data::AsObject(data::Member(cues, CueNames[i]), CueNames[i]);
            settings.cues[i].clip = data::String(data::Member(cue, "clip"), "clip");
            if (settings.cues[i].clip.empty()) throw std::runtime_error("a cue names its clip");
            settings.cues[i].volume = Bounded(data::Member(cue, "volume"), 0, 4, "volumes are in [0, 4]");
        }
        const auto spatial = data::AsObject(data::Member(json, "spatial"), "spatial");
        settings.referenceDistance =
            Bounded(data::Member(spatial, "reference_distance"), 0.01F, 1000, "the reference distance is positive");
        settings.minimumGain = Bounded(data::Member(spatial, "minimum_gain"), 0, 1, "the minimum gain is in [0, 1]");
        const auto footsteps = data::AsObject(data::Member(json, "footsteps"), "footsteps");
        settings.strideDistance = Bounded(data::Member(footsteps, "stride_distance"), 0.01F, 100, "the stride is positive");
        settings.teleportDistance =
            Bounded(data::Member(footsteps, "teleport_distance"), 0.01F, 1000, "the teleport distance is positive");
        // Version 2's section, moved to the weapon's event sounds: a file
        // still carrying it would sound nothing it names.
        if (data::HasMember(json, "animation_events"))
            throw std::runtime_error("animation event sounds are the weapon's event sounds, not animation_events");
        return settings;
    } catch (const std::exception& failure) {
        error = std::string("sound settings: ") + failure.what();
        return std::nullopt;
    }
}

StereoGains CentredGains(const float volume) noexcept { return {volume, volume}; }

StereoGains SpatialGains(const Math::Vec3 listener, const float listenerYaw, const Math::Vec3 source,
                         const float volume, const SoundSettings& settings) noexcept {
    const float dx = source.x - listener.x, dz = source.z - listener.z;
    const float distance = std::sqrt(dx * dx + dz * dz + (source.y - listener.y) * (source.y - listener.y));
    const float falloff = Math::Max(settings.minimumGain,
        distance <= settings.referenceDistance ? 1.0F : settings.referenceDistance / distance);
    // At the listener, or straight above or below, the cue is centred.
    float pan = 0;
    if (dx * dx + dz * dz > 1e-6F) pan = std::sin(Math::WrapRadians(std::atan2(dx, dz) - listenerYaw));
    // Constant power: left = cos, right = sin of (pan + 1) * pi / 4, equal at the centre.
    const float angle = (pan + 1.0F) * std::numbers::pi_v<float> * 0.25F;
    const float scale = volume * falloff * std::numbers::sqrt2_v<float>;
    return {Math::Min(4.0F, scale * std::cos(angle)), Math::Min(4.0F, scale * std::sin(angle))};
}

FootstepTracker::Step FootstepTracker::Observe(const Math::Vec3 position, const bool grounded,
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

} // namespace object_fps_pvp::present
