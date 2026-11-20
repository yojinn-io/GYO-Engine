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

// Early first, then the data (with TickCount, the Runs of Tick the test counted), then Tick (stages A, B, C).
void Base(Registrar& r, bool allotter = false) {
    ScheduleOptions early;
    early.name = "Early";
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
    r.Resource<TickCount>({.name = "TickCount", .zone = Zone::C, .role = Role::Input});
    ScheduleOptions tick;
    tick.name = "Tick";
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
    despawn.Entity(7);
    spawn.New(7, Body{77});
    return Ok();
}
SystemResult FlagTwenty(SpawnWithId<Bundle<Signal>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(20, Signal{20});
    return Ok();
}
SystemResult FlagTwentyAndTwentyOne(SpawnWithId<Bundle<Signal>> spawn) {
    spawn.New(20, Signal{20});
    spawn.New(21, Signal{21});
    return Ok();
}
SystemResult HookDespawnsSeven(HookCtx, Despawn despawn) {
    despawn.Entity(7);
    return Ok();
}
SystemResult SpawnSevenAtTickOne(SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(7, Body{77});
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
SystemResult SpawnElevenAtTickOne(SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(11, Body{1111});
    return Ok();
}
SystemResult DespawnThenSpawnElevenAtTickOne(Despawn despawn, SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value != 1) return Ok();
    despawn.Entity(11);
    spawn.New(11, Body{1111});
    return Ok();
}
SystemResult SpawnTwelveA(SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(12, Body{121});
    return Ok();
}
SystemResult SpawnTwelveB(SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(12, Body{122});
    return Ok();
}
SystemResult HookSpawnsSeven(HookCtx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(7, Body{700});
    return Ok();
}
SystemResult SpawnFortyAndFlag(SpawnWithId<Bundle<Body, Signal>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(40, Body{40}, Signal{40});
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
SystemResult SpawnSevenAtTickTwo(SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value == 2) spawn.New(7, Body{72});
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
    despawn.Entity(150);
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
SystemResult SpawnAliveByTick(SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) {
        spawn.New(7, Body{70});
        spawn.New(8, Body{80});
    } else if (tick->value == 2) {
        spawn.New(7, Body{71});
    }
    return Ok();
}
SystemResult SpawnSevenAlways(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(7, Body{70});
    return Ok();
}
SystemResult FailsInB() { return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "FailsInB fails")); }
SystemResult ZoneBDespawnsE(Despawn despawn) {
    despawn.Entity(60);
    return Ok();
}
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
        const std::uint64_t epochBefore = world.SyncEpoch();
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
        // Dropped at the Run's first sync point: the sync points completed before it (L8).
        CHECK_MESSAGE(r.syncEpoch == epochBefore, "J-55 player-7: sync point number");
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
// Partly solved (v8-arch-gyo2-summary-padding-a-then-c): World::Summary hashes
// raw bytes, so every type compared here must have no padding. The full
// solution, a hash function per type, is batch 4.
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

// The record carries the World's sync point number (L8; the domain tick left with the clock, G8).
TEST_CASE("J-55: the record's sync point number is the World's when it was dropped") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&SpawnSevenAtTickTwo>(In("SpawnSevenAtTickTwo", "A"));
    });
    rig->world().Insert(7, Body{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::uint64_t epochBefore = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 1, "J-55 second-tick: one record");
    CHECK_MESSAGE(epochBefore == 3, "J-55 second-tick: the first Run applied 3 sync points (stages A, B, C)");
    CHECK_MESSAGE(rejected[0].syncEpoch == epochBefore,
                  "J-55 second-tick: sync point number " << rejected[0].syncEpoch << ", not the first one of Run 2 (" << epochBefore << ")");
}

TEST_CASE("J-55: the allotter's range: in the range comes before alive") {
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

TEST_CASE("J-55: the allotter's range: in the range comes before spawned twice") {
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

TEST_CASE("J-55: the allotter's range: an allotter whose resource is absent is not checked") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r, true);
        r.AddSystem<&SpawnHundred>(In("SpawnHundred", "A"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(BodyIs(rig->world(), 100, 100), "J-55 range-absent: 100 spawns");
    CHECK(rig->world().RejectedSpawns() == 0);
}

TEST_CASE("J-55: the allotter's range: despawn then spawn in the range: the despawn applies, the spawn is dropped") {
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

TEST_CASE("J-55: the allotter's range: an insert into a dropped id that is not alive is dropped too") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r, true);
        r.AddSystem<&SpawnHundredFiftyAndInsert>(In("SpawnHundredFiftyAndInsert", "A"));
    });
    rig->world().SetResource(RangeAllotter::Of(100, 199));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(!rig->world().Alive(150));
    CHECK_MESSAGE(rig->world().DroppedMissingEntity() == 1, "J-55 range-insert: the insert is dropped and counted");
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

