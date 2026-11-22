// Judgement 2 of the declaration: the restore cases the restore-entry
// declaration (revisions 10, 11) asks this batch for.

#include "SaveGame.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace GYO::Test::Orchestration;
using namespace GYO::Test::Orchestration::Save;

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
                                                       "the save's version is not supported", {}, "save-version-too-old"));
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
    CHECK_MESSAGE(GoldOf(p) == 130, "[or-restore-gold-startup] " << GoldOf(p));
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
    CHECK_MESSAGE(GoldOf(p) == 120, "[or-restore-gold-in-game] " << GoldOf(p));
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
            CHECK_MESSAGE(refused.error().reporterCode == "save-version-too-old", "[or-restore-reporter-code]");
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
    CHECK_MESSAGE(*second == first, "[or-restore-twice-same]");
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

// Restore-entry declaration revision 12, section 18.3 (judgement 1 cases 2, 6, 7 and 8): loads put back while the
// new World generates (O-14): the load System is in a set with WhileGenerating() on Main/Update and records deferred
// spawns, unlike the cases above (an exclusive System in beforeSwap). Each CHECK carries a tag the mutants of 18.3
// expect.
namespace {

struct Monster {
    int kind = 0;
};
struct BloodBar {
    K::EntityId owner = 0;
};
struct Marker {
    int v = 0;
};
// Loads that wait one generating frame before putting anything back (case 7: the reader has run before).
struct LoadGate {
    int waited = 0;
};
inline void HashValue(K::Hasher& h, const Monster& v) { HashValue(h, v.kind); }
inline void HashValue(K::Hasher& h, const BloodBar& v) { HashValue(h, v.owner); }
inline void HashValue(K::Hasher& h, const Marker& v) { HashValue(h, v.v); }
inline void HashValue(K::Hasher& h, const LoadGate& v) { HashValue(h, v.waited); }

using SavedEnt = K::Bundle<Player, Pos, ChaseTarget>;

// Case 2: renumbering. The save's entities in ascending saved id, each spawned through the allotter; the author's
// table maps a saved id to its new one and rewrites ChaseTarget (the saved target is spawned first). The gold goes
// back through ResMut.
K::SystemResult RenumberLoad(K::Spawn<SavedEnt> spawn, K::Res<SaveData> save, K::ResMut<Gold> gold) {
    std::vector<SaveData::Ent> ents(save->ents.begin(), save->ents.end());
    std::sort(ents.begin(), ents.end(), [](const SaveData::Ent& a, const SaveData::Ent& b) { return a.id < b.id; });
    std::vector<std::pair<K::EntityId, K::EntityId>> table;
    for (const SaveData::Ent& e : ents) {
        K::EntityId chase = 0;
        for (const auto& [from, to] : table) {
            if (from == e.chase) chase = to;
        }
        auto id = spawn.New(Player{e.hp}, Pos{e.x}, ChaseTarget{chase});
        if (!id) return GYO::Engine::Base::Err(std::move(id).error());
        table.emplace_back(e.id, *id);
    }
    gold->v = save->gold;
    return {};
}

// Case 6: monsters put back with their saved ids; Monster's OnAdd spawns a blood bar (Bevy #14300's shape).
K::SystemResult MonsterLoad(K::SpawnWithId<K::Bundle<Monster, Pos>> spawn) {
    spawn.New(12, Monster{1}, Pos{1});
    spawn.New(57, Monster{2}, Pos{2});
    spawn.New(58, Monster{3}, Pos{3});
    return {};
}
K::SystemResult LogMonsterAdd(K::HookCtx ctx, K::Spawn<K::Bundle<BloodBar>> spawn) {
    Log().push_back("add Monster " + std::to_string(ctx.Entity()));
    auto bar = spawn.New(BloodBar{ctx.Entity()});
    if (!bar) return GYO::Engine::Base::Err(std::move(bar).error());
    return {};
}
K::SystemResult LogPosAdd(K::HookCtx ctx) {
    Log().push_back("add Pos " + std::to_string(ctx.Entity()));
    return {};
}

// Case 7: one generating frame of waiting, then the save's entities with their ids.
K::SystemResult GatedLoad(K::SpawnWithId<SavedEnt> spawn, K::Res<SaveData> save, K::ResMut<LoadGate> gate,
                          K::ResMut<O::WorldGeneration> generation) {
    if (gate->waited == 0) {
        gate->waited = 1;
        generation->Hold();
        return {};
    }
    if (gate->waited == 1) {
        for (const SaveData::Ent& e : save->ents) spawn.New(e.id, Player{e.hp}, Pos{e.x}, ChaseTarget{e.chase});
        gate->waited = 2;
    }
    return {};
}
// Case 7's reader: every frame, no run condition.
K::SystemResult SeeAddedPos(K::Query<K::Read<Pos>, K::Added<Pos>> added) {
    Log().push_back("added " + std::to_string(added.Count()));
    return {};
}

// Case 8: one System spawns an entity with only a marker, then inserts Pos into it.
K::SystemResult SpawnThenInsertLoad(K::Spawn<K::Bundle<Marker>> spawn, K::Commands commands) {
    auto id = spawn.New(Marker{1});
    if (!id) return GYO::Engine::Base::Err(std::move(id).error());
    commands.Insert(*id, Pos{42});
    return {};
}

enum class GenLoad { Renumber, Monsters, Gated, SpawnInsert };

K::SystemOptions GenSystem(std::string name, std::string stage = std::string(O::DefaultSchedules::update),
                           std::vector<std::string> sets = {"Gen"}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::string(O::DefaultSchedules::frame);
    m.stage = std::move(stage);
    m.sets = std::move(sets);
    o.mounts.push_back(std::move(m));
    return o;
}

// A one-World product on the default skeleton; the description's prepare puts the save and the allotter (the lower
// bound moved past the save's ids only for the loads that keep the ids).
std::unique_ptr<O::Product> GenLoadProduct(GenLoad load) {
    O::WorldDescription w = World("Main", [load](O::WorldSetup& s) {
        K::Registrar& r = s.Registrar();
        r.Component<Player>({.name = "Player"});
        r.Component<Pos>({.name = "Pos"});
        r.Component<ChaseTarget>({.name = "ChaseTarget"});
        r.Component<Monster>({.name = "Monster"});
        r.Component<BloodBar>({.name = "BloodBar"});
        r.Component<Marker>({.name = "Marker"});
        r.Resource<Gold>({.name = "Gold"});
        r.Resource<SaveData>({.name = "SaveData"});
        r.Resource<LoadGate>({.name = "LoadGate"});
        r.Resource<GYO::Test::TestRangeAllotter>({.name = "Allotter"});
        r.Allotter<GYO::Test::TestRangeAllotter>();
        const std::string main(O::DefaultSchedules::frame);
        r.Set(K::SetOptions{.name = "Gen", .schedule = main}, {O::WhileGenerating()});
        switch (load) {
        case GenLoad::Renumber: r.AddSystem<&RenumberLoad>(GenSystem("RenumberLoad")); break;
        case GenLoad::Monsters:
            r.Hook<Monster, &LogMonsterAdd>(K::HookTrigger::OnAdd, {.name = "LogMonsterAdd"});
            r.Hook<Pos, &LogPosAdd>(K::HookTrigger::OnAdd, {.name = "LogPosAdd"});
            r.AddSystem<&MonsterLoad>(GenSystem("MonsterLoad"));
            break;
        case GenLoad::Gated:
            r.AddSystem<&GatedLoad>(GenSystem("GatedLoad"));
            r.AddSystem<&SeeAddedPos>(GenSystem("SeeAddedPos", std::string(O::DefaultSchedules::preUpdate), {}));
            break;
        case GenLoad::SpawnInsert: r.AddSystem<&SpawnThenInsertLoad>(GenSystem("SpawnThenInsertLoad")); break;
        }
    });
    const bool keepsIds = load == GenLoad::Monsters || load == GenLoad::Gated;
    w.prepare.push_back([keepsIds](K::World& world) -> GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Gold{0});
        world.SetResource(TheSave());
        world.SetResource(LoadGate{});
        world.SetResource(GYO::Test::TestRangeAllotter::Of(keepsIds ? TheSave().next : 1, kLast));
        return {};
    });
    O::ProductDescription d;
    d.name = "rpg";
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units().Take());
    ORCH_REQUIRE_BUILT(built);
    return std::move(*built);
}

