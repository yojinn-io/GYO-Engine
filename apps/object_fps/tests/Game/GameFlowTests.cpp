#include "../TestSupport.hpp"

#include "RetroFPS/Game/GameFlow.hpp"

namespace fps::tests {
namespace {

void TestSemanticScreenTransitions(TestContext& context) {
    GameFlow flow;
    context.Expect(
        flow.GetScreen() == GameScreen::MainMenu,
        "flow starts at the main menu without owning UI selection state");

    flow.OpenControls();
    context.Expect(
        flow.GetScreen() == GameScreen::Controls,
        "the OpenControls semantic transition opens Controls");

    GameFlowInput back{};
    back.escapePressed = true;
    const GameFlowResult closed = flow.Update(back);
    context.Expect(
        closed.screenChanged && flow.GetScreen() == GameScreen::MainMenu,
        "Escape keeps the app-level Controls back policy");

    flow.OpenControls();
    flow.CloseControls();
    context.Expect(
        flow.GetScreen() == GameScreen::MainMenu,
        "the CloseControls semantic transition returns to MainMenu");

    flow.EnterResults();
    const GameFlowResult resultsIdle = flow.Update(back);
    context.Expect(
        !resultsIdle.screenChanged && !resultsIdle.simulateGameplay &&
            flow.GetScreen() == GameScreen::Results,
        "Results ignores Escape and never simulates gameplay");

    flow.ReturnToMainMenu();
    context.Expect(
        flow.GetScreen() == GameScreen::MainMenu,
        "results can be committed back to the main menu by semantic action");
}

void TestPauseFocusAndReleaseBoundary(TestContext& context) {
    GameFlow flow;
    flow.EnterPlaying();

    GameFlowInput pause{};
    pause.escapePressed = true;
    const GameFlowResult paused = flow.Update(pause);
    context.Expect(
        paused.screenChanged && !paused.simulateGameplay &&
            flow.GetScreen() == GameScreen::Paused,
        "Escape pauses without simulating the transition frame");

    const GameFlowResult frozen = flow.Update({});
    context.Expect(!frozen.simulateGameplay, "paused frames remain frozen");

    const GameFlowResult resumed = flow.Update(pause);
    context.Expect(
        resumed.screenChanged && !resumed.simulateGameplay &&
            flow.GetScreen() == GameScreen::Playing,
        "Escape resumes without simulating the transition frame");
    context.Expect(
        flow.Update({}).simulateGameplay,
        "a stable Playing frame simulates gameplay");

    GameFlowInput focusLost{};
    focusLost.focusLost = true;
    const GameFlowResult focusPaused = flow.Update(focusLost);
    context.Expect(
        focusPaused.screenChanged && flow.GetScreen() == GameScreen::Paused,
        "focus loss still pauses active gameplay");
}

} // namespace

void RunGameFlowTests(TestContext& context) {
    TestSemanticScreenTransitions(context);
    TestPauseFocusAndReleaseBoundary(context);
}

} // namespace fps::tests
