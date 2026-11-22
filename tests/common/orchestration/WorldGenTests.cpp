// World generation (world-gen declaration docs/architecture/target/v2/world-gen-declaration.zh-Hant.md, sections
// 5.1 to 5.5 and 11, judgement 1 cases C1 to C3, C5 to C12, C17, C18; O-14). A synthetic single-player game: the
// product's own build registers the save's types (SaveGame.hpp), a test allotter (no Module), and two sets on the
// default skeleton's Main/Update: "Gen" with the run condition WhileGenerating() and "Game" with AfterGenerating().
// What the Systems see goes to the test log; the World outside the Systems is read through the Product.
#include "OrchestrationTestSupport.hpp"
#include "SaveGame.hpp"
#include "TestRangeAllotter.hpp"

#include "engine/orchestration/WorldGeneration.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace OrchTest;
namespace S = OrchTest::Save;

namespace {

// The World-outside flag a generation waits for (an asset that has arrived, a server's snapshot).
struct Ready {
    std::int32_t on = 0;
};
// The restore's cursor over the save's entities.
struct LoadCursor {
    std::int32_t next = 0;
};
// A generation System that fails while this is set.
struct FailGen {
    std::int32_t on = 0;
};
inline void HashValue(K::Hasher& h, const Ready& v) { HashValue(h, v.on); }
inline void HashValue(K::Hasher& h, const LoadCursor& v) { HashValue(h, v.next); }
inline void HashValue(K::Hasher& h, const FailGen& v) { HashValue(h, v.on); }

// How many save entities one generating frame puts back (not a World value: the summary must not see it, C3).
int& BatchSize() {
    static int size = 3;
    return size;
}

using SavedBundle = K::Bundle<S::Player, S::Pos, S::ChaseTarget>;

// Gen: puts the save back, BatchSize() entities per frame; holds until every one is back (restore 5.1: the allotter's
// lower bound was moved by a prepare function before the first generating frame). An id of 0 is "no save".
K::SystemResult RestoreBatch(K::SpawnWithId<SavedBundle> spawn, K::Res<S::SaveData> save, K::ResMut<LoadCursor> cursor,
                             K::ResMut<S::Gold> gold, K::ResMut<O::WorldGeneration> generation) {
    const int total = static_cast<int>(std::count_if(save->ents.begin(), save->ents.end(),
                                                     [](const S::SaveData::Ent& e) { return e.id != 0; }));
    const int end = std::min(total, cursor->next + BatchSize());
    for (int i = cursor->next; i < end; ++i) {
        const S::SaveData::Ent& e = save->ents[static_cast<std::size_t>(i)];
        spawn.New(e.id, S::Player{e.hp}, S::Pos{e.x}, S::ChaseTarget{e.chase});
    }
    cursor->next = end;
    if (cursor->next < total) {
        generation->Hold();
    } else if (total > 0) {
        gold->v = save->gold;
    }
    Log().push_back("restore " + std::to_string(cursor->next));
    return {};
}

// Gen: holds until the World-outside flag is set.
K::SystemResult HoldUntilReady(K::Res<Ready> ready, K::ResMut<O::WorldGeneration> generation) {
    if (ready->on == 0) generation->Hold();
    Log().push_back("gen");
    return {};
}

// Gen: fails while FailGen is set (C7).
K::SystemResult FailWhileGenerating(K::Res<FailGen> fail) {
    if (fail->on != 0) return GYO::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, "generation failed"));
    return {};
}

// Game: the game's logic, once generation has ended.
K::SystemResult Play() {
    Log().push_back("play");
    return {};
}

// Game: calls Hold() every frame after generation has ended (C18: no effect).
K::SystemResult HoldAfterGeneration(K::ResMut<O::WorldGeneration> generation) {
    generation->Hold();
    return {};
}

// What a schedule outside the frame list sees (C17): game initialization, beforeSwap, a RunSchedule.
K::SystemResult ProbeGeneration(K::Res<O::WorldGeneration> generation) {
    Log().push_back(std::string("probe ") + (generation->Generating() ? "generating" : "done"));
    return {};
}

