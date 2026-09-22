#include <doctest/doctest.h>

#include "RetroFPS/App/ObjectFpsUi.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

namespace {

using namespace fps;

std::shared_ptr<Engine::Ui::UiDocument> DebugTestDocument() {
    auto document = std::make_shared<Engine::Ui::UiDocument>();
    document->fonts.emplace("ui", "synthetic.ui.font");
    document->defaultFont = "ui";
    document->colors.emplace("backdrop", Engine::Ui::UiColor{0, 0, 0, 0.75F});
    for (const auto* action : {"start_game", "open_controls", "close_controls",
                              "resume", "return_main_menu", "quit"}) {
        document->actions.push_back({std::string("object_fps_v2.") + action,
            Engine::Ui::UiActionPayloadType::None});
    }
    for (const auto* action : {"set_gamma", "set_exposure"}) {
        document->actions.push_back({std::string("object_fps_v2.") + action,
            Engine::Ui::UiActionPayloadType::Number});
    }
    for (const auto* name : {"main_menu", "controls", "pause", "results"}) {
        Engine::Ui::UiCanvas canvas;
        canvas.id = name;
        canvas.backdropColor = "backdrop";
        document->canvases.push_back(std::move(canvas));
    }
    return document;
}

const Engine::Ui::UiTextDraw* FindText(
    const Engine::Ui::UiDrawList& list, std::string_view label) {
    for (const auto& command : list.commands) {
        const auto* text = std::get_if<Engine::Ui::UiTextDraw>(&command);
        if (text && text->utf8 == label) return text;
    }
    return nullptr;
}

TEST_CASE("Collision legend follows app display state through playing and pause") {
    ObjectFpsUi ui;
    std::string error;
    REQUIRE_MESSAGE(ui.Initialize(DebugTestDocument(), error), error);
    ObjectFpsDisplaySettings display;
    CHECK_FALSE(display.showCollisionVolumes);
    GameSessionSnapshot snapshot;
    snapshot.screen = GameScreen::Playing;
    Engine::Ui::UiDrawList list;
    constexpr Engine::Ui::UiViewport viewport{1280.0F, 720.0F};
    REQUIRE_MESSAGE(ui.Compose(snapshot, display, viewport, list, error), error);
    CHECK(FindText(list, "F3  COLLISION: OFF"));
    CHECK_FALSE(FindText(list, "BODY CAPSULE"));

    display.showCollisionVolumes = true;
    for (const auto screen : {GameScreen::Playing, GameScreen::Paused}) {
        snapshot.screen = screen;
        REQUIRE_MESSAGE(ui.Compose(snapshot, display, viewport, list, error), error);
        CHECK(FindText(list, "F3  COLLISION: ON"));
        const auto* body = FindText(list, "BODY CAPSULE");
        const auto* hurtbox = FindText(list, "BONE HURTBOX");
        const auto* attack = FindText(list, "ACTIVE ATTACK");
        const auto* world = FindText(list, "WORLD BOX");
        REQUIRE(body); REQUIRE(hurtbox); REQUIRE(attack); REQUIRE(world);
        CHECK(body->color.green > body->color.red);
        CHECK(hurtbox->color.red > hurtbox->color.blue);
        CHECK(hurtbox->color.green > hurtbox->color.blue);
        CHECK(attack->color.red > attack->color.green);
        CHECK(world->color.green > world->color.red);
        CHECK(world->color.blue > world->color.red);
        REQUIRE_FALSE(list.commands.empty());
        const auto* last = std::get_if<Engine::Ui::UiTextDraw>(&list.commands.back());
        REQUIRE(last);
        CHECK(last->utf8 == "WORLD BOX"); // Legend stays above the pause backdrop.
        CHECK(display.showCollisionVolumes);
    }

    snapshot.screen = GameScreen::MainMenu;
    REQUIRE_MESSAGE(ui.Compose(snapshot, display, viewport, list, error), error);
    CHECK_FALSE(FindText(list, "F3  COLLISION: ON"));
    CHECK(display.showCollisionVolumes); // Campaign/screen transitions do not reset it.
    snapshot.screen = GameScreen::Playing;
    REQUIRE_MESSAGE(ui.Compose(snapshot, display, viewport, list, error), error);
    CHECK(FindText(list, "F3  COLLISION: ON"));
    display.showCollisionVolumes = false;
    REQUIRE_MESSAGE(ui.Compose(snapshot, display, viewport, list, error), error);
    CHECK_FALSE(FindText(list, "BONE HURTBOX"));
    CHECK(FindText(list, "F3  COLLISION: OFF"));
}

} // namespace
