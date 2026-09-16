#pragma once

#include "RetroFPS/Data/GameData.hpp"

#include <cstdint>

namespace fps {

class WeaponController;

enum class WeaponAction : std::uint8_t { Draw, Idle, Shoot, Reload, Hide, Holstered };

struct WeaponPresentationSnapshot final {
    WeaponDefinitionId weaponId{};
    WeaponAction action{WeaponAction::Holstered};
    float elapsedSeconds{};
    float durationSeconds{};
    std::uint64_t revision{};
};

struct WeaponActionEvent final {
    WeaponDefinitionId weaponId{};
    WeaponAction action{WeaponAction::Holstered};
    std::uint64_t revision{};
};

class WeaponState final {
public:
    WeaponState() noexcept = default;

    [[nodiscard]] const WeaponDefinitionId& GetWeaponId() const noexcept {
        return weaponId_;
    }
    [[nodiscard]] std::uint32_t GetMagazineAmmo() const noexcept { return magazineAmmo_; }
    [[nodiscard]] std::uint32_t GetReserveAmmo() const noexcept { return reserveAmmo_; }
    [[nodiscard]] float GetFireCooldownSeconds() const noexcept {
        return fireCooldownSeconds_;
    }
    [[nodiscard]] bool IsReloading() const noexcept { return action_ == WeaponAction::Reload; }
    [[nodiscard]] float GetReloadElapsedSeconds() const noexcept {
        return IsReloading() ? actionElapsedSeconds_ : 0.0f;
    }
    [[nodiscard]] WeaponAction GetAction() const noexcept { return action_; }
    [[nodiscard]] float GetActionElapsedSeconds() const noexcept { return actionElapsedSeconds_; }
    [[nodiscard]] std::uint64_t GetActionRevision() const noexcept { return actionRevision_; }
    [[nodiscard]] float GetRecoilDegrees() const noexcept { return recoilDegrees_; }
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

private:
    friend class WeaponController;

    WeaponDefinitionId weaponId_{};
    std::uint32_t magazineAmmo_ = 0;
    std::uint32_t reserveAmmo_ = 0;
    float fireCooldownSeconds_ = 0.0f;
    float actionElapsedSeconds_ = 0.0f;
    WeaponAction action_ = WeaponAction::Holstered;
    std::uint64_t actionRevision_ = 0;
    float recoilDegrees_ = 0.0f;
    bool initialized_ = false;
};

struct ShotEvent final {
    WeaponDefinitionId weaponId{};
    float damage = 0.0f;
    float recoilDegrees = 0.0f;
    std::uint32_t magazineAmmoAfterShot = 0;
};

struct WeaponHudSnapshot final {
    WeaponDefinitionId weaponId{};
    std::uint32_t magazineAmmo = 0;
    std::uint32_t reserveAmmo = 0;
    bool reloading = false;
    float reloadProgress = 0.0f;
    float recoilDegrees = 0.0f;
    float crosshairExpansion = 0.0f;
};

struct WeaponControllerSettings final {
    float recoilRecoveryDegreesPerSecond = 8.0f;
    float maximumAccumulatedRecoilDegrees = 12.0f;
};

} // namespace fps
