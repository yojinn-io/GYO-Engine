// Scenario test segment 1 (declaration section 8.2, judgement 3): the game
// "walk and score" goes through the batch-1 mechanisms and the orchestration
// layer once. A failure is classified by contract first (section 8.4): the
// scenario's writing, the mechanism, or something no contract promised.
// Each later batch adds a segment here; earlier segments keep running.

#include "WalkerGame.hpp"

#include <doctest/doctest.h>

#include <algorithm>
// MSVC: doctest messages stream std::string_view; that needs the full ostream.
#include <ostream>
#include <string>
#include <vector>

using namespace Walker;

namespace {

std::string Text(const std::optional<int>& v) { return v ? std::to_string(*v) : std::string("absent"); }
std::string SeenText(const std::vector<bool>& seen) {
    std::string out;
    for (bool b : seen) out += b ? '1' : '0';
    return out;
}

O::Product& Built(::Engine::Base::Result<std::unique_ptr<O::Product>, O::ProductStartupReport>& built) {
    REQUIRE_MESSAGE(static_cast<bool>(built), "[sc-s1-1-built] " << (built ? std::string() : built.error().detail));
    return **built;
}

void Frames(O::Product& p, int n, const char* step) {
    for (int i = 0; i < n; ++i) {
        auto ran = p.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), step << " frame " << i << ": " << (ran ? std::string() : ran.error().message));
    }
}

// S1-1 to S1-5: the game until its save. Returns the save.
SaveData PlayUntilSave(O::Product& p) {
    // S1-1: the product's registration and Plans; the game initialization list.
    CHECK_MESSAGE((p.GameInitList() == std::vector<std::string>{"Boot"}), "[sc-s1-1-gameinit-list]");
    CHECK_MESSAGE(p.World("Main").GetWorld().Types().SystemNamed("Walk") != nullptr, "[sc-s1-1-registered]");
    CHECK_MESSAGE(!p.GameInitRan(), "[sc-s1-1-not-yet]");

    // S1-2: RunSchedule runs game initialization first (O-1).
    REQUIRE_MESSAGE(static_cast<bool>(p.RunSchedule("Main", "PlacePlayer")), "[sc-s1-2-ran]");
    const K::World& w = p.World("Main").GetWorld();
    CHECK_MESSAGE((Seen().bootRuns == 1 && ScoreOf(w) == 0), "[sc-s1-2-boot-ran] boot " << Seen().bootRuns << " score " << Text(ScoreOf(w)));
    CHECK_MESSAGE((Seen().order == std::vector<std::string>{"boot", "place"}), "[sc-s1-2-boot-first] " << Seen().order.size() << " entries, first " << (Seen().order.empty() ? std::string("none") : Seen().order.front()));
    CHECK_MESSAGE((w.Keys<Player>() == std::vector<K::EntityId>{1}), "[sc-s1-2-player-id]");
    CHECK_MESSAGE(XOf(w, 1) == 0, "[sc-s1-2-player-at-origin] " << Text(XOf(w, 1)));

    // S1-3: five frames; Commands' BestScore arrives at the first sync point (DI-KS 15 (5)).
    Frames(p, 5, "S1-3");
    const K::World& w3 = p.World("Main").GetWorld();
    CHECK_MESSAGE((XOf(w3, 1) == 5 && ScoreOf(w3) == 5), "[sc-s1-3-walked] x " << Text(XOf(w3, 1)) << " score " << Text(ScoreOf(w3)));
    CHECK_MESSAGE(BestOf(w3) == 1, "[sc-s1-3-best-set] " << Text(BestOf(w3)));
    CHECK_MESSAGE((Seen().bestSeenBeforeSync == std::vector<bool>{false, true, true, true, true}),
                  "[sc-s1-3-best-at-sync] seen before the sync point, frame by frame: " << SeenText(Seen().bestSeenBeforeSync));
    CHECK_MESSAGE(Seen().bootRuns == 1, "[sc-s1-3-boot-once] " << Seen().bootRuns);

    // S1-4: the console inserts Accelerate before Walk (DI-KS 26); five frames at speed 2.
    REQUIRE_MESSAGE(static_cast<bool>(p.InsertSystems("Main", "console", &InsertAccelerate)), "[sc-s1-4-inserted]");
    Frames(p, 5, "S1-4");
    const K::World& w4 = p.World("Main").GetWorld();
    CHECK_MESSAGE((XOf(w4, 1) == 15 && ScoreOf(w4) == 10), "[sc-s1-4-accelerated] x " << Text(XOf(w4, 1)) << " score " << Text(ScoreOf(w4)));
    CHECK_MESSAGE(JumpOf(w4) == 7, "[sc-s1-10-rebound] the options menu did not rebind: " << Text(JumpOf(w4)));

    // S1-5: the author's save.
    const SaveData save = MakeSave(w4);
    CHECK_MESSAGE((save.player == 1 && save.x == 15 && save.score == 10 && save.best == 1 && save.next == 2),
                  "[sc-s1-5-save] player " << save.player << " x " << save.x << " score " << save.score << " best " << save.best
                                           << " next " << save.next);
    // Two more frames before the load: the old World moves on (x 19, score 12), so
    // what the load puts back is told apart from what the old World still has.
    Frames(p, 2, "after the save");
    REQUIRE_MESSAGE((XOf(p.World("Main").GetWorld(), 1) == 19 && ScoreOf(p.World("Main").GetWorld()) == 12), "[sc-s1-5-moved-on]");
    return save;
}

