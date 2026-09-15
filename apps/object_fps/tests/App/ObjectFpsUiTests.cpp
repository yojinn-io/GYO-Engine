#include "../TestSupport.hpp"

#include "RetroFPS/App/ObjectFpsUi.hpp"

#include "ui/UiDocumentCodec.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace fps::tests {
namespace {

constexpr Engine::Ui::UiViewport kViewport{1280.0F, 720.0F};

[[nodiscard]] std::shared_ptr<Engine::Ui::UiDocument> LoadDocument(
    TestContext& context) {
    const std::filesystem::path path =
        std::filesystem::path{RETROFPS_TEST_RESOURCE_ROOT} / "ui" / "screens.json";
    std::ifstream stream(path, std::ios::binary);
    const std::string json{
        std::istreambuf_iterator<char>{stream},
        std::istreambuf_iterator<char>{}};
    context.Expect(stream.good() || stream.eof(), "real Object_FPS screens.json is readable");
    auto parsed = Engine::Ui::UiDocumentCodec::Parse(json, path.string());
    context.Expect(
        static_cast<bool>(parsed),
        parsed ? "real Object_FPS screens.json parses"
               : "real Object_FPS screens.json failed: " + parsed.error().message);
    if (!parsed) {
        return {};
    }
    return std::make_shared<Engine::Ui::UiDocument>(std::move(parsed.value()));
}

[[nodiscard]] bool HasText(
    const Engine::Ui::UiDrawList& drawList,
    const std::string_view expected) {
    return std::any_of(
        drawList.commands.begin(),
        drawList.commands.end(),
        [expected](const Engine::Ui::UiDrawCommand& command) {
            const auto* text = std::get_if<Engine::Ui::UiTextDraw>(&command);
            return text != nullptr && text->utf8.find(expected) != std::string::npos;
        });
}

[[nodiscard]] bool InitializeUi(
    ObjectFpsUi& ui,
    std::shared_ptr<const Engine::Ui::UiDocument> document,
    TestContext& context) {
    std::string error;
    const bool initialized = ui.Initialize(std::move(document), error);
    context.Expect(
        initialized,
        initialized ? "ObjectFpsUi initializes" : "ObjectFpsUi: " + error);
    return initialized;
}

void ExpectActionContractRejection(
    TestContext& context,
    std::shared_ptr<const Engine::Ui::UiDocument> document,
    const std::string_view expectedError,
    const std::string_view description) {
    ObjectFpsUi ui;
    std::string error;
    const bool initialized = ui.Initialize(std::move(document), error);
    context.Expect(
        !initialized && !ui.IsInitialized(),
        description);
    context.Expect(
        error.find(expectedError) != std::string::npos,
        std::string(description) + " reports the action contract violation");
}

void TestRealJsonScreensAndHud(TestContext& context) {
    auto document = LoadDocument(context);
    if (!document) {
        return;
    }
    ObjectFpsUi ui;
    if (!InitializeUi(ui, document, context)) {
        return;
    }

    ObjectFpsDisplaySettings settings;
    Engine::Ui::UiDrawList drawList;
    std::string error;
    GameSessionSnapshot snapshot;

    snapshot.screen = GameScreen::MainMenu;
    context.Expect(
        ui.Compose(snapshot, settings, kViewport, drawList, error) &&
            HasText(drawList, "OBJECT FPS") &&
            HasText(drawList, "ゲームスタート") &&
            HasText(drawList, "説明") && HasText(drawList, "終わり"),
        "MainMenu content is supplied by the real JSON, including Japanese labels");

    snapshot.screen = GameScreen::Controls;
    context.Expect(
        ui.Compose(snapshot, settings, kViewport, drawList, error) &&
            HasText(drawList, "CONTROLS") && HasText(drawList, "MOVE FORWARD") &&
            HasText(drawList, "BACK"),
        "Controls is supplied by the real JSON");

    snapshot.screen = GameScreen::Playing;
    snapshot.player = PlayerSnapshot{{}, 1.6F, 0.0F, 0.0F, 72.0F, 100.0F};
    snapshot.weapon.magazineAmmo = 7;
    snapshot.weapon.reserveAmmo = 35;
    snapshot.weapon.reloading = true;
    snapshot.weapon.reloadProgress = 0.5F;
    snapshot.activeStage = ActiveStageSnapshot{"stage_01", "REACTOR", 0, 4, false};
    context.Expect(
        ui.Compose(snapshot, settings, kViewport, drawList, error) &&
            HasText(drawList, "HP 72") && HasText(drawList, "7 / 35") &&
            HasText(drawList, "RELOADING") &&
            HasText(drawList, "STAGE 1 / 4  REACTOR"),
        "the C++ HUD policy leaves Object_FPS as the content owner but emits GYO UiDrawList");

    snapshot.screen = GameScreen::Paused;
    context.Expect(
        ui.Compose(snapshot, settings, kViewport, drawList, error) &&
            HasText(drawList, "HP 72") && HasText(drawList, "PAUSED") &&
            HasText(drawList, "GAMMA") && HasText(drawList, "EXPOSURE EV"),
        "Pause composes the C++ HUD before the JSON overlay and display controls");
}

void TestResultsBindings(TestContext& context) {
    auto document = LoadDocument(context);
    if (!document) {
        return;
    }
    ObjectFpsUi ui;
    if (!InitializeUi(ui, document, context)) {
        return;
    }
    ObjectFpsDisplaySettings settings;
    Engine::Ui::UiDrawList drawList;
    std::string error;
    GameSessionSnapshot snapshot;
    snapshot.screen = GameScreen::Results;
    snapshot.campaignRooms = {
        {"stage_01", "ROOM 01", 3, true},
        {"stage_02", "ROOM 02", 2, true},
        {"stage_03", "ROOM 03", 0, false},
    };

    for (const auto [outcome, title] : {
             std::pair{CampaignOutcome::Completed, std::string_view{"RUN COMPLETE"}},
             std::pair{CampaignOutcome::PlayerDied, std::string_view{"YOU DIED"}},
             std::pair{CampaignOutcome::InProgress, std::string_view{"RUN ENDED"}}}) {
        snapshot.campaignOutcome = outcome;
        context.Expect(
            ui.Compose(snapshot, settings, kViewport, drawList, error) &&
                HasText(drawList, title) && HasText(drawList, "ROOMS 2 / 3") &&
                HasText(drawList, "KILLS 5") && HasText(drawList, "ROOM 01") &&
                HasText(drawList, "3 KILLS"),
            "Results resolves typed outcome, aggregate and fixed-step-list bindings");
    }
}

void TestActionsAreIdsNotIndices(TestContext& context) {
    auto document = LoadDocument(context);
    if (!document) {
        return;
    }
    const auto canvas = std::find_if(
        document->canvases.begin(),
        document->canvases.end(),
        [](const Engine::Ui::UiCanvas& item) { return item.id == "main_menu"; });
    if (canvas == document->canvases.end()) {
        context.Expect(false, "main_menu canvas exists");
        return;
    }
    std::reverse(canvas->children.begin(), canvas->children.end());

    ObjectFpsUi ui;
    if (!InitializeUi(ui, document, context)) {
        return;
    }
    GameSessionSnapshot snapshot;
    ObjectFpsDisplaySettings settings;
    std::vector<GameSessionCommand> commands;
    std::string error;
    Engine::Ui::UiInputFrame input;
    input.activatePressed = true;
    context.Expect(
        ui.Update(snapshot, input, kViewport, settings, commands, error) &&
            commands.size() == 1 &&
            std::holds_alternative<StartCampaignCommand>(commands.front()),
        "reordering JSON elements still invokes StartCampaign through its action id");
}

void TestControlsButtonEmitsOpenControls(TestContext& context) {
    auto document = LoadDocument(context);
    if (!document) {
        return;
    }
    ObjectFpsUi ui;
    if (!InitializeUi(ui, document, context)) {
        return;
    }

    GameSessionSnapshot snapshot;
    snapshot.screen = GameScreen::MainMenu;
    ObjectFpsDisplaySettings settings;
    std::vector<GameSessionCommand> commands;
    std::string error;
    Engine::Ui::UiInputFrame input;
    input.pointerAvailable = true;
    input.pointerPixels = {640.0F, 432.0F};
    input.pointerPrimaryPressed = true;

    context.Expect(
        ui.Update(snapshot, input, kViewport, settings, commands, error) &&
            commands.size() == 1 &&
            std::holds_alternative<OpenControlsCommand>(commands.front()),
        "clicking the real JSON explanation button emits OpenControls");
}

void TestPauseSliderUpdatesSameFrame(TestContext& context) {
    auto document = LoadDocument(context);
    if (!document) {
        return;
    }
    const auto pause = std::find_if(
        document->canvases.begin(),
        document->canvases.end(),
        [](const Engine::Ui::UiCanvas& item) { return item.id == "pause"; });
    if (pause == document->canvases.end()) {
        context.Expect(false, "pause canvas exists");
        return;
    }
    pause->defaultFocus = "pause_gamma";

    ObjectFpsUi ui;
    if (!InitializeUi(ui, document, context)) {
        return;
    }
    GameSessionSnapshot snapshot;
    snapshot.screen = GameScreen::Paused;
    ObjectFpsDisplaySettings settings;
    std::vector<GameSessionCommand> commands;
    std::string error;
    Engine::Ui::UiInputFrame input;
    input.adjustNextPressed = true;
    context.Expect(
        ui.Update(snapshot, input, kViewport, settings, commands, error) &&
            std::abs(settings.gammaAdjustment - 1.05F) < 0.001F &&
            commands.empty(),
        "Pause gamma adjustment is quantized and visible to rendering in the same frame");
}

void TestActionContractRejectsEditedDocuments(TestContext& context) {
    const auto source = LoadDocument(context);
    if (!source) {
        return;
    }

    auto unsupported = std::make_shared<Engine::Ui::UiDocument>(*source);
    const auto openControls = std::find_if(
        unsupported->actions.begin(),
        unsupported->actions.end(),
        [](const Engine::Ui::UiActionDeclaration& action) {
            return action.id == "object_fps.open_controls";
        });
    if (openControls == unsupported->actions.end()) {
        context.Expect(false, "test fixture declares object_fps.open_controls");
        return;
    }
    openControls->id = "display.exposure_ev";
    ExpectActionContractRejection(
        context,
        unsupported,
        "unsupported action 'display.exposure_ev'",
        "an editor-corrupted unknown action is rejected during UI initialization");

    auto missing = std::make_shared<Engine::Ui::UiDocument>(*source);
    std::erase_if(
        missing->actions,
        [](const Engine::Ui::UiActionDeclaration& action) {
            return action.id == "object_fps.open_controls";
        });
    ExpectActionContractRejection(
        context,
        missing,
        "missing required action 'object_fps.open_controls'",
        "a missing Object_FPS action is rejected during UI initialization");

    auto wrongPayload = std::make_shared<Engine::Ui::UiDocument>(*source);
    const auto setGamma = std::find_if(
        wrongPayload->actions.begin(),
        wrongPayload->actions.end(),
        [](const Engine::Ui::UiActionDeclaration& action) {
            return action.id == "object_fps.set_gamma";
        });
    if (setGamma == wrongPayload->actions.end()) {
        context.Expect(false, "test fixture declares object_fps.set_gamma");
        return;
    }
    setGamma->payload = Engine::Ui::UiActionPayloadType::None;
    ExpectActionContractRejection(
        context,
        wrongPayload,
        "must declare payload 'number' (found 'none')",
        "a wrong Object_FPS action payload is rejected during UI initialization");
}

} // namespace

void RunObjectFpsUiTests(TestContext& context) {
    TestRealJsonScreensAndHud(context);
    TestResultsBindings(context);
    TestActionsAreIdsNotIndices(context);
    TestControlsButtonEmitsOpenControls(context);
    TestPauseSliderUpdatesSameFrame(context);
    TestActionContractRejectsEditedDocuments(context);
}

} // namespace fps::tests