// Z5 went with the run-time interception (v8-arch-gyo2-no-runtime-interception):
// the despawn (step 2) applies before the spawn (step 4), so 60 comes back new.
TEST_CASE("J-55: a zone B despawn and a zone C spawn of one id: the despawn applies first, then the spawn") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&ZoneBDespawnsE>(In("ZoneBDespawnsE", "A"));
        r.AddSystem<&ZoneCSpawnsE>(In("ZoneCSpawnsE", "A", Zone::C));
    });
    rig->world().Insert(60, Tag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().State() == WorldState::Ready);
    REQUIRE_MESSAGE(rig->world().Alive(60), "J-55 despawn-then-spawn: 60 is spawned again");
    REQUIRE(rig->world().Get<CPos>(60) != nullptr);
    CHECK(rig->world().Get<CPos>(60)->x == 60);
    CHECK_MESSAGE(rig->world().Get<Tag>(60) == nullptr, "J-55 despawn-then-spawn: the old entity's Tag is gone");
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
    CHECK_MESSAGE(frozen.PlanOf("Tick")->fingerprint.version == 8U, "J-55 versions: Plan fingerprint version 8");
    CHECK_MESSAGE(frozen.CFingerprint().version == 11U, "J-55 versions: C fingerprint version 11");
    CHECK_MESSAGE(frozen.RegistryFingerprint().version == 7U, "J-55 versions: Registry fingerprint version 7");
}

// ---- Cases added after the adversarial check of the mutant list (Fable, v8-arch-gyo2-spawn-reject)

namespace {

struct CVel { std::int32_t v; };  // zone C (registered by the X-55 rigs that need it)

SystemResult FlagSignalLabelSignal(SpawnWithId<Bundle<Signal>> signal, SpawnWithId<Bundle<Label>> label, Res<TickCount> tick) {
    if (tick->value != 1) return Ok();
    signal.New(20, Signal{20});
    label.New(21, Label{21});
    signal.New(22, Signal{22});
    return Ok();
}
SystemResult HookASpawnsThirty(HookCtx ctx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(30, Body{static_cast<std::int32_t>(ctx.Entity())});
    return Ok();
}
SystemResult HookBSpawnsThirty(HookCtx ctx, SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(30, Body{static_cast<std::int32_t>(ctx.Entity()) + 1000});
    return Ok();
}
SystemResult EarlySpawnsSeven(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(7, Body{70});
    return Ok();
}
SystemResult SpawnSevenAndElevenAtTickOne(SpawnWithId<Bundle<Body>> spawn, Res<TickCount> tick) {
    if (tick->value != 1) return Ok();
    spawn.New(7, Body{77});
    spawn.New(11, Body{1111});
    return Ok();
}
SystemResult SpawnElevenTagAtTickOne(SpawnWithId<Bundle<Tag>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(11, Tag{11});
    return Ok();
}
SystemResult ZoneCSpawnsNinePos(SpawnWithId<Bundle<CPos>> spawn) {
    spawn.New(9, CPos{9});
    return Ok();
}
SystemResult ZoneCSpawnsNineVel(SpawnWithId<Bundle<CVel>> spawn) {
    spawn.New(9, CVel{9});
    return Ok();
}
// Zone C and Shared, so two spawners of one bundle are not also two writers of an
// Exclusive column (D-E2 would fail Finish and hide the tie).
struct CSharedPos { std::int32_t x; };
SystemResult ZoneCSpawnsNineShared(SpawnWithId<Bundle<CSharedPos>> spawn) {
    spawn.New(9, CSharedPos{9});
    return Ok();
}
SystemResult ZoneCSpawnsNineSharedAgain(SpawnWithId<Bundle<CSharedPos>> spawn) {
    spawn.New(9, CSharedPos{10});
    return Ok();
}

// Partly solved (v8-arch-gyo2-summary-padding-a-then-c): World::Summary hashes
// raw bytes, so every type compared here must have no padding. The full
// solution, a hash function per type, is batch 4.
Fingerprint ThreeRecords(bool twiceFirst, bool& nineAlive, std::vector<RejectedSpawn>& rejected) {
    auto rig = MakeRig([twiceFirst](Registrar& r) {
        Base(r);
        if (twiceFirst) {
            r.AddSystem<&SpawnNineTwice>(In("SpawnNineTwice", "A"));
            r.AddSystem<&SpawnNineTag>(In("SpawnNineTag", "A"));
        } else {
            r.AddSystem<&SpawnNineTag>(In("SpawnNineTag", "A"));
            r.AddSystem<&SpawnNineTwice>(In("SpawnNineTwice", "A"));
        }
    });
    const auto result = rig->Run();
    REQUIRE_MESSAGE(static_cast<bool>(result), Text(result));
    nineAlive = rig->world().Alive(9);
    rejected = Rejected(rig->world());
    return rig->world().Summary();
}

} // namespace

