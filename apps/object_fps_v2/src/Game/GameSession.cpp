#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "RetroFPS/Game/GameSession.hpp"

#include "RetroFPS/Collision/CombatCollision.hpp"
#include "RetroFPS/Gameplay/Enemy/EnemySpawnDirector.hpp"
#include "RetroFPS/Gameplay/Player/Player.hpp"
#include "RetroFPS/Gameplay/Player/PlayerCombatState.hpp"
#include "RetroFPS/Gameplay/Player/PlayerController.hpp"
#include "RetroFPS/Gameplay/Weapon/WeaponController.hpp"
#include "RetroFPS/World/World.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fps {
namespace {

constexpr float kMaximumShotDistance = 50.0F;
constexpr float kLengthEpsilon = 0.000001F;

struct ViewBasis final {
    Float3 forward{};
    Float3 right{};
    Float3 up{};
};

[[nodiscard]] ViewBasis MakeViewBasis(
    const float yawRadians,
    const float pitchRadians) noexcept {
    const float sineYaw = std::sin(yawRadians);
    const float cosineYaw = std::cos(yawRadians);
    const float sinePitch = std::sin(pitchRadians);
    const float cosinePitch = std::cos(pitchRadians);
    return {
        {sineYaw * cosinePitch, -sinePitch, cosineYaw * cosinePitch},
        {cosineYaw, 0.0F, -sineYaw},
        {sineYaw * sinePitch, cosinePitch, cosineYaw * sinePitch},
    };
}

[[nodiscard]] Float3 AddScaled(
    const Float3 value,
    const Float3 direction,
    const float distance) noexcept {
    return {
        value.x + direction.x * distance,
        value.y + direction.y * distance,
        value.z + direction.z * distance,
    };
}

[[nodiscard]] Float3 Subtract(const Float3 left, const Float3 right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

[[nodiscard]] float Length(const Float3 value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

[[nodiscard]] std::vector<CombatTarget> MakeCombatTargets(
    const std::span<const EnemySnapshot> snapshots) {
    std::vector<CombatTarget> targets;
    targets.reserve(snapshots.size());
    for (const EnemySnapshot& enemy : snapshots) {
        if (enemy.state != EnemyState::Dead) {
            for (const auto& region : enemy.hurtboxes)
                targets.push_back({enemy.id,region.shape,region.region});
        }
    }
    return targets;
}

[[nodiscard]] bool SegmentIntersectsCell(
    const Float2 start,
    const Float2 end,
    const GridCoordinate cell,
    const float cellSize) noexcept {
    const double minimumX = static_cast<double>(cell.column) * cellSize;
    const double maximumX = minimumX + cellSize;
    const double minimumZ = static_cast<double>(cell.row) * cellSize;
    const double maximumZ = minimumZ + cellSize;
    const double deltaX = static_cast<double>(end.x) - start.x;
    const double deltaZ = static_cast<double>(end.z) - start.z;
    double minimumTime = 0.0;
    double maximumTime = 1.0;

    const auto clipAxis = [&minimumTime, &maximumTime](
                              const double origin,
                              const double delta,
                              const double minimum,
                              const double maximum) {
        if (delta == 0.0) {
            return origin >= minimum && origin <= maximum;
        }
        double entryTime = (minimum - origin) / delta;
        double exitTime = (maximum - origin) / delta;
        if (entryTime > exitTime) {
            std::swap(entryTime, exitTime);
        }
        minimumTime = (std::max)(minimumTime, entryTime);
        maximumTime = (std::min)(maximumTime, exitTime);
        return minimumTime <= maximumTime;
    };

    return clipAxis(start.x, deltaX, minimumX, maximumX) &&
           clipAxis(start.z, deltaZ, minimumZ, maximumZ);
}

[[nodiscard]] bool IsMovementHeld(const GameFrameInput& input) noexcept {
    return std::abs(input.moveForward) > 0.0001F ||
           std::abs(input.moveRight) > 0.0001F;
}

} // namespace

struct GameSession::Impl final {
    enum class DestinationKind {
        Stage,
        Results,
        MainMenu,
    };

    struct Destination final {
        DestinationKind kind{DestinationKind::MainMenu};
        std::size_t stageIndex{};
    };

    struct Transition final {
        StageTransitionPhase phase{StageTransitionPhase::Idle};
        std::optional<Destination> destination;
        float elapsed{};
        float opacity{};

        [[nodiscard]] bool Begin(const Destination requested) noexcept {
            if (phase != StageTransitionPhase::Idle) {
                return false;
            }
            destination = requested;
            phase = StageTransitionPhase::FadingOut;
            elapsed = 0.0F;
            opacity = 0.0F;
            return true;
        }

        [[nodiscard]] bool Update(
            const float deltaSeconds,
            const GameSessionConfig& config) noexcept {
            if (phase == StageTransitionPhase::FadingOut) {
                elapsed = (std::min)(config.fadeOutSeconds, elapsed + deltaSeconds);
                const float progress = elapsed / config.fadeOutSeconds;
                opacity = progress * progress * (3.0F - 2.0F * progress);
                if (elapsed >= config.fadeOutSeconds) {
                    phase = StageTransitionPhase::CommitPending;
                    opacity = 1.0F;
                    return true;
                }
            } else if (phase == StageTransitionPhase::FadingIn) {
                elapsed = (std::min)(config.fadeInSeconds, elapsed + deltaSeconds);
                const float progress = elapsed / config.fadeInSeconds;
                opacity = 1.0F - progress * progress * (3.0F - 2.0F * progress);
                if (elapsed >= config.fadeInSeconds) {
                    phase = StageTransitionPhase::Idle;
                    destination.reset();
                    elapsed = 0.0F;
                    opacity = 0.0F;
                }
            }
            return false;
        }

        void CompleteCommit() noexcept {
            phase = StageTransitionPhase::FadingIn;
            elapsed = 0.0F;
            opacity = 1.0F;
        }
    };

    struct StageRuntime final {
        std::size_t contentIndex{};
        World world;
        Player player;
        EnemySystem enemies;
        EnemySpawnDirector spawnDirector;
        ProjectileSystem projectiles;
        bool doorVisible{};
        bool exitRequiresLeave{};
    };

    std::shared_ptr<const CampaignContent> content;
    GameSessionConfig config;
    EnemyDefinition meleeDefinition;
    EnemyDefinition rangedDefinition;
    WeaponDefinition weaponDefinition;
    WeaponShotGeometry weaponShotGeometry;
    std::unique_ptr<StageRuntime> stage;
    PlayerController playerController;
    PlayerCombatState playerCombat;
    WeaponController weaponController;
    WeaponState weaponState;
    CampaignRunState campaign;
    GameFlow flow;
    Transition transition;
    GameSessionSnapshot snapshot;
    std::vector<GameSessionEvent> events;
    std::uint64_t nextEventSequence{1};
    bool movementReleaseRequired{};
    bool fireReleaseRequired{};
    bool jumpReleaseRequired{};
    bool holsterReleaseRequired{};
    bool focusLossPending{};
    bool quitRequested{};

    template <class Payload>
    void Emit(Payload payload) {
        events.push_back({nextEventSequence++, GameSessionEventPayload{std::move(payload)}});
    }

    void EmitScreenChange(const GameScreen previous) {
        const GameScreen current = flow.GetScreen();
        if (previous != current) {
            Emit(ScreenChangedEvent{previous, current});
        }
    }

    [[nodiscard]] bool Initialize(
        std::shared_ptr<const CampaignContent> requestedContent,
        const GameSessionConfig& requestedConfig,
        std::string& error) {
        error.clear();
        if (!requestedContent || requestedContent->Stages().empty()) {
            error = "GameSession requires non-empty CampaignContent.";
            return false;
        }
        if (!std::isfinite(requestedConfig.fadeOutSeconds) ||
            requestedConfig.fadeOutSeconds <= 0.0F ||
            !std::isfinite(requestedConfig.fadeInSeconds) ||
            requestedConfig.fadeInSeconds <= 0.0F) {
            error = "GameSession fade durations must be finite and greater than zero.";
            return false;
        }

        if (!IsValidWeaponVerticalFov(requestedConfig.worldVerticalFovRadians)) {
            error = "GameSession world vertical FOV must be finite and in (0, pi).";
            return false;
        }

        content = std::move(requestedContent);
        config = requestedConfig;
        const GameDataCatalog& data = content->Data();
        const EnemyDefinition* melee = data.enemies.FindById(config.meleeEnemyId);
        const EnemyDefinition* ranged = data.enemies.FindById(config.rangedEnemyId);
        const WeaponDefinition* weapon = data.weapons.FindById(config.startingWeaponId);
        if (melee == nullptr || melee->kind != EnemyKind::Melee) {
            error = "Configured melee enemy definition is missing or has the wrong kind.";
            return false;
        }
        if (ranged == nullptr || ranged->kind != EnemyKind::Ranged) {
            error = "Configured ranged enemy definition is missing or has the wrong kind.";
            return false;
        }
        if (weapon == nullptr) {
            error = "Configured starting weapon definition is missing.";
            return false;
        }
        meleeDefinition = *melee;
        rangedDefinition = *ranged;
        weaponDefinition = *weapon;
        const WeaponShotGeometry* geometry = content->FindWeaponShotGeometry(weapon->id);
        if (geometry == nullptr) {
            error = "Configured starting weapon shot geometry is missing.";
            return false;
        }
        weaponShotGeometry = *geometry;

        if (!playerController.Configure(config.player, error) ||
            !weaponController.Configure(weaponDefinition, config.weapon, error) ||
            !weaponController.Initialize(weaponState, error)) {
            return false;
        }

        std::vector<CampaignRoomDefinition> rooms;
        rooms.reserve(content->Stages().size());
        for (const CampaignStageContent& item : content->Stages()) {
            rooms.push_back({item.definition.id, item.definition.name});
        }
        if (!campaign.Initialize(rooms, error)) {
            return false;
        }

        // Consumer-driven preflight: every data-defined stage must be capable
        // of constructing the current gameplay mechanisms before the run starts.
        for (std::size_t index = 0; index < content->Stages().size(); ++index) {
            std::unique_ptr<StageRuntime> candidate = BuildStage(index, error);
            if (!candidate) {
                return false;
            }
        }

        flow.ReturnToMainMenu();
        campaign.ResetRun();
        playerCombat.Reset();
        stage.reset();
        transition = {};
        events.clear();
        nextEventSequence = 1;
        movementReleaseRequired = false;
        fireReleaseRequired = false;
        jumpReleaseRequired = false;
        holsterReleaseRequired = false;
        focusLossPending = false;
        quitRequested = false;
        RefreshSnapshot();
        return true;
    }

    [[nodiscard]] std::unique_ptr<StageRuntime> BuildStage(
        const std::size_t index,
        std::string& error) {
        error.clear();
        if (!content || index >= content->Stages().size()) {
            error = "Requested stage is outside CampaignContent.";
            return nullptr;
        }

        const CampaignStageContent& item = content->Stages()[index];
        try {
            auto candidate = std::make_unique<StageRuntime>();
            candidate->contentIndex = index;
            candidate->world.Initialize(item.map, config.world);
            if (!playerController.Initialize(
                    candidate->player,
                    candidate->world.GetMap(),
                    candidate->world.GetSettings(),
                    error)) {
                error = "Stage '" + item.definition.id + "' player: " + error;
                return nullptr;
            }
            if (!candidate->enemies.Initialize(
                    candidate->world.GetMap(),
                    candidate->player.GetPositionXZ(),
                    playerController.GetSettings().collisionRadius,
                    candidate->world.GetSettings().cellSize,
                    config.enemies,
                    error, config.world.wallHeight, config.player.bodyHeight)) {
                error = "Stage '" + item.definition.id + "' enemies: " + error;
                return nullptr;
            }
            if (!candidate->spawnDirector.Initialize(
                    candidate->world.GetMap(),
                    item.definition,
                    meleeDefinition,
                    rangedDefinition,
                    error)) {
                error = "Stage '" + item.definition.id + "' spawn policy: " + error;
                return nullptr;
            }
            static_cast<void>(candidate->spawnDirector.SpawnAvailable(
                candidate->enemies,
                candidate->world.GetMap(),
                candidate->player.GetPositionXZ(),
                playerController.GetSettings().collisionRadius,
                error));
            if (!error.empty()) {
                error = "Stage '" + item.definition.id + "' initial spawn: " + error;
                return nullptr;
            }
            if (!candidate->projectiles.Configure(config.projectiles)) {
                error = "Stage '" + item.definition.id + "' projectile settings are invalid.";
                return nullptr;
            }
            return candidate;
        } catch (const std::exception& exception) {
            error = "Stage '" + item.definition.id + "' construction failed: " +
                    exception.what();
        } catch (...) {
            error = "Stage '" + item.definition.id +
                    "' construction failed with an unknown error.";
        }
        return nullptr;
    }

    void ResetRun(std::string& error) {
        playerCombat.Reset();
        if (!weaponController.Initialize(weaponState, error)) {
            return;
        }
        for (const WeaponActionEvent& action : weaponController.GetActionEvents()) {
            Emit(action);
        }
        campaign.ResetRun();
        movementReleaseRequired = true;
        fireReleaseRequired = true;
        jumpReleaseRequired = true;
        holsterReleaseRequired = true;
    }

    [[nodiscard]] bool BeginStageTransition(
        const std::size_t index,
        std::string& error) {
        if (!content || index >= content->Stages().size()) {
            error = "requested stage does not exist";
            return false;
        }
        if (!transition.Begin({DestinationKind::Stage, index})) {
            error = "a stage transition is already active";
            return false;
        }
        return true;
    }

    [[nodiscard]] bool ApplyCommand(
        const GameSessionCommand& command,
        std::string& error) {
        error.clear();
        return std::visit(
            [this, &error](const auto& value) -> bool {
                using Command = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, RequestQuitCommand>) {
                    if (!quitRequested) {
                        quitRequested = true;
                        Emit(QuitRequestedEvent{});
                    }
                    return true;
                } else if constexpr (std::is_same_v<Command, StartCampaignCommand>) {
                    if (flow.GetScreen() != GameScreen::MainMenu ||
                        transition.phase != StageTransitionPhase::Idle) {
                        error = "StartCampaign is only legal from the idle main menu";
                        return false;
                    }
                    ResetRun(error);
                    return error.empty() && BeginStageTransition(0, error);
                } else if constexpr (std::is_same_v<Command, OpenControlsCommand>) {
                    if (flow.GetScreen() != GameScreen::MainMenu ||
                        transition.phase != StageTransitionPhase::Idle) {
                        error = "OpenControls is only legal from the idle main menu";
                        return false;
                    }
                    const GameScreen previous = flow.GetScreen();
                    flow.OpenControls();
                    EmitScreenChange(previous);
                    return true;
                } else if constexpr (std::is_same_v<Command, CloseControlsCommand>) {
                    if (flow.GetScreen() != GameScreen::Controls ||
                        transition.phase != StageTransitionPhase::Idle) {
                        error = "CloseControls is only legal from Controls";
                        return false;
                    }
                    const GameScreen previous = flow.GetScreen();
                    flow.CloseControls();
                    EmitScreenChange(previous);
                    return true;
                } else if constexpr (std::is_same_v<Command, PauseCommand>) {
                    if (flow.GetScreen() != GameScreen::Playing ||
                        transition.phase != StageTransitionPhase::Idle) {
                        error = "Pause is only legal while an idle stage is playing";
                        return false;
                    }
                    const GameScreen previous = flow.GetScreen();
                    flow.EnterPaused();
                    EmitScreenChange(previous);
                    return true;
                } else if constexpr (std::is_same_v<Command, ResumeCommand>) {
                    if (flow.GetScreen() != GameScreen::Paused ||
                        transition.phase != StageTransitionPhase::Idle) {
                        error = "Resume is only legal from pause";
                        return false;
                    }
                    const GameScreen previous = flow.GetScreen();
                    flow.EnterPlaying();
                    movementReleaseRequired = true;
                    fireReleaseRequired = true;
                    jumpReleaseRequired = true;
                    holsterReleaseRequired = true;
                    EmitScreenChange(previous);
                    return true;
                } else {
                    static_assert(
                        std::is_same_v<Command, ReturnToMainMenuCommand>);
                    if (flow.GetScreen() == GameScreen::MainMenu ||
                        transition.phase != StageTransitionPhase::Idle) {
                        error = "ReturnToMainMenu requires a non-menu idle state";
                        return false;
                    }
                    if (!transition.Begin({DestinationKind::MainMenu, 0})) {
                        error = "main-menu transition was rejected";
                        return false;
                    }
                    return true;
                }
            },
            command);
    }

    [[nodiscard]] bool Advance(
        const float deltaSeconds,
        const GameFrameInput& input,
        const std::span<const GameSessionCommand> commands,
        std::string& error) {
        error.clear();
        events.clear();
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F) {
            error = "GameSession delta time must be finite and non-negative.";
            return false;
        }

        try {
            // Window focus loss is a safety input, not menu navigation. Keep
            // the edge across a fade/commit input lock so the first stable
            // Playing frame still pauses even though the SDL edge occurred
            // while transition input was suppressed.
            focusLossPending = focusLossPending || input.focusLost;

            for (const GameSessionCommand& command : commands) {
                std::string rejection;
                if (!ApplyCommand(command, rejection)) {
                    Emit(CommandRejectedEvent{std::move(rejection)});
                }
            }

            if (transition.phase != StageTransitionPhase::Idle) {
                if (transition.Update(deltaSeconds, config)) {
                    if (!CommitTransition(error)) {
                        return false;
                    }
                }
                RefreshSnapshot();
                return true;
            }

            const GameScreen previousScreen = flow.GetScreen();
            const GameFlowResult flowResult = flow.Update({
                input.backPressed,
                focusLossPending,
            });
            focusLossPending = false;
            if (previousScreen == GameScreen::Paused &&
                flow.GetScreen() == GameScreen::Playing) {
                movementReleaseRequired = true;
                fireReleaseRequired = true;
                jumpReleaseRequired = true;
                holsterReleaseRequired = true;
            }
            EmitScreenChange(previousScreen);

            if (flowResult.simulateGameplay && stage) {
                GameFrameInput gameplayInput = input;
                if (movementReleaseRequired) {
                    if (!IsMovementHeld(input)) {
                        movementReleaseRequired = false;
                    }
                    gameplayInput.moveForward = 0.0F;
                    gameplayInput.moveRight = 0.0F;
                }
                if (fireReleaseRequired) {
                    if (!input.fireHeld) {
                        fireReleaseRequired = false;
                    }
                    gameplayInput.fireHeld = false;
                    gameplayInput.firePressed = false;
                }
                if (jumpReleaseRequired) {
                    if (!input.jumpHeld) jumpReleaseRequired = false;
                    gameplayInput.jumpPressed = false;
                }
                if (holsterReleaseRequired) {
                    if (!input.holsterToggleHeld) holsterReleaseRequired = false;
                    gameplayInput.holsterTogglePressed = false;
                }
                UpdateGameplay(gameplayInput, deltaSeconds, error);
                if (!error.empty()) {
                    return false;
                }
            }
            RefreshSnapshot();
            return true;
        } catch (const std::exception& exception) {
            error = std::string{"GameSession update failed: "} + exception.what();
        } catch (...) {
            error = "GameSession update failed with an unknown error.";
        }
        return false;
    }

