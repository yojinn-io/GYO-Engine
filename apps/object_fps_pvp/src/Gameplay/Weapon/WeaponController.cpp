#include "RetroFPS/Gameplay/Weapon/WeaponController.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fps {
namespace {

[[nodiscard]] bool ValidateDefinition(
    const WeaponDefinition& definition, std::string& error) {
    if (definition.id.empty()) {
        error = "weapon ID must not be empty";
        return false;
    }
    if (!std::isfinite(definition.damage) || definition.damage <= 0.0f) {
        error = "weapon damage must be finite and greater than zero";
        return false;
    }
    if (definition.magazineCapacity == 0) {
        error = "weapon magazine capacity must be greater than zero";
        return false;
    }
    if (!std::isfinite(definition.recoilDegrees) || definition.recoilDegrees < 0.0f) {
        error = "weapon recoil must be finite and non-negative";
        return false;
    }
    if (!std::isfinite(definition.fireIntervalSeconds) ||
        definition.fireIntervalSeconds <= 0.0f) {
        error = "weapon fire interval must be finite and greater than zero";
        return false;
    }
    if (!std::isfinite(definition.reloadSeconds) || definition.reloadSeconds <= 0.0f) {
        error = "weapon reload duration must be finite and greater than zero";
        return false;
    }
    if (!std::isfinite(definition.drawSeconds) || definition.drawSeconds <= 0.0f ||
        !std::isfinite(definition.hideSeconds) || definition.hideSeconds <= 0.0f) {
        error = "weapon draw/hide duration must be finite and greater than zero";
        return false;
    }
    return true;
}

} // namespace

bool ValidateWeaponControllerSettings(
    const WeaponControllerSettings& settings, std::string& error) {
    error.clear();
    if (!std::isfinite(settings.recoilRecoveryDegreesPerSecond) ||
        settings.recoilRecoveryDegreesPerSecond <= 0.0f) {
        error = "weapon recoil recovery must be finite and greater than zero";
        return false;
    }
    if (!std::isfinite(settings.maximumAccumulatedRecoilDegrees) ||
        settings.maximumAccumulatedRecoilDegrees <= 0.0f) {
        error = "weapon maximum accumulated recoil must be finite and greater than zero";
        return false;
    }
    return true;
}

bool WeaponController::Configure(
    WeaponDefinition definition,
    WeaponControllerSettings settings,
    std::string& error) {
    error.clear();
    if (!ValidateDefinition(definition, error) ||
        !ValidateWeaponControllerSettings(settings, error)) {
        return false;
    }

    definition_ = std::move(definition);
    settings_ = settings;
    configured_ = true;
    shotEvents_.clear();
    shotEvents_.reserve(1);
    actionEvents_.clear();
    actionEvents_.reserve(2);
    return true;
}

bool WeaponController::Configure(
    WeaponDefinition definition, std::string& error) {
    return Configure(std::move(definition), WeaponControllerSettings{}, error);
}

bool WeaponController::Initialize(
    WeaponState& state, std::string& error) {
    error.clear();
    if (!configured_) {
        error = "weapon controller must be configured before state initialization";
        return false;
    }

    state = WeaponState{};
    state.weaponId_ = definition_.id;
    state.magazineAmmo_ = definition_.magazineCapacity;
    state.reserveAmmo_ = definition_.reserveAmmo;
    state.initialized_ = true;
    shotEvents_.clear();
    actionEvents_.clear();
    BeginAction(state, WeaponAction::Draw);
    return true;
}

void WeaponController::ResetVisualFeedback(WeaponState& state) const noexcept {
    if (configured_ && state.initialized_ && state.weaponId_ == definition_.id) {
        state.recoilDegrees_ = 0.0f;
    }
}

float WeaponController::ActionDuration(const WeaponAction action) const noexcept {
    switch (action) {
    case WeaponAction::Draw: return definition_.drawSeconds;
    case WeaponAction::Shoot: return definition_.fireIntervalSeconds;
    case WeaponAction::Reload: return definition_.reloadSeconds;
    case WeaponAction::Hide: return definition_.hideSeconds;
    default: return 0.0f;
    }
}