K::SystemOptions InSet(std::string name, std::string set) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::string(O::DefaultSchedules::frame);
    m.stage = std::string(O::DefaultSchedules::update);
    m.sets = {std::move(set)};
    o.mounts.push_back(std::move(m));
    return o;
}

// Which generation Systems a World has.
struct Pieces {
    bool restore = false;
    bool holdUntilReady = false;
    bool fail = false;
    bool holdAfter = false;
    bool spawnRock = false;
};

std::function<void(O::WorldSetup&)> Build(Pieces pieces) {
    return [pieces](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Component<S::Player>({.name = "Player"});
        r.Component<S::Pos>({.name = "Pos"});
        r.Component<S::ChaseTarget>({.name = "ChaseTarget"});
        r.Resource<S::Gold>({.name = "Gold"});
        r.Resource<S::SaveData>({.name = "SaveData"});
        r.Resource<Ready>({.name = "Ready"});
        r.Resource<LoadCursor>({.name = "LoadCursor"});
        r.Resource<FailGen>({.name = "FailGen"});
        r.Resource<GYO::Test::TestRangeAllotter>({.name = "Allotter"});
        r.Allotter<GYO::Test::TestRangeAllotter>();
        r.Schedule(Once("Boot"));
        r.Schedule(Once("Probe"));
        r.AddSystem<&ProbeGeneration>(In("ProbeBoot", "Boot"));
        r.AddSystem<&ProbeGeneration>(In("ProbeProbe", "Probe"));
        const std::string main(O::DefaultSchedules::frame);
        r.Set(K::SetOptions{.name = "Gen", .schedule = main}, {O::WhileGenerating()});
        r.Set(K::SetOptions{.name = "Game", .schedule = main}, {O::AfterGenerating()});
        if (pieces.restore) r.AddSystem<&RestoreBatch>(InSet("RestoreBatch", "Gen"));
        if (pieces.holdUntilReady) r.AddSystem<&HoldUntilReady>(InSet("HoldUntilReady", "Gen"));
        if (pieces.fail) r.AddSystem<&FailWhileGenerating>(InSet("FailWhileGenerating", "Gen"));
        r.AddSystem<&Play>(InSet("Play", "Game"));
        if (pieces.holdAfter) r.AddSystem<&HoldAfterGeneration>(InSet("HoldAfterGeneration", "Game"));
        if (pieces.spawnRock) r.AddSystem<&S::SpawnRock>(InSet("SpawnRock", "Game"));
    };
}

// The description's prepare: the defaults and, for a startup load, the save with the moved lower bound.
O::PrepareFn Defaults(bool startupSave) {
    return [startupSave](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        const O::WorldGeneration* generation = w.Resource<O::WorldGeneration>();
        Log().push_back(std::string("prepare ") + (generation != nullptr && generation->Generating() ? "generating" : "done"));
        w.SetResource(S::Gold{0});
        w.SetResource(Ready{});
        w.SetResource(LoadCursor{});
        w.SetResource(FailGen{});
        if (startupSave) {
            w.SetResource(S::TheSave());
            w.SetResource(GYO::Test::TestRangeAllotter::Of(S::TheSave().next, S::kLast));
        } else {
            w.SetResource(S::SaveData{});
            w.SetResource(GYO::Test::TestRangeAllotter::Of(1, S::kLast));
        }
        return {};
    };
}

O::WorldDescription GenWorld(std::string name, Pieces pieces, bool startupSave = false) {
    O::WorldDescription w = World(std::move(name), Build(pieces));
    w.prepare.push_back(Defaults(startupSave));
    return w;
}

std::unique_ptr<O::Product> Make(std::vector<O::WorldDescription> worlds) {
    O::ProductDescription d;
    d.name = "rpg";
    d.gameInit = {"Boot"};
    d.worlds = std::move(worlds);
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    return std::move(*built);
}

std::unique_ptr<O::Product> Make(O::WorldDescription world) {
    std::vector<O::WorldDescription> worlds;
    worlds.push_back(std::move(world));
    return Make(std::move(worlds));
}

void Frame(O::Product& p) {
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

K::World& WorldOf(O::Product& p, std::string_view name = "Main") { return p.World(name).GetWorld(); }

// The extra prepare of an in-game load: the save, a fresh cursor and the allotter's lower bound (restore 5.1).
O::PrepareFn LoadSave() {
    return [](K::World& w) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        w.SetResource(S::TheSave());
        w.SetResource(LoadCursor{});
        w.SetResource(GYO::Test::TestRangeAllotter::Of(S::TheSave().next, S::kLast));
        return {};
    };
}