    [[nodiscard]] bool CommitTransition(std::string& error) {
        if (transition.phase != StageTransitionPhase::CommitPending ||
            !transition.destination.has_value()) {
            error = "transition commit has no pending destination";
            return false;
        }

        const Destination destination = *transition.destination;
        const GameScreen previous = flow.GetScreen();
        if (destination.kind == DestinationKind::Stage) {
            std::unique_ptr<StageRuntime> next = BuildStage(destination.stageIndex, error);
            if (!next) {
                return false;
            }
            const CampaignStageContent& item =
                content->Stages()[destination.stageIndex];
            if (!campaign.EnterRoom(item.definition.id)) {
                error = "campaign rejected the committed stage";
                return false;
            }
            stage = std::move(next);
            weaponController.ResetVisualFeedback(weaponState);
            playerController.ClearVerticalRecoil(stage->player);
            flow.EnterPlaying();
            movementReleaseRequired = true;
            fireReleaseRequired = true;
            jumpReleaseRequired = true;
            holsterReleaseRequired = true;
            EmitScreenChange(previous);
            Emit(StageEnteredEvent{
                item.definition.id,
                destination.stageIndex,
                content->Stages().size(),
            });
            for (const EnemySnapshot& enemy : stage->enemies.GetSnapshots()) {
                Emit(EnemySpawnedEvent{enemy.id, enemy.definitionId});
            }
        } else if (destination.kind == DestinationKind::Results) {
            stage.reset();
            flow.EnterResults();
            EmitScreenChange(previous);
        } else {
            stage.reset();
            flow.ReturnToMainMenu();
            EmitScreenChange(previous);
        }
        transition.CompleteCommit();
        return true;
    }

