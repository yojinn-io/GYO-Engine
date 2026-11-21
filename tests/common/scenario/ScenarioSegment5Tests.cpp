// Scenario test segment 5 (authority-rollback declaration
// docs/architecture/target/v2/authority-rollback-declaration.zh-Hant.md, 9.3, written there as "segment 3";
// segments 3 and 4 went to logging and trace first): "duel", one Product of two Worlds with the time, rollback
// and authority Modules. Match is the server (server authority, no rollback); Client predicts (server authority
// and the official rollback strategy) and registers one more thing for its screen (HitFlash).
//
// Each fixed step: Move adds the step's Command.move to every Position, Damage takes Command.hit off every
// Health, Reap despawns what has no Health left, Footstep counts a footstep sound (a side effect, skipped while
// resimulating). Feed, the input's entry (authority.Ingress), writes the next step's Command from the World's
// script. Health and Position are decision data, Command the decision input, Tick marked.
//
//   S5-1  both ends' consistency fingerprints are equal; Client's HitFlash registration does not change it, a
//         decision registration does.
//   S5-2  Client predicts 6 steps without the server's hit; the digests differ.
//   S5-3  Match's decision values of step 1 go into Client's inbox: Client resimulates 5 steps and its digest
//         equals Match's; the footstep sound counted only on the first run.
//   S5-4  values equal to the history: no rollback (the default mode Check).
//   S5-5  Client predicted a kill at step 3; step 1's values keep the entity alive: it comes back (an id inside
//         the allotter's range), and OnAdd and the Add<Health> observer run after every cell is put back.
//   S5-6  a World choosing the peer-to-peer placeholder fails at startup, saying it is not implemented.
//   S5-7  a single-player World choosing nothing: no history, no authority checks.
//   S5-8  the same save read twice (a float and a std::vector, hashed by the author's function): equal summaries.

#include "engine/authority/AuthorityModule.hpp"
#include "engine/authority/Consistency.hpp"
#include "engine/authority/Registrations.hpp"
#include "engine/authority/ServerAuthorityStrategy.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

#include <doctest/doctest.h>

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace K = GYO::Kernel;
namespace O = GYO::Engine::Orchestration;
namespace A = GYO::Engine::Authority;
namespace R = GYO::Engine::Rollback;
namespace W = GYO::Engine::WorldTime;

constexpr W::Nanoseconds kStep = 15'625'000;
constexpr K::EntityId kPlayer1 = 1001;
constexpr K::EntityId kPlayer2 = 1002;

struct Health {
    int hp = 0;
    friend bool operator==(const Health&, const Health&) = default;
};
void HashValue(K::Hasher& h, const Health& v) { HashValue(h, v.hp); }
struct Position {
    int x = 0;
    friend bool operator==(const Position&, const Position&) = default;
};
void HashValue(K::Hasher& h, const Position& v) { HashValue(h, v.x); }
struct Command {
    int move = 0;
    int hit = 0;
    friend bool operator==(const Command&, const Command&) = default;
};
void HashValue(K::Hasher& h, const Command& v) {
    HashValue(h, v.move);
    HashValue(h, v.hit);
}
// Client's screen only.
struct HitFlash {
    int frames = 0;
};
void HashValue(K::Hasher& h, const HitFlash& v) { HashValue(h, v.frames); }
// The World's input script (the test's, indexed by tick) and the footstep count (a side effect).
struct Script {
    std::array<Command, 16> steps{};
};
void HashValue(K::Hasher&, const Script&) {}
struct Footsteps {
    int played = 0;
};
void HashValue(K::Hasher& h, const Footsteps& v) { HashValue(h, v.played); }

// ---- The game's Systems

K::SystemResult Move(K::Query<K::Write<Position>> positions, K::Res<Command> command) {
    const int move = command->move;
    positions.ForEach([move](K::EntityId, K::Mut<Position> p) { p->x += move; });
    return {};
}
K::SystemResult Damage(K::Query<K::Write<Health>> health, K::Res<Command> command) {
    const int hit = command->hit;
    health.ForEach([hit](K::EntityId, K::Mut<Health> h) { h->hp -= hit; });
    return {};
}
K::SystemResult Reap(K::Query<K::Read<Health>> health, K::Despawn despawn) {
    for (const K::EntityId entity : health.Entities()) {
        if (health.Get<Health>(entity)->hp <= 0) despawn.Entity(entity);
    }
    return {};
}
K::SystemResult Footstep(K::ResMut<Footsteps> footsteps) {
    ++footsteps->played;
    return {};
}
K::SystemResult Feed(K::ResMut<Command> command, K::Res<Script> script, K::Res<W::Tick> tick) {
    *command = script->steps[(tick->value + 1) % script->steps.size()];
    return {};
}

