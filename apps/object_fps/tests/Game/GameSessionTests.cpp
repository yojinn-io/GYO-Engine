#include "../TestSupport.hpp"

#include "RetroFPS/Data/GameData.hpp"
#include "RetroFPS/Game/CampaignContent.hpp"
#include "RetroFPS/Game/GameSession.hpp"
#include "RetroFPS/World/GridMapLoader.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace fps::tests {
namespace {

constexpr std::string_view kEnemies =
    "enemy_id,kind,damage,attack_interval_seconds,hp,defense,hitbox_radius,hitbox_height,render_width,render_height,texture_asset_id,frame_width_px,frame_height_px\n"
    "melee_basic,melee,15,0.9,50,5,0.2,0.8,0.973913,0.8,object_fps.texture.enemy.blood_dog,560,460\n"
    "ranged_basic,ranged,10,1.25,40,0,0.2,1.6,1.230769,1.6,object_fps.texture.enemy.spitter,700,910\n";

constexpr std::string_view kAnimations =
    "enemy_id,state,origin_x_px,origin_y_px,frame_count,seconds_per_frame,event_frame_index,muzzle_x_px,muzzle_y_px\n"
    "melee_basic,idle,0,0,3,0.1,,,\n"
    "melee_basic,move,0,460,4,0.1,,,\n"
    "melee_basic,attack,0,920,6,0.05,3,,\n"
    "melee_basic,dead,0,1380,4,0.1,,,\n"
    "ranged_basic,idle,0,0,3,0.1,,,\n"
    "ranged_basic,move,0,910,4,0.1,,,\n"
    "ranged_basic,attack,0,1820,5,0.05,2,350,420\n"
    "ranged_basic,dead,0,2730,4,0.1,,,\n";

constexpr std::string_view kWeapons =
    "weapon_id,damage,magazine_size,reserve_ammo,recoil,automatic,fire_interval_seconds,reload_seconds,texture_asset_id\n"
    "starter_pistol,25,12,48,1.5,false,0.2,1.5,object_fps.texture.weapon.starter_pistol\n";

constexpr std::string_view kMap =
    "##############\n"
    "#P..........R#\n"
    "#..###..#....#\n"
    "#......##....#\n"
    "#......##....#\n"
    "#......##....#\n"
    "#..#...##....#\n"
    "#..#..####...#\n"
    "#.M.........D#\n"
    "##############\n";

[[nodiscard]] std::string MakeLevels(const std::size_t count) {
    std::ostringstream csv;
    csv << "level_id,level_name,map_asset_id,next_level_id,ranged_enemy_count,"
           "melee_enemy_count,active_enemy_limit,clear_kill_count\n";
    for (std::size_t index = 0; index < count; ++index) {
        csv << "room_" << index << ",ROOM " << index << ",object_fps.map.room_"
            << index << ',';
        if (index + 1 < count) {
            csv << "room_" << (index + 1);
        }
        csv << ",1,1,2,1\n";
    }
    return csv.str();
}

[[nodiscard]] std::shared_ptr<const CampaignContent> MakeContent(
    TestContext& context,
    const std::size_t stageCount) {
    GameDataLoadResult data =
        GameDataLoader::Parse(kEnemies, kAnimations, kWeapons, MakeLevels(stageCount));
    context.Expect(data.Succeeded(), "session fixture game data parses");
    if (!data.catalog.has_value()) {
        return {};
    }

    std::vector<GridMap> maps;
    maps.reserve(stageCount);
    for (std::size_t index = 0; index < stageCount; ++index) {
        MapLoadResult map = GridMapLoader::Parse(kMap);
        context.Expect(map.Succeeded(), "session fixture map parses");
        if (!map.map.has_value()) {
            return {};
        }
        maps.push_back(std::move(*map.map));
    }

    CampaignContentBuildResult content =
        CampaignContent::Build(std::move(*data.catalog), std::move(maps));
    context.Expect(content.Succeeded(), "session fixture CampaignContent builds");
    if (!content.content.has_value()) {
        return {};
    }
    return std::make_shared<CampaignContent>(std::move(*content.content));
}

template <class Payload>
[[nodiscard]] bool HasEvent(const std::span<const GameSessionEvent> events) {
    for (const GameSessionEvent& event : events) {
        if (std::holds_alternative<Payload>(event.payload)) {
            return true;
        }
    }
    return false;
}

void TestContentCardinality(TestContext& context) {
    for (const std::size_t stageCount : {std::size_t{1}, std::size_t{2}, std::size_t{4}}) {
        const std::shared_ptr<const CampaignContent> content =
            MakeContent(context, stageCount);
        context.Expect(
            content && content->Stages().size() == stageCount,
            "CampaignContent preserves arbitrary data-defined cardinality");
        if (content) {
            context.Expect(
                content->Stages().back().definition.nextLevelId == std::nullopt,
                "the chain-defined terminal stage is retained");
        }
    }
}

void TestSessionCommandsSnapshotsAndEvents(TestContext& context) {
    const std::shared_ptr<const CampaignContent> content = MakeContent(context, 4);
    if (!content) {
        return;
    }

    GameSession session;
    GameSessionConfig config;
    config.fadeOutSeconds = 0.1F;
    config.fadeInSeconds = 0.1F;
    std::string error;
    context.Expect(
        session.Initialize(content, config, error),
        "GameSession initializes from immutable CampaignContent");
    context.Expect(error.empty(), "successful GameSession initialization clears errors");

    const GameSessionCommand illegal = PauseCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&illegal, 1}, error),
        "illegal external command does not corrupt the session");
    context.Expect(
        HasEvent<CommandRejectedEvent>(session.Events()),
        "illegal external command produces a typed rejection event");
    context.Expect(
        session.Snapshot().screen == GameScreen::MainMenu,
        "rejected command leaves query state unchanged");

    const GameSessionCommand openControls = OpenControlsCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&openControls, 1}, error) &&
            session.Snapshot().screen == GameScreen::Controls,
        "OpenControls is accepted as an opaque-UI semantic command");
    const GameSessionCommand closeControls = CloseControlsCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&closeControls, 1}, error) &&
            session.Snapshot().screen == GameScreen::MainMenu,
        "CloseControls returns to MainMenu without a menu-index switch");

    const GameSessionCommand start = StartCampaignCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&start, 1}, error),
        "StartCampaign command enters the transition protocol");
    context.Expect(
        session.Snapshot().transitionPhase == StageTransitionPhase::FadingOut,
        "query exposes the pending runtime transition");

    context.Expect(
        session.Advance(0.1F, {}, {}, error),
        "data-defined first stage commits after fade out");
    const GameSessionSnapshot& playing = session.Snapshot();
    context.Expect(
        playing.screen == GameScreen::Playing && playing.activeStage.has_value(),
        "query exposes the committed playing stage");
    context.Expect(
        playing.activeStage.has_value() && playing.activeStage->levelId == "room_0" &&
            playing.activeStage->stageCount == 4,
        "stage snapshot uses catalog identity and dynamic cardinality");
    context.Expect(
        playing.player.has_value() && playing.enemies.size() == 2,
        "snapshot contains observer-safe player and enemy values");
    context.Expect(
        HasEvent<StageEnteredEvent>(session.Events()) &&
            HasEvent<EnemySpawnedEvent>(session.Events()),
        "stage commit emits typed runtime facts");

    context.Expect(
        session.Advance(0.1F, {}, {}, error),
        "fade in completes without changing stage policy");
    const GameSessionCommand pause = PauseCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&pause, 1}, error),
        "Pause is accepted through the command boundary");
    context.Expect(
        session.Snapshot().screen == GameScreen::Paused &&
            HasEvent<ScreenChangedEvent>(session.Events()),
        "pause updates query state and emits a typed screen event");

    const GameSessionCommand resume = ResumeCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&resume, 1}, error),
        "Resume is accepted through the same command boundary");
    context.Expect(
        session.Snapshot().screen == GameScreen::Playing,
        "resume returns the session to its active stage");

    context.Expect(
        session.Advance(0.0F, {}, std::span{&pause, 1}, error),
        "session can pause again before testing local resume input");
    const std::uint32_t ammoBeforeLocalResume =
        session.Snapshot().weapon.magazineAmmo;
    const Float2 positionBeforeLocalResume = session.Snapshot().player.has_value()
        ? session.Snapshot().player->position
        : Float2{};
    GameFrameInput resumeInput;
    resumeInput.backPressed = true;
    resumeInput.moveForward = 1.0F;
    resumeInput.fireHeld = true;
    resumeInput.firePressed = true;
    context.Expect(
        session.Advance(0.0F, resumeInput, {}, error) &&
            session.Snapshot().screen == GameScreen::Playing,
        "escape resumes through the local GameFlow path");

    GameFrameInput heldAfterResume;
    heldAfterResume.moveForward = 1.0F;
    heldAfterResume.fireHeld = true;
    heldAfterResume.firePressed = true;
    context.Expect(
        session.Advance(0.05F, heldAfterResume, {}, error),
        "first held-input frame after local resume advances safely");
    context.Expect(
        session.Snapshot().weapon.magazineAmmo == ammoBeforeLocalResume,
        "local resume requires fire release before gameplay accepts another shot");
    context.Expect(
        session.Snapshot().player.has_value() &&
            session.Snapshot().player->position.x == positionBeforeLocalResume.x &&
            session.Snapshot().player->position.z == positionBeforeLocalResume.z,
        "local resume requires movement release before gameplay moves the player");

    const GameSessionCommand quit = RequestQuitCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&quit, 1}, error),
        "quit intent is accepted without platform coupling");
    context.Expect(
        session.Snapshot().quitRequested &&
            HasEvent<QuitRequestedEvent>(session.Events()),
        "quit intent is visible through query and event seams");
}