    void UpdateGameplay(
        const GameFrameInput& input,
        const float deltaSeconds,
        std::string& error) {
        const Float2 previousPlayerPosition = stage->player.GetPositionXZ();
        const auto blockers = stage->enemies.CollectAliveBodies();
        playerController.Update(
            stage->player,
            {
                input.moveForward,
                input.moveRight,
                input.lookDeltaX,
                input.lookDeltaY,
                input.lookEnabled,
                input.jumpPressed,
            },
            deltaSeconds,
            stage->world.GetMap(),
            stage->world.GetSettings(),
            blockers);

        stage->enemies.Update(
            stage->world.GetMap(),
            {
                stage->player.GetPositionXZ(),
                playerController.GetSettings().collisionRadius,
                playerController.GetSettings().bodyHeight,
                stage->player.GetFeetY(),
            },
            deltaSeconds);

        const float recoveredRecoil = (std::max)(
            0.0F,
            weaponState.GetRecoilDegrees() -
                config.weapon.recoilRecoveryDegreesPerSecond * deltaSeconds);
        static_cast<void>(playerController.SetVerticalRecoilDegrees(
            stage->player, recoveredRecoil));
        weaponController.Update(
            weaponState,
            {input.fireHeld, input.firePressed, input.reloadPressed, input.holsterTogglePressed},
            deltaSeconds);
        for (const WeaponActionEvent& action : weaponController.GetActionEvents()) {
            Emit(action);
        }
        std::vector<Float3> resolvedShotPoints;
        resolvedShotPoints.reserve(weaponController.GetShotEvents().size());
        for (const ShotEvent& shot : weaponController.GetShotEvents()) {
            Emit(shot);
            resolvedShotPoints.push_back(ResolvePlayerShot(shot));
        }
        static_cast<void>(playerController.SetVerticalRecoilDegrees(
            stage->player, weaponState.GetRecoilDegrees()));

        const VerticalCapsule playerCapsule{
            stage->player.GetPositionXZ(),
            playerController.GetSettings().bodyHeight,
            playerController.GetSettings().collisionRadius,
            stage->player.GetFeetY(),
        };
        const std::span<const PlayerProjectileHit> projectileHits = stage->projectiles.Update(
            stage->world.GetMap(), stage->world.GetSettings(), playerCapsule, deltaSeconds);
        // Existing projectiles advance before these new cosmetics are born.
        // Use the final camera (including this shot's recoil), but retain the
        // authoritative pre-recoil impact and never apply damage a second time.
        for (const Float3 point : resolvedShotPoints) SpawnPlayerShotTracer(point);
        for (const PlayerProjectileHit& hit : projectileHits) {
            ApplyPlayerDamage(hit.damage);
            if (playerCombat.IsDead()) {
                BeginDeathResults(error);
                return;
            }
        }

        for (const EnemyAttackEvent& attack : stage->enemies.GetAttackEvents()) {
            if (attack.kind == EnemyKind::Melee) {
                ApplyPlayerDamage(attack.damage);
                if (playerCombat.IsDead()) {
                    BeginDeathResults(error);
                    return;
                }
            } else {
                const Float3 direction = Subtract(attack.target, attack.origin);
                if (Length(direction) > kLengthEpsilon) {
                    static_cast<void>(stage->projectiles.SpawnEnemyProjectile(
                        attack.origin, attack.target, attack.damage));
                }
            }
        }

        static_cast<void>(stage->enemies.RetireExpiredDead());
        std::unordered_set<EnemyId> existing;
        for (const EnemySnapshot& enemy : stage->enemies.GetSnapshots()) {
            existing.insert(enemy.id);
        }
        static_cast<void>(stage->spawnDirector.SpawnAvailable(
            stage->enemies,
            stage->world.GetMap(),
            stage->player.GetPositionXZ(),
            playerController.GetSettings().collisionRadius,
            error));
        if (!error.empty()) {
            return;
        }
        for (const EnemySnapshot& enemy : stage->enemies.GetSnapshots()) {
            if (!existing.contains(enemy.id)) {
                Emit(EnemySpawnedEvent{enemy.id, enemy.definitionId});
            }
        }

        UpdateDoorState();
        HandleStageExit(previousPlayerPosition, error);
    }

