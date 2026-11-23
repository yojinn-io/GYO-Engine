#include "object_connect/world/ObjectConnectWorld.hpp"

#include "object_connect/world/GameScreenState.hpp"
#include "object_connect/world/Messages.hpp"
#include "object_connect/world/Resources.hpp"

#include "engine/base/Result.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/state_machine/RegisterStateMachine.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// One frame of the original Game::Impl::Update (Object_Connect ca4482c,
// src/ObjectConnect/Game/Game.cpp), split into Systems. The input sampling, the
// UI hit test, the page arrows, the cursor and the audio calls stay outside:
// FrameInput comes in, Messages go out.
//
// The one change of timing: the original changed the screen at once, the state
// machine applies a request at the next frame's apply. Each System below reads
// the screen the frame ends on where the original read the screen after the
// change (EffectiveScreen), so a frame decides the same things; the new screen
// is the current one from the next frame on.

namespace object_connect::world {
namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace SM = GYO::Engine::StateMachine;
namespace WT = GYO::Engine::WorldTime;

[[nodiscard]] K::SystemResult Failed(std::string message) {
    return GYO::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, std::move(message)));
}

[[nodiscard]] float Seconds(const WT::Nanoseconds delta) noexcept {
    const float seconds = static_cast<float>(static_cast<double>(delta) / 1e9);
    return std::isfinite(seconds) && seconds > 0.0f ? seconds : 0.0f;
}

[[nodiscard]] std::optional<std::size_t> NextPuzzleIndex(const Catalog& catalog, const Session& session) noexcept {
    const std::vector<PuzzleDefinition>& puzzles = catalog.puzzles.GetPuzzles();
    if (!session.puzzleIndex || *session.puzzleIndex >= puzzles.size()) {
        return std::nullopt;
    }
    const PuzzleDefinition& current = puzzles[*session.puzzleIndex];
    if (!current.nextLevelId || current.nextLevelId->empty() || IsFinalResultsTarget(current)) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < puzzles.size(); ++index) {
        if (puzzles[index].id == *current.nextLevelId) {
            return index;
        }
    }
    return std::nullopt;
}

void Show(ScreenRequest& request, MenuSelection& selection, const GameScreen screen) noexcept {
    request.Request(screen);
    selection.item = 0;
}

void ResetRun(RunResults& results) noexcept {
    results.tracker.Reset();
    results.summary = {};
}

// Drops the drag of the level being played; true when there was one.
[[nodiscard]] bool CancelDrag(Session& session) noexcept {
    if (!session.board || !session.board->IsDragging()) {
        return false;
    }
    session.board->CancelDrag(true);
    return true;
}

// Builds the level's board and moves to Playing; the board is replaced only when it was built.
[[nodiscard]] K::SystemResult StartPuzzle(const Catalog& catalog, const Playfield& playfield,
                                          const std::size_t index, Session& session,
                                          ScreenRequest& request, MenuSelection& selection,
                                          K::MessageWriter<LineHoldEnded>& holdEnded) {
    if (index >= catalog.puzzles.GetPuzzles().size()) {
        return Failed("the selected puzzle index is out of range");
    }
    PuzzleBoard board;
    std::string error;
    if (!board.Initialize(catalog.puzzles.GetPuzzles()[index], playfield.bounds, error)) {
        return Failed("the puzzle '" + catalog.puzzles.GetPuzzles()[index].id + "' did not start: " + error);
    }
    if (CancelDrag(session)) {
        holdEnded.Send({LineHoldEnd::Cancelled});
    }
    session.board = std::move(board);
    session.puzzleIndex = index;
    session.solvedElapsedSeconds = 0.0f;
    session.resultRecorded = false;
    session.music.RecordPuzzleEntered();
    Show(request, selection, GameScreen::Playing);
    return {};
}

void LeavePuzzle(Session& session, RunResults& results, K::MessageWriter<LineHoldEnded>& holdEnded) noexcept {
    if (CancelDrag(session)) {
        holdEnded.Send({LineHoldEnd::Cancelled});
    }
    session.board.reset();
    session.puzzleIndex.reset();
    session.solvedElapsedSeconds = 0.0f;
    session.resultRecorded = false;
    ResetRun(results);
}

// Esc or a lost focus while playing drops the drag before the menu pauses.
K::SystemResult CancelDragOnLeave(K::Res<FrameInput> input, K::Res<ScreenState> screen, K::ResMut<Session> session,
                                  K::MessageWriter<LineHoldEnded> holdEnded) {
    if (screen->Current() != GameScreen::Playing || !(input->menu.escapePressed || input->menu.focusLost)) {
        return {};
    }
    if (CancelDrag(session.Get())) {
        holdEnded.Send({LineHoldEnd::Cancelled});
    }
    return {};
}

