#pragma once

namespace fps {

enum class GameScreen {
    MainMenu,
    Controls,
    Playing,
    Paused,
    Results,
};

struct GameFlowInput final {
    bool escapePressed = false;
    bool focusLost = false;
};

struct GameFlowResult final {
    bool screenChanged = false;
    bool simulateGameplay = false;
};

// Engine-independent screen state. Menu focus, hit testing, and action choice
// belong to GYO::Ui; GameFlow owns only legal screen transitions and the
// gameplay-specific Escape/focus-loss policy.
class GameFlow final {
public:
    [[nodiscard]] GameFlowResult Update(const GameFlowInput& input) noexcept;
    void OpenControls() noexcept;
    void CloseControls() noexcept;
    void EnterPlaying() noexcept;
    void EnterPaused() noexcept;
    void EnterResults() noexcept;
    void ReturnToMainMenu() noexcept;

    [[nodiscard]] GameScreen GetScreen() const noexcept { return screen_; }

private:
    GameScreen screen_ = GameScreen::MainMenu;
};

} // namespace fps