    void ApplyPlayerDamage(const float damage) {
        const PlayerDamageResult result = playerCombat.ApplyDamage(damage);
        if (result.applied) {
            Emit(PlayerDamagedEvent{result.appliedDamage, result.remainingHealth});
        }
    }

    [[nodiscard]] Float3 ResolvePlayerMuzzle() const {
        const Float3 cameraOrigin = stage->player.GetEyePosition(
            playerController.GetSettings().eyeHeight);
        const ViewBasis basis = MakeViewBasis(
            stage->player.GetYawRadians(), stage->player.GetPitchRadians());
        const Float3 cameraMuzzle = ResolveWeaponMuzzleCameraPosition(
            weaponShotGeometry, config.worldVerticalFovRadians);
        Float3 muzzle = AddScaled(cameraOrigin, basis.forward, cameraMuzzle.z);
        muzzle = AddScaled(muzzle, basis.right, cameraMuzzle.x);
        muzzle = AddScaled(muzzle, basis.up, cameraMuzzle.y);
        // Authored offsets may extend past the body's collision radius. Keep
        // the ray's origin on the camera side of adjacent world geometry.
        return CombatCollision::ClampSegmentToWorld(
            stage->world.GetMap(), stage->world.GetSettings(), cameraOrigin, muzzle);
    }