TEST_CASE("J-55: a spawn dropped by step 0 as IdAlive does not count as spawned at this sync point") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
        r.Hook<Signal, &HookSpawnsSeven>(HookTrigger::OnAdd, {.name = "HookSpawnsSeven", .zone = Zone::B});
        r.AddSystem<&SpawnSevenAtTickOne>(In("SpawnSevenAtTickOne", "B"));
    });
    rig->world().Insert(7, Body{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(BodyIs(rig->world(), 7, 1));
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 2, "J-55 dropped-not-spawned: two records");
    CHECK_MESSAGE((rejected[0].emitterIsHook && rejected[1].emitter == "SpawnSevenAtTickOne"),
                  "J-55 dropped-not-spawned: the hook's record first, then the System's");
    CHECK_MESSAGE((rejected[0].reason == SpawnRejectReason::IdAlive && rejected[1].reason == SpawnRejectReason::IdAlive &&
                   rejected[1].otherEmitter.empty()),
                  "J-55 dropped-not-spawned: both IdAlive, the window's names no other emitter");
}

TEST_CASE("J-55: three records of one id from two Systems are all dropped, in both registration orders") {
    bool nineAlive = true;
    std::vector<RejectedSpawn> rejected;
    const Fingerprint first = ThreeRecords(true, nineAlive, rejected);
    CHECK_MESSAGE(!nineAlive, "J-55 three-records: 9 is not spawned (the System with two records first)");
    REQUIRE_MESSAGE(rejected.size() == 3, "J-55 three-records: three records");
    CHECK_MESSAGE((rejected[0].emitter == "SpawnNineTwice" && rejected[1].emitter == "SpawnNineTwice" && rejected[2].emitter == "SpawnNineTag"),
                  "J-55 three-records: Plan order follows the registration order");
    CHECK_MESSAGE((rejected[0].otherEmitter == "SpawnNineTag" && rejected[1].otherEmitter == "SpawnNineTag" &&
                   rejected[2].otherEmitter == "SpawnNineTwice"),
                  "J-55 three-records: each names the other emitter");
    const Fingerprint second = ThreeRecords(false, nineAlive, rejected);
    CHECK_MESSAGE(!nineAlive, "J-55 three-records: 9 is not spawned (the System with one record first)");
    REQUIRE(rejected.size() == 3);
    CHECK_MESSAGE(rejected[0].emitter == "SpawnNineTag", "J-55 three-records: the Plan order is reversed with the registration order");
    CHECK_MESSAGE(first == second, "J-55 three-records: the World is the same in both orders");
}

TEST_CASE("J-55: one hook called before and after another hook: all three records of the id are dropped") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&FlagSignalLabelSignal>(In("FlagSignalLabelSignal", "A"));
        r.Hook<Signal, &HookASpawnsThirty>(HookTrigger::OnAdd, {.name = "HookASpawnsThirty", .zone = Zone::B});
        r.Hook<Label, &HookBSpawnsThirty>(HookTrigger::OnAdd, {.name = "HookBSpawnsThirty", .zone = Zone::B});
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(!rig->world().Alive(30), "J-55 hooks-a-b-a: 30 is not spawned");
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 3, "J-55 hooks-a-b-a: three records");
    CHECK_MESSAGE((rejected[0].emitter == "HookASpawnsThirty" && rejected[1].emitter == "HookBSpawnsThirty" &&
                   rejected[2].emitter == "HookASpawnsThirty"),
                  "J-55 hooks-a-b-a: the calls are A, B, A");
}