// S5-5: what the hook and the observer saw of the entity when its Health came back.
std::vector<std::string>& Seen() {
    static std::vector<std::string> seen;
    return seen;
}
K::SystemResult HealthBack(K::HookCtx ctx, K::Query<K::Read<Position>> positions) {
    Seen().push_back(std::string("hook:") + (positions.Get<Position>(ctx.Entity()) != nullptr ? "position" : "none"));
    return {};
}
K::SystemResult HealthAddedObserver(K::On<K::Add<Health>> on, K::Query<K::Read<Position>> positions) {
    const bool whole = on.Target() && positions.Get<Position>(*on.Target()) != nullptr;
    Seen().push_back(std::string("observer:") + (whole ? "position" : "none"));
    return {};
}

K::SystemOptions In(std::string name, std::string schedule, std::string stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    o.mounts.push_back(K::Mount{std::move(schedule), std::move(stage), {}, {}, {}, {}});
    return o;
}

// The game's registration, the same at both ends but for what the variant says.
struct Variant {
    bool client = false;
    bool hitFlash = false;
    std::string hitFlashName = "HitFlash";
    bool shield = false;
    bool hooks = false;
};

struct Shield {
    int s = 0;
    friend bool operator==(const Shield&, const Shield&) = default;
};
void HashValue(K::Hasher& h, const Shield& v) { HashValue(h, v.s); }

void Register(O::WorldSetup& s, const Variant& v) {
    A::AuthorityEntry authority(s);
    authority.Component<Health>("Health");
    authority.Component<Position>("Position");
    authority.Resource<Command>("Command", {.input = true});
    authority.Mark<W::Tick>();
    if (v.shield) authority.Component<Shield>("Shield");
    R::RollbackEntry rollback(s);
    rollback.Input<Command>(64);
    rollback.StepSchedule("FixedUpdate", "Last");
    s.Registrar().Resource<Script>({.name = "Script"});
    s.Registrar().Resource<Footsteps>({.name = "Footsteps"});
    s.Registrar().Resource<K::RangeAllotter>({.name = "Allotter"});
    s.Registrar().Allotter<K::RangeAllotter>();
    rollback.Resource<K::RangeAllotter>({.equal = [](const K::RangeAllotter& a, const K::RangeAllotter& b) {
        return a.first == b.first && a.last == b.last && a.next == b.next && a.exhausted == b.exhausted;
    }});
    if (v.hitFlash) s.Registrar().Component<HitFlash>({.name = v.hitFlashName});
    s.Registrar().AddSystem<&Move>(In("Move", "FixedUpdate", "Update"));
    K::SystemOptions damage = In("Damage", "FixedUpdate", "Update");
    damage.mounts[0].after = {"Move"};
    s.Registrar().AddSystem<&Damage>(damage);
    s.Registrar().AddSystem<&Reap>(In("Reap", "FixedUpdate", "PostUpdate"));
    K::SystemOptions footstep = In("Footstep", "FixedUpdate", "PostUpdate");
    rollback.SkipDuringResimulation(footstep);
    s.Registrar().AddSystem<&Footstep>(footstep);
    authority.Ingress<&Feed>(In("Feed", "Main", "PreUpdate"));
    if (v.hooks) {
        s.Registrar().Hook<Health, &HealthBack>(K::HookTrigger::OnAdd, {.name = "HealthBack"});
        s.Registrar().Observer<K::Add<Health>, &HealthAddedObserver>({.name = "HealthAddedObserver"});
    }
}

O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}
O::RegisteredUnits Units() {
    O::RegisteredUnits units;
    units.rows.push_back(ModuleRow("world_time"));
    units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
    units.rows.push_back(ModuleRow("rollback", {"world_time"}));
    units.modules.push_back({"rollback", std::make_unique<R::RollbackModule>()});
    units.rows.push_back(ModuleRow("authority", {"world_time", "rollback"}));
    units.modules.push_back({"authority", std::make_unique<A::AuthorityModule>()});
    return units;
}

