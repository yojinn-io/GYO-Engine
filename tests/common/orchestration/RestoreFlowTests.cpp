// Judgement 2 of the declaration: the restore cases the restore-entry
// declaration (revisions 10, 11) asks this batch for.

#include "SaveGame.hpp"

#include <algorithm>

using namespace OrchTest;
using namespace OrchTest::Save;

namespace {

int Entities(O::Product& p) {
    return static_cast<int>(p.World("Main").GetWorld().Keys<Pos>().size());
}
int GoldOf(O::Product& p) {
    const Gold* gold = p.World("Main").GetWorld().Resource<Gold>();
    return gold == nullptr ? -1 : gold->v;
}

O::PrepareFn Unsupported() {
    return [](K::World&) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        return GYO::Engine::Base::Err(O::OrchestrationError(O::OrchestrationErrorCode::PrepareFailed,
                                                       "the save's version is not supported", {}, "save-version-unsupported"));
    };
}

// Four frames (four rocks: not the save's three entities), a dropped spawn and an
// inserted System: the "current game" before a load.
void PlayABit(O::Product& p) {
    for (int i = 0; i < 4; ++i) REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE(static_cast<bool>(p.RunSchedule("Main", "Junk")));
    REQUIRE(static_cast<bool>(p.InsertSystems("Main", "console", [](K::Registrar& r) {
        r.AddSystem<&Mark<'i'>>(In("Inserted", "Play"));
    })));
}

} // namespace

TEST_CASE("restore: case 1, loading at startup: ids, values, the reference, a later spawn above the range, nothing dropped") {
    Log().clear();
    Options options;
    options.first = 900;
    options.startupSave = true;
    auto built = O::Product::Build(Game(options), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunSchedule("Main", "Load")));
    REQUIRE(static_cast<bool>(p.RunFrame()));
    K::World& w = p.World("Main").GetWorld();
    for (const SaveData::Ent& e : TheSave().ents) {
        REQUIRE_MESSAGE(w.Get<Player>(e.id) != nullptr, "[or-restore-ids] " << e.id);
        CHECK(w.Get<Player>(e.id)->hp == e.hp);
        CHECK(w.Get<Pos>(e.id)->x == e.x);
    }
    CHECK_MESSAGE(w.Get<ChaseTarget>(57)->target == 12, "[or-restore-ref]");
    const std::vector<K::EntityId> rocks = w.Keys<Pos>();
    CHECK_MESSAGE(std::any_of(rocks.begin(), rocks.end(), [](K::EntityId id) { return id >= 900; }), "[or-restore-range]");
    CHECK_MESSAGE(GoldOf(p) == 130, "[or-restore-gold] " << GoldOf(p));
    CHECK_MESSAGE(w.RejectedSpawns() == 0, "[or-restore-nothing-dropped] " << w.RejectedSpawns());
}

TEST_CASE("restore: case 2, without moving the lower bound every SpawnWithId is dropped as IdInAllotterRange") {
    Log().clear();
    Options options;
    options.startupSave = true;  // the range stays [1, last]
    auto built = O::Product::Build(Game(options), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    REQUIRE(static_cast<bool>((*built)->RunSchedule("Main", "Load")));
    K::World& w = (*built)->World("Main").GetWorld();
    CHECK_MESSAGE(w.RejectedSpawns() == 3, "[or-restore-control] " << w.RejectedSpawns());
    const auto rejected = w.Diagnostics<K::RejectedSpawn>();
    CHECK(std::all_of(rejected.begin(), rejected.end(),
                      [](const K::RejectedSpawn& r) { return r.reason == K::SpawnRejectReason::IdInAllotterRange; }));
}

TEST_CASE("restore: case 3, loading in game: only what the load puts back, gold from the save, no old record, no game init again, no inserted System") {
    Log().clear();
    BootRuns() = 0;
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    PlayABit(p);
    REQUIRE(GoldOf(p) == 40);
    REQUIRE(p.World("Main").GetWorld().RejectedSpawns() == 1);
    auto replaced = p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(TheSave()), {"Load"}, O::ReplaceOrder::BuildNewFirst});
    REQUIRE_MESSAGE(static_cast<bool>(replaced), replaced.error().message << " " << replaced.error().detail);
    K::World& w = p.World("Main").GetWorld();
    CHECK_MESSAGE(w.Resource<SaveData>() != nullptr, "[or-restore-extra-ran] the new World has no SaveData");
    CHECK_MESSAGE(Entities(p) == 3, "[or-restore-new-world] " << Entities(p));
    CHECK_MESSAGE(GoldOf(p) == 120, "[or-restore-gold] " << GoldOf(p));
    CHECK_MESSAGE(w.RejectedSpawns() == 0, "[or-restore-new-records] " << w.RejectedSpawns());
    CHECK_MESSAGE(BootRuns() == 1, "[or-restore-no-game-init] " << BootRuns());
    CHECK_MESSAGE(w.Types().SystemNamed("Inserted") == nullptr, "[or-restore-reruns-registration] the inserted System is there");
    REQUIRE(static_cast<bool>(p.RunFrame()));
    CHECK_MESSAGE(GoldOf(p) == 130, "[or-restore-frame-on-new] " << GoldOf(p));
}