void CheckSaveIsBack(K::World& w) {
    const auto* p12 = w.Get<S::Player>(12);
    const auto* p57 = w.Get<S::Player>(57);
    const auto* p58 = w.Get<S::Player>(58);
    REQUIRE((p12 != nullptr && p57 != nullptr && p58 != nullptr));
    CHECK(p12->hp == 5);
    CHECK(p57->hp == 7);
    CHECK(p58->hp == 9);
    const auto* chase = w.Get<S::ChaseTarget>(57);
    REQUIRE(chase != nullptr);
    CHECK(chase->target == 12);
    CHECK(w.Resource<S::Gold>()->v == 120);
    CHECK(w.RejectedSpawns() == 0);
}

}  // namespace

// C1: a startup load in one generating frame.
TEST_CASE("O-14: a startup load put back in the first frame; the game's logic runs from the second") {
    Log().clear();
    BatchSize() = 3;
    Pieces pieces;
    pieces.restore = true;
    pieces.spawnRock = true;
    auto p = Make(GenWorld("Main", pieces, true));
    Frame(*p);
    CHECK_MESSAGE(!p->Generating("Main"), "[wg-c1-ended] nobody held: generation ended after the first frame");
    CHECK_MESSAGE(CountLogged("play") == 0, "[wg-c1-not-yet] the game's logic did not run in the generating frame");
    CheckSaveIsBack(WorldOf(*p));
    Frame(*p);
    CHECK_MESSAGE(CountLogged("play") == 1, "[wg-c1-play] the game's logic runs once generation has ended");
    const std::vector<K::EntityId> rocks = WorldOf(*p).Keys<S::Pos>();
    CHECK(std::any_of(rocks.begin(), rocks.end(), [](K::EntityId id) { return id >= S::TheSave().next; }));
}

// C2: an in-game load over three generating frames.
TEST_CASE("O-14: an in-game load over three frames; the game's logic waits; game initialization does not run again") {
    Log().clear();
    BatchSize() = 1;
    Pieces pieces;
    pieces.restore = true;
    auto p = Make(GenWorld("Main", pieces));
    Frame(*p);
    Frame(*p);
    REQUIRE(CountLogged("play") == 1);
    O::ReplaceRequest request;
    request.extra = LoadSave();
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", std::move(request))));
    CHECK_MESSAGE(p->Generating("Main"), "[wg-c2-replaced] the new World starts generating");
    Frame(*p);
    CHECK_MESSAGE(p->Generating("Main"), "[wg-c2-held] one put back, held: still generating");
    Frame(*p);
    CHECK(p->Generating("Main"));
    CHECK(CountLogged("play") == 1);
    Frame(*p);
    CHECK_MESSAGE(!p->Generating("Main"), "[wg-c2-ends] the third frame put the last one back and nobody held");
    CHECK(CountLogged("play") == 1);
    Frame(*p);
    CHECK(CountLogged("play") == 2);
    CheckSaveIsBack(WorldOf(*p));
    CHECK(CountLogged("probe generating") == 1);  // game initialization ran once, on the first World
}

// C3: the same save put back in 1, 2 or 3 frames gives one World summary.
TEST_CASE("O-14: a save put back in 1, 2 or 3 frames gives the same World summary") {
    std::vector<std::string> summaries;
    for (int size : {3, 2, 1}) {
        Log().clear();
        BatchSize() = size;
        Pieces pieces;
        pieces.restore = true;
        auto p = Make(GenWorld("Main", pieces, true));
        // Bounded: a generation that never ends fails here instead of hanging.
        for (int frame = 0; frame < 5 && p->Generating("Main"); ++frame) Frame(*p);
        REQUIRE_MESSAGE(!p->Generating("Main"), "[wg-c3-ended] batch size " << size << ": generation did not end in 5 frames");
        auto summary = WorldOf(*p).Summary();
        REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().message));
        summaries.push_back(summary->Text());
    }
    CHECK_MESSAGE(summaries[0] == summaries[1], "[wg-c3-summary] 1 and 2 frames differ");
    CHECK_MESSAGE(summaries[0] == summaries[2], "[wg-c3-summary] 1 and 3 frames differ");
}