TEST_CASE("J-55: an alive id is IdAlive although step 0 spawned a larger id at this sync point") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
        r.Hook<Signal, &HookSpawnsEleven>(HookTrigger::OnAdd, {.name = "HookSpawnsEleven", .zone = Zone::B});
        r.AddSystem<&SpawnSevenAndElevenAtTickOne>(In("SpawnSevenAndElevenAtTickOne", "B"));
    });
    rig->world().Insert(7, Body{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(BodyIs(rig->world(), 11, 1100));
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 2, "J-55 smaller-alive-id: two records");
    CHECK_MESSAGE((rejected[0].entity == 7 && rejected[0].reason == SpawnRejectReason::IdAlive && rejected[0].otherEmitter.empty()),
                  "J-55 smaller-alive-id: 7 is IdAlive with no other emitter");
    CHECK_MESSAGE((rejected[1].entity == 11 && rejected[1].reason == SpawnRejectReason::IdSpawnedTwice &&
                   rejected[1].otherEmitter == "HookSpawnsEleven"),
                  "J-55 smaller-alive-id: 11 is IdSpawnedTwice, the hook is the other emitter");
}

TEST_CASE("J-55: the record's sync point number and schedule are those of the sync point that dropped it") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&EarlySpawnsSeven>(Sys("EarlySpawnsSeven", Zone::B, {KernelTest::At("Early", "X")}));
    });
    rig->world().Insert(7, Body{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::uint64_t epochBefore = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run("Early"));
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 1, "J-55 early: one record");
    CHECK_MESSAGE(rejected[0].syncEpoch == epochBefore,
                  "J-55 early: sync point " << rejected[0].syncEpoch << ", not Early's (" << epochBefore << ")");
    CHECK_MESSAGE(rejected[0].schedule == "Early", "J-55 early: schedule Early");
}

// Order of the records of one id: a Spawn (allotter) record that collides, then a SpawnWithId in the range.
TEST_CASE("J-55: records of one id are in (planPos, recSeq) order when one is in the range and one is alive") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r, true);
        r.AddSystem<&AllotOne>(In("AllotOne", "A"));
        r.AddSystem<&SpawnHundredTag>(In("SpawnHundredTag", "A"));
    });
    rig->world().SetResource(RangeAllotter::Of(100, 101));
    rig->world().Insert(100, Label{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 2, "J-55 order-in-one-id: two records");
    CHECK_MESSAGE(StepOf(*rig, "AllotOne") < StepOf(*rig, "SpawnHundredTag"), "J-55 order-in-one-id: AllotOne is first in the Plan");
    CHECK_MESSAGE((rejected[0].emitter == "AllotOne" && rejected[0].reason == SpawnRejectReason::IdAlive &&
                   rejected[1].emitter == "SpawnHundredTag" && rejected[1].reason == SpawnRejectReason::IdInAllotterRange),
                  "J-55 order-in-one-id: the records are in (id, planPos, recSeq) order");
}

