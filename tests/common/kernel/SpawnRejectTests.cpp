// Spec J-55 (F-13; batch v8-arch-gyo2-spawn-reject, declaration
// docs/architecture/target/v2/spawn-reject-declaration.zh-Hant.md section 6
// judgement 1): the sync point drops the spawn records it cannot apply.
//
// A spawn into an alive id, a second spawn of one id at one sync point and a
// SpawnWithId into the World's allotter range used to fail a GYO_ASSERT (the
// process ended, also in release). Now step 4 of every phase drops such a record
// (no entity, no bundle, no initial values, no OnAdd), appends a RejectedSpawn
// diagnostics record and counts it; it is a result, not an error, so the Run goes
// on. Of the records of one id left after the range check, those of one emitter
// keep the first and those of two or more emitters are all dropped.
//
// Every judging check carries its own message "J-55 <case>: ..." (the mutation
// list expects these). Every rig registers the schedule "Early" before "Tick",
// so a schedule name taken from the first schedule is caught.

#include "WorldTestSupport.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

struct Body { std::int32_t m; };   // zone B, Shared
struct Seen { std::int32_t s; };   // zone B, Shared: the initial value of Body
struct Label { std::int32_t l; };  // zone B, Shared
struct Signal { std::int32_t f; };   // zone B, Shared: its OnAdd hooks record spawns
struct CPos { std::int32_t x; };   // zone C

Seen SeenFromBody(const Body& body) { return Seen{body.m + 1}; }

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) out += (out.empty() ? "" : " ") + part;
    return out;
}