// C5: game initialization before the first generating frame; not again after a replacement.
TEST_CASE("O-14: game initialization runs before the first World's first generating frame, once") {
    Log().clear();
    Pieces pieces;
    pieces.holdUntilReady = true;
    std::vector<O::WorldDescription> worlds;
    worlds.push_back(GenWorld("Main", pieces));
    worlds.push_back(GenWorld("Side", pieces));
    auto p = Make(std::move(worlds));
    WorldOf(*p).SetResource(Ready{1});
    WorldOf(*p, "Side").SetResource(Ready{1});
    Frame(*p);
    const auto probe = std::find(Log().begin(), Log().end(), "probe generating");
    const auto gen = std::find(Log().begin(), Log().end(), "gen");
    REQUIRE((probe != Log().end() && gen != Log().end()));
    CHECK_MESSAGE(probe < gen, "[wg-c5-init-first] game initialization ran before the first generating frame");
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", O::ReplaceRequest{})));
    CHECK(p->Generating("Main"));
    Frame(*p);
    CHECK(CountLogged("probe generating") == 1);
}

// C6: each World on its own.
TEST_CASE("O-14: each World generates on its own: one holds, the other has ended") {
    Log().clear();
    Pieces holding;
    holding.holdUntilReady = true;
    std::vector<O::WorldDescription> worlds;
    worlds.push_back(GenWorld("Map", holding));
    worlds.push_back(GenWorld("Menu", Pieces{}));
    auto p = Make(std::move(worlds));
    Frame(*p);
    Frame(*p);
    CHECK(p->Generating("Map"));
    CHECK_MESSAGE(!p->Generating("Menu"), "[wg-c6-own] Menu ended in its first frame while Map holds");
    CHECK(CountLogged("play") == 1);  // Menu's second frame
}

// C7: an Err in generation ends the Product like any frame.
TEST_CASE("O-14: an Err of a generation System ends the Product; the failure names the schedule and RunFrame") {
    Log().clear();
    Pieces pieces;
    pieces.fail = true;
    auto p = Make(GenWorld("Main", pieces));
    WorldOf(*p).SetResource(FailGen{1});
    auto ran = p->RunFrame();
    REQUIRE(!ran);
    REQUIRE(p->Failure() != nullptr);
    CHECK(p->Failure()->schedule == std::string(O::DefaultSchedules::frame));
    CHECK(p->Failure()->entry == "RunFrame");
}

// C8: DropOldFirst, two generating frames.
TEST_CASE("O-14: a DropOldFirst load over two frames") {
    Log().clear();
    BatchSize() = 2;
    Pieces pieces;
    pieces.restore = true;
    auto p = Make(GenWorld("Main", pieces));
    Frame(*p);
    O::ReplaceRequest request;
    request.extra = LoadSave();
    request.order = O::ReplaceOrder::DropOldFirst;
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", std::move(request))));
    CHECK_MESSAGE(p->Generating("Main"), "[wg-c8-replaced] the new World starts generating");
    Frame(*p);
    CHECK(p->Generating("Main"));
    Frame(*p);
    CHECK(!p->Generating("Main"));
    CheckSaveIsBack(WorldOf(*p));
}

// C9: a replacement while generating; a failed one leaves the old World generating.
TEST_CASE("O-14: replacing a generating World starts the new one generating; a failed replacement keeps the old one") {
    Log().clear();
    Pieces holding;
    holding.holdUntilReady = true;
    auto p = Make(GenWorld("Main", holding));
    Frame(*p);
    Frame(*p);
    REQUIRE(p->Generating("Main"));
    O::ReplaceRequest failing;
    failing.extra = [](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        O::OrchestrationError error(O::OrchestrationErrorCode::PrepareFailed, "the save is unreadable");
        error.reporterCode = "save-unreadable";
        return GYO::Engine::Base::Err(std::move(error));
    };
    REQUIRE(!p->ReplaceWorld("Main", std::move(failing)));
    CHECK(p->Generating("Main"));
    Frame(*p);
    CHECK_MESSAGE(p->Generating("Main"), "[wg-c9-old-holds] the old World still holds");
    WorldOf(*p).SetResource(Ready{1});
    Frame(*p);
    CHECK(!p->Generating("Main"));
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", O::ReplaceRequest{})));
    CHECK(p->Generating("Main"));
}

