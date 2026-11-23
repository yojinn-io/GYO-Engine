// Object_Connect in a GYO World, run headless frame by frame: the frame input
// goes in through the FrameInput Resource (what the input and UI Systems will
// write once the window exists), the screen is read from the state machine, and
// the Messages are recorded by a System in Main/Last.
#include <doctest/doctest.h>

#include "object_connect/world/GameScreenState.hpp"
#include "object_connect/world/Messages.hpp"
#include "object_connect/world/ObjectConnectWorld.hpp"
#include "object_connect/world/Resources.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/world_time/TimeTypes.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace WT = GYO::Engine::WorldTime;
namespace OC = object_connect;
namespace OW = object_connect::world;

using OC::GameScreen;

// The Messages of the frames since the last Take, in the order the recorder read them.
struct Recorded {
    std::vector<std::string> events;
};

K::SystemResult Record(K::MessageReader<OW::LineHoldStarted> started, K::MessageReader<OW::LineHoldEnded> ended,
                       K::MessageReader<OW::MusicRestarted> music, K::MessageReader<OW::QuitChosen> quit,
                       K::ResMut<Recorded> recorded) {
    for ([[maybe_unused]] const OW::LineHoldStarted* m : started.Read()) recorded->events.push_back("hold started");
    for (const OW::LineHoldEnded* m : ended.Read()) {
        recorded->events.push_back(m->reason == OW::LineHoldEnd::Released ? "hold released" : "hold cancelled");
    }
    for ([[maybe_unused]] const OW::MusicRestarted* m : music.Read()) recorded->events.push_back("music restarted");
    for ([[maybe_unused]] const OW::QuitChosen* m : quit.Read()) recorded->events.push_back("quit");
    return {};
}

OC::NodeDefinition Node(const char* id, const OC::NodeType type, const std::uint32_t tileX, const std::uint32_t tileY) {
    OC::NodeDefinition node;
    node.id = id;
    node.type = type;
    node.widthTiles = 2;
    node.heightTiles = 2;
    node.tilePosition = OC::TilePosition{tileX, tileY};
    node.maxIncoming = type == OC::NodeType::Root ? 0 : 1;
    node.maxOutgoing = type == OC::NodeType::End ? 0 : 1;
    node.maxOutgoingLength = 1000.0f;
    return node;
}

// Two levels, each one root and one end on a row; level_a leads to level_b, level_b to the final results.
OC::PuzzleCatalog TwoLevels() {
    std::vector<OC::PuzzleDefinition> puzzles;
    for (const char* id : {"level_a", "level_b"}) {
        OC::PuzzleDefinition puzzle;
        puzzle.id = id;
        puzzle.title = id;
        puzzle.mapPath = std::string("maps/") + id + ".csv";
        puzzle.totalLength = 5000.0f;
        puzzle.minimumSlackRatio = 1.0f;
        puzzle.nodes = {Node("root", OC::NodeType::Root, 2, 2), Node("end", OC::NodeType::End, 12, 2)};
        puzzles.push_back(std::move(puzzle));
    }
    puzzles[0].nextLevelId = "level_b";
    puzzles[1].nextLevelId = std::string(OC::kFinalResultsTargetId);
    return OC::PuzzleCatalog{std::move(puzzles)};
}

constexpr OC::Vec2 kRootCenter{48.0f, 48.0f};   // tile (2, 2), 2 x 2 tiles of 16 px
constexpr OC::Vec2 kEndCenter{208.0f, 48.0f};   // tile (12, 2)
constexpr WT::Nanoseconds kFrame = 100'000'000; // 0.1 s per frame

class Game {
public:
    Game() {
        O::RegisteredUnits units;
        O::RegisterRow row;
        row.name = "world_time";
        row.kind = O::UnitKind::Module;
        row.libraryOnly = false;
        row.defaultOn = true;
        units.rows.push_back(row);
        units.modules.push_back({"world_time", std::make_unique<WT::WorldTimeModule>()});

        O::WorldDescription world;
        world.name = "Main";
        world.build = [](O::WorldSetup& setup) {
            OW::BuildWorld(setup);
            K::Registrar& r = setup.Registrar();
            r.Resource<Recorded>({.name = "test.Recorded"});
            K::SystemOptions record;
            record.name = "test.Record";
            K::Mount mount;
            mount.schedule = std::string(O::DefaultSchedules::frame);
            mount.stage = std::string(O::DefaultSchedules::last);
            record.mounts.push_back(std::move(mount));
            r.AddSystem<&Record>(std::move(record));
            setup.Prepare([](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
                w.SetResource(Recorded{});
                return {};
            });
        };
        O::ProductDescription description;
        description.name = "object_connect";
        description.worlds.push_back(std::move(world));
        auto built = O::Product::Build(std::move(description), std::move(units));
        REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().message + "\n" + built.error().detail));
        product_ = std::move(built).value();

        WT::TimeSampling* sampling = World().ResourceMut<WT::TimeSampling>();
        REQUIRE(sampling != nullptr);
        sampling->mode = WT::SamplingMode::ManualDelta;
        sampling->value = kFrame;
        World().ResourceMut<OW::Catalog>()->puzzles = TwoLevels();
    }

    K::World& World() { return product_->World("Main").GetWorld(); }

    // Runs one frame on this input.
    void Frame(OW::FrameInput input = {}) {
        World().SetResource(std::move(input));
        auto ran = product_->RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
    }

    [[nodiscard]] GameScreen Screen() {
        const auto* state = World().Resource<OW::ScreenState>();
        REQUIRE(state != nullptr);
        REQUIRE(state->Current().has_value());
        return *state->Current();
    }
    [[nodiscard]] const OW::Session& Session() { return *World().Resource<OW::Session>(); }
    [[nodiscard]] const OW::RunResults& Results() { return *World().Resource<OW::RunResults>(); }
    [[nodiscard]] std::size_t Selected() { return World().Resource<OW::MenuSelection>()->item; }

    // The Messages recorded since the last call.
    [[nodiscard]] std::vector<std::string> Take() {
        return std::exchange(World().ResourceMut<Recorded>()->events, {});
    }