// Early (Frame clock) first, then the data, then Tick (Fixed clock, stages A, B, C).
void Base(Registrar& r, bool allotter = false) {
    ScheduleOptions early;
    early.name = "Early";
    early.domain = Domain::Frame;
    early.clock = true;
    early.stages = {"X"};
    r.Schedule(early);
    r.Component<Body>({.name = "Body", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Seen>({.name = "Seen", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Tag>({.name = "Tag", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Label>({.name = "Label", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Signal>({.name = "Signal", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<CPos>({.name = "CPos", .zone = Zone::C, .rollback = Rollback::Yes});
    if (allotter) {
        r.Resource<RangeAllotter>({.name = "Allotter", .zone = Zone::B});
        r.Allotter<RangeAllotter>();
    }
    ScheduleOptions tick;
    tick.name = "Tick";
    tick.domain = Domain::Fixed;
    tick.clock = true;
    tick.stages = {"A", "B", "C"};
    r.Schedule(tick);
}

SystemOptions In(std::string name, std::string stage, Zone zone = Zone::B, std::vector<std::string> after = {}) {
    return Sys(std::move(name), zone, {KernelTest::At("Tick", std::move(stage), {}, std::move(after))});
}

std::uint32_t StepOf(Rig& rig, std::string_view name) {
    const Plan* plan = rig.built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    return static_cast<std::uint32_t>(PositionOf(*plan, name));
}

// The position of the first sync point at or after position.
std::uint32_t SyncAfter(Rig& rig, std::uint32_t position) {
    const Plan* plan = rig.built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    for (std::uint32_t p = position; p < plan->steps.size(); ++p) {
        if (plan->steps[p].kind == StepKind::SyncPoint) return p;
    }
    FAIL("no sync point after step " << position);
    return 0;
}

std::uint32_t LastSync(Rig& rig) {
    const Plan* plan = rig.built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    std::uint32_t last = 0;
    for (std::uint32_t p = 0; p < plan->steps.size(); ++p) {
        if (plan->steps[p].kind == StepKind::SyncPoint) last = p;
    }
    return last;
}

bool BodyIs(World& world, EntityId id, std::int32_t value) {
    const Body* body = world.Get<Body>(id);
    return body != nullptr && body->m == value;
}

std::vector<RejectedSpawn> Rejected(World& world) { return world.Diagnostics<RejectedSpawn>(); }

// ---- Systems

SystemResult SpawnSevenEightNine(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(7, Body{70});
    spawn.New(8, Body{80});
    spawn.New(9, Body{90});
    return Ok();
}
SystemResult LogAddBody(HookCtx ctx) {
    Log().push_back("add:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult LogRemoveBody(HookCtx ctx) {
    Log().push_back("remove:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult DespawnThenSpawnSeven(Despawn despawn, SpawnWithId<Bundle<Body>> spawn) {
    if (auto r = despawn.Entity(7); !r) return r;
    spawn.New(7, Body{77});
    return Ok();
}
SystemResult FlagTwenty(SpawnWithId<Bundle<Signal>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(20, Signal{20});
    return Ok();
}
SystemResult FlagTwentyAndTwentyOne(SpawnWithId<Bundle<Signal>> spawn) {
    spawn.New(20, Signal{20});
    spawn.New(21, Signal{21});
    return Ok();
}
SystemResult HookDespawnsSeven(HookCtx, Despawn despawn) { return despawn.Entity(7); }
SystemResult SpawnSevenAtTickOne(SpawnWithId<Bundle<Body>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(7, Body{77});
    return Ok();
}
SystemResult SpawnSevenTwice(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(7, Body{71});
    spawn.New(7, Body{72});
    return Ok();
}
SystemResult SpawnNineTwice(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(9, Body{91});
    spawn.New(9, Body{92});
    return Ok();
}
SystemResult SpawnFiveThenNineTwice(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(5, Body{50});
    spawn.New(9, Body{91});
    spawn.New(9, Body{92});
    return Ok();
}
SystemResult SpawnFiveThenNine(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(5, Body{50});
    spawn.New(9, Body{91});
    return Ok();
}
SystemResult SpawnNineTag(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(9, Tag{92});
    return Ok();
}
SystemResult HookSpawnThirtyFirst(HookCtx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(30, Body{301});
    return Ok();
}
SystemResult HookSpawnThirtySecond(HookCtx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(30, Body{302});
    return Ok();
}
SystemResult HookSpawnThirtyFromEntity(HookCtx ctx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(30, Body{static_cast<std::int32_t>(ctx.Entity())});
    return Ok();
}
SystemResult SpawnManyTwice(SpawnWithId<Bundle<Body>> spawn) {
    for (EntityId id = 100; id < 164; ++id) spawn.New(id, Body{static_cast<std::int32_t>(id * 10)});
    for (EntityId id = 100; id < 164; ++id) spawn.New(id, Body{static_cast<std::int32_t>(id * 10 + 1)});
    return Ok();
}
SystemResult HookSpawnsEleven(HookCtx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(11, Body{1100});
    return Ok();
}
SystemResult SpawnElevenAtTickOne(SpawnWithId<Bundle<Body>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(11, Body{1111});
    return Ok();
}
SystemResult DespawnThenSpawnElevenAtTickOne(Despawn despawn, SpawnWithId<Bundle<Body>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    if (auto r = despawn.Entity(11); !r) return r;
    spawn.New(11, Body{1111});
    return Ok();
}
SystemResult SpawnTwelveA(SpawnWithId<Bundle<Body>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(12, Body{121});
    return Ok();
}
SystemResult SpawnTwelveB(SpawnWithId<Bundle<Body>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(12, Body{122});
    return Ok();
}
SystemResult HookSpawnsSeven(HookCtx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(7, Body{700});
    return Ok();
}
SystemResult SpawnFortyAndFlag(SpawnWithId<Bundle<Body, Signal>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(40, Body{40}, Signal{40});
    return Ok();
}
SystemResult HookSpawnsForty(HookCtx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(40, Body{4000});
    return Ok();
}
SystemResult ZoneCSpawnsSeven(SpawnWithId<Bundle<CPos>> spawn) {
    spawn.New(7, CPos{7});
    return Ok();
}
SystemResult SpawnSevenAtTickTwo(SpawnWithId<Bundle<Body>> spawn, CurrentTick tick) {
    if (tick.Tick() == 2) spawn.New(7, Body{72});
    return Ok();
}
SystemResult SpawnHundred(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(100, Body{100});
    return Ok();
}
SystemResult AllotOne(Spawn<Bundle<Body>> spawn) {
    auto id = spawn.New(Body{1});
    if (!id) return Engine::Base::Err(std::move(id).error().Wrap("AllotOne"));
    return Ok();
}
SystemResult AllotTwo(Spawn<Bundle<Body>> spawn) {
    for (int i = 0; i < 2; ++i) {
        auto id = spawn.New(Body{2});
        if (!id) return Engine::Base::Err(std::move(id).error().Wrap("AllotTwo"));
    }
    return Ok();
}
SystemResult SpawnHundredTag(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(100, Tag{100});
    return Ok();
}
SystemResult DespawnThenSpawnHundredFifty(Despawn despawn, SpawnWithId<Bundle<Body>> spawn) {
    if (auto r = despawn.Entity(150); !r) return r;
    spawn.New(150, Body{150});
    return Ok();
}
SystemResult SpawnSevenInsertAndRemove(SpawnWithId<Bundle<Body>> spawn, CommandsFor<Inserts<Tag>, Removes<Label>> commands) {
    spawn.New(7, Body{70});
    commands.Insert(7, Tag{70});
    commands.Remove<Label>(7);
    return Ok();
}
SystemResult SpawnHundredFiftyAndInsert(SpawnWithId<Bundle<Body>> spawn, CommandsFor<Inserts<Tag>, Removes<>> commands) {
    spawn.New(150, Body{150});
    commands.Insert(150, Tag{150});
    return Ok();
}
SystemResult SpawnNineThenSeven(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(9, Body{90});
    spawn.New(7, Body{70});
    return Ok();
}
SystemResult SpawnAliveByTick(SpawnWithId<Bundle<Body>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) {
        spawn.New(7, Body{70});
        spawn.New(8, Body{80});
    } else if (tick.Tick() == 2) {
        spawn.New(7, Body{71});
    }
    return Ok();
}
SystemResult SpawnSevenAlways(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(7, Body{70});
    return Ok();
}
SystemResult FailsInB() { return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "FailsInB fails")); }
SystemResult ZoneBDespawnsE(Despawn despawn) { return despawn.Entity(60); }
SystemResult ZoneCSpawnsE(SpawnWithId<Bundle<CPos>> spawn) {
    spawn.New(60, CPos{60});
    return Ok();
}

} // namespace

TEST_CASE("J-55: an alive id: the spawn is dropped, the old entity is untouched, the others spawn, a record is kept") {
    for (const bool verification : {true, false}) {
        CAPTURE(verification);
        auto rig = MakeRig(
            [](Registrar& r) {
                Base(r);
                r.InitialValue<Seen, &SeenFromBody>();
                r.AddSystem<&SpawnSevenEightNine>(In("SpawnSevenEightNine", "A"));
                r.Hook<Body, &LogAddBody>(HookTrigger::OnAdd, {.name = "LogAddBody", .zone = Zone::B});
            },
            verification);
        World& world = rig->world();
        world.Insert(7, Body{1});
        world.Insert(7, Tag{1});
        const auto bodyBefore = world.Stamps<Body>(7);
        const auto tagBefore = world.Stamps<Tag>(7);
        Log().clear();
        const auto result = rig->Run();
        REQUIRE_MESSAGE(static_cast<bool>(result), "J-55 player-7: Run ok: " << Text(result));
        CHECK(world.State() == WorldState::Ready);
        CHECK_MESSAGE(BodyIs(world, 7, 1), "J-55 player-7: 7 keeps Body 1");
        CHECK_MESSAGE((world.Get<Tag>(7) != nullptr && world.Get<Tag>(7)->t == 1), "J-55 player-7: 7 keeps Tag 1");
        CHECK_MESSAGE((world.Stamps<Body>(7) == bodyBefore && world.Stamps<Tag>(7) == tagBefore),
                      "J-55 player-7: 7's stamps are unchanged");
        CHECK_MESSAGE(world.Get<Seen>(7) == nullptr, "J-55 player-7: 7 has no initial value");
        CHECK_MESSAGE(BodyIs(world, 8, 80), "J-55 player-7: 8 spawns with Body 80");
        CHECK_MESSAGE(BodyIs(world, 9, 90), "J-55 player-7: 9 spawns with Body 90");
        CHECK_MESSAGE((world.Get<Seen>(8) != nullptr && world.Get<Seen>(9) != nullptr),
                      "J-55 player-7: 8 and 9 have their initial value");
        CHECK_MESSAGE(Join(Log()) == "add:8 add:9", "J-55 player-7: OnAdd for 8 and 9 only");
        CHECK_MESSAGE(world.RejectedSpawns() == 1, "J-55 player-7: one dropped spawn counted");
        const std::vector<RejectedSpawn> rejected = Rejected(world);
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 player-7: one record");
        const RejectedSpawn& r = rejected[0];
        CHECK_MESSAGE(r.entity == 7, "J-55 player-7: entity");
        CHECK_MESSAGE(r.reason == SpawnRejectReason::IdAlive, "J-55 player-7: reason IdAlive");
        CHECK_MESSAGE(r.emitter == "SpawnSevenEightNine", "J-55 player-7: emitter");
        CHECK_MESSAGE(!r.emitterIsHook, "J-55 player-7: emitter is not a hook");
        CHECK_MESSAGE(r.emitterZone == Zone::B, "J-55 player-7: emitter zone");
        CHECK_MESSAGE(!r.fromAllotter, "J-55 player-7: not from the allotter");
        CHECK_MESSAGE(r.schedule == "Tick", "J-55 player-7: schedule");
        const std::uint32_t step = StepOf(*rig, "SpawnSevenEightNine");
        CHECK_MESSAGE(r.step == step, "J-55 player-7: step");
        CHECK_MESSAGE(r.appliedAt == SyncAfter(*rig, step), "J-55 player-7: appliedAt");
        CHECK_MESSAGE(r.tick == 1, "J-55 player-7: tick");
        CHECK_MESSAGE(r.otherEmitter.empty(), "J-55 player-7: no other emitter");
    }
}

TEST_CASE("J-55: despawn then spawn of one id in one System is a rejoin, not dropped") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&DespawnThenSpawnSeven>(In("DespawnThenSpawnSeven", "A"));
        r.Hook<Body, &LogAddBody>(HookTrigger::OnAdd, {.name = "LogAddBody", .zone = Zone::B});
        r.Hook<Body, &LogRemoveBody>(HookTrigger::OnRemove, {.name = "LogRemoveBody", .zone = Zone::B});
    });
    rig->world().Insert(7, Body{1});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(BodyIs(rig->world(), 7, 77), "J-55 rejoin: the new 7 lives");
    CHECK(Join(Log()) == "remove:7 add:7");
    CHECK_MESSAGE(rig->world().RejectedSpawns() == 0, "J-55 rejoin: nothing dropped");
}

TEST_CASE("J-55: a despawn recorded by the previous window's hook and a spawn of this window") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
        r.Hook<Signal, &HookDespawnsSeven>(HookTrigger::OnAdd, {.name = "HookDespawnsSeven", .zone = Zone::B});
        r.AddSystem<&SpawnSevenAtTickOne>(In("SpawnSevenAtTickOne", "B"));
    });
    rig->world().Insert(7, Body{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(BodyIs(rig->world(), 7, 77), "J-55 hook-despawn: the new 7 lives");
    CHECK_MESSAGE(rig->world().RejectedSpawns() == 0, "J-55 hook-despawn: nothing dropped");
}

TEST_CASE("J-55: two spawns of an alive id are both dropped as IdAlive") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnSevenTwice>(In("SpawnSevenTwice", "A"));
    });
    rig->world().Insert(7, Body{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(BodyIs(rig->world(), 7, 1));
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 2, "J-55 alive-twice: two records");
    CHECK_MESSAGE((rejected[0].reason == SpawnRejectReason::IdAlive && rejected[1].reason == SpawnRejectReason::IdAlive),
                  "J-55 alive-twice: both IdAlive");
    CHECK_MESSAGE((rejected[0].otherEmitter.empty() && rejected[1].otherEmitter.empty()),
                  "J-55 alive-twice: no other emitter");
}

TEST_CASE("J-55: one emitter spawns one id twice: the first record is kept") {
    SUBCASE("two records") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnNineTwice>(In("SpawnNineTwice", "A"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(BodyIs(rig->world(), 9, 91), "J-55 one-call-twice: 9 keeps the first value");
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 one-call-twice: one record");
        CHECK_MESSAGE(rejected[0].reason == SpawnRejectReason::IdSpawnedTwice, "J-55 one-call-twice: reason IdSpawnedTwice");
        CHECK_MESSAGE(rejected[0].otherEmitter == "SpawnNineTwice", "J-55 one-call-twice: other emitter is itself");
    }
    SUBCASE("another id recorded first") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnFiveThenNineTwice>(In("SpawnFiveThenNineTwice", "A"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(BodyIs(rig->world(), 9, 91), "J-55 one-call-later: 9 keeps its first value");
        CHECK(BodyIs(rig->world(), 5, 50));
    }
}

namespace {
Fingerprint TwoEmitters(bool bodyFirst, bool& nineAlive, std::vector<RejectedSpawn>& rejected, bool& fiveAlive) {
    auto rig = MakeRig([bodyFirst](Registrar& r) {
        Base(r);
        if (bodyFirst) {
            r.AddSystem<&SpawnFiveThenNine>(In("SpawnFiveThenNine", "A"));
            r.AddSystem<&SpawnNineTag>(In("SpawnNineTag", "A"));
        } else {
            r.AddSystem<&SpawnNineTag>(In("SpawnNineTag", "A"));
            r.AddSystem<&SpawnFiveThenNine>(In("SpawnFiveThenNine", "A"));
        }
    });
    const auto result = rig->Run();
    REQUIRE_MESSAGE(static_cast<bool>(result), Text(result));
    nineAlive = rig->world().Alive(9);
    fiveAlive = BodyIs(rig->world(), 5, 50);
    rejected = Rejected(rig->world());
    return rig->world().Summary();
}
} // namespace

TEST_CASE("J-55: two emitters spawn one id: both records are dropped, whatever the registration order") {
    bool nineAlive = true;
    bool fiveAlive = false;
    std::vector<RejectedSpawn> rejected;
    const Fingerprint first = TwoEmitters(true, nineAlive, rejected, fiveAlive);
    CHECK_MESSAGE(!nineAlive, "J-55 two-emitters: 9 is not spawned");
    CHECK_MESSAGE(fiveAlive, "J-55 two-emitters: 5 spawns");
    REQUIRE_MESSAGE(rejected.size() == 2, "J-55 two-emitters: two records");
    CHECK_MESSAGE((rejected[0].reason == SpawnRejectReason::IdSpawnedTwice && rejected[1].reason == SpawnRejectReason::IdSpawnedTwice),
                  "J-55 two-emitters: both IdSpawnedTwice");
    // Records in (id, planPos, recSeq) order: the System registered first is first in the Plan
    // (they do not conflict), although its record of 9 is its second and the other's its first.
    CHECK_MESSAGE((rejected[0].emitter == "SpawnFiveThenNine" && rejected[0].otherEmitter == "SpawnNineTag" &&
                   rejected[1].emitter == "SpawnNineTag" && rejected[1].otherEmitter == "SpawnFiveThenNine"),
                  "J-55 two-emitters: records in Plan order, each names the other");
    const Fingerprint second = TwoEmitters(false, nineAlive, rejected, fiveAlive);
    CHECK_MESSAGE(first == second, "J-55 registration-order: the World is the same in both orders");
    CHECK(!nineAlive);
}

TEST_CASE("J-55: two hooks spawn one id in one phase: both are dropped; one hook called twice keeps its first") {
    SUBCASE("two hooks") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
            // One name for two hooks (hook names are not unique): the emitter is the hook, not its name.
            r.Hook<Signal, &HookSpawnThirtyFirst>(HookTrigger::OnAdd, {.name = "HookSpawnThirty", .zone = Zone::B});
            r.Hook<Signal, &HookSpawnThirtySecond>(HookTrigger::OnAdd, {.name = "HookSpawnThirty", .zone = Zone::B});
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(!rig->world().Alive(30), "J-55 two-hooks: 30 is not spawned");
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 2, "J-55 two-hooks: two records");
        CHECK((rejected[0].emitterIsHook && rejected[1].emitterIsHook));
    }
    SUBCASE("one hook, two calls") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&FlagTwentyAndTwentyOne>(In("FlagTwentyAndTwentyOne", "A"));
            r.Hook<Signal, &HookSpawnThirtyFromEntity>(HookTrigger::OnAdd, {.name = "HookSpawnThirtyFromEntity", .zone = Zone::B});
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(BodyIs(rig->world(), 30, 20), "J-55 one-hook-twice: 30 keeps the first call's value");
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 one-hook-twice: one record");
        CHECK(rejected[0].otherEmitter == "HookSpawnThirtyFromEntity");
    }
}

TEST_CASE("J-55: many ids recorded twice by one System keep their first records") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnManyTwice>(In("SpawnManyTwice", "A"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    bool firstKept = true;
    for (EntityId id = 100; id < 164; ++id) firstKept = firstKept && BodyIs(rig->world(), id, static_cast<std::int32_t>(id * 10));
    CHECK_MESSAGE(firstKept, "J-55 many: every id keeps its first record");
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 64, "J-55 many: 64 records");
    bool ascending = true;
    for (std::size_t i = 0; i < rejected.size(); ++i) ascending = ascending && rejected[i].entity == 100 + i;
    CHECK_MESSAGE(ascending, "J-55 many: records in ascending id");
    CHECK_MESSAGE(rig->world().RejectedSpawns() == 64, "J-55 many: 64 counted");
}

TEST_CASE("J-55: across the two phases of one sync point") {
    SUBCASE("the hook queue's spawn is kept; the window's is dropped as spawned twice") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
            r.Hook<Signal, &HookSpawnsEleven>(HookTrigger::OnAdd, {.name = "HookSpawnsEleven", .zone = Zone::B});
            r.AddSystem<&SpawnElevenAtTickOne>(In("SpawnElevenAtTickOne", "B"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(BodyIs(rig->world(), 11, 1100), "J-55 cross-phase: 11 has the hook's value");
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 cross-phase: one record");
        CHECK_MESSAGE(rejected[0].reason == SpawnRejectReason::IdSpawnedTwice, "J-55 cross-phase: reason IdSpawnedTwice");
        CHECK(rejected[0].emitter == "SpawnElevenAtTickOne");
        CHECK_MESSAGE(rejected[0].otherEmitter == "HookSpawnsEleven", "J-55 cross-phase: other emitter is the hook");
    }
    SUBCASE("the window despawns then spawns it again: spawned") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
            r.Hook<Signal, &HookSpawnsEleven>(HookTrigger::OnAdd, {.name = "HookSpawnsEleven", .zone = Zone::B});
            r.AddSystem<&DespawnThenSpawnElevenAtTickOne>(In("DespawnThenSpawnElevenAtTickOne", "B"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(BodyIs(rig->world(), 11, 1111), "J-55 cross-phase-respawn: 11 has the window's value");
        CHECK_MESSAGE(rig->world().RejectedSpawns() == 0, "J-55 cross-phase-respawn: nothing dropped");
    }
}

TEST_CASE("J-55: an id spawned at the previous sync point is alive, not spawned twice") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnTwelveA>(In("SpawnTwelveA", "A"));
        r.AddSystem<&SpawnTwelveB>(In("SpawnTwelveB", "B"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(BodyIs(rig->world(), 12, 121));
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 1, "J-55 previous-sync: one record");
    CHECK_MESSAGE(rejected[0].reason == SpawnRejectReason::IdAlive, "J-55 previous-sync: reason IdAlive");
}

TEST_CASE("J-55: a hook's spawn dropped at the next sync point and in a hook round") {
    SUBCASE("a hook triggered at a middle sync point") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
            r.Hook<Signal, &HookSpawnsSeven>(HookTrigger::OnAdd, {.name = "HookSpawnsSeven", .zone = Zone::B});
        });
        rig->world().Insert(7, Body{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(BodyIs(rig->world(), 7, 1));
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 mid-hook: one record");
        const RejectedSpawn& r = rejected[0];
        CHECK_MESSAGE(r.emitterIsHook, "J-55 mid-hook: emitter is a hook");
        CHECK_MESSAGE(r.emitter == "HookSpawnsSeven", "J-55 mid-hook: emitter is the hook's name");
        const std::uint32_t triggered = SyncAfter(*rig, StepOf(*rig, "FlagTwenty"));
        CHECK_MESSAGE(r.step == triggered, "J-55 mid-hook: step is the sync point that triggered it");
        CHECK_MESSAGE(r.appliedAt == SyncAfter(*rig, triggered + 1), "J-55 mid-hook: appliedAt is the next sync point");
        CHECK_MESSAGE(r.reason == SpawnRejectReason::IdAlive, "J-55 mid-hook: reason IdAlive");
    }
    SUBCASE("a hook round after the last sync point") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnFortyAndFlag>(In("SpawnFortyAndFlag", "C"));
            r.Hook<Signal, &HookSpawnsForty>(HookTrigger::OnAdd, {.name = "HookSpawnsForty", .zone = Zone::B});
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(BodyIs(rig->world(), 40, 40));
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 round-hook: one record");
        const RejectedSpawn& r = rejected[0];
        CHECK_MESSAGE(r.reason == SpawnRejectReason::IdAlive, "J-55 round-hook: reason IdAlive");
        CHECK_MESSAGE(r.emitterIsHook, "J-55 round-hook: emitter is a hook");
        CHECK_MESSAGE((r.step == LastSync(*rig) && r.appliedAt == LastSync(*rig)),
                      "J-55 round-hook: step and appliedAt are the last sync point");
    }
}

TEST_CASE("J-55: a zone C spawn blocked by an entity with zone B data only is dropped (LIM-7)") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&ZoneCSpawnsSeven>(In("ZoneCSpawnsSeven", "A", Zone::C));
    });
    rig->world().Insert(7, Tag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(rig->world().Get<CPos>(7) == nullptr, "J-55 zone-c: 7 gets no zone C data");
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 1, "J-55 zone-c: one record");
    CHECK_MESSAGE(rejected[0].emitterZone == Zone::C, "J-55 zone-c: emitter zone C");
}