    void SpawnPlayerShotTracer(const Float3 resolvedPoint) {
        const Float3 muzzle = ResolvePlayerMuzzle();
        const Float3 direction = Subtract(resolvedPoint, muzzle);
        const float distance = Length(direction);
        if (distance <= kLengthEpsilon) return;
        // Camera recoil can move the cosmetic path behind a corner even though
        // the shot was valid. Clip only against the world, not enemy capsules.
        const std::optional<CombatHit> worldHit = CombatCollision::Raycast(
            stage->world.GetMap(), stage->world.GetSettings(), muzzle, direction, distance);
        static_cast<void>(stage->projectiles.SpawnPlayerTracer(
            muzzle, worldHit.has_value() ? worldHit->position : resolvedPoint));
    }

    [[nodiscard]] Float3 ResolvePlayerShot(const ShotEvent& shot) {
        const Float3 cameraOrigin = stage->player.GetEyePosition(
            playerController.GetSettings().eyeHeight);
        const ViewBasis basis = MakeViewBasis(
            stage->player.GetYawRadians(), stage->player.GetPitchRadians());
        const std::vector<CombatTarget> targets =
            MakeCombatTargets(stage->enemies.GetSnapshots());
        const std::optional<CombatHit> aimHit = CombatCollision::Raycast(
            stage->world.GetMap(),
            stage->world.GetSettings(),
            cameraOrigin,
            basis.forward,
            kMaximumShotDistance,
            targets);
        const Float3 aimPoint = aimHit.has_value()
                                    ? aimHit->position
                                    : AddScaled(
                                          cameraOrigin,
                                          basis.forward,
                                          kMaximumShotDistance);
        const Float3 muzzle = ResolvePlayerMuzzle();

        std::optional<CombatHit> resolvedHit;
        Float3 resolvedPoint = aimPoint;
        const Float3 muzzleToAim = Subtract(aimPoint, muzzle);
        const float muzzleDistance = Length(muzzleToAim);
        if (muzzleDistance > kLengthEpsilon) {
            resolvedHit = CombatCollision::Raycast(
                stage->world.GetMap(),
                stage->world.GetSettings(),
                muzzle,
                muzzleToAim,
                muzzleDistance,
                targets);
            resolvedPoint = resolvedHit.has_value() ? resolvedHit->position : aimPoint;
            if (!resolvedHit.has_value()) {
                resolvedHit = aimHit;
            }
        } else {
            resolvedHit = aimHit;
        }

        if (resolvedHit.has_value() &&
            resolvedHit->kind == CombatHitKind::Target) {
            EnemyDefinitionId definitionId;
            for (const EnemySnapshot& enemy : stage->enemies.GetSnapshots()) {
                if (enemy.id == resolvedHit->targetId) {
                    definitionId = enemy.definitionId;
                    break;
                }
            }
            const EnemyDamageResult damage = stage->enemies.ApplyDamage(
                resolvedHit->targetId, shot.damage);
            if (damage.killed) {
                const LevelDefinition& definition =
                    content->Stages()[stage->contentIndex].definition;
                static_cast<void>(campaign.RecordKill(definition.id));
                Emit(EnemyDestroyedEvent{resolvedHit->targetId, std::move(definitionId)});
            }
        }
        return resolvedPoint;
    }