// C10: RunSchedule neither clears held nor judges the end.
TEST_CASE("O-14: RunSchedule does not touch generation; the next RunFrame judges") {
    Log().clear();
    Pieces holding;
    holding.holdUntilReady = true;
    auto p = Make(GenWorld("Main", holding));
    Frame(*p);
    REQUIRE(p->Generating("Main"));
    WorldOf(*p).SetResource(Ready{1});
    REQUIRE(static_cast<bool>(p->RunSchedule("Main", std::string(O::DefaultSchedules::frame))));
    CHECK_MESSAGE(p->Generating("Main"), "[wg-c10-runschedule] RunSchedule ran Main with nobody holding and did not end generation");
    Frame(*p);
    CHECK(!p->Generating("Main"));
}

// C11: with the skeleton off, WorldGeneration is still registered.
TEST_CASE("O-14: with the skeleton off WorldGeneration is registered; a World nobody holds ends in its first frame") {
    Log().clear();
    O::ProductDescription d;
    d.name = "tool";
    O::WorldDescription w = World(
        "Main", [](O::WorldSetup& s) { Basics(s.Registrar(), {Once("Tick")}); }, {"Tick"});
    w.skeleton = O::ScheduleSkeleton::Off;
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    const O::WorldGeneration* generation = WorldOf(p).Resource<O::WorldGeneration>();
    REQUIRE_MESSAGE(generation != nullptr, "[wg-c11-registered] WorldGeneration is registered with the skeleton off");
    CHECK(generation->Generating());
    Frame(p);
    CHECK(!p.Generating("Main"));
}

// C12: the default runner runs one frame; a held World is still generating and the run succeeds.
TEST_CASE("O-14: the default runner returns Success while a World holds") {
    Log().clear();
    Pieces holding;
    holding.holdUntilReady = true;
    auto p = Make(GenWorld("Main", holding));
    const O::RunnerExit exit = p->Run();
    CHECK(exit.code == O::RunnerExitCode::Success);
    CHECK(p->Generating("Main"));
}

// C17: generating from the moment the World is built.
TEST_CASE("O-14: a World is generating from when it is built: prepare, game initialization, beforeSwap, RunSchedule") {
    Log().clear();
    auto p = Make(GenWorld("Main", Pieces{}));
    CHECK_MESSAGE(Logged("prepare generating"), "[wg-c17-built] the prepare function sees a generating World");
    CHECK(p->Generating("Main"));
    REQUIRE(static_cast<bool>(p->RunSchedule("Main", "Probe")));  // game initialization first, then Probe
    CHECK(CountLogged("probe generating") == 2);
    O::ReplaceRequest request;
    request.beforeSwap = {"Probe"};
    Frame(*p);
    REQUIRE(!p->Generating("Main"));
    REQUIRE(static_cast<bool>(p->ReplaceWorld("Main", std::move(request))));
    CHECK(CountLogged("probe generating") == 3);
    CHECK(CountLogged("prepare generating") == 2);
}

// C18: Hold() after generation has ended has no effect.
TEST_CASE("O-14: Hold after generation has ended leaves held false and the summary unchanged") {
    Log().clear();
    Pieces pieces;
    pieces.holdAfter = true;
    auto p = Make(GenWorld("Main", pieces));
    Frame(*p);
    REQUIRE(!p->Generating("Main"));
    auto before = WorldOf(*p).Summary();
    REQUIRE(static_cast<bool>(before));
    Frame(*p);
    const O::WorldGeneration* generation = WorldOf(*p).Resource<O::WorldGeneration>();
    REQUIRE(generation != nullptr);
    CHECK_MESSAGE(!generation->held, "[wg-c18-held] Hold after the end set held");
    CHECK(!p->Generating("Main"));
    auto after = WorldOf(*p).Summary();
    REQUIRE(static_cast<bool>(after));
    CHECK(before->Text() == after->Text());
}
