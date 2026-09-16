#pragma once

#include "RetroFPS/Game/CampaignContent.hpp"
#include "RetroFPS/Game/CampaignRunState.hpp"
#include "RetroFPS/Game/GameFlow.hpp"
#include "RetroFPS/Gameplay/Combat/ProjectileSystem.hpp"
#include "RetroFPS/Gameplay/Enemy/EnemySystem.hpp"
#include "RetroFPS/Gameplay/Player/PlayerSettings.hpp"
#include "RetroFPS/Gameplay/Weapon/WeaponState.hpp"
#include "RetroFPS/Math/Vector.hpp"
#include "RetroFPS/World/WorldSettings.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace fps {

struct GameSessionConfig final {
    WeaponDefinitionId startingWeaponId{"starter_pistol"};
    EnemyDefinitionId meleeEnemyId{"melee_basic"};
    EnemyDefinitionId rangedEnemyId{"ranged_basic"};
    WorldSettings world{};
    PlayerSettings player{};
    EnemySettings enemies{};
    WeaponControllerSettings weapon{};
    ProjectileSettings projectiles{};
    float fadeOutSeconds{0.4F};
    float fadeInSeconds{0.4F};
    float worldVerticalFovRadians{std::numbers::pi_v<float> / 3.0F};
};

// Semantic game input. Physical bindings belong to the app's GYO
// InputActionMap configuration, not to gameplay simulation.
struct GameFrameInput final {
    float moveForward{};
    float moveRight{};
    float lookDeltaX{};
    float lookDeltaY{};
    bool lookEnabled{};
    bool fireHeld{};
    bool firePressed{};
    bool reloadPressed{};
    bool backPressed{};
    bool focusLost{};
    bool jumpPressed{};
    bool jumpHeld{};
    bool holsterTogglePressed{};
    bool holsterToggleHeld{};
};

struct StartCampaignCommand final {};
struct OpenControlsCommand final {};
struct CloseControlsCommand final {};
struct PauseCommand final {};
struct ResumeCommand final {};
struct ReturnToMainMenuCommand final {};
struct RequestQuitCommand final {};

using GameSessionCommand = std::variant<
    StartCampaignCommand,
    OpenControlsCommand,
    CloseControlsCommand,
    PauseCommand,
    ResumeCommand,
    ReturnToMainMenuCommand,
    RequestQuitCommand>;

struct ScreenChangedEvent final {
    GameScreen previous{GameScreen::MainMenu};
    GameScreen current{GameScreen::MainMenu};
};

struct StageEnteredEvent final {
    LevelDefinitionId levelId;
    // Zero-based campaign content index. Presentation may convert it to a
    // one-based player-facing stage number.
    std::size_t ordinal{};
    std::size_t stageCount{};
};

struct StageCompletedEvent final {
    LevelDefinitionId levelId;
};

struct EnemySpawnedEvent final {
    EnemyId enemyId{};
    EnemyDefinitionId definitionId;
};

struct EnemyDestroyedEvent final {
    EnemyId enemyId{};
    EnemyDefinitionId definitionId;
};

struct PlayerDamagedEvent final {
    float appliedDamage{};
    float remainingHealth{};
};

struct CampaignFinishedEvent final {
    CampaignOutcome outcome{CampaignOutcome::InProgress};
};

struct QuitRequestedEvent final {};

struct CommandRejectedEvent final {
    std::string reason;
};

using GameSessionEventPayload = std::variant<
    ScreenChangedEvent,
    StageEnteredEvent,
    StageCompletedEvent,
    EnemySpawnedEvent,
    EnemyDestroyedEvent,
    PlayerDamagedEvent,
    CampaignFinishedEvent,
    QuitRequestedEvent,
    CommandRejectedEvent,
    ShotEvent,
    WeaponActionEvent>;

struct GameSessionEvent final {
    std::uint64_t sequence{};
    GameSessionEventPayload payload;
};

enum class StageTransitionPhase {
    Idle,
    FadingOut,
    CommitPending,
    FadingIn,
};

struct ActiveStageSnapshot final {
    LevelDefinitionId levelId;
    std::string levelName;
    // Zero-based because presentation also uses this value to index the
    // campaign's immutable stage resources.
    std::size_t ordinal{};
    std::size_t stageCount{};
    bool doorVisible{};
};

struct PlayerSnapshot final {
    Float2 position{};
    float eyeHeight{};
    float yawRadians{};
    float pitchRadians{};
    float health{};
    float maximumHealth{};
    float feetY{};
    float verticalVelocity{};
    bool grounded{true};
    float bodyHeight{};
};

struct GameSessionSnapshot final {
    GameScreen screen{GameScreen::MainMenu};
    StageTransitionPhase transitionPhase{StageTransitionPhase::Idle};
    float fadeOpacity{};
    std::optional<ActiveStageSnapshot> activeStage;
    std::optional<PlayerSnapshot> player;
    WeaponHudSnapshot weapon;
    WeaponPresentationSnapshot weaponPresentation;
    std::vector<EnemySnapshot> enemies;
    std::vector<ProjectileSnapshot> projectiles;
    CampaignOutcome campaignOutcome{CampaignOutcome::InProgress};
    std::vector<CampaignRoomStats> campaignRooms;
    bool quitRequested{};
    float worldVerticalFovRadians{std::numbers::pi_v<float> / 3.0F};
};

class GameSession final {
public:
    GameSession() noexcept;
    ~GameSession();

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;
    GameSession(GameSession&&) noexcept;
    GameSession& operator=(GameSession&&) noexcept;

    [[nodiscard]] bool Initialize(
        std::shared_ptr<const CampaignContent> content,
        const GameSessionConfig& config,
        std::string& error);

    [[nodiscard]] bool Advance(
        float deltaSeconds,
        const GameFrameInput& input,
        std::span<const GameSessionCommand> commands,
        std::string& error);

    [[nodiscard]] const GameSessionSnapshot& Snapshot() const noexcept;
    [[nodiscard]] std::span<const GameSessionEvent> Events() const noexcept;
    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace fps
