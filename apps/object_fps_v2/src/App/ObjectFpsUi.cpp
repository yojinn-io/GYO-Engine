#include "RetroFPS/App/ObjectFpsUi.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace fps {
namespace {

constexpr float kReferenceWidth = 1280.0F;
constexpr float kReferenceHeight = 720.0F;
constexpr std::string_view kUiFont = "object_fps_v2.font.ui";

struct UiActionContractEntry final {
    std::string_view id;
    Engine::Ui::UiActionPayloadType payload;
};

constexpr std::array<UiActionContractEntry, 8> kUiActionContract{{
    {"object_fps_v2.start_game", Engine::Ui::UiActionPayloadType::None},
    {"object_fps_v2.open_controls", Engine::Ui::UiActionPayloadType::None},
    {"object_fps_v2.close_controls", Engine::Ui::UiActionPayloadType::None},
    {"object_fps_v2.resume", Engine::Ui::UiActionPayloadType::None},
    {"object_fps_v2.return_main_menu", Engine::Ui::UiActionPayloadType::None},
    {"object_fps_v2.quit", Engine::Ui::UiActionPayloadType::None},
    {"object_fps_v2.set_gamma", Engine::Ui::UiActionPayloadType::Number},
    {"object_fps_v2.set_exposure", Engine::Ui::UiActionPayloadType::Number},
}};

constexpr Engine::Ui::UiColor kPanel{0.055F, 0.075F, 0.11F, 0.94F};
constexpr Engine::Ui::UiColor kAccent{0.25F, 0.82F, 0.95F, 1.0F};
constexpr Engine::Ui::UiColor kText{0.92F, 0.96F, 1.0F, 1.0F};
constexpr Engine::Ui::UiColor kDanger{0.93F, 0.25F, 0.22F, 1.0F};
constexpr Engine::Ui::UiColor kHealth{0.18F, 0.78F, 0.35F, 1.0F};
constexpr Engine::Ui::UiColor kBarTrack{0.08F, 0.10F, 0.13F, 0.96F};

[[nodiscard]] std::string FormatUiError(const Engine::Ui::UiError& value) {
    std::string result = value.message;
    if (!value.source.empty()) {
        result += " [" + value.source + ']';
    }
    if (!value.jsonPointer.empty()) {
        result += " at " + value.jsonPointer;
    }
    return result;
}

[[nodiscard]] constexpr std::string_view PayloadTypeName(
    const Engine::Ui::UiActionPayloadType payload) noexcept {
    switch (payload) {
    case Engine::Ui::UiActionPayloadType::None:
        return "none";
    case Engine::Ui::UiActionPayloadType::Number:
        return "number";
    }
    return "unknown";
}

[[nodiscard]] bool ValidateActionContract(
    const Engine::Ui::UiDocument& document,
    std::string& error) {
    std::array<bool, kUiActionContract.size()> declared{};
    for (const Engine::Ui::UiActionDeclaration& action : document.actions) {
        const auto expected = std::find_if(
            kUiActionContract.begin(),
            kUiActionContract.end(),
            [&action](const UiActionContractEntry& candidate) {
                return candidate.id == action.id;
            });
        if (expected == kUiActionContract.end()) {
            error = "Object_FPS UI action contract contains unsupported action '" +
                    action.id + "'";
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(
            std::distance(kUiActionContract.begin(), expected));
        if (declared[index]) {
            error = "Object_FPS UI action contract declares action '" +
                    action.id + "' more than once";
            return false;
        }
        if (action.payload != expected->payload) {
            error = "Object_FPS UI action '" + action.id +
                    "' must declare payload '" +
                    std::string(PayloadTypeName(expected->payload)) +
                    "' (found '" +
                    std::string(PayloadTypeName(action.payload)) + "')";
            return false;
        }
        declared[index] = true;
    }

    for (std::size_t index = 0; index < kUiActionContract.size(); ++index) {
        if (!declared[index]) {
            error = "Object_FPS UI action contract is missing required action '" +
                    std::string(kUiActionContract[index].id) + "'";
            return false;
        }
    }
    return true;
}

struct HudLayout final {
    float scale{};
    float offsetX{};
    float offsetY{};
};

[[nodiscard]] std::optional<HudLayout> MakeHudLayout(
    const Engine::Ui::UiViewport viewport) noexcept {
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height) ||
        viewport.width <= 0.0F || viewport.height <= 0.0F) {
        return std::nullopt;
    }
    const float scale = std::min(
        viewport.width / kReferenceWidth,
        viewport.height / kReferenceHeight);
    return HudLayout{
        scale,
        (viewport.width - kReferenceWidth * scale) * 0.5F,
        (viewport.height - kReferenceHeight * scale) * 0.5F,
    };
}

