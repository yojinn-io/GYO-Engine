#include "object_fps_pvp/present/HitFeedback.hpp"

#include "object_fps_pvp/sim/ViewLimits.hpp"
#include "data/JsonFields.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <cmath>
#include <stdexcept>

namespace object_fps_pvp::present {

namespace Json = GYO::Engine::Json;
namespace Math = GYO::Engine::Math;

namespace {

std::array<float, 3> Color(const Json::Value& value) {
    const auto array = value.AsArray();
    if (!array || array.value().Size() != 3) throw std::runtime_error("colors are three components");
    std::array<float, 3> color{};
    std::size_t i = 0;
    for (const auto component : array.value()) {
        color[i] = static_cast<float>(data::Number(component, "a color component"));
        if (!std::isfinite(color[i]) || color[i] < 0 || color[i] > 1) throw std::runtime_error("color components are in [0, 1]");
        ++i;
    }
    return color;
}

double Seconds(const Json::Value& value) {
    const auto seconds = data::Number(value, "a duration");
    if (!std::isfinite(seconds) || seconds <= 0) throw std::runtime_error("durations are positive seconds");
    return seconds;
}

float Positive(const Json::Value& value) {
    const auto number = static_cast<float>(data::Number(value, "a size or amplitude"));
    if (!std::isfinite(number) || number <= 0) throw std::runtime_error("sizes and amplitudes are positive");
    return number;
}

// Linear fade from 1 at the start to 0 after the duration.
float Fade(double elapsed, double seconds) {
    return elapsed < 0 || elapsed >= seconds ? 0.0F : static_cast<float>(1.0 - elapsed / seconds);
}

} // namespace

std::optional<HitFeedbackSettings> ParseHitFeedbackSettings(const std::string& text, std::string& error) {
    error.clear();
    try {
        const auto document = data::ParseDocument(text);
        const auto json = data::AsObject(document.Root(), "the settings");
        if (data::Integer(data::Member(json, "version"), "version") != 1) throw std::runtime_error("unsupported version");
        HitFeedbackSettings settings;
        const auto flash = data::AsObject(data::Member(json, "flash"), "flash");
        settings.flash.color = Color(data::Member(flash, "color"));
        settings.flash.peakAlpha = static_cast<float>(data::Number(data::Member(flash, "peak_alpha"), "peak_alpha"));
        if (!std::isfinite(settings.flash.peakAlpha) || settings.flash.peakAlpha <= 0 || settings.flash.peakAlpha > 1)
            throw std::runtime_error("flash peak alpha is in (0, 1]");
        settings.flash.seconds = Seconds(data::Member(flash, "seconds"));
        const auto hp = data::AsObject(data::Member(json, "hp_highlight"), "hp_highlight");
        settings.hpHighlight.color = Color(data::Member(hp, "color"));
        settings.hpHighlight.seconds = Seconds(data::Member(hp, "seconds"));
        const auto direction = data::AsObject(data::Member(json, "direction"), "direction");
        settings.direction.radiusPixels = Positive(data::Member(direction, "radius_pixels"));
        settings.direction.arcRadians = Math::DegreesToRadians(Positive(data::Member(direction, "arc_degrees")));
        settings.direction.dotPixels = Positive(data::Member(direction, "dot_pixels"));
        const auto dots = data::Unsigned(data::Member(direction, "dots"), "dots");
        if (dots < 2 || dots > 64) throw std::runtime_error("direction dots are 2-64");
        settings.direction.dots = static_cast<std::uint32_t>(dots);
        settings.direction.color = Color(data::Member(direction, "color"));
        settings.direction.seconds = Seconds(data::Member(direction, "seconds"));
        const auto shake = data::AsObject(data::Member(json, "shake"), "shake");
        settings.shake.pitchRadians = Math::DegreesToRadians(Positive(data::Member(shake, "pitch_degrees")));
        settings.shake.rollRadians = Math::DegreesToRadians(Positive(data::Member(shake, "roll_degrees")));
        settings.shake.seconds = Seconds(data::Member(shake, "seconds"));
        return settings;
    } catch (const std::exception& exception) {
        error = "hit feedback settings: " + std::string(exception.what());
        return std::nullopt;
    }
}

HitFeedbackObservation LocalHitFeedback::Observe(const LifeHitRecord& own, const sim::LifeState lifeState,
                                                const Math::Vec3 ownPosition,
                                                const std::optional<Math::Vec3> attackerPosition,
                                                const double nowSeconds) noexcept {
    if (!seen_ || own.lifeGeneration != lifeGeneration_) {
        // A join or a new life: remember the state, play nothing old.
        seen_ = true;
        lifeGeneration_ = own.lifeGeneration;
        damageCount_ = own.damageCount;
        active_ = false;
        attackerBearing_.reset();
        if (own.damageCount == 0) return {};
        return {HitFeedbackOutcome::SuppressedNewLife, own.damageCount};
    }
    if (lifeState == sim::LifeState::Dead) {
        const auto hits = own.damageCount > damageCount_ ? own.damageCount - damageCount_ : 0U;
        damageCount_ = own.damageCount;
        active_ = false;
        if (hits == 0) return {};
        return {HitFeedbackOutcome::SuppressedDead, hits};
    }
    if (own.damageCount <= damageCount_) return {};
    const auto hits = own.damageCount - damageCount_;
    damageCount_ = own.damageCount;
    active_ = true;
    startSeconds_ = nowSeconds;
    attackerBearing_.reset();
    if (attackerPosition) {
        const float dx = attackerPosition->x - ownPosition.x;
        const float dz = attackerPosition->z - ownPosition.z;
        // Yaw turns +Z toward +X, so the bearing of (dx, dz) is atan2(dx, dz).
        if (dx * dx + dz * dz > 1e-6F) attackerBearing_ = std::atan2(dx, dz);
    }
    return {HitFeedbackOutcome::Played, hits};
}

HitFeedbackSample LocalHitFeedback::Sample(const double nowSeconds, const float viewYaw,
                                           const HitFeedbackSettings& settings) const noexcept {
    HitFeedbackSample sample;
    if (!active_ || !std::isfinite(nowSeconds)) return sample;
    const double elapsed = nowSeconds - startSeconds_;
    sample.flashAlpha = settings.flash.peakAlpha * Fade(elapsed, settings.flash.seconds);
    sample.hpHighlighted = elapsed >= 0 && elapsed < settings.hpHighlight.seconds;
    const float shake = Fade(elapsed, settings.shake.seconds);
    // One damped swing: up and to the side, settling as it fades.
    const float swing = shake * shake;
    sample.shakePitchRadians = -settings.shake.pitchRadians * swing;
    sample.shakeRollRadians = settings.shake.rollRadians * swing;
    if (attackerBearing_) {
        const float alpha = Fade(elapsed, settings.direction.seconds);
        if (alpha > 0) {
            sample.directionRadians = Math::WrapRadians(*attackerBearing_ - viewYaw);
            sample.directionAlpha = alpha;
        }
    }
    return sample;
}

void ClientView::Reset(const float yaw, const float pitch) noexcept {
    view_ = {yaw, pitch};
    shakePitch_ = shakeRoll_ = 0;
}

void ClientView::Turn(const float deltaX, const float deltaY) noexcept {
    view_.yaw = Math::WrapRadians(view_.yaw + deltaX * MouseRadiansPerCount);
    view_.pitch = Math::Clamp(view_.pitch + deltaY * MouseRadiansPerCount,
                              -sim::MaximumPitchRadians, sim::MaximumPitchRadians);
}

void ClientView::Present(const HitFeedbackSample& feedback) noexcept {
    shakePitch_ = feedback.shakePitchRadians;
    shakeRoll_ = feedback.shakeRollRadians;
}

CameraAngles ClientView::Camera() const noexcept {
    return {view_.pitch + shakePitch_, view_.yaw, shakeRoll_};
}

} // namespace object_fps_pvp::present