O::WorldDescription World(std::string name, Variant v, std::shared_ptr<A::AuthorityStrategy> authority) {
    O::WorldDescription w;
    w.name = std::move(name);
    w.dynamicInsert = false;
    if (authority) w.options.Set(A::AuthorityOptions{std::move(authority)});
    if (v.client) w.options.Set(R::RollbackOptions{std::make_shared<R::StateRollbackStrategy>()});
    w.build = [v](O::WorldSetup& s) { Register(s, v); };
    return w;
}

std::string Reported(const auto& built) {
    if (built) return "built";
    std::string out;
    for (const O::UnitReportedError& error : built.error().unitErrors) out += error.code + " " + error.message + "\n";
    return out.empty() ? built.error().detail : out;
}

// One Product: Match first, then Client.
std::unique_ptr<O::Product> Duel(Variant match, Variant client) {
    client.client = true;
    O::ProductDescription d;
    d.name = "duel";
    d.worlds.push_back(World("Match", match, std::make_shared<A::ServerAuthorityStrategy>()));
    d.worlds.push_back(World("Client", client, std::make_shared<A::ServerAuthorityStrategy>()));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    for (const char* name : {"Match", "Client"}) {
        K::World& world = (*built)->World(name).GetWorld();
        world.SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
        world.SetResource(Command{});
        world.SetResource(Script{});
        world.SetResource(Footsteps{});
        world.SetResource(K::RangeAllotter::Of(1000, 1999));
        // Two players, alive from the start, with ids inside the allotter's range (the host's Insert neither checks
        // nor moves the range, DI-KS 18): bringing one back is a restore's spawn of such an id (S5-5).
        world.Insert(kPlayer1, Health{10});
        world.Insert(kPlayer1, Position{0});
        world.Insert(kPlayer2, Health{10});
        world.Insert(kPlayer2, Position{5});
    }
    return std::move(*built);
}

K::World& WorldOf(O::Product& p, const char* name) { return p.World(name).GetWorld(); }

void Frames(O::Product& p, int count) {
    for (int i = 0; i < count; ++i) {
        auto ran = p.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
    }
}

std::uint64_t DigestOf(O::Product& p, const char* name) {
    const auto digest = A::Digest(WorldOf(p, name));
    REQUIRE_MESSAGE(static_cast<bool>(digest), (digest ? std::string() : digest.error().message));
    return *digest;
}
std::string FingerprintOf(O::Product& p, const char* name) {
    const auto fingerprint = A::ConsistencyFingerprintOf(WorldOf(p, name));
    REQUIRE_MESSAGE(static_cast<bool>(fingerprint), (fingerprint ? std::string() : fingerprint.error().message));
    return fingerprint->Text();
}

void SetScript(O::Product& p, const char* name, std::map<int, Command> steps) {
    Script& script = *WorldOf(p, name).ResourceMut<Script>();
    for (auto& [tick, command] : steps) script.steps[tick] = command;
}

// The decision values of every player of Match now, as step tick's authoritative values for Client.
void Confirm(O::Product& p, std::uint64_t tick, const std::map<K::EntityId, std::pair<Health, Position>>& values) {
    R::RollbackInbox& inbox = *WorldOf(p, "Client").ResourceMut<R::RollbackInbox>();
    for (const auto& [entity, value] : values) {
        inbox.Value<Health>(entity, tick, value.first);
        inbox.Value<Position>(entity, tick, value.second);
    }
    inbox.ConfirmedThrough(tick);
}
std::map<K::EntityId, std::pair<Health, Position>> ValuesOf(O::Product& p, const char* name) {
    std::map<K::EntityId, std::pair<Health, Position>> out;
    K::World& world = WorldOf(p, name);
    for (const K::EntityId entity : world.Keys<Health>()) out[entity] = {*world.Get<Health>(entity), *world.Get<Position>(entity)};
    return out;
}

} // namespace