void WeaponController::BeginAction(WeaponState& state, const WeaponAction action) {
    state.action_ = action;
    state.actionElapsedSeconds_ = 0.0f;
    ++state.actionRevision_;
    actionEvents_.push_back({definition_.id, action, state.actionRevision_});
}

void WeaponController::Update(
    WeaponState& state, const WeaponControlInput& input, const float deltaSeconds) {
    shotEvents_.clear();
    actionEvents_.clear();
    if (!configured_ || !state.initialized_ || state.weaponId_ != definition_.id ||
        !std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
        return;
    }

    state.fireCooldownSeconds_ = (std::max)(0.0f, state.fireCooldownSeconds_ - deltaSeconds);
    state.recoilDegrees_ = (std::max)(0.0f, state.recoilDegrees_ -
        settings_.recoilRecoveryDegreesPerSecond * deltaSeconds);
    const float duration = ActionDuration(state.action_);
    if (duration > 0.0f) {
        state.actionElapsedSeconds_ = (std::min)(duration, state.actionElapsedSeconds_ + deltaSeconds);
    }

    // Reload/equip transitions are intentionally non-interruptible. Commands
    // received during them are discarded, including their completion frame.
    if (state.action_ == WeaponAction::Reload || state.action_ == WeaponAction::Draw ||
        state.action_ == WeaponAction::Hide) {
        if (state.actionElapsedSeconds_ >= duration) {
            const WeaponAction completed = state.action_;
            if (completed == WeaponAction::Reload) {
                const std::uint32_t missing = definition_.magazineCapacity - state.magazineAmmo_;
                const std::uint32_t transferred = (std::min)(missing, state.reserveAmmo_);
                state.magazineAmmo_ += transferred;
                state.reserveAmmo_ -= transferred;
            }
            BeginAction(state, completed == WeaponAction::Hide ? WeaponAction::Holstered : WeaponAction::Idle);
        }
        return;
    }
    if (state.action_ == WeaponAction::Shoot && state.actionElapsedSeconds_ >= duration) {
        BeginAction(state, WeaponAction::Idle);
    }
    if (input.holsterTogglePressed) {
        BeginAction(state, state.action_ == WeaponAction::Holstered ? WeaponAction::Draw : WeaponAction::Hide);
        return;
    }
    if (state.action_ == WeaponAction::Holstered) {
        return;
    }
    if (input.reloadPressed && state.magazineAmmo_ < definition_.magazineCapacity && state.reserveAmmo_ > 0) {
        BeginAction(state, WeaponAction::Reload);
        return;
    }
    const bool fireRequested = definition_.automatic ? input.fireHeld : input.firePressed;
    if (!fireRequested || state.fireCooldownSeconds_ > 0.0f || state.magazineAmmo_ == 0) {
        return;
    }
    --state.magazineAmmo_;
    state.fireCooldownSeconds_ = definition_.fireIntervalSeconds;
    state.recoilDegrees_ = (std::min)(settings_.maximumAccumulatedRecoilDegrees,
                                    state.recoilDegrees_ + definition_.recoilDegrees);
    BeginAction(state, WeaponAction::Shoot);
    shotEvents_.push_back({definition_.id, definition_.damage, definition_.recoilDegrees, state.magazineAmmo_});
}

WeaponPresentationSnapshot WeaponController::MakePresentationSnapshot(const WeaponState& state) const {
    if (!configured_ || !state.initialized_ || state.weaponId_ != definition_.id) {
        return {};
    }
    return {state.weaponId_, state.action_, state.actionElapsedSeconds_,
            ActionDuration(state.action_), state.actionRevision_};
}

WeaponHudSnapshot WeaponController::MakeHudSnapshot(const WeaponState& state) const {
    if (!configured_ || !state.initialized_ || state.weaponId_ != definition_.id) {
        return {};
    }
    const float reloadProgress = state.IsReloading()
        ? std::clamp(state.actionElapsedSeconds_ / definition_.reloadSeconds, 0.0f, 1.0f) : 0.0f;
    return {state.weaponId_, state.magazineAmmo_, state.reserveAmmo_, state.IsReloading(),
            reloadProgress, state.recoilDegrees_,
            std::clamp(state.recoilDegrees_ / settings_.maximumAccumulatedRecoilDegrees, 0.0f, 1.0f)};
}

} // namespace fps