    void BeginDeathResults(std::string& error) {
        if (campaign.GetOutcome() != CampaignOutcome::InProgress) {
            return;
        }
        campaign.Fail();
        Emit(CampaignFinishedEvent{CampaignOutcome::PlayerDied});
        if (!transition.Begin({DestinationKind::Results, 0})) {
            error = "death results transition was rejected";
        }
    }

    void UpdateDoorState() {
        if (!stage || stage->doorVisible) {
            return;
        }
        const LevelDefinition& definition =
            content->Stages()[stage->contentIndex].definition;
        const CampaignRoomStats* room = campaign.FindRoom(definition.id);
        if (room == nullptr || room->kills < definition.clearKillCount) {
            return;
        }
        stage->doorVisible = true;
        stage->exitRequiresLeave = IsPlayerOnExit();
    }

    [[nodiscard]] bool IsPlayerOnExit() const {
        if (!stage) {
            return false;
        }
        const GridMap& map = stage->world.GetMap();
        const std::optional<GridCoordinate> coordinate =
            map.TryGetCoordinateAtPosition(
                stage->player.GetPositionXZ(),
                stage->world.GetSettings().cellSize);
        return coordinate.has_value() &&
               map.GetTile(coordinate->row, coordinate->column) ==
                   TileType::NextMapExit;
    }