TEST_CASE("restore: case 4, a load that fails: BuildNewFirst keeps the old World, DropOldFirst leaves the place empty and ends the Product") {
    Log().clear();
    SUBCASE("(a) BuildNewFirst, the prepare refuses the save's version; (b) the entity part cannot be read") {
        auto built = O::Product::Build(Game(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        PlayABit(p);
        {
            WarningCounter warnings;
            auto refused = p.ReplaceWorld("Main", O::ReplaceRequest{Unsupported(), {"Load"}, O::ReplaceOrder::BuildNewFirst});
            REQUIRE_FALSE_MESSAGE(static_cast<bool>(refused), "[or-restore-fail-a] replaced despite the refused save");
            CHECK_MESSAGE(refused.error().code == O::OrchestrationErrorCode::ReplacePrepareFailed, "[or-restore-fail-a] " << refused.error().message);
            CHECK_MESSAGE(refused.error().reporterCode == "save-version-unsupported", "[or-restore-reporter-code]");
            CHECK_MESSAGE(warnings.Warnings() == 0, "[or-restore-fail-silent] " << warnings.Warnings());  // error levels D2
        }
        CHECK_MESSAGE((GoldOf(p) == 40 && Entities(p) == 4), "[or-restore-keep-old] gold " << GoldOf(p) << " entities " << Entities(p));
        SaveData broken = TheSave();
        broken.unreadable = true;
        auto unreadable = p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(broken), {"Load"}, O::ReplaceOrder::BuildNewFirst});
        REQUIRE_FALSE_MESSAGE(static_cast<bool>(unreadable), "[or-restore-fail-b] replaced despite the unreadable save");
        CHECK_MESSAGE(unreadable.error().code == O::OrchestrationErrorCode::ReplaceScheduleFailed, "[or-restore-fail-b]");
        CHECK_MESSAGE(GoldOf(p) == 40, "[or-restore-keep-old-b] " << GoldOf(p));
        CHECK(static_cast<bool>(p.RunFrame()));
    }
    SUBCASE("(c) DropOldFirst refused: the place is empty and RunFrame says WorldMissing; (d) a readable save does not fill it") {
        auto built = O::Product::Build(Game(), Units().Take());
        ORCH_REQUIRE_BUILT(built);
        O::Product& p = **built;
        PlayABit(p);
        auto refused = p.ReplaceWorld("Main", O::ReplaceRequest{Unsupported(), {"Load"}, O::ReplaceOrder::DropOldFirst});
        REQUIRE_FALSE(static_cast<bool>(refused));
        CHECK_MESSAGE(!p.HasWorld("Main"), "[or-drop-old-empty]");
        auto frame = p.RunFrame();
        REQUIRE_FALSE(static_cast<bool>(frame));
        CHECK_MESSAGE(frame.error().code == O::OrchestrationErrorCode::WorldMissing, "[or-drop-old-missing] " << frame.error().message);
        auto filled = p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(TheSave()), {"Load"}, O::ReplaceOrder::DropOldFirst});
        REQUIRE_FALSE_MESSAGE(static_cast<bool>(filled), "[or-drop-old-not-filled] the empty place was filled");
        CHECK_MESSAGE((filled.error().code == O::OrchestrationErrorCode::WorldMissing && !p.HasWorld("Main")), "[or-drop-old-not-filled]");
    }
}

TEST_CASE("restore: case 5, the same save loaded twice gives the same World summary") {
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(TheSave()), {"Load"}})));
    const auto firstSummary = p.World("Main").GetWorld().Summary();
    REQUIRE_MESSAGE(static_cast<bool>(firstSummary), (firstSummary ? std::string() : firstSummary.error().Text()));
    const K::Fingerprint first = *firstSummary;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(TheSave()), {"Load"}})));
    const auto second = p.World("Main").GetWorld().Summary();
    REQUIRE_MESSAGE(static_cast<bool>(second), (second ? std::string() : second.error().Text()));
    CHECK(*second == first);
}

TEST_CASE("restore: case 6, what game initialization put in the first World: a marked value carries over (changed in game), the rest is fresh") {
    Log().clear();
    auto built = O::Product::Build(Game(), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    O::Product& p = **built;
    REQUIRE(static_cast<bool>(p.RunFrame()));
    p.World("Main").GetWorld().ResourceMut<KeyBindings>()->jump = 7;  // the options menu changed it
    REQUIRE(ScoreOf(p) == 5);
    auto failed = p.ReplaceWorld("Main", O::ReplaceRequest{Unsupported(), {"Load"}});
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(failed), "[or-carry-back] replaced despite the refused save");
    const KeyBindings* kept = p.World("Main").GetWorld().Resource<KeyBindings>();
    REQUIRE_MESSAGE(kept != nullptr, "[or-carry-back] the old World lost KeyBindings");
    CHECK_MESSAGE(kept->jump == 7, "[or-carry-back] the old World's KeyBindings changed");
    REQUIRE(static_cast<bool>(p.ReplaceWorld("Main", O::ReplaceRequest{LoadInto(TheSave()), {"Load"}})));
    CHECK_MESSAGE(JumpOf(p) == 7, "[or-carry-over] KeyBindings did not carry over");
    CHECK_MESSAGE(ScoreOf(p) == 0, "[or-carry-only-marked] Score carried over");
}