TEST_CASE("scenario segment 5: S5-1 Match and Client have one consistency fingerprint; Client's screen data does not change it") {
    auto duel = Duel({}, {.hitFlash = true});
    CHECK_MESSAGE(FingerprintOf(*duel, "Match") == FingerprintOf(*duel, "Client"), "[s5-1-equal] the two ends do not connect");
    auto renamed = Duel({}, {.hitFlash = true, .hitFlashName = "HitFlashFrames"});
    CHECK_MESSAGE(FingerprintOf(*renamed, "Client") == FingerprintOf(*duel, "Client"), "[s5-1-screen] the screen's registration changed it");
    auto shielded = Duel({}, {.hitFlash = true, .shield = true});
    CHECK_MESSAGE(FingerprintOf(*shielded, "Client") != FingerprintOf(*shielded, "Match"), "[s5-1-decision] a decision registration did not");
}

TEST_CASE("scenario segment 5: S5-2 to S5-4 Client predicts, the server's step 1 comes, Client goes back and agrees") {
    auto duel = Duel({}, {.hitFlash = true});
    // The same moves at both ends; only Match knows of the hit at step 1.
    const std::map<int, Command> moves{{1, {1, 0}}, {2, {2, 0}}, {3, {1, 0}}, {4, {3, 0}}, {5, {1, 0}}, {6, {2, 0}}, {7, {1, 0}}};
    SetScript(*duel, "Match", moves);
    SetScript(*duel, "Client", moves);
    WorldOf(*duel, "Match").ResourceMut<Script>()->steps[1].hit = 3;
    Frames(*duel, 1);
    const auto step1 = ValuesOf(*duel, "Match");
    Frames(*duel, 5);
    REQUIRE(WorldOf(*duel, "Client").Resource<W::Tick>()->value == 6);
    // S5-2: the prediction left out the hit.
    CHECK_MESSAGE(DigestOf(*duel, "Match") != DigestOf(*duel, "Client"), "[s5-2-differ]");
    const int footstepsBefore = WorldOf(*duel, "Client").Resource<Footsteps>()->played;
    // S5-3: step 1 from the server; Client goes back to step 1 and runs steps 2 to 6 again (then step 7).
    Confirm(*duel, 1, step1);
    Frames(*duel, 1);
    CHECK_MESSAGE(DigestOf(*duel, "Client") == DigestOf(*duel, "Match"), "[s5-3-agree] the digests differ after the rollback");
    CHECK_MESSAGE(WorldOf(*duel, "Client").Resource<Footsteps>()->played == footstepsBefore + 1,
                  "[s5-3-footstep-once] footsteps: " << WorldOf(*duel, "Client").Resource<Footsteps>()->played << " vs "
                                                     << footstepsBefore + 1);
    // S5-4: step 7's values are what Client has: no rollback (one footstep, no resimulation).
    Confirm(*duel, 7, ValuesOf(*duel, "Match"));
    const int footstepsNow = WorldOf(*duel, "Client").Resource<Footsteps>()->played;
    const std::uint64_t lastStamp = WorldOf(*duel, "Client").Resource<R::RollbackHistory>()->lastStamp;
    Frames(*duel, 1);
    CHECK_MESSAGE(WorldOf(*duel, "Client").Resource<Footsteps>()->played == footstepsNow + 1, "[s5-4-no-rollback]");
    CHECK_MESSAGE(WorldOf(*duel, "Client").Resource<R::RollbackHistory>()->decided == std::optional<std::uint64_t>(7),
                  "[s5-4-no-rollback] decided " << lastStamp);
    CHECK_MESSAGE(DigestOf(*duel, "Client") == DigestOf(*duel, "Match"), "[s5-4-no-rollback] digests");
}