void GenFrame(O::Product& p) {
    auto ran = p.RunFrame();
    REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
}

K::EntityId IdOfHp(K::World& w, int hp) {
    for (K::EntityId id : w.Keys<Player>()) {
        if (w.Get<Player>(id)->hp == hp) return id;
    }
    return 0;
}

} // namespace

TEST_CASE("restore: renumbering while generating: fresh ids from the allotter, the saved order, the reference rewritten, gold through ResMut") {
    Log().clear();
    auto p = GenLoadProduct(GenLoad::Renumber);
    GenFrame(*p);
    REQUIRE_FALSE(p->Generating("Main"));
    K::World& w = p->World("Main").GetWorld();
    // The saved ids 12, 57, 58 (hp 5, 7, 9); the lower bound stays at 1 (renumbering does not move it).
    const K::EntityId n12 = IdOfHp(w, 5);
    const K::EntityId n57 = IdOfHp(w, 7);
    const K::EntityId n58 = IdOfHp(w, 9);
    std::vector<K::EntityId> ids{n12, n57, n58};
    std::sort(ids.begin(), ids.end());
    CHECK_MESSAGE((ids == std::vector<K::EntityId>{1, 2, 3}), "[or-renumber-fresh] " << n12 << " " << n57 << " " << n58);
    CHECK_MESSAGE((n12 < n57 && n57 < n58), "[or-renumber-order] " << n12 << " " << n57 << " " << n58);
    const ChaseTarget* chase = w.Get<ChaseTarget>(n57);
    CHECK_MESSAGE((chase != nullptr && chase->target == n12), "[or-renumber-ref] " << (chase == nullptr ? 0 : chase->target));
    CHECK_MESSAGE(GoldOf(*p) == 120, "[or-gen-resmut-gold] " << GoldOf(*p));
    CHECK(w.RejectedSpawns() == 0);
}