[[nodiscard]] Engine::Ui::UiRect Place(
    const HudLayout& layout,
    const float x,
    const float y,
    const float width,
    const float height) noexcept {
    return {
        layout.offsetX + x * layout.scale,
        layout.offsetY + y * layout.scale,
        width * layout.scale,
        height * layout.scale,
    };
}

void AddQuad(
    Engine::Ui::UiDrawList& drawList,
    const Engine::Ui::UiRect bounds,
    const Engine::Ui::UiColor color) {
    drawList.commands.emplace_back(Engine::Ui::UiQuadDraw{bounds, color});
}

void AddText(
    Engine::Ui::UiDrawList& drawList,
    std::string text,
    const Engine::Ui::UiRect bounds,
    const float sizePixels,
    const Engine::Ui::UiColor color = kText,
    const Engine::Ui::UiHorizontalAlign horizontal =
        Engine::Ui::UiHorizontalAlign::Left) {
    Engine::Ui::UiTextDraw draw;
    draw.boundsPixels = bounds;
    draw.utf8 = std::move(text);
    draw.fontAssetId = std::string{kUiFont};
    draw.pointSizePixels = sizePixels;
    draw.color = color;
    draw.horizontalAlign = horizontal;
    draw.verticalAlign = Engine::Ui::UiVerticalAlign::Center;
    drawList.commands.emplace_back(std::move(draw));
}