private:
    std::unique_ptr<O::Product> product_;
};

OW::FrameInput Confirm() {
    OW::FrameInput input;
    input.menu.confirmPressed = true;
    return input;
}
OW::FrameInput Next() {
    OW::FrameInput input;
    input.menu.nextPressed = true;
    return input;
}
OW::FrameInput Escape() {
    OW::FrameInput input;
    input.menu.escapePressed = true;
    return input;
}
OW::FrameInput FocusLost() {
    OW::FrameInput input;
    input.menu.focusLost = true;
    return input;
}
OW::FrameInput Press(const OC::Vec2 at) {
    OW::FrameInput input;
    input.pointer = {at, true, true, false, std::nullopt};
    return input;
}
OW::FrameInput Release(const OC::Vec2 at) {
    OW::FrameInput input;
    input.pointer = {at, false, false, true, std::nullopt};
    return input;
}

// From the first frame to playing the first level.
void PlayFirstLevel(Game& game) {
    game.Frame();          // the state machine enters MainMenu
    game.Frame(Confirm()); // "start" -> level select
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::LevelSelect);
    game.Frame(Confirm()); // the first level
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
}

// Empty frames until the Solved menu takes input; at most ten (0.1 s each, the delay is 0.6 s).
void WaitForSolvedMenu(Game& game) {
    for (int frame = 0; frame < 10 && game.Session().solvedElapsedSeconds < OW::kSolvedMenuDelaySeconds; ++frame) {
        game.Frame();
    }
    REQUIRE(game.Session().solvedElapsedSeconds >= OW::kSolvedMenuDelaySeconds);
    REQUIRE(game.Screen() == GameScreen::Solved);
}

void SolveLevel(Game& game) {
    game.Frame(Press(kRootCenter));
    game.Frame(Release(kEndCenter));
    game.Frame();
}

} // namespace

TEST_CASE("world: the first frame enters the main menu; confirming start opens level select the frame after") {
    Game game;
    game.Frame();
    CHECK(game.Screen() == GameScreen::MainMenu);
    game.Frame(Confirm());
    CHECK(game.Screen() == GameScreen::MainMenu);  // asked for; applied at the next frame
    game.Frame();
    CHECK(game.Screen() == GameScreen::LevelSelect);
    CHECK(game.Selected() == 0);
    CHECK_FALSE(game.Session().board.has_value());
}

TEST_CASE("world: choosing a level builds its board inside the playfield and plays it") {
    Game game;
    PlayFirstLevel(game);
    REQUIRE(game.Session().board.has_value());
    CHECK(game.Session().puzzleIndex == std::optional<std::size_t>{0});
    CHECK(game.Session().board->GetDefinition().id == "level_a");
    REQUIRE(game.Session().board->MakeSnapshot().playfieldBounds.has_value());
    CHECK(game.Session().board->MakeSnapshot().playfieldBounds->maximum.x == 1280.0f);
}

TEST_CASE("world: dragging from the root to the end solves the level, records it and shows Solved") {
    Game game;
    PlayFirstLevel(game);
    static_cast<void>(game.Take());
    game.Frame(Press(kRootCenter));
    CHECK(game.Session().board->IsDragging());
    CHECK(game.Take() == std::vector<std::string>{"hold started"});
    game.Frame(Release(kEndCenter));
    CHECK(game.Session().board->IsSolved());
    CHECK(game.Take() == std::vector<std::string>{"hold released"});
    CHECK(game.Session().resultRecorded);
    const OC::FinalResultsSummary summary = game.Results().tracker.BuildSummary();
    REQUIRE(summary.stages.size() == 1);
    CHECK(summary.stages[0].puzzleId == "level_a");
    CHECK(summary.stages[0].connectedOrganCount == 2);
    CHECK(summary.stages[0].totalOrganCount == 2);
    game.Frame();
    CHECK(game.Screen() == GameScreen::Solved);
}