// The declared case "across two phases, the window despawns first" with the System after it.
TEST_CASE("J-55: step 0 spawned it; the window despawns and spawns it and another System spawns it too") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
        r.Hook<Signal, &HookSpawnsEleven>(HookTrigger::OnAdd, {.name = "HookSpawnsEleven", .zone = Zone::B});
        r.AddSystem<&DespawnThenSpawnElevenAtTickOne>(In("DespawnThenSpawnElevenAtTickOne", "B"));
        r.AddSystem<&SpawnElevenTagAtTickOne>(In("SpawnElevenTagAtTickOne", "B"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(!rig->world().Alive(11), "J-55 respawn-two-emitters: 11 is gone (despawned, both spawns dropped)");
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    REQUIRE_MESSAGE(rejected.size() == 2, "J-55 respawn-two-emitters: two records");
    CHECK_MESSAGE((rejected[0].reason == SpawnRejectReason::IdSpawnedTwice && rejected[0].otherEmitter == "SpawnElevenTagAtTickOne" &&
                   rejected[1].reason == SpawnRejectReason::IdSpawnedTwice && rejected[1].otherEmitter == "DespawnThenSpawnElevenAtTickOne"),
                  "J-55 respawn-two-emitters: each names the other System, not the hook");
}

// Whether the two Systems of "two emitters" really do not conflict: the same shape in zone C,
// where a conflict without a declared order is a tie listed in TieBreaks.
// Partly solved (v8-arch-gyo2-summary-padding-a-then-c): World::Summary hashes
// raw bytes, so every type compared here must have no padding. The full
// solution, a hash function per type, is batch 4.
TEST_CASE("J-55: two zone C Systems with disjoint bundles do not conflict; with one bundle they do") {
    SUBCASE("disjoint bundles: Finish succeeds in both orders, both spawns of 9 are dropped, the World is the same") {
        Fingerprint summaries[2];
        for (int order = 0; order < 2; ++order) {
            auto rig = MakeRig([order](Registrar& r) {
                Base(r);
                r.Component<CVel>({.name = "CVel", .zone = Zone::C, .rollback = Rollback::Yes});
                if (order == 0) {
                    r.AddSystem<&ZoneCSpawnsNinePos>(In("ZoneCSpawnsNinePos", "A", Zone::C));
                    r.AddSystem<&ZoneCSpawnsNineVel>(In("ZoneCSpawnsNineVel", "A", Zone::C));
                } else {
                    r.AddSystem<&ZoneCSpawnsNineVel>(In("ZoneCSpawnsNineVel", "A", Zone::C));
                    r.AddSystem<&ZoneCSpawnsNinePos>(In("ZoneCSpawnsNinePos", "A", Zone::C));
                }
            });
            CHECK_MESSAGE(rig->built.registry->Frozen().TieBreaks().empty(),
                          "J-55 zone-c-pair: disjoint bundles give no tie");
            KERNEL_REQUIRE_RUN_OK(rig->Run());
            CHECK_MESSAGE(!rig->world().Alive(9), "J-55 zone-c-pair: 9 is not spawned");
            const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
            REQUIRE(rejected.size() == 2);
            CHECK_MESSAGE(rejected[0].emitter == (order == 0 ? "ZoneCSpawnsNinePos" : "ZoneCSpawnsNineVel"),
                          "J-55 zone-c-pair: the Plan order follows the registration order");
            summaries[order] = rig->world().Summary();
        }
        CHECK_MESSAGE(summaries[0] == summaries[1], "J-55 zone-c-pair: the World is the same in both orders");
    }
    SUBCASE("one bundle: the two Systems conflict, tied by registration without a declared order") {
        Built built = Finish({{"test", [](Registrar& r) {
                                   Base(r);
                                   r.Component<CSharedPos>({.name = "CSharedPos", .zone = Zone::C,
                                                            .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
                                   r.AddSystem<&ZoneCSpawnsNineShared>(In("ZoneCSpawnsNineShared", "A", Zone::C));
                                   r.AddSystem<&ZoneCSpawnsNineSharedAgain>(In("ZoneCSpawnsNineSharedAgain", "A", Zone::C));
                               }}});
        REQUIRE_MESSAGE(static_cast<bool>(built.result), Describe(built));
        const auto ties = built.registry->Frozen().TieBreaks();
        CHECK_MESSAGE((ties.size() == 1 && ties[0].first == "ZoneCSpawnsNineShared" &&
                       ties[0].second == "ZoneCSpawnsNineSharedAgain"),
                      "J-55 zone-c-pair: two zone C Systems that spawn one bundle conflict (one tie)");
    }
}

// The registration-order sentence alone (no REQUIRE of the record count before it).
TEST_CASE("J-55: two emitters: the World is the same in both registration orders (alone)") {
    bool nineAlive = true;
    bool fiveAlive = false;
    std::vector<RejectedSpawn> rejected;
    const Fingerprint first = TwoEmitters(true, nineAlive, rejected, fiveAlive);
    const std::string firstEmitter = rejected.empty() ? std::string() : rejected[0].emitter;
    const Fingerprint second = TwoEmitters(false, nineAlive, rejected, fiveAlive);
    const std::string secondEmitter = rejected.empty() ? std::string() : rejected[0].emitter;
    CHECK_MESSAGE(first == second, "J-55 registration-order-alone: the World is the same in both orders");
    CHECK_MESSAGE((firstEmitter == "SpawnFiveThenNine" && secondEmitter == "SpawnNineTag"),
                  "J-55 registration-order-alone: the Plan order is the registration order in both rigs");
}

namespace {
SystemResult SpawnHundredFifty(SpawnWithId<Bundle<Body>> spawn) {
    spawn.New(150, Body{150});
    return Ok();
}
} // namespace

TEST_CASE("J-55: a dropped record triggers no OnAdd and no initial value, whatever the reason") {
    SUBCASE("one emitter's second record") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.InitialValue<Seen, &SeenFromBody>();
            r.AddSystem<&SpawnNineTwice>(In("SpawnNineTwice", "A"));
            r.Hook<Body, &LogAddBody>(HookTrigger::OnAdd, {.name = "LogAddBody", .zone = Zone::B});
        });
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()) == "add:9", "J-55 no-onadd same-emitter: OnAdd for 9 once");
        CHECK_MESSAGE((rig->world().Get<Seen>(9) != nullptr && rig->world().Get<Seen>(9)->s == 92),
                      "J-55 no-onadd same-emitter: the initial value comes from the kept record");
    }
    SUBCASE("two emitters, all dropped") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&SpawnFiveThenNine>(In("SpawnFiveThenNine", "A"));
            r.AddSystem<&SpawnNineTag>(In("SpawnNineTag", "A"));
            r.Hook<Body, &LogAddBody>(HookTrigger::OnAdd, {.name = "LogAddBody", .zone = Zone::B});
            r.Hook<Tag, &LogAddBody>(HookTrigger::OnAdd, {.name = "LogAddTag", .zone = Zone::B});
        });
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()) == "add:5", "J-55 no-onadd two-emitters: OnAdd for 5 only");
    }
    SUBCASE("in the allotter's range") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r, true);
            r.AddSystem<&SpawnHundredFifty>(In("SpawnHundredFifty", "A"));
            r.Hook<Body, &LogAddBody>(HookTrigger::OnAdd, {.name = "LogAddBody", .zone = Zone::B});
        });
        rig->world().SetResource(RangeAllotter::Of(100, 199));
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()).empty(), "J-55 no-onadd range: no OnAdd");
    }
    SUBCASE("step 0 spawned the id") {
        auto rig = MakeRig([](Registrar& r) {
            Base(r);
            r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
            r.Hook<Signal, &HookSpawnsEleven>(HookTrigger::OnAdd, {.name = "HookSpawnsEleven", .zone = Zone::B});
            r.AddSystem<&SpawnElevenAtTickOne>(In("SpawnElevenAtTickOne", "B"));
            r.Hook<Body, &LogAddBody>(HookTrigger::OnAdd, {.name = "LogAddBody", .zone = Zone::B});
        });
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_MESSAGE(Join(Log()) == "add:11", "J-55 no-onadd cross-phase: OnAdd for 11 once");
    }
}

