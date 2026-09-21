#include "../TestSupport.hpp"

#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Gameplay/Weapon/WeaponController.hpp"
#include "RetroFPS/Gameplay/Weapon/WeaponState.hpp"

#include <limits>
#include <stdexcept>
#include <string>

namespace fps::tests {
namespace {

[[nodiscard]] WeaponDefinition MakeDefinition(const bool automatic = false) {
    return {
        "test_weapon",
        12.5f,
        5,
        7,
        2.0f,
        automatic,
        Engine::Asset::AssetId::FromString("object_fps.weapon.test"),
        0.10f,
        1.0f,
    };
}

void ConfigureAndInitialize(
    TestContext& context,
    WeaponController& controller,
    WeaponState& state,
    WeaponDefinition definition,
    const WeaponControllerSettings settings = {}) {
    std::string error;
    const bool configured = controller.Configure(definition, settings, error);
    context.Expect(configured && error.empty(), "valid weapon definition configures");
    if (!configured) {
        throw std::runtime_error("weapon test configuration failed: " + error);
    }

    const bool initialized = controller.Initialize(state, error);
    context.Expect(initialized && error.empty(), "configured weapon state initializes");
    if (!initialized) {
        throw std::runtime_error("weapon test state initialization failed: " + error);
    }
    controller.Update(state, {}, definition.drawSeconds);
}

void TestInitialStateAndSemiAutomaticFire(TestContext& context) {
    WeaponController controller;
    WeaponState state;
    ConfigureAndInitialize(context, controller, state, MakeDefinition());

    const WeaponHudSnapshot initial = controller.MakeHudSnapshot(state);
    context.Expect(
        state.IsInitialized() && state.GetWeaponId() == "test_weapon" &&
            initial.magazineAmmo == 5 && initial.reserveAmmo == 7 &&
            !initial.reloading && NearlyEqual(initial.reloadProgress, 0.0f) &&
            NearlyEqual(
                controller.GetSettings().recoilRecoveryDegreesPerSecond,
                8.0f),
        "weapon state starts with configured magazine and reserve ammo");

    WeaponControlInput heldOnly{};
    heldOnly.fireHeld = true;
    controller.Update(state, heldOnly, 0.0f);
    context.Expect(
        controller.GetShotEvents().empty() && state.GetMagazineAmmo() == 5,
        "semi-automatic weapon ignores a held button without a press edge");

    WeaponControlInput pressed{};
    pressed.firePressed = true;
    controller.Update(state, pressed, 0.0f);
    const std::span<const ShotEvent> firstShot = controller.GetShotEvents();
    context.Expect(
        firstShot.size() == 1 && firstShot[0].weaponId == "test_weapon" &&
            NearlyEqual(firstShot[0].damage, 12.5f) &&
            NearlyEqual(firstShot[0].recoilDegrees, 2.0f) &&
            firstShot[0].magazineAmmoAfterShot == 4 &&
            state.GetMagazineAmmo() == 4,
        "semi-automatic press emits one shot event and consumes one round");

    controller.Update(state, pressed, 0.05f);
    context.Expect(
        controller.GetShotEvents().empty() && state.GetMagazineAmmo() == 4,
        "fire interval blocks an early repeated semi-automatic press");

    controller.Update(state, {}, 0.05f);
    controller.Update(state, pressed, 0.0f);
    context.Expect(
        controller.GetShotEvents().size() == 1 && state.GetMagazineAmmo() == 3,
        "semi-automatic weapon fires again after its interval expires");

    std::string resetError;
    context.Expect(
        controller.Initialize(state, resetError) &&
            controller.GetShotEvents().empty() && state.GetMagazineAmmo() == 5 &&
            state.GetReserveAmmo() == 7,
        "state reinitialization resets ammo, timers, recoil, and stale shot events");
}

void TestAutomaticFireAndNoAutomaticReload(TestContext& context) {
    WeaponDefinition definition = MakeDefinition(true);
    definition.magazineCapacity = 2;
    definition.reserveAmmo = 4;

    WeaponController controller;
    WeaponState state;
    ConfigureAndInitialize(context, controller, state, definition);

    WeaponControlInput pressedOnly{};
    pressedOnly.firePressed = true;
    controller.Update(state, pressedOnly, 0.0f);
    context.Expect(
        controller.GetShotEvents().empty(),
        "automatic weapon uses held input rather than the press edge alone");

    WeaponControlInput held{};
    held.fireHeld = true;
    controller.Update(state, held, 0.0f);
    context.Expect(
        controller.GetShotEvents().size() == 1 && state.GetMagazineAmmo() == 1,
        "automatic weapon fires immediately while held");

    controller.Update(state, held, 0.05f);
    context.Expect(
        controller.GetShotEvents().empty() && state.GetMagazineAmmo() == 1,
        "automatic weapon respects its fire interval while held");
    controller.Update(state, held, 0.05f);
    context.Expect(
        controller.GetShotEvents().size() == 1 && state.GetMagazineAmmo() == 0,
        "automatic weapon repeats when the interval expires");

    controller.Update(state, held, 1.0f);
    context.Expect(
        controller.GetShotEvents().empty() && !state.IsReloading() &&
            state.GetMagazineAmmo() == 0 && state.GetReserveAmmo() == 4,
        "empty automatic weapon does not start an implicit reload");
}

void TestManualReloadAndAmmoTransfer(TestContext& context) {
    WeaponController controller;
    WeaponState state;
    ConfigureAndInitialize(context, controller, state, MakeDefinition());

    WeaponControlInput fire{};
    fire.firePressed = true;
    controller.Update(state, fire, 0.0f);

    WeaponControlInput reload{};
    reload.reloadPressed = true;
    controller.Update(state, reload, 0.0f);
    context.Expect(
        state.IsReloading() && state.GetMagazineAmmo() == 4 &&
            state.GetReserveAmmo() == 7,
        "R starts a reload without transferring ammo immediately");

    WeaponControlInput fireDuringReload = fire;
    controller.Update(state, fireDuringReload, 0.4f);
    const WeaponHudSnapshot halfway = controller.MakeHudSnapshot(state);
    context.Expect(
        controller.GetShotEvents().empty() && halfway.reloading &&
            NearlyEqual(halfway.reloadProgress, 0.4f) &&
            halfway.magazineAmmo == 4 && halfway.reserveAmmo == 7,
        "reload progress advances and blocks firing");

    // A paused caller performs no Update; read-only snapshots do not advance
    // any timers or transfer ammunition.
    const WeaponHudSnapshot pausedAgain = controller.MakeHudSnapshot(state);
    context.Expect(
        NearlyEqual(pausedAgain.reloadProgress, halfway.reloadProgress) &&
            pausedAgain.magazineAmmo == halfway.magazineAmmo,
        "state remains frozen when the caller does not update gameplay");

    controller.Update(state, fireDuringReload, 0.6f);
    context.Expect(
        controller.GetShotEvents().empty() && !state.IsReloading() &&
            state.GetMagazineAmmo() == 5 && state.GetReserveAmmo() == 6,
        "completed reload transfers only the missing magazine rounds");

    controller.Update(state, reload, 0.0f);
    context.Expect(
        !state.IsReloading(),
        "R does not reload an already full magazine");

    WeaponDefinition limitedDefinition = MakeDefinition(true);
    limitedDefinition.reserveAmmo = 2;
    WeaponController limitedController;
    WeaponState limitedState;
    ConfigureAndInitialize(
        context, limitedController, limitedState, limitedDefinition);

    WeaponControlInput held{};
    held.fireHeld = true;
    for (std::uint32_t shot = 0; shot < limitedDefinition.magazineCapacity; ++shot) {
        limitedController.Update(
            limitedState,
            held,
            shot == 0 ? 0.0f : limitedDefinition.fireIntervalSeconds);
    }
    limitedController.Update(limitedState, reload, 0.0f);
    limitedController.Update(limitedState, {}, limitedDefinition.reloadSeconds);
    context.Expect(
        limitedState.GetMagazineAmmo() == 2 &&
            limitedState.GetReserveAmmo() == 0 && !limitedState.IsReloading(),
        "reload transfers all remaining reserve when it cannot fill the magazine");
}

void TestRecoilAccumulationAndRecovery(TestContext& context) {
    WeaponControllerSettings settings{};
    settings.recoilRecoveryDegreesPerSecond = 10.0f;
    settings.maximumAccumulatedRecoilDegrees = 3.0f;

    WeaponController controller;
    WeaponState state;
    ConfigureAndInitialize(context, controller, state, MakeDefinition(), settings);

    WeaponControlInput fire{};
    fire.firePressed = true;
    controller.Update(state, fire, 0.0f);
    const WeaponHudSnapshot expanded = controller.MakeHudSnapshot(state);
    context.Expect(
        NearlyEqual(expanded.recoilDegrees, 2.0f) &&
            NearlyEqual(expanded.crosshairExpansion, 2.0f / 3.0f),
        "shot recoil expands the HUD crosshair snapshot");

    controller.Update(state, {}, 0.1f);
    const WeaponHudSnapshot recovering = controller.MakeHudSnapshot(state);
    context.Expect(
        NearlyEqual(recovering.recoilDegrees, 1.0f) &&
            recovering.crosshairExpansion < expanded.crosshairExpansion,
        "recoil and crosshair expansion recover while shooting stops");

    controller.Update(state, {}, 1.0f);
    const WeaponHudSnapshot recovered = controller.MakeHudSnapshot(state);
    context.Expect(
        NearlyEqual(recovered.recoilDegrees, 0.0f) &&
            NearlyEqual(recovered.crosshairExpansion, 0.0f),
        "recoil recovery clamps cleanly at rest");
}

void TestRoomTransitionVisualReset(TestContext& context) {
    WeaponController controller;
    WeaponState state;
    ConfigureAndInitialize(context, controller, state, MakeDefinition());

    WeaponControlInput fire{};
    fire.firePressed = true;
    controller.Update(state, fire, 0.0f);
    WeaponControlInput reload{};
    reload.reloadPressed = true;
    controller.Update(state, reload, 0.0f);
    controller.ResetVisualFeedback(state);

    context.Expect(
        NearlyEqual(state.GetRecoilDegrees(), 0.0f) && state.IsReloading() &&
            NearlyEqual(state.GetFireCooldownSeconds(), 0.1f) &&
            state.GetMagazineAmmo() == 4 && state.GetReserveAmmo() == 7,
        "room transition clears visual recoil while preserving ammo, reload, and cooldown");
}

void TestValidationAndInvalidDelta(TestContext& context) {
    WeaponState state;
    WeaponController unconfigured;
    std::string error;
    context.Expect(
        !unconfigured.Initialize(state, error) && !error.empty(),
        "unconfigured weapon controller cannot initialize state");

    const auto expectInvalidDefinition = [&context](
                                             WeaponDefinition definition,
                                             const std::string_view description) {
        WeaponController controller;
        std::string configureError;
        context.Expect(
            !controller.Configure(definition, configureError) &&
                !configureError.empty(),
            description);
    };

    WeaponDefinition definition = MakeDefinition();
    definition.id.clear();
    expectInvalidDefinition(definition, "empty weapon ID is rejected");
    definition = MakeDefinition();
    definition.damage = 0.0f;
    expectInvalidDefinition(definition, "non-positive weapon damage is rejected");
    definition = MakeDefinition();
    definition.magazineCapacity = 0;
    expectInvalidDefinition(definition, "zero magazine capacity is rejected");
    definition = MakeDefinition();
    definition.recoilDegrees = -1.0f;
    expectInvalidDefinition(definition, "negative weapon recoil is rejected");
    definition = MakeDefinition();
    definition.fireIntervalSeconds = 0.0f;
    expectInvalidDefinition(definition, "zero fire interval is rejected");
    definition = MakeDefinition();
    definition.reloadSeconds = (std::numeric_limits<float>::quiet_NaN)();
    expectInvalidDefinition(definition, "non-finite reload duration is rejected");

    WeaponControllerSettings invalidSettings{};
    invalidSettings.recoilRecoveryDegreesPerSecond = 0.0f;
    context.Expect(
        !ValidateWeaponControllerSettings(invalidSettings, error) && !error.empty(),
        "zero recoil recovery is rejected");

    WeaponController controller;
    ConfigureAndInitialize(context, controller, state, MakeDefinition());
    WeaponControlInput reload{};
    reload.reloadPressed = true;
    WeaponControlInput fire{};
    fire.firePressed = true;
    controller.Update(state, fire, 0.0f);
    controller.Update(state, reload, 0.0f);
    const WeaponHudSnapshot beforeInvalidDelta = controller.MakeHudSnapshot(state);
    controller.Update(
        state, {}, (std::numeric_limits<float>::quiet_NaN)());
    const WeaponHudSnapshot afterInvalidDelta = controller.MakeHudSnapshot(state);
    context.Expect(
        NearlyEqual(
            beforeInvalidDelta.reloadProgress,
            afterInvalidDelta.reloadProgress) &&
            beforeInvalidDelta.magazineAmmo == afterInvalidDelta.magazineAmmo &&
            afterInvalidDelta.reloading && controller.GetShotEvents().empty(),
        "invalid delta clears frame events but freezes weapon state");
}

void TestAuthoritativeActions(TestContext& context) {
    WeaponController controller;
    WeaponState state;
    const WeaponDefinition definition = MakeDefinition();
    std::string error;
    context.Expect(controller.Configure(definition, error) && controller.Initialize(state, error),
                   "animated weapon initializes without rendering resources");
    auto snapshot = controller.MakePresentationSnapshot(state);
    context.Expect(snapshot.action == WeaponAction::Draw && snapshot.revision == 1 &&
                       NearlyEqual(snapshot.durationSeconds, definition.drawSeconds),
                   "initial draw exposes authoritative timing and revision");
    controller.Update(state, {true, true, true, true}, definition.drawSeconds);
    context.Expect(state.GetAction() == WeaponAction::Idle && state.GetMagazineAmmo() == 5 &&
                       controller.GetShotEvents().empty(),
                   "draw completion ignores queued weapon inputs");
    controller.Update(state, {false, true, false, false}, 0.0f);
    const auto shot = controller.MakePresentationSnapshot(state);
    context.Expect(shot.action == WeaponAction::Shoot && state.GetMagazineAmmo() == 4 &&
                       controller.GetShotEvents().size() == 1 && controller.GetActionEvents().size() == 1,
                   "accepted shot synchronizes ammo, action snapshot and events");
    controller.Update(state, {false, true, false, false}, definition.fireIntervalSeconds);
    context.Expect(state.GetMagazineAmmo() == 3 && state.GetAction() == WeaponAction::Shoot &&
                       state.GetActionRevision() > shot.revision,
                   "consecutive legal shots restart the clip with a new revision");
    controller.Update(state, {false, true, true, true}, 0.0f);
    context.Expect(state.GetAction() == WeaponAction::Hide && state.GetMagazineAmmo() == 3,
                   "holster has priority over reload and fire");
    controller.Update(state, {true, true, true, true}, definition.hideSeconds * 0.5f);
    const auto hiding = controller.MakePresentationSnapshot(state);
    controller.ResetVisualFeedback(state);
    context.Expect(state.GetAction() == WeaponAction::Hide &&
                       NearlyEqual(state.GetActionElapsedSeconds(), hiding.elapsedSeconds),
                   "room visual reset preserves equip action timing");
    controller.Update(state, {true, true, true, true}, definition.hideSeconds * 0.5f);
    controller.Update(state, {true, true, true, false}, 10.0f);
    context.Expect(state.GetAction() == WeaponAction::Holstered && state.GetMagazineAmmo() == 3 &&
                       controller.GetShotEvents().empty(),
                   "hidden weapon stays hidden and cannot fire or reload");
    controller.Update(state, {false, false, false, true}, 0.0f);
    controller.Update(state, {}, definition.drawSeconds);
    controller.Update(state, {false, true, true, false}, 0.0f);
    context.Expect(state.GetAction() == WeaponAction::Reload && state.GetMagazineAmmo() == 3,
                   "reload starts after draw and takes priority over shooting");
    controller.Update(state, {true, true, false, true}, definition.reloadSeconds * 0.5f);
    snapshot = controller.MakePresentationSnapshot(state);
    context.Expect(snapshot.action == WeaponAction::Reload && state.GetMagazineAmmo() == 3,
                   "reload is non-interruptible and does not refill early");
    controller.Update(state, {}, (std::numeric_limits<float>::quiet_NaN)());
    context.Expect(state.GetActionRevision() == snapshot.revision &&
                       NearlyEqual(state.GetActionElapsedSeconds(), snapshot.elapsedSeconds),
                   "invalid update freezes action state and time");
    controller.Update(state, {true, true, true, true}, definition.reloadSeconds * 0.5f);
    context.Expect(state.GetAction() == WeaponAction::Idle && state.GetMagazineAmmo() == 5 &&
                       state.GetReserveAmmo() == 5 && controller.GetShotEvents().empty(),
                   "reload completion transfers missing rounds exactly once without acting on input");
}

} // namespace

void RunWeaponControllerTests(TestContext& context) {
    TestAuthoritativeActions(context);
    TestInitialStateAndSemiAutomaticFire(context);
    TestAutomaticFireAndNoAutomaticReload(context);
    TestManualReloadAndAmmoTransfer(context);
    TestRecoilAccumulationAndRecovery(context);
    TestRoomTransitionVisualReset(context);
    TestValidationAndInvalidDelta(context);
}

} // namespace fps::tests