    void HandleStageExit(
        const Float2 previousPlayerPosition,
        std::string& error) {
        if (!stage || !stage->doorVisible) {
            return;
        }
        const GridMap& map = stage->world.GetMap();
        const bool endedOnExit = IsPlayerOnExit();
        const bool crossedExit = SegmentIntersectsCell(
            previousPlayerPosition,
            stage->player.GetPositionXZ(),
            map.GetNextMapExitCell(),
            stage->world.GetSettings().cellSize);
        if (stage->exitRequiresLeave) {
            if (!endedOnExit) {
                stage->exitRequiresLeave = false;
            }
            return;
        }
        if (!endedOnExit && !crossedExit) {
            return;
        }

        const LevelDefinition& current =
            content->Stages()[stage->contentIndex].definition;
        Emit(StageCompletedEvent{current.id});
        if (!current.nextLevelId.has_value()) {
            campaign.Complete();
            Emit(CampaignFinishedEvent{CampaignOutcome::Completed});
            if (!transition.Begin({DestinationKind::Results, 0})) {
                error = "campaign results transition was rejected";
            }
            return;
        }

        const CampaignStageContent* next = content->FindStage(*current.nextLevelId);
        if (next == nullptr) {
            error = "current stage references an unknown next_level_id";
            return;
        }
        const auto stages = content->Stages();
        const std::size_t nextIndex = static_cast<std::size_t>(next - stages.data());
        static_cast<void>(BeginStageTransition(nextIndex, error));
    }