TEST_CASE("J-55: the record's tick is the schedule's domain tick") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnSevenAtTickTwo>(In("SpawnSevenAtTickTwo", "A"));
    });
    rig->world().Insert(7, Body{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 1, "J-55 second-tick: one record");
    CHECK_MESSAGE(rejected[0].tick == 2, "J-55 second-tick: tick 2");
}

TEST_CASE("J-55: the allotter's range") {
    SUBCASE("in the range comes before alive") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r, true);
            r.AddSystem<&SpawnHundred>(In("SpawnHundred", "A"));
        });
        rig->world().SetResource(RangeAllotter::Of(100, 101));
        rig->world().Insert(100, Tag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 range-first: one record");
        CHECK_MESSAGE(rejected[0].reason == SpawnRejectReason::IdInAllotterRange, "J-55 range-first: reason IdInAllotterRange");
    }
    SUBCASE("in the range comes before spawned twice") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r, true);
            r.AddSystem<&AllotOne>(In("AllotOne", "A"));
            r.AddSystem<&SpawnHundredTag>(In("SpawnHundredTag", "A"));
        });
        rig->world().SetResource(RangeAllotter::Of(100, 101));
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(BodyIs(rig->world(), 100, 1), "J-55 range-before-twice: 100 has the allotter's spawn");
        CHECK(rig->world().Get<Tag>(100) == nullptr);
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 range-before-twice: one record");
        CHECK_MESSAGE(rejected[0].reason == SpawnRejectReason::IdInAllotterRange, "J-55 range-before-twice: reason IdInAllotterRange");
    }
    SUBCASE("an allotter whose resource is absent is not checked") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r, true);
            r.AddSystem<&SpawnHundred>(In("SpawnHundred", "A"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(BodyIs(rig->world(), 100, 100), "J-55 range-absent: 100 spawns");
        CHECK(rig->world().RejectedSpawns() == 0);
    }
    SUBCASE("despawn then spawn in the range: the despawn applies, the spawn is dropped") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r, true);
            r.AddSystem<&DespawnThenSpawnHundredFifty>(In("DespawnThenSpawnHundredFifty", "A"));
        });
        rig->world().SetResource(RangeAllotter::Of(100, 199));
        rig->world().Insert(150, Tag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(!rig->world().Alive(150), "J-55 range-respawn: 150 is gone");
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE_MESSAGE(rejected.size() == 1, "J-55 range-respawn: one record");
        CHECK(rejected[0].reason == SpawnRejectReason::IdInAllotterRange);
    }
    SUBCASE("an insert into a dropped id that is not alive is dropped too") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r, true);
            r.AddSystem<&SpawnHundredFiftyAndInsert>(In("SpawnHundredFiftyAndInsert", "A"));
        });
        rig->world().SetResource(RangeAllotter::Of(100, 199));
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(!rig->world().Alive(150));
        CHECK_MESSAGE(rig->world().DroppedMissingEntity() == 1, "J-55 range-insert: the insert is dropped and counted");
    }
}

