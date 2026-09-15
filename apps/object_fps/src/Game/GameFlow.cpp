#include "RetroFPS/Game/GameFlow.hpp"

namespace fps {

GameFlowResult GameFlow::Update(const GameFlowInput& input) noexcept {
    const GameScreen previousScreen = screen_;

    if (screen_ == GameScreen::Playing) {
        if (input.escapePressed || input.focusLost) {
            EnterPaused();
        }
        return {
            screen_ != previousScreen,
            screen_ == GameScreen::Playing && previousScreen == GameScreen::Playing,
        };
    }

    if (screen_ == GameScreen::Controls) {
        if (input.escapePressed) {
            CloseControls();
        }
        return {screen_ != previousScreen, false};
    }

    if (screen_ == GameScreen::Paused && input.escapePressed) {
        EnterPlaying();
        return {true, false};
    }

    return {screen_ != previousScreen, false};
}

void GameFlow::OpenControls() noexcept {
    screen_ = GameScreen::Controls;
}

void GameFlow::CloseControls() noexcept {
    screen_ = GameScreen::MainMenu;
}

void GameFlow::EnterPlaying() noexcept {
    screen_ = GameScreen::Playing;
}

void GameFlow::EnterPaused() noexcept {
    screen_ = GameScreen::Paused;
}

void GameFlow::EnterResults() noexcept {
    screen_ = GameScreen::Results;
}

void GameFlow::ReturnToMainMenu() noexcept {
    screen_ = GameScreen::MainMenu;
}

} // namespace fps