// The menu on this frame's input: GameFlow resumed at the current screen and item.
K::SystemResult StepMenu(K::Res<FrameInput> input, K::Res<ScreenState> screen, K::ResMut<ScreenRequest> request,
                         K::ResMut<MenuSelection> selection, K::Res<Catalog> catalog, K::Res<Session> session,
                         K::MessageWriter<MenuStepped> stepped) {
    const GameScreen current = screen->Current().value_or(GameScreen::MainMenu);
    GameFlowInput menu = input->menu;
    if (current == GameScreen::Solved && session->solvedElapsedSeconds < kSolvedMenuDelaySeconds) {
        menu = {};
    }
    GameFlow flow(current, selection.Read().item);
    const GameFlowResult result =
        flow.Update(menu, catalog->puzzles.GetPuzzles().size(), NextPuzzleIndex(*catalog, *session).has_value());
    if (flow.GetScreen() != current) {
        request->Request(flow.GetScreen());
    }
    if (flow.GetSelectedItem() != selection.Read().item) {
        selection->item = flow.GetSelectedItem();
    }
    stepped.Send({result.command, result.puzzleIndex, result.simulatePuzzle});
    return {};
}

// Start, retry, next level, leave, quit (the original Game::Impl::ApplyCommand).
K::SystemResult ApplyMenuCommand(K::MessageReader<MenuStepped> stepped, K::Res<Catalog> catalog,
                                 K::Res<Playfield> playfield,
                                 K::ResMut<Session> session, K::ResMut<RunResults> results,
                                 K::ResMut<ScreenRequest> request, K::ResMut<MenuSelection> selection,
                                 K::MessageWriter<LineHoldEnded> holdEnded, K::MessageWriter<MusicRestarted> music,
                                 K::MessageWriter<QuitChosen> quit) {
    for (const MenuStepped* step : stepped.Read()) {
        switch (step->command) {
        case GameCommand::None:
            break;
        case GameCommand::OpenLevelSelect:
            LeavePuzzle(session.Get(), results.Get(), holdEnded);
            Show(request.Get(), selection.Get(), GameScreen::LevelSelect);
            break;
        case GameCommand::StartPuzzle: {
            if (!step->puzzleIndex) {
                return Failed("the menu started a puzzle without its index");
            }
            if (auto started = StartPuzzle(*catalog, *playfield, *step->puzzleIndex, session.Get(), request.Get(),
                                           selection.Get(), holdEnded);
                !started) {
                return started;
            }
            ResetRun(results.Get());
            break;
        }
        case GameCommand::RetryPuzzle: {
            if (!session.Read().puzzleIndex) {
                return Failed("retry was chosen without a puzzle being played");
            }
            const std::size_t index = *session.Read().puzzleIndex;
            const bool discardRecorded = session.Read().resultRecorded;
            if (auto started = StartPuzzle(*catalog, *playfield, index, session.Get(), request.Get(), selection.Get(), holdEnded);
                !started) {
                return started;
            }
            // A stale completion is worse than losing the earlier summary.
            if (discardRecorded &&
                !results->tracker.RemoveLastCompletedStage(catalog->puzzles.GetPuzzles()[index].id)) {
                results->tracker.Reset();
            }
            break;
        }
        case GameCommand::NextPuzzle: {
            if (!session.Read().puzzleIndex) {
                return Failed("the next puzzle was chosen without a puzzle being played");
            }
            if (const std::optional<std::size_t> next = NextPuzzleIndex(*catalog, session.Read())) {
                if (auto started =
                        StartPuzzle(*catalog, *playfield, *next, session.Get(), request.Get(), selection.Get(), holdEnded);
                    !started) {
                    return started;
                }
            } else {
                LeavePuzzle(session.Get(), results.Get(), holdEnded);
                Show(request.Get(), selection.Get(), GameScreen::LevelSelect);
            }
            break;
        }
        case GameCommand::ReturnToMainMenu:
            LeavePuzzle(session.Get(), results.Get(), holdEnded);
            Show(request.Get(), selection.Get(), GameScreen::MainMenu);
            if (session->music.OnScreenEntered(GameScreen::MainMenu)) {
                music.Send({});
            }
            break;
        case GameCommand::QuitGame:
            if (CancelDrag(session.Get())) {
                holdEnded.Send({LineHoldEnd::Cancelled});
            }
            quit.Send({});
            break;
        }
    }
    return {};
}