TEST_CASE("scenario segment 5: S5-5 a predicted kill undone: the entity comes back, hooks and observers see it whole") {
    auto duel = Duel({.hooks = true}, {.hitFlash = true, .hooks = true});
    // Client predicted hits of 4 at steps 1 to 3 (10 - 12: player 1 and 2 die at step 3); the server had no hit at
    // step 1 (10 - 8 = 2: alive).
    std::map<int, Command> client{{1, {0, 4}}, {2, {0, 4}}, {3, {0, 4}}};
    std::map<int, Command> match{{1, {0, 0}}, {2, {0, 4}}, {3, {0, 4}}};
    SetScript(*duel, "Match", match);
    SetScript(*duel, "Client", client);
    Frames(*duel, 1);
    const auto step1 = ValuesOf(*duel, "Match");
    Frames(*duel, 5);
    REQUIRE_MESSAGE(!WorldOf(*duel, "Client").Alive(kPlayer1), "Client's prediction should have killed player 1 at step 3");
    REQUIRE(WorldOf(*duel, "Match").Alive(kPlayer1));
    Seen().clear();
    // Client's own script for step 1 was a hit of 4 too; the server's step 1 says no hit, and step 1's input is
    // Client's history: the resimulation from step 2 uses Client's later inputs (4, 4): 10 - 8 = 2, alive.
    Confirm(*duel, 1, step1);
    Frames(*duel, 1);
    CHECK_MESSAGE((WorldOf(*duel, "Client").Alive(kPlayer1) && WorldOf(*duel, "Client").Alive(kPlayer2)), "[s5-5-revived]");
    CHECK_MESSAGE(WorldOf(*duel, "Client").Get<Health>(kPlayer1)->hp == WorldOf(*duel, "Match").Get<Health>(kPlayer1)->hp,
                  "[s5-5-revived] health " << WorldOf(*duel, "Client").Get<Health>(kPlayer1)->hp);
    std::string seen;
    for (const std::string& s : Seen()) seen += s + " ";
    CHECK_MESSAGE((!Seen().empty() && seen.find("none") == std::string::npos),
                  "[s5-5-whole] a hook or an observer saw the entity before its other cells came back: " << seen);
    CHECK_MESSAGE(DigestOf(*duel, "Client") == DigestOf(*duel, "Match"), "[s5-5-agree]");
}

TEST_CASE("scenario segment 5: S5-6 a World choosing the peer-to-peer placeholder does not start") {
    O::ProductDescription d;
    d.name = "fighting";
    d.worlds.push_back(World("Versus", {}, std::make_shared<A::PeerAuthorityStrategy>()));
    auto built = O::Product::Build(std::move(d), Units());
    CHECK_MESSAGE((!built && Reported(built).find("AU-E1") != std::string::npos &&
                   Reported(built).find("not implemented yet") != std::string::npos),
                  "[s5-6-placeholder] " << Reported(built));
}

TEST_CASE("scenario segment 5: S5-7 a single-player World chooses nothing: no history, no authority checks") {
    O::ProductDescription d;
    d.name = "platformer";
    O::WorldDescription w;
    w.name = "Level";
    w.build = [](O::WorldSetup& s) {
        s.Registrar().Component<Position>({.name = "Position"});
        s.Registrar().AddSystem<&Move>(In("Move", "FixedUpdate", "Update"));
        s.Registrar().Resource<Command>({.name = "Command"});
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Level").GetWorld();
    CHECK_MESSAGE(!world.Types().ResourceIdOf<R::RollbackHistory>().has_value(), "[s5-7-no-history]");
    CHECK_MESSAGE(world.Types().SystemNamed("RunRollback") == nullptr, "[s5-7-no-history] RunRollback");
    CHECK_MESSAGE((world.Resource<A::AuthorityRegistry>() != nullptr && !world.Resource<A::AuthorityRegistry>()->strategy &&
                   world.Resource<A::AuthorityRegistry>()->data.empty()),
                  "[s5-7-no-authority]");
}

namespace {

// S5-8: an author's save type with a float and a std::vector, hashed by the author's function.
struct SaveFile {
    float volume = 0;
    std::vector<int> unlocked;
};
void HashValue(K::Hasher& h, const SaveFile& v) {
    HashValue(h, v.volume);
    HashValue(h, v.unlocked);
}

SaveFile ReadSave() {
    SaveFile save;
    save.volume = 0.75f;
    save.unlocked = {3, 1, 4, 1, 5};
    return save;
}

std::string SummaryAfterLoad() {
    O::ProductDescription d;
    d.name = "saves";
    O::WorldDescription w;
    w.name = "Menu";
    w.build = [](O::WorldSetup& s) { s.Registrar().Resource<SaveFile>({.name = "SaveFile"}); };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), Reported(built));
    K::World& world = (*built)->World("Menu").GetWorld();
    world.SetResource(ReadSave());
    const auto summary = world.Summary();
    REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().Text()));
    return summary->Text();
}

} // namespace

TEST_CASE("scenario segment 5: S5-8 the same save read twice gives the same summary") {
    CHECK_MESSAGE(SummaryAfterLoad() == SummaryAfterLoad(), "[s5-8-same]");
}
