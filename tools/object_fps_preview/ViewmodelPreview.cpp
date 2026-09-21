#include "ViewmodelPreview.hpp"
#include "RetroFPS/App/ObjectFpsPresentation.hpp"
#include "RetroFPS/App/ObjectFpsUi.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/loaders/TextLoader.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/Renderer.hpp"
#include "ui/UiDocumentCodec.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
namespace {
template <class Error> void LogError(const Error& error) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s%s%s", error.message.c_str(),
        error.detail.empty() ? "" : ": ", error.detail.c_str());
}
void LogError(const std::string& error) { SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.c_str()); }
}
// Interactive design preview. Asset editing remains outside the game product.
int RunViewmodelPreview(
    Engine::Platform::Sdl::SdlPlatform& platform,
    Engine::Render::Renderer& renderer,
    fps::ObjectFpsPresentation& presentation,
    Engine::Asset::AssetManager& assets,
    const std::shared_ptr<const fps::CampaignContent>& content,
    const float width,
    const float height) {
    const auto documentHandle = assets.Load(
        Engine::Asset::AssetId::FromString("object_fps.ui.screens"),
        Engine::Asset::AssetRequest::WithTypeHint(Engine::Asset::AssetType::Text()));
    if (!documentHandle) { LogError(documentHandle.error()); return 1; }
    const auto text = assets.GetSharedConst<Engine::Asset::Loaders::TextAsset>(documentHandle.value());
    assets.Release(documentHandle.value());
    if (!text) { LogError(std::string{"visual probe UI text is missing"}); return 1; }
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(text->text, "object_fps.ui.screens");
    if (!parsed) { LogError(parsed.error().message); return 1; }
    fps::ObjectFpsUi ui;
    std::string error;
    if (!ui.Initialize(std::make_shared<const Engine::Ui::UiDocument>(std::move(parsed).value()), error)) {
        LogError(error); return 1;
    }
    const auto& stage = content->Stages().front();
    const auto& definition = content->Data().weapons.GetDefinitions().front();
    fps::GameSessionSnapshot snapshot;
    snapshot.screen = fps::GameScreen::Playing;
    snapshot.activeStage = fps::ActiveStageSnapshot{
        stage.definition.id, stage.definition.name, 0, content->Stages().size(), true};
    snapshot.player = fps::PlayerSnapshot{
        stage.map.GetSpawnPosition(), 1.6F, 0, 0, 100, 100};
    // The verification room has a clear approach to its exit: keep the white
    // door and its adjacent wall visible while checking weapon framing.
    const auto exit = stage.map.GetNextMapExitCell();
    if (exit.row >= 2 && stage.map.IsWalkable(
            static_cast<std::ptrdiff_t>(exit.row) - 2,
            static_cast<std::ptrdiff_t>(exit.column))) {
        snapshot.player->position = stage.map.GetCellCenter({exit.row - 2, exit.column});
    }
    snapshot.weapon.weaponId = definition.id;
    snapshot.weapon.magazineAmmo = definition.magazineCapacity;
    snapshot.weapon.reserveAmmo = definition.reserveAmmo;
    snapshot.weaponPresentation.weaponId = definition.id;
    constexpr std::array actions{fps::WeaponAction::Idle, fps::WeaponAction::Shoot,
        fps::WeaponAction::Reload, fps::WeaponAction::Draw, fps::WeaponAction::Hide,
        fps::WeaponAction::Holstered};
    constexpr std::array names{"Idle", "Shoot", "Reload", "Draw", "Hide", "Holstered"};
    const std::array durations{0.0F, definition.fireIntervalSeconds, definition.reloadSeconds,
        definition.drawSeconds, definition.hideSeconds, 0.0F};
    std::size_t action = 0;
    float progress = 0.5F;
    bool running = true;
    while (running) {
        if (platform.PumpEvents([&](const SDL_Event& event) {
            if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat) return;
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
            if (event.key.scancode >= SDL_SCANCODE_1 && event.key.scancode <= SDL_SCANCODE_6) {
                action = static_cast<std::size_t>(event.key.scancode - SDL_SCANCODE_1);
            }
            if (event.key.scancode == SDL_SCANCODE_LEFT) progress = (std::max)(0.0F, progress - 0.1F);
            if (event.key.scancode == SDL_SCANCODE_RIGHT) progress = (std::min)(1.0F, progress + 0.1F);
            if (event.key.scancode == SDL_SCANCODE_SPACE) snapshot.player->feetY = snapshot.player->feetY > 0 ? 0 : 0.6F;
        }) == Engine::Runtime::RuntimeControl::Stop) break;
        snapshot.weaponPresentation.action = actions[action];
        snapshot.weaponPresentation.durationSeconds = durations[action];
        snapshot.weaponPresentation.elapsedSeconds = progress * durations[action];
        snapshot.weapon.reloading = actions[action] == fps::WeaponAction::Reload;
        snapshot.weapon.reloadProgress = snapshot.weapon.reloading ? progress : 0.0F;
        const std::string title = std::string{"Object_FPS Mark23 preview | "} + names[action] +
            " " + std::to_string(static_cast<int>(progress * 100)) + "% | 1-6 action, arrows time, Space height, Esc close";
        SDL_SetWindowTitle(platform.NativeWindow(), title.c_str());
        Engine::Ui::UiDrawList draw;
        if (!ui.Compose(snapshot, {}, {width, height}, draw, error) ||
            !presentation.Present(snapshot, {}, draw, error)) { LogError(error); return 1; }
        SDL_Delay(16);
    }
    return 0;
}