// The level on this frame's pointer and time; a solve records the result and
// moves to Solved, or to FinalResults when the level's next is final_results.
K::SystemResult SimulateBoard(K::MessageReader<MenuStepped> stepped, K::Res<FrameInput> input,
                              K::Res<WT::ActiveTime> time, K::Res<ScreenState> screen,
                              K::ResMut<ScreenRequest> request, K::ResMut<MenuSelection> selection,
                              K::ResMut<Session> session, K::ResMut<RunResults> results,
                              K::MessageWriter<LineHoldStarted> holdStarted,
                              K::MessageWriter<LineHoldEnded> holdEnded) {
    bool simulate = false;
    for (const MenuStepped* step : stepped.Read()) {
        simulate = step->simulatePuzzle;
    }
    if (!session.Read().board) {
        return {};
    }
    const float delta = Seconds(time->delta);
    const GameScreen ending = EffectiveScreen(*screen, request.Read());
    if (simulate && ending == GameScreen::Playing) {
        PuzzleBoard& board = *session->board;
        const bool wasDragging = board.IsDragging();
        board.Update(input->pointer, delta);
        if (!wasDragging && board.IsDragging()) {
            holdStarted.Send({});
        } else if (wasDragging && !board.IsDragging()) {
            holdEnded.Send({input->pointer.leftReleased ? LineHoldEnd::Released : LineHoldEnd::Cancelled});
        }
        if (board.IsSolved() && !session.Read().resultRecorded) {
            results->tracker.RecordCompletedStage(board.GetDefinition(), board.MakeSnapshot());
            session->resultRecorded = true;
            session->solvedElapsedSeconds = 0.0f;
            if (IsFinalResultsTarget(board.GetDefinition())) {
                results->summary = results.Read().tracker.BuildSummary();
                Show(request.Get(), selection.Get(), GameScreen::FinalResults);
            } else {
                Show(request.Get(), selection.Get(), GameScreen::Solved);
            }
        }
    } else if (ending == GameScreen::Solved) {
        session->board->Update({}, delta);
    }
    return {};
}

// Time on the Solved screen (reset when the frame neither ends nor started on it).
K::SystemResult TickSolvedTimer(K::Res<WT::ActiveTime> time, K::Res<ScreenState> screen,
                                K::Res<ScreenRequest> request, K::ResMut<Session> session) {
    if (EffectiveScreen(*screen, *request) == GameScreen::Solved) {
        session->solvedElapsedSeconds += Seconds(time->delta);
    } else if (screen->Current() != GameScreen::Solved && session.Read().solvedElapsedSeconds != 0.0f) {
        session->solvedElapsedSeconds = 0.0f;
    }
    return {};
}

K::SystemOptions InUpdate(std::string name, std::vector<std::string> after) {
    K::SystemOptions options;
    options.name = std::move(name);
    K::Mount mount;
    mount.schedule = std::string(O::DefaultSchedules::frame);
    mount.stage = std::string(O::DefaultSchedules::update);
    mount.after = std::move(after);
    options.mounts.push_back(std::move(mount));
    return options;
}

} // namespace

void BuildWorld(O::WorldSetup& setup) {
    SM::RegisterStateMachine<GameScreen>(setup);
    K::Registrar& r = setup.Registrar();
    r.Resource<Catalog>({.name = "object_connect.Catalog"});
    r.Resource<Playfield>({.name = "object_connect.Playfield"});
    r.Resource<FrameInput>({.name = "object_connect.FrameInput"});
    r.Resource<MenuSelection>({.name = "object_connect.MenuSelection"});
    r.Resource<Session>({.name = "object_connect.Session"});
    r.Resource<RunResults>({.name = "object_connect.RunResults"});
    r.Message<MenuStepped>({.name = "object_connect.MenuStepped"});
    r.Message<LineHoldStarted>({.name = "object_connect.LineHoldStarted"});
    r.Message<LineHoldEnded>({.name = "object_connect.LineHoldEnded"});
    r.Message<MusicRestarted>({.name = "object_connect.MusicRestarted"});
    r.Message<QuitChosen>({.name = "object_connect.QuitChosen"});
    r.AddSystem<&CancelDragOnLeave>(InUpdate("object_connect.CancelDragOnLeave", {}));
    r.AddSystem<&StepMenu>(InUpdate("object_connect.StepMenu", {"object_connect.CancelDragOnLeave"}));
    r.AddSystem<&ApplyMenuCommand>(InUpdate("object_connect.ApplyMenuCommand", {"object_connect.StepMenu"}));
    r.AddSystem<&SimulateBoard>(InUpdate("object_connect.SimulateBoard", {"object_connect.ApplyMenuCommand"}));
    r.AddSystem<&TickSolvedTimer>(InUpdate("object_connect.TickSolvedTimer", {"object_connect.SimulateBoard"}));
    setup.Prepare([](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Catalog{});
        world.SetResource(Playfield{});
        world.SetResource(FrameInput{});
        world.SetResource(MenuSelection{});
        world.SetResource(Session{});
        world.SetResource(RunResults{});
        return {};
    });
}

} // namespace object_connect::world