    void RefreshSnapshot() {
        snapshot = {};
        snapshot.screen = flow.GetScreen();
        snapshot.transitionPhase = transition.phase;
        snapshot.fadeOpacity = transition.opacity;
        snapshot.campaignOutcome = campaign.GetOutcome();
        snapshot.quitRequested = quitRequested;
        snapshot.worldVerticalFovRadians = config.worldVerticalFovRadians;
        snapshot.campaignRooms.assign(
            campaign.GetRooms().begin(), campaign.GetRooms().end());
        snapshot.weapon = weaponController.MakeHudSnapshot(weaponState);
        snapshot.weaponPresentation = weaponController.MakePresentationSnapshot(weaponState);

        if (!stage || !content || stage->contentIndex >= content->Stages().size()) {
            return;
        }
        const CampaignStageContent& item = content->Stages()[stage->contentIndex];
        snapshot.activeStage = ActiveStageSnapshot{
            item.definition.id,
            item.definition.name,
            stage->contentIndex,
            content->Stages().size(),
            stage->doorVisible,
        };
        snapshot.player = PlayerSnapshot{
            stage->player.GetPositionXZ(),
            playerController.GetSettings().eyeHeight,
            stage->player.GetYawRadians(),
            stage->player.GetPitchRadians(),
            playerCombat.GetHealth(),
            playerCombat.GetMaximumHealth(),
            stage->player.GetFeetY(),
            stage->player.GetVerticalVelocity(),
            stage->player.IsGrounded(),
            playerController.GetSettings().bodyHeight,
            playerController.GetSettings().collisionRadius,
        };
        snapshot.worldCollisionBoxes = BuildWorldCollisionBoxes(stage->world.GetMap(),stage->world.GetSettings());
        snapshot.enemies.assign(
            stage->enemies.GetSnapshots().begin(),
            stage->enemies.GetSnapshots().end());
        snapshot.projectiles.assign(
            stage->projectiles.GetSnapshots().begin(),
            stage->projectiles.GetSnapshots().end());
    }
};

GameSession::GameSession() noexcept = default;
GameSession::~GameSession() = default;
GameSession::GameSession(GameSession&&) noexcept = default;
GameSession& GameSession::operator=(GameSession&&) noexcept = default;

bool GameSession::Initialize(
    std::shared_ptr<const CampaignContent> content,
    const GameSessionConfig& config,
    std::string& error) {
    auto next = std::make_unique<Impl>();
    if (!next->Initialize(std::move(content), config, error)) {
        return false;
    }
    impl_ = std::move(next);
    return true;
}

bool GameSession::Advance(
    const float deltaSeconds,
    const GameFrameInput& input,
    const std::span<const GameSessionCommand> commands,
    std::string& error) {
    if (!impl_) {
        error = "GameSession must be initialized before Advance.";
        return false;
    }
    return impl_->Advance(deltaSeconds, input, commands, error);
}

const GameSessionSnapshot& GameSession::Snapshot() const noexcept {
    static const GameSessionSnapshot empty;
    return impl_ ? impl_->snapshot : empty;
}

std::span<const GameSessionEvent> GameSession::Events() const noexcept {
    return impl_ ? std::span<const GameSessionEvent>{impl_->events}
                 : std::span<const GameSessionEvent>{};
}

bool GameSession::IsInitialized() const noexcept {
    return impl_ != nullptr;
}

} // namespace fps