// Its own case, so a failure of another range case cannot stop it before its check.
TEST_CASE("J-55: only SpawnWithId is checked against the allotter's range") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r, true);
        r.AddSystem<&AllotTwo>(In("AllotTwo", "A"));
    });
    rig->world().SetResource(RangeAllotter::Of(100, 101));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE((BodyIs(rig->world(), 100, 2) && BodyIs(rig->world(), 101, 2)), "J-55 range-spawn: 100 and 101 spawn");
    CHECK(rig->world().RejectedSpawns() == 0);
}

TEST_CASE("J-55: an allotter id that collides with an alive entity is dropped with the allotter mark") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r, true);
        r.AddSystem<&AllotOne>(In("AllotOne", "A"));
    });
    rig->world().SetResource(RangeAllotter::Of(100, 101));
    rig->world().Insert(100, Tag{1});
    const auto result = rig->Run();
    REQUIRE_MESSAGE(static_cast<bool>(result), "J-55 allotter-collision: Run ok: " << Text(result));
    CHECK(rig->world().Get<Body>(100) == nullptr);
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 1, "J-55 allotter-collision: one record");
    CHECK_MESSAGE(rejected[0].fromAllotter, "J-55 allotter-collision: from the allotter");
    CHECK_MESSAGE(rejected[0].reason == SpawnRejectReason::IdAlive, "J-55 allotter-collision: reason IdAlive");
}