TEST_CASE("restore: hooks while generating: one OnAdd per put-back cell, and an OnAdd that spawns a blood bar works") {
    Log().clear();
    auto p = GenLoadProduct(GenLoad::Monsters);
    GenFrame(*p);
    REQUIRE_FALSE(p->Generating("Main"));
    K::World& w = p->World("Main").GetWorld();
    for (K::EntityId id : {12u, 57u, 58u}) {
        CHECK_MESSAGE((CountLogged("add Monster " + std::to_string(id)) == 1 && CountLogged("add Pos " + std::to_string(id)) == 1),
                      "[or-restore-onadd-once] " << id << ": " << Joined());
    }
    const std::vector<K::EntityId> bars = w.Keys<BloodBar>();
    std::vector<K::EntityId> owners;
    for (K::EntityId bar : bars) owners.push_back(w.Get<BloodBar>(bar)->owner);
    std::sort(owners.begin(), owners.end());
    CHECK_MESSAGE((owners == std::vector<K::EntityId>{12, 57, 58}), "[or-restore-hook-spawn] " << bars.size() << " bars");
    CHECK(w.RejectedSpawns() == 0);
    CHECK(w.State() != K::WorldState::Interrupted);
}

TEST_CASE("restore: Added after a load: a reader that ran before the load sees the put-back cells once") {
    Log().clear();
    auto p = GenLoadProduct(GenLoad::Gated);
    GenFrame(*p);  // waits (holds); the reader sees nothing
    GenFrame(*p);  // puts back; the spawns apply at the sync point after Update, after the reader
    REQUIRE(Log() == std::vector<std::string>{"added 0", "added 0"});
    GenFrame(*p);
    CHECK_MESSAGE(Log().back() == "added 3", "[or-restore-added-seen] " << Joined());
    GenFrame(*p);
    CHECK_MESSAGE(Log().back() == "added 0", "[or-restore-added-once] " << Joined());
}

TEST_CASE("restore: one System spawns a marker entity then inserts Pos into it; Pos is there with its value") {
    auto p = GenLoadProduct(GenLoad::SpawnInsert);
    GenFrame(*p);
    K::World& w = p->World("Main").GetWorld();
    const std::vector<K::EntityId> marked = w.Keys<Marker>();
    REQUIRE(marked.size() == 1);
    const Pos* pos = w.Get<Pos>(marked.front());
    CHECK_MESSAGE((pos != nullptr && pos->x == 42), "[or-restore-spawn-insert] " << (pos == nullptr ? -1 : pos->x));
}