TEST_CASE("world: Esc while dragging cancels the drag and pauses; Esc on the pause menu resumes") {
    Game game;
    PlayFirstLevel(game);
    game.Frame(Press(kRootCenter));
    static_cast<void>(game.Take());
    game.Frame(Escape());
    CHECK_FALSE(game.Session().board->IsDragging());
    CHECK(game.Take() == std::vector<std::string>{"hold cancelled"});
    game.Frame();
    CHECK(game.Screen() == GameScreen::Paused);
    game.Frame(Escape());
    game.Frame();
    CHECK(game.Screen() == GameScreen::Playing);
    CHECK(game.Session().board.has_value());  // pausing keeps the level
}

TEST_CASE("world: losing the focus while playing pauses; the pointer does not reach the board on the pause frame") {
    Game game;
    PlayFirstLevel(game);
    OW::FrameInput input = FocusLost();
    input.pointer = {kRootCenter, true, true, false, std::nullopt};
    game.Frame(input);
    CHECK_FALSE(game.Session().board->IsDragging());
    game.Frame();
    CHECK(game.Screen() == GameScreen::Paused);
    game.Frame(Press(kRootCenter));  // a paused level is not played
    CHECK_FALSE(game.Session().board->IsDragging());
}

TEST_CASE("world: the Solved menu takes no input before its delay, then 'next' plays the next level") {
    Game game;
    PlayFirstLevel(game);
    SolveLevel(game);
    REQUIRE(game.Screen() == GameScreen::Solved);
    // 0.1 s per frame: the solve frame and the frame after count 0.2 s.
    game.Frame(Confirm());
    game.Frame();
    CHECK(game.Screen() == GameScreen::Solved);
    CHECK(game.Session().puzzleIndex == std::optional<std::size_t>{0});
    WaitForSolvedMenu(game);
    game.Frame(Confirm());  // item 0 with a next level: "next"
    game.Frame();
    CHECK(game.Screen() == GameScreen::Playing);
    CHECK(game.Session().board->GetDefinition().id == "level_b");
    CHECK_FALSE(game.Session().resultRecorded);
    CHECK(game.Results().tracker.BuildSummary().stages.size() == 1);  // "next" keeps the run
}

TEST_CASE("world: solving a level whose next is final_results shows every level cleared in the run") {
    Game game;
    PlayFirstLevel(game);
    SolveLevel(game);
    WaitForSolvedMenu(game);
    game.Frame(Confirm());
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Playing);
    SolveLevel(game);
    CHECK(game.Screen() == GameScreen::FinalResults);
    REQUIRE(game.Results().summary.stages.size() == 2);
    CHECK(game.Results().summary.stages[0].puzzleId == "level_a");
    CHECK(game.Results().summary.stages[1].puzzleId == "level_b");
    game.Frame(Escape());  // final results -> level select, the run is cleared
    game.Frame();
    CHECK(game.Screen() == GameScreen::LevelSelect);
    CHECK_FALSE(game.Session().board.has_value());
    CHECK(game.Results().tracker.BuildSummary().stages.empty());
}

TEST_CASE("world: retry after a solve takes the level's result out and rebuilds the board") {
    Game game;
    PlayFirstLevel(game);
    SolveLevel(game);
    WaitForSolvedMenu(game);
    game.Frame(Next());
    game.Frame(Next());
    CHECK(game.Selected() == 2);  // next, level select, retry
    game.Frame(Confirm());
    CHECK(game.Results().tracker.BuildSummary().stages.empty());
    CHECK_FALSE(game.Session().board->IsSolved());
    game.Frame();
    CHECK(game.Screen() == GameScreen::Playing);
    SolveLevel(game);
    CHECK(game.Results().tracker.BuildSummary().stages.size() == 1);
}

TEST_CASE("world: back to the main menu after a level restarts the music once; quitting sends QuitChosen") {
    Game game;
    PlayFirstLevel(game);
    game.Frame(Escape());
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::Paused);
    for (int i = 0; i < 3; ++i) game.Frame(Next());
    CHECK(game.Selected() == 3);  // resume, retry, level select, main menu, quit
    static_cast<void>(game.Take());
    game.Frame(Confirm());
    CHECK(game.Take() == std::vector<std::string>{"music restarted"});
    game.Frame();
    CHECK(game.Screen() == GameScreen::MainMenu);
    CHECK_FALSE(game.Session().board.has_value());
    game.Frame(Confirm());  // level select, then straight back: no level entered, no restart
    game.Frame();
    game.Frame(Escape());
    game.Frame();
    REQUIRE(game.Screen() == GameScreen::MainMenu);
    CHECK(game.Take().empty());
    game.Frame(Next());
    game.Frame(Confirm());
    CHECK(game.Take() == std::vector<std::string>{"quit"});
}