TEST_CASE("J-55: inserts and removes of the same call land on the alive entity") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnSevenInsertAndRemove>(In("SpawnSevenInsertAndRemove", "A"));
    });
    rig->world().Insert(7, Body{1});
    rig->world().Insert(7, Tag{1});
    rig->world().Insert(7, Label{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(BodyIs(rig->world(), 7, 1));
    CHECK_MESSAGE((rig->world().Get<Tag>(7) != nullptr && rig->world().Get<Tag>(7)->t == 70),
                  "J-55 same-call: the insert lands on the alive 7");
    CHECK_MESSAGE(rig->world().Get<Label>(7) == nullptr, "J-55 same-call: the remove lands on the alive 7");
}

TEST_CASE("J-55: the records, the count and the World's states") {
    SUBCASE("records in (id, planPos, recSeq) order") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnNineThenSeven>(In("SpawnNineThenSeven", "A"));
        });
        rig->world().Insert(7, Body{1});
        rig->world().Insert(9, Body{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
        REQUIRE(rejected.size() == 2);
        CHECK_MESSAGE((rejected[0].entity == 7 && rejected[1].entity == 9), "J-55 order: 7 before 9");
    }
    SUBCASE("TakeDiagnostics empties the records, not the count") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnAliveByTick>(In("SpawnAliveByTick", "A"));
        });
        rig->world().Insert(7, Body{1});
        rig->world().Insert(8, Body{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(rig->world().TakeDiagnostics<RejectedSpawn>().size() == 2, "J-55 take: two records taken");
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Rejected(rig->world()).size() == 1, "J-55 take: one record after the take");
        CHECK_MESSAGE(rig->world().RejectedSpawns() == 3, "J-55 take: the count is 3");
    }
    SUBCASE("an Interrupted World keeps them") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnSevenAlways>(In("SpawnSevenAlways", "A"));
            r.AddSystem<&FailsInB>(In("FailsInB", "B"));
        });
        rig->world().Insert(7, Body{1});
        const auto result = rig->Run();
        CHECK(!result);
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK_MESSAGE(Rejected(rig->world()).size() == 1, "J-55 interrupted: the record is kept");
        CHECK_MESSAGE(rig->world().RejectedSpawns() == 1, "J-55 interrupted: the count is kept");
    }
    SUBCASE("no break hook call") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnSevenAlways>(In("SpawnSevenAlways", "A"));
        });
        rig->world().SetResource(KernelSettings{true, true});
        int breaks = 0;
        rig->world().SetBreakHook([&breaks](const KernelError&) { ++breaks; });
        rig->world().Insert(7, Body{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(rig->world().RejectedSpawns() == 1);
        CHECK_MESSAGE(breaks == 0, "J-55 no-break: the break hook is not called");
    }
}