void AddHud(
    Engine::Ui::UiDrawList& drawList,
    const GameSessionSnapshot& snapshot,
    const HudLayout& layout) {
    const float health = snapshot.player.has_value() ? snapshot.player->health : 0.0F;
    const float maximumHealth = snapshot.player.has_value()
        ? snapshot.player->maximumHealth
        : 0.0F;
    const float healthRatio = maximumHealth > 0.0F
        ? std::clamp(health / maximumHealth, 0.0F, 1.0F)
        : 0.0F;

    AddQuad(drawList, Place(layout, 32.0F, 626.0F, 290.0F, 62.0F), kPanel);
    AddText(
        drawList,
        "HP " + std::to_string(
            static_cast<std::uint32_t>(std::max(health, 0.0F))),
        Place(layout, 48.0F, 637.0F, 94.0F, 28.0F),
        20.0F * layout.scale);
    AddQuad(drawList, Place(layout, 142.0F, 644.0F, 160.0F, 16.0F), kBarTrack);
    AddQuad(
        drawList,
        Place(layout, 142.0F, 644.0F, 160.0F * healthRatio, 16.0F),
        healthRatio <= 0.25F ? kDanger : kHealth);

    AddQuad(drawList, Place(layout, 1000.0F, 626.0F, 248.0F, 62.0F), kPanel);
    AddText(
        drawList,
        std::to_string(snapshot.weapon.magazineAmmo) + " / " +
            std::to_string(snapshot.weapon.reserveAmmo),
        Place(layout, 1016.0F, 637.0F, 216.0F, 32.0F),
        25.0F * layout.scale,
        kText,
        Engine::Ui::UiHorizontalAlign::Right);

    if (snapshot.weapon.reloading) {
        const float progress = std::clamp(
            snapshot.weapon.reloadProgress,
            0.0F,
            1.0F);
        AddText(
            drawList,
            "RELOADING",
            Place(layout, 500.0F, 624.0F, 280.0F, 30.0F),
            18.0F * layout.scale,
            kAccent,
            Engine::Ui::UiHorizontalAlign::Center);
        AddQuad(drawList, Place(layout, 520.0F, 662.0F, 240.0F, 10.0F), kBarTrack);
        AddQuad(
            drawList,
            Place(layout, 520.0F, 662.0F, 240.0F * progress, 10.0F),
            kAccent);
    }

    if (snapshot.activeStage.has_value()) {
        const ActiveStageSnapshot& stage = *snapshot.activeStage;
        AddQuad(drawList, Place(layout, 28.0F, 24.0F, 430.0F, 48.0F), kPanel);
        AddText(
            drawList,
            "STAGE " + std::to_string(stage.ordinal + 1U) + " / " +
                std::to_string(stage.stageCount) + "  " + stage.levelName,
            Place(layout, 44.0F, 32.0F, 398.0F, 32.0F),
            18.0F * layout.scale);
    }

    const float expansion = std::clamp(
        snapshot.weapon.crosshairExpansion,
        0.0F,
        48.0F);
    const float gap = 8.0F + expansion;
    constexpr float armLength = 14.0F;
    constexpr float thickness = 3.0F;
    constexpr float centerX = kReferenceWidth * 0.5F;
    constexpr float centerY = kReferenceHeight * 0.5F;
    AddQuad(drawList, Place(
        layout, centerX - gap - armLength, centerY - thickness * 0.5F,
        armLength, thickness), kText);
    AddQuad(drawList, Place(
        layout, centerX + gap, centerY - thickness * 0.5F,
        armLength, thickness), kText);
    AddQuad(drawList, Place(
        layout, centerX - thickness * 0.5F, centerY - gap - armLength,
        thickness, armLength), kText);
    AddQuad(drawList, Place(
        layout, centerX - thickness * 0.5F, centerY + gap,
        thickness, armLength), kText);
}

[[nodiscard]] Engine::Ui::UiBindingTable MakeBindings(
    const GameSessionSnapshot& snapshot,
    const ObjectFpsDisplaySettings& displaySettings) {
    Engine::Ui::UiBindingTable result;
    result.emplace("display.exposure_ev", static_cast<double>(displaySettings.exposureEv));
    result.emplace("display.gamma", static_cast<double>(displaySettings.gammaAdjustment));

    std::string outcome = "in_progress";
    if (snapshot.campaignOutcome == CampaignOutcome::Completed) {
        outcome = "completed";
    } else if (snapshot.campaignOutcome == CampaignOutcome::PlayerDied) {
        outcome = "player_died";
    }
    result.emplace("results.outcome", std::move(outcome));

    std::int64_t totalKills = 0;
    std::int64_t visitedRooms = 0;
    Engine::Ui::UiList rooms;
    for (const CampaignRoomStats& room : snapshot.campaignRooms) {
        totalKills += static_cast<std::int64_t>(room.kills);
        if (!room.visited) {
            continue;
        }
        ++visitedRooms;
        Engine::Ui::UiListItem item;
        item.fields.emplace("name", room.levelName);
        item.fields.emplace("kills", static_cast<std::int64_t>(room.kills));
        rooms.items.push_back(std::move(item));
    }
    result.emplace("results.visited_rooms", visitedRooms);
    result.emplace(
        "results.total_rooms",
        static_cast<std::int64_t>(snapshot.campaignRooms.size()));
    result.emplace("results.total_kills", totalKills);
    result.emplace("results.rooms", std::move(rooms));
    return result;
}

