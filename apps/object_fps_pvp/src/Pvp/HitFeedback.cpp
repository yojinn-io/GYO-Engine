#include "RetroFPS/Pvp/HitFeedback.hpp"

#include "RetroFPS/Pvp/Movement.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/math/scalar/Scalar.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <numbers>

namespace fps::pvp {
namespace {

std::array<float, 3> Color(const nlohmann::json& value) {
    if (!value.is_array() || value.size() != 3) throw std::runtime_error("colors are three components");
    std::array<float, 3> color{};
    for (std::size_t i = 0; i < 3; ++i) {
        color[i] = value[i].get<float>();
        if (!std::isfinite(color[i]) || color[i] < 0 || color[i] > 1) throw std::runtime_error("color components are in [0, 1]");
    }
    return color;
}

double Seconds(const nlohmann::json& value) {
    const auto seconds = value.get<double>();
    if (!std::isfinite(seconds) || seconds <= 0) throw std::runtime_error("durations are positive seconds");
    return seconds;
}

float Positive(const nlohmann::json& value) {
    const auto number = value.get<float>();
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
        const auto json = nlohmann::json::parse(text);
        if (json.at("version").get<int>() != 1) throw std::runtime_error("unsupported version");
        HitFeedbackSettings settings;
        const auto& flash = json.at("flash");
        settings.flash.color = Color(flash.at("color"));
        settings.flash.peakAlpha = flash.at("peak_alpha").get<float>();
        if (!std::isfinite(settings.flash.peakAlpha) || settings.flash.peakAlpha <= 0 || settings.flash.peakAlpha > 1)
            throw std::runtime_error("flash peak alpha is in (0, 1]");
        settings.flash.seconds = Seconds(flash.at("seconds"));
        const auto& hp = json.at("hp_highlight");
        settings.hpHighlight.color = Color(hp.at("color"));
        settings.hpHighlight.seconds = Seconds(hp.at("seconds"));
        const auto& direction = json.at("direction");
        settings.direction.radiusPixels = Positive(direction.at("radius_pixels"));
        settings.direction.arcRadians = Engine::Math::DegreesToRadians(Positive(direction.at("arc_degrees")));
        settings.direction.dotPixels = Positive(direction.at("dot_pixels"));
        settings.direction.dots = direction.at("dots").get<std::uint32_t>();
        if (settings.direction.dots < 2 || settings.direction.dots > 64) throw std::runtime_error("direction dots are 2-64");
        settings.direction.color = Color(direction.at("color"));
        settings.direction.seconds = Seconds(direction.at("seconds"));
        const auto& shake = json.at("shake");
        settings.shake.pitchRadians = Engine::Math::DegreesToRadians(Positive(shake.at("pitch_degrees")));
        settings.shake.rollRadians = Engine::Math::DegreesToRadians(Positive(shake.at("roll_degrees")));
        settings.shake.seconds = Seconds(shake.at("seconds"));
        return settings;
    } catch (const std::exception& exception) {
        error = "hit feedback settings: " + std::string(exception.what());
        return std::nullopt;
    }
}

void LocalHitFeedback::Observe(const CombatState& own, const LifeState lifeState, const Engine::Math::Vec3 ownPosition,
                               const std::optional<Engine::Math::Vec3> attackerPosition, const double nowSeconds) noexcept {
    if (!seen_ || own.lifeGeneration != lifeGeneration_) {
        // A join or a new life: remember the state, play nothing old.
        seen_ = true;
        lifeGeneration_ = own.lifeGeneration;
        damageCount_ = own.damageCount;
        active_ = false;
        attackerBearing_.reset();
        return;
    }
    if (lifeState == LifeState::Dead) {
        damageCount_ = own.damageCount;
        active_ = false;
        return;
    }
    if (own.damageCount <= damageCount_) return;
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
            sample.directionRadians = Engine::Math::WrapRadians(*attackerBearing_ - viewYaw);
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
    view_.yaw = Engine::Math::WrapRadians(view_.yaw + deltaX * MouseRadiansPerCount);
    view_.pitch = Engine::Math::Clamp(view_.pitch + deltaY * MouseRadiansPerCount,
                                      -MovementMaximumPitch, MovementMaximumPitch);
}

void ClientView::Present(const HitFeedbackSample& feedback) noexcept {
    shakePitch_ = feedback.shakePitchRadians;
    shakeRoll_ = feedback.shakeRollRadians;
}

CameraAngles ClientView::Camera() const noexcept {
    return {view_.pitch + shakePitch_, view_.yaw, shakeRoll_};
}

} // namespace fps::pvp