// Two hooks spawn one id at step 0 (both dropped); a System of the window spawns it too. Each phase of a sync
// point is judged on its own (user 2026-11-15): the window has one emitter and the id is not alive, so it spawns.
namespace {
SystemResult SpawnThirtyAtTickOne(SpawnWithId<Bundle<Tag>> spawn, Res<TickCount> tick) {
    if (tick->value == 1) spawn.New(30, Tag{30});
    return Ok();
}
} // namespace

TEST_CASE("J-55: two hooks collide at step 0 and a System of the window spawns the id: each phase is judged on its own") {
    auto rig = MakeRig([](Registrar& r) {
        Base(r);
        r.AddSystem<&FlagTwenty>(In("FlagTwenty", "A"));
        r.Hook<Signal, &HookSpawnThirtyFirst>(HookTrigger::OnAdd, {.name = "HookSpawnThirtyFirst", .zone = Zone::B});
        r.Hook<Signal, &HookSpawnThirtySecond>(HookTrigger::OnAdd, {.name = "HookSpawnThirtySecond", .zone = Zone::B});
        r.AddSystem<&SpawnThirtyAtTickOne>(In("SpawnThirtyAtTickOne", "B"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE((rig->world().Get<Tag>(30) != nullptr && rig->world().Get<Tag>(30)->t == 30 && rig->world().Get<Body>(30) == nullptr),
                  "J-55 phases-apart: the window's spawn of 30 applies");
    const std::vector<RejectedSpawn> rejected = Rejected(rig->world());
    CHECK_MESSAGE((rejected.size() == 2 && rejected[0].emitterIsHook && rejected[1].emitterIsHook &&
                   rejected[0].reason == SpawnRejectReason::IdSpawnedTwice && rejected[1].reason == SpawnRejectReason::IdSpawnedTwice),
                  "J-55 phases-apart: the two hooks' records are dropped");
}