[[nodiscard]] float Quantize(
    const double value,
    const float minimum,
    const float maximum,
    const float step) noexcept {
    if (!std::isfinite(value)) {
        return minimum;
    }
    const double clamped = std::clamp(
        value,
        static_cast<double>(minimum),
        static_cast<double>(maximum));
    const double index = std::round(
        (clamped - static_cast<double>(minimum)) /
        static_cast<double>(step));
    return static_cast<float>(std::clamp(
        static_cast<double>(minimum) + index * static_cast<double>(step),
        static_cast<double>(minimum),
        static_cast<double>(maximum)));
}

[[nodiscard]] std::optional<std::string_view> CanvasFor(
    const GameScreen screen) noexcept {
    switch (screen) {
    case GameScreen::MainMenu:
        return "main_menu";
    case GameScreen::Controls:
        return "controls";
    case GameScreen::Paused:
        return "pause";
    case GameScreen::Results:
        return "results";
    case GameScreen::Playing:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

struct ObjectFpsUi::Impl final {
    Engine::Ui::UiRuntime runtime;
    std::optional<GameScreen> activeScreen;
    bool initialized{};

    [[nodiscard]] bool EnsureCanvas(
        const GameScreen screen,
        std::string& error) {
        if (activeScreen == screen) {
            return true;
        }
        const std::optional<std::string_view> canvas = CanvasFor(screen);
        if (!canvas.has_value()) {
            activeScreen = screen;
            return true;
        }
        auto activated = runtime.ActivateCanvas(*canvas);
        if (!activated) {
            error = FormatUiError(activated.error());
            return false;
        }
        activeScreen = screen;
        return true;
    }
};

ObjectFpsUi::ObjectFpsUi()
    : impl_(std::make_unique<Impl>()) {}

ObjectFpsUi::~ObjectFpsUi() = default;
ObjectFpsUi::ObjectFpsUi(ObjectFpsUi&&) noexcept = default;
ObjectFpsUi& ObjectFpsUi::operator=(ObjectFpsUi&&) noexcept = default;

bool ObjectFpsUi::Initialize(
    std::shared_ptr<const Engine::Ui::UiDocument> document,
    std::string& error) {
    error.clear();
    impl_->runtime.Reset();
    impl_->activeScreen.reset();
    impl_->initialized = false;
    if (!document) {
        error = "ObjectFpsUi requires a non-null UI document";
        return false;
    }
    if (!ValidateActionContract(*document, error)) {
        return false;
    }
    auto initialized = impl_->runtime.Initialize(std::move(document));
    if (!initialized) {
        error = FormatUiError(initialized.error());
        return false;
    }
    // Object_FPS requires all four authored screens. This is intentionally a
    // strict one-time load with no compiled fallback.
    for (const std::string_view canvas : {
             std::string_view{"main_menu"},
             std::string_view{"controls"},
             std::string_view{"pause"},
             std::string_view{"results"}}) {
        auto activated = impl_->runtime.ActivateCanvas(canvas);
        if (!activated) {
            error = "required Object_FPS UI canvas '" + std::string(canvas) +
                    "' is unavailable: " + FormatUiError(activated.error());
            impl_->runtime.Reset();
            return false;
        }
    }
    impl_->initialized = true;
    return true;
}

bool ObjectFpsUi::Update(
    const GameSessionSnapshot& snapshot,
    const Engine::Ui::UiInputFrame& input,
    const Engine::Ui::UiViewport viewport,
    ObjectFpsDisplaySettings& displaySettings,
    std::vector<GameSessionCommand>& commands,
    std::string& error) {
    error.clear();
    if (!impl_->initialized) {
        error = "ObjectFpsUi is not initialized";
        return false;
    }
    if (!impl_->EnsureCanvas(snapshot.screen, error)) {
        return false;
    }
    if (snapshot.screen == GameScreen::Playing ||
        snapshot.transitionPhase != StageTransitionPhase::Idle) {
        return true;
    }

    const Engine::Ui::UiBindingTable bindings =
        MakeBindings(snapshot, displaySettings);
    auto updated = impl_->runtime.Update(input, bindings, viewport);
    if (!updated) {
        error = FormatUiError(updated.error());
        return false;
    }

    // TODO: デザイン、問題がある
    for (const Engine::Ui::UiActionEvent& event : updated.value()) {
        if (event.action == "object_fps_v2.start_game") {
            commands.emplace_back(StartCampaignCommand{});
        } else if (event.action == "object_fps_v2.open_controls") {
            commands.emplace_back(OpenControlsCommand{});
        } else if (event.action == "object_fps_v2.close_controls") {
            commands.emplace_back(CloseControlsCommand{});
        } else if (event.action == "object_fps_v2.resume") {
            commands.emplace_back(ResumeCommand{});
        } else if (event.action == "object_fps_v2.return_main_menu") {
            commands.emplace_back(ReturnToMainMenuCommand{});
        } else if (event.action == "object_fps_v2.quit") {
            commands.emplace_back(RequestQuitCommand{});
        } else if (event.action == "object_fps_v2.set_gamma") {
            const double* value = std::get_if<double>(&event.payload);
            if (value == nullptr) {
                error = "set_gamma UI action omitted its declared number payload";
                return false;
            }
            displaySettings.gammaAdjustment = Quantize(
                *value,
                ObjectFpsDisplaySettings::kMinimumGammaAdjustment,
                ObjectFpsDisplaySettings::kMaximumGammaAdjustment,
                ObjectFpsDisplaySettings::kGammaStep);
        } else if (event.action == "object_fps_v2.set_exposure") {
            const double* value = std::get_if<double>(&event.payload);
            if (value == nullptr) {
                error = "set_exposure UI action omitted its declared number payload";
                return false;
            }
            displaySettings.exposureEv = Quantize(
                *value,
                ObjectFpsDisplaySettings::kMinimumExposureEv,
                ObjectFpsDisplaySettings::kMaximumExposureEv,
                ObjectFpsDisplaySettings::kExposureStepEv);
        } else {
            error = "Object_FPS UI emitted unknown action '" + event.action + "'";
            return false;
        }
    }
    return true;
}

bool ObjectFpsUi::Compose(
    const GameSessionSnapshot& snapshot,
    const ObjectFpsDisplaySettings& displaySettings,
    const Engine::Ui::UiViewport viewport,
    Engine::Ui::UiDrawList& drawList,
    std::string& error) {
    error.clear();
    drawList.commands.clear();
    if (!impl_->initialized) {
        error = "ObjectFpsUi is not initialized";
        return false;
    }
    if (!impl_->EnsureCanvas(snapshot.screen, error)) {
        return false;
    }

    const std::optional<HudLayout> hudLayout = MakeHudLayout(viewport);
    if (!hudLayout.has_value()) {
        error = "ObjectFpsUi requires a finite positive viewport";
        return false;
    }
    if (snapshot.screen == GameScreen::Playing ||
        snapshot.screen == GameScreen::Paused) {
        AddHud(drawList, snapshot, *hudLayout);
    }
    if (snapshot.screen == GameScreen::Playing) {
        return true;
    }

    const Engine::Ui::UiBindingTable bindings =
        MakeBindings(snapshot, displaySettings);
    auto composed = impl_->runtime.Compose(bindings, viewport);
    if (!composed) {
        error = FormatUiError(composed.error());
        return false;
    }
    Engine::Ui::UiDrawList authored = std::move(composed.value());
    drawList.commands.insert(
        drawList.commands.end(),
        std::make_move_iterator(authored.commands.begin()),
        std::make_move_iterator(authored.commands.end()));
    return true;
}

bool ObjectFpsUi::IsInitialized() const noexcept {
    return impl_->initialized;
}

const Engine::Ui::UiInteractionState& ObjectFpsUi::InteractionState()
    const noexcept {
    return impl_->runtime.InteractionState();
}

} // namespace fps
