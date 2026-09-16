#include "HeadlessSmoke.hpp"

#include "RetroFPS/Game/GameSession.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string_view>
#include <variant>

bool RunHeadlessSmoke(
    std::shared_ptr<const fps::CampaignContent> content,
    std::string& report,
    std::string& error) {
    using namespace fps;
    constexpr float stepSeconds = 1.0F / 60.0F;
    constexpr unsigned maximumPhaseSteps = 900; // 15 simulated seconds per phase.
    report.clear();
    error.clear();
    const auto require = [&](const bool condition, const std::string_view message) {
        if (!condition) error = "Headless smoke failed: " + std::string(message);
        return condition;
    };

    GameSessionConfig config;
    GameSession session;
    if (!session.Initialize(content, config, error)) return false;
    const auto* weapon = content->Data().weapons.FindById(config.startingWeaponId);
    if (!require(weapon != nullptr, "starting weapon definition is missing")) return false;
    if (!require(weapon->reserveAmmo > 0, "reload scenario requires starting reserve ammo")) return false;
    if (!require(session.Snapshot().screen == GameScreen::MainMenu &&
                     !session.Snapshot().activeStage && !session.Snapshot().player,
                 "initial session must be an empty main menu")) return false;

    std::uint64_t lastSequence = 0;
    unsigned steps = 0, stageEvents = 0, shotEvents = 0, reloadEvents = 0, drawEvents = 0;
    unsigned pauseEvents = 0, resumeEvents = 0;
    float simulatedSeconds = 0;
    const auto advance = [&](const float dt, const GameFrameInput& input = {},
                             const std::span<const GameSessionCommand> commands = {}) {
        if (!session.Advance(dt, input, commands, error)) return false;
        ++steps;
        simulatedSeconds += dt;
        for (const auto& event : session.Events()) {
            if (!require(event.sequence > lastSequence, "event sequence must increase")) return false;
            lastSequence = event.sequence;
            if (const auto* rejected = std::get_if<CommandRejectedEvent>(&event.payload)) {
                error = "Headless smoke command rejected: " + rejected->reason;
                return false;
            }
            if (std::holds_alternative<StageEnteredEvent>(event.payload)) ++stageEvents;
            if (std::holds_alternative<ShotEvent>(event.payload)) ++shotEvents;
            if (const auto* action = std::get_if<WeaponActionEvent>(&event.payload)) {
                if (action->action == WeaponAction::Reload) ++reloadEvents;
                if (action->action == WeaponAction::Draw) ++drawEvents;
            }
            if (const auto* screen = std::get_if<ScreenChangedEvent>(&event.payload)) {
                if (screen->current == GameScreen::Paused) ++pauseEvents;
                if (screen->previous == GameScreen::Paused && screen->current == GameScreen::Playing) ++resumeEvents;
            }
        }
        const auto& snapshot = session.Snapshot();
        return require(snapshot.campaignOutcome == CampaignOutcome::InProgress && !snapshot.quitRequested,
                       "campaign unexpectedly ended during the startup scenario");
    };
    const auto waitUntil = [&](const auto& predicate, const std::string_view phase) {
        for (unsigned index = 0; index < maximumPhaseSteps; ++index) {
            if (predicate()) return true;
            if (!advance(stepSeconds)) return false;
        }
        error = "Headless smoke timed out waiting for " + std::string(phase);
        return false;
    };

    const GameSessionCommand start = StartCampaignCommand{};
    if (!advance(0, {}, std::span{&start, 1})) return false;
    if (!waitUntil([&] {
            return session.Snapshot().screen == GameScreen::Playing &&
                   session.Snapshot().transitionPhase == StageTransitionPhase::Idle;
        }, "first playable stage")) return false;
    const auto& spawned = session.Snapshot();
    if (!require(spawned.activeStage && spawned.player &&
                     spawned.activeStage->levelId == content->Stages().front().definition.id &&
                     spawned.player->grounded && spawned.player->feetY == 0 &&
                     spawned.weaponPresentation.action == WeaponAction::Draw &&
                     spawned.weapon.magazineAmmo == weapon->magazineCapacity &&
                     spawned.weapon.reserveAmmo == weapon->reserveAmmo && stageEvents == 1,
                 "campaign spawn, starting ammo or initial Draw did not match packaged content")) return false;
    if (!waitUntil([&] { return session.Snapshot().weaponPresentation.action == WeaponAction::Idle; },
                   "Draw completion")) return false;

    const float standingCameraY = session.Snapshot().player->eyeHeight;
    GameFrameInput jump;
    jump.jumpPressed = jump.jumpHeld = true;
    if (!advance(stepSeconds, jump)) return false;
    const auto airborne = session.Snapshot();
    if (!require(airborne.player && !airborne.player->grounded && airborne.player->feetY > 0 &&
                     airborne.player->verticalVelocity > 0 && airborne.player->bodyHeight > 0 &&
                     airborne.player->feetY + airborne.player->eyeHeight > standingCameraY,
                 "jump did not lift the player capsule and camera-height snapshot")) return false;

    const GameSessionCommand pause = PauseCommand{};
    if (!advance(0, {}, std::span{&pause, 1})) return false;
    GameFrameInput ignored;
    ignored.firePressed = ignored.fireHeld = ignored.reloadPressed = true;
    ignored.jumpPressed = ignored.jumpHeld = true;
    if (!advance(2.0F, ignored)) return false;
    const auto& paused = session.Snapshot();
    if (!require(paused.screen == GameScreen::Paused && paused.player &&
                     paused.player->feetY == airborne.player->feetY &&
                     paused.player->verticalVelocity == airborne.player->verticalVelocity &&
                     paused.weaponPresentation.elapsedSeconds == airborne.weaponPresentation.elapsedSeconds &&
                     paused.weaponPresentation.revision == airborne.weaponPresentation.revision &&
                     paused.weapon.magazineAmmo == airborne.weapon.magazineAmmo && shotEvents == 0,
                 "pause advanced jump/weapon time or accepted gameplay input")) return false;
    const GameSessionCommand resume = ResumeCommand{};
    if (!advance(0, {}, std::span{&resume, 1})) return false;
    if (!waitUntil([&] { return session.Snapshot().player && session.Snapshot().player->grounded; },
                   "landing after resume")) return false;
    if (!require(session.Snapshot().screen == GameScreen::Playing &&
                     session.Snapshot().player->feetY == 0 &&
                     session.Snapshot().player->verticalVelocity == 0,
                 "landing failed to restore ground height and vertical velocity")) return false;

    GameFrameInput fire;
    fire.firePressed = fire.fireHeld = true;
    if (!advance(stepSeconds, fire)) return false;
    if (!require(shotEvents == 1 && session.Snapshot().weapon.magazineAmmo == weapon->magazineCapacity - 1 &&
                     session.Snapshot().weaponPresentation.action == WeaponAction::Shoot,
                 "one shot must emit one event and consume exactly one round")) return false;
    GameFrameInput reload;
    reload.reloadPressed = true;
    if (!advance(0, reload)) return false;
    if (!require(session.Snapshot().weaponPresentation.action == WeaponAction::Reload &&
                     session.Snapshot().weapon.reloading &&
                     std::abs(session.Snapshot().weaponPresentation.durationSeconds - weapon->reloadSeconds) < 0.00001F,
                 "Reload did not use the packaged weapon duration")) return false;

    // Stop strictly before completion and check that ammo transfers only at
    // the action endpoint; using the CSV duration keeps this data-driven.
    float remainingHalf = weapon->reloadSeconds * 0.5F;
    for (unsigned index = 0; remainingHalf > 0 && index < maximumPhaseSteps; ++index) {
        const float dt = (std::min)(stepSeconds, remainingHalf);
        if (!advance(dt)) return false;
        remainingHalf -= dt;
    }
    if (!require(remainingHalf <= 0 && session.Snapshot().weapon.reloading &&
                     session.Snapshot().weapon.magazineAmmo == weapon->magazineCapacity - 1 &&
                     session.Snapshot().weapon.reserveAmmo == weapon->reserveAmmo,
                 "reload timed out or transferred ammunition before completion")) return false;
    if (!waitUntil([&] { return session.Snapshot().weaponPresentation.action == WeaponAction::Idle; },
                   "Reload completion")) return false;
    if (!require(!session.Snapshot().weapon.reloading &&
                     session.Snapshot().weapon.magazineAmmo == weapon->magazineCapacity &&
                     session.Snapshot().weapon.reserveAmmo == weapon->reserveAmmo - 1 &&
                     stageEvents == 1 && drawEvents == 1 && shotEvents == 1 && reloadEvents == 1 &&
                     pauseEvents == 1 && resumeEvents == 1,
                 "final ammo or campaign/weapon event counts are incorrect")) return false;

    std::ostringstream summary;
    summary << "Headless smoke passed: packaged campaign=" << content->Stages().front().definition.id
            << ", stages=" << content->Stages().size() << ", steps=" << steps
            << ", simulated_seconds=" << simulatedSeconds
            << ", checks=main_menu/start/draw/jump/pause/resume/landing/shot/reload"
            << ", shot_events=" << shotEvents << ", reload_events=" << reloadEvents
            << ", window=none, gpu=none";
    report = summary.str();
    return true;
}