// S1-6 to S1-9 (and S1-10's check) after a load with the given order.
void LoadAndPlay(O::Product& p, const SaveData& save, O::ReplaceOrder order) {
    const WorldLife* oldLife = p.World("Main").GetWorld().Resource<WorldLife>();
    REQUIRE(oldLife != nullptr);
    const int oldSerial = oldLife->serial;
    const int bootBefore = Seen().bootRuns;
    // S1-6: replace the World from the description, the save in the extra, Load before the swap (O-7, O-8).
    auto replaced = p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(save), {"Load"}, order});
    REQUIRE_MESSAGE(static_cast<bool>(replaced), "[sc-s1-6-replaced] " << (replaced ? std::string() : replaced.error().message + " " + replaced.error().detail));
    const K::World& w = p.World("Main").GetWorld();

    // S1-7: LoadSave put the player back with its id on the spot and saw it in the same System (DI-KS 18).
    CHECK_MESSAGE(!Seen().playerRejected.has_value(), "[sc-s1-7-put-rejected] reason " << (Seen().playerRejected ? static_cast<int>(*Seen().playerRejected) : -1));
    CHECK_MESSAGE(Seen().playerSeenOnSpot == 15, "[sc-s1-7-on-spot] " << Text(Seen().playerSeenOnSpot));
    CHECK_MESSAGE((w.Keys<Player>() == std::vector<K::EntityId>{1} && XOf(w, 1) == 15), "[sc-s1-7-player-back]");
    const Velocity* v = w.Get<Velocity>(1);
    CHECK_MESSAGE((v != nullptr && v->x == 1), "[sc-s1-7-default-velocity]");
    CHECK_MESSAGE((ScoreOf(w) == 10 && BestOf(w) == 1), "[sc-s1-7-scores] " << Text(ScoreOf(w)) << " " << Text(BestOf(w)));
    CHECK_MESSAGE((Seen().cursor.has_value() && *Seen().cursor >= save.next), "[sc-s1-7-cursor-above-range]");
    CHECK_MESSAGE((Seen().cursor.has_value() && w.Keys<Cursor>() == std::vector<K::EntityId>{*Seen().cursor}), "[sc-s1-7-cursor-in-world]");
    CHECK_MESSAGE(w.RejectedSpawns() == 0, "[sc-s1-7-nothing-dropped] " << w.RejectedSpawns());

    // S1-10: the key bindings came over as the player left them; the score is the save's.
    CHECK_MESSAGE(JumpOf(w) == 7, "[sc-s1-10-carried] " << Text(JumpOf(w)));

    // S1-8: a new registration (no Accelerate), game initialization not again, the old World gone.
    CHECK_MESSAGE(w.Types().SystemNamed("Accelerate") == nullptr, "[sc-s1-8-registration-new]");
    CHECK_MESSAGE(Seen().bootRuns == bootBefore, "[sc-s1-8-no-game-init] " << Seen().bootRuns);
    const std::vector<int>& destroyed = WorldLife::Destroyed();
    CHECK_MESSAGE(std::count(destroyed.begin(), destroyed.end(), oldSerial) == 1, "[sc-s1-8-old-gone] " << std::count(destroyed.begin(), destroyed.end(), oldSerial));
    const WorldLife* newLife = w.Resource<WorldLife>();
    CHECK_MESSAGE((newLife != nullptr && newLife->serial != oldSerial &&
                   std::count(destroyed.begin(), destroyed.end(), newLife->serial) == 0), "[sc-s1-8-new-alive]");
    Frames(p, 5, "S1-8");
    const K::World& w8 = p.World("Main").GetWorld();
    CHECK_MESSAGE((XOf(w8, 1) == 20 && ScoreOf(w8) == 15), "[sc-s1-8-walked] x " << Text(XOf(w8, 1)) << " score " << Text(ScoreOf(w8)));
    CHECK_MESSAGE(Seen().bootRuns == bootBefore, "[sc-s1-8-no-game-init-after-frames] " << Seen().bootRuns);

    // S1-9: between two Runs the console inserts Accelerate again (DI-KS 26 rule 4).
    REQUIRE_MESSAGE(static_cast<bool>(p.InsertSystems("Main", "console", &InsertAccelerate)), "[sc-s1-9-inserted]");
    Frames(p, 1, "S1-9");
    CHECK_MESSAGE(XOf(p.World("Main").GetWorld(), 1) == 22, "[sc-s1-9-accelerated] " << Text(XOf(p.World("Main").GetWorld(), 1)));
    CHECK_MESSAGE(JumpOf(p.World("Main").GetWorld()) == 7, "[sc-s1-10-kept]");
}

} // namespace