void TestInvalidDeltaDoesNotAdvance(TestContext& context) {
    const std::shared_ptr<const CampaignContent> content = MakeContent(context, 1);
    if (!content) {
        return;
    }
    GameSession session;
    std::string error;
    context.Expect(session.Initialize(content, {}, error), "delta test session initializes");
    context.Expect(
        !session.Advance(-0.1F, {}, {}, error) && !error.empty(),
        "negative delta is rejected at the game-policy boundary");
}

void TestFocusLossSurvivesTransitionInputLock(TestContext& context) {
    const std::shared_ptr<const CampaignContent> content = MakeContent(context, 1);
    if (!content) {
        return;
    }

    GameSession session;
    GameSessionConfig config;
    config.fadeOutSeconds = 0.1F;
    config.fadeInSeconds = 0.1F;
    std::string error;
    context.Expect(
        session.Initialize(content, config, error),
        "focus-loss transition session initializes");

    const GameSessionCommand start = StartCampaignCommand{};
    context.Expect(
        session.Advance(0.0F, {}, std::span{&start, 1}, error),
        "focus-loss transition starts a campaign");

    GameFrameInput focusLost;
    focusLost.focusLost = true;
    context.Expect(
        session.Advance(0.05F, focusLost, {}, error) &&
            session.Snapshot().transitionPhase != StageTransitionPhase::Idle,
        "focus loss is accepted while fade input is locked");
    context.Expect(
        session.Advance(0.05F, {}, {}, error) &&
            session.Snapshot().screen == GameScreen::Playing,
        "the stage commits after the focus-loss edge");
    context.Expect(
        session.Advance(0.1F, {}, {}, error) &&
            session.Snapshot().transitionPhase == StageTransitionPhase::Idle,
        "the fade completes before safety input is delivered");
    context.Expect(
        session.Advance(0.0F, {}, {}, error) &&
            session.Snapshot().screen == GameScreen::Paused,
        "latched focus loss pauses the first stable Playing frame");
}

} // namespace

void RunGameSessionTests(TestContext& context) {
    TestContentCardinality(context);
    TestSessionCommandsSnapshotsAndEvents(context);
    TestInvalidDeltaDoesNotAdvance(context);
    TestFocusLossSurvivesTransitionInputLock(context);
}

} // namespace fps::tests