TEST_CASE("J-55: a zone B despawn and a zone C spawn of one id are still Z5") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&ZoneBDespawnsE>(In("ZoneBDespawnsE", "A"));
        r.AddSystem<&ZoneCSpawnsE>(In("ZoneCSpawnsE", "A", Zone::C));
    });
    rig->world().Insert(60, Tag{1});
    const auto result = rig->Run();
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::ZoneCDespawn), "J-55 z5: Z5 as before");
    CHECK(rig->world().State() == WorldState::Interrupted);
    CHECK(rig->world().Alive(60));
    CHECK(rig->world().RejectedSpawns() == 0);
}

TEST_CASE("J-55: a dropped spawn takes no stamp") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnSevenAtTickOne>(In("SpawnSevenAtTickOne", "A"));
    });
    rig->world().Insert(7, Body{1});
    const ChangeStamp stamp0 = rig->world().Stamp();
    const std::uint64_t epoch0 = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const ChangeStamp stamp1 = rig->world().Stamp();
    const std::uint64_t epoch1 = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().RejectedSpawns() == 1);
    CHECK_MESSAGE((stamp1 - stamp0 == rig->world().Stamp() - stamp1 && epoch1 - epoch0 == rig->world().SyncEpoch() - epoch1),
                  "J-55 no-stamp: the same stamp and sync epoch steps with and without a dropped spawn");
}

TEST_CASE("J-55: the fingerprint versions") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnSevenAlways>(In("SpawnSevenAlways", "A"));
    });
    const FrozenRegistry& frozen = rig->built.registry->Frozen();
    CHECK_MESSAGE(frozen.PlanOf("Tick")->fingerprint.version == 3U, "J-55 versions: Plan fingerprint version 3");
    CHECK_MESSAGE(frozen.CFingerprint().version == 5U, "J-55 versions: C fingerprint version 5");
    CHECK_MESSAGE(frozen.RegistryFingerprint().version == 2U, "J-55 versions: Registry fingerprint version 2");
}