TEST_CASE("scenario segment 1: walk, score, insert, save, load into a new World, walk on") {
    SUBCASE("S1-6 BuildNewFirst") {
        Seen() = Notes{};
        auto built = O::Product::Build(Game(), O::RegisteredUnits{});
        O::Product& p = Built(built);
        const SaveData save = PlayUntilSave(p);
        LoadAndPlay(p, save, O::ReplaceOrder::BuildNewFirst);
    }
    SUBCASE("S1-6c DropOldFirst: S1-7 to S1-9 the same") {
        Seen() = Notes{};
        auto built = O::Product::Build(Game(), O::RegisteredUnits{});
        O::Product& p = Built(built);
        const SaveData save = PlayUntilSave(p);
        LoadAndPlay(p, save, O::ReplaceOrder::DropOldFirst);
    }
}

// S1-7b guards DI-KS 18 ("a direct insert does not look at the allotter"): the
// player put back with World::Insert leaves the range at 1, so the next Spawn
// gets a live id; the author handles AllottedIdAlive and the World goes on.
TEST_CASE("scenario segment 1: S1-7b, the lower bound forgotten: the cursor's spawn gets AllottedIdAlive, handled, the World goes on") {
    Seen() = Notes{};
    auto built = O::Product::Build(Game(), O::RegisteredUnits{});
    O::Product& p = Built(built);
    const SaveData save = PlayUntilSave(p);
    auto replaced = p.ReplaceWorld("Main", O::ReplaceRequest{LoadForgettingTheLowerBound(save), {"Load"}, O::ReplaceOrder::BuildNewFirst});
    REQUIRE_MESSAGE(static_cast<bool>(replaced), "[sc-s1-7b-replaced] " << (replaced ? std::string() : replaced.error().message + " " + replaced.error().detail));
    const K::World& w = p.World("Main").GetWorld();
    CHECK_MESSAGE(Seen().handledSpawnError == K::KernelErrorCode::AllottedIdAlive, "[sc-s1-7b-alive] the cursor's spawn did not return AllottedIdAlive");
    CHECK_MESSAGE(!Seen().cursor.has_value(), "[sc-s1-7b-no-cursor]");
    CHECK_MESSAGE(w.RejectedSpawns() == 1, "[sc-s1-7b-recorded] " << w.RejectedSpawns());
    const auto rejected = w.Diagnostics<K::RejectedSpawn>();
    CHECK_MESSAGE((rejected.size() == 1 && rejected[0].fromAllotter && rejected[0].entity == 1 &&
                   rejected[0].reason == K::SpawnRejectReason::IdAlive && rejected[0].emitter == "LoadSave"),
                  "[sc-s1-7b-record-is-the-cursor]");
    CHECK_MESSAGE(w.State() != K::WorldState::Interrupted, "[sc-s1-7b-goes-on]");
    CHECK_MESSAGE(XOf(w, 1) == 15, "[sc-s1-7b-player-kept] " << Text(XOf(w, 1)));
    Frames(p, 1, "S1-7b");
    CHECK_MESSAGE(XOf(p.World("Main").GetWorld(), 1) == 16, "[sc-s1-7b-walks]");
}
