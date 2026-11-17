// Immediate structural changes of ExclusiveWorld and the resource records of
// Commands (declaration v8-arch-gyo2-kernel-subtract-structure 3.5 and 3.6,
// section 5.3 rows S5 and S6): Spawn, SpawnWithId, Insert, Remove, Despawn,
// SetResource and RemoveResource applied now, the hook rounds they set off
// applied before the call returns, the refusals, the membership check and
// access verification around them; Commands::SetResource / RemoveResource at
// the sync point's resource step. target/kernel DI-KS 9, 18, 19, 21.
// Each CHECK names its case in brackets ("[ks-s5-...]", "[ks-s6-...]"): the
// mutants of the batch expect it (docs/architecture/plans/kernel/scripts/mutations.json).

#include "WorldTestSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

// Types of this file only.
struct Hp { std::int32_t v; };       // component, zone B, Exclusive; OnAdd and OnRemove log
struct Shield { std::int32_t v; };   // component, zone B; its initial value comes from Hp
struct Ball { std::int32_t v; };     // components, zone B, Shared: two hooks that never settle
struct Bat { std::int32_t v; };
struct Score { std::int64_t v; };    // resource, zone B

std::string Joined() {
    std::string out;
    for (const std::string& line : Log()) out += (out.empty() ? "" : " | ") + line;
    return out;
}
bool Logged(const std::string& line) {
    return std::find(Log().begin(), Log().end(), line) != Log().end();
}
std::size_t Index(const std::string& line) {
    return static_cast<std::size_t>(std::find(Log().begin(), Log().end(), line) - Log().begin());
}
const Detail::SystemContext& Current() {
    const Detail::SystemContext* call = Detail::CurrentThreadCall();
    REQUIRE(call != nullptr);
    return *call;
}
// The stamp of the exclusive System's call, as it logged it ("stamp N").
ChangeStamp LoggedStamp() {
    for (const std::string& line : Log()) {
        if (line.rfind("stamp ", 0) == 0) return std::stoull(line.substr(6));
    }
    FAIL("no stamp logged: " << Joined());
    return 0;
}
std::string Code(const KernelError& error) {
    return std::string(ToString(error.code));
}
template <class R>
std::string Outcome(const R& result) {
    return result ? std::string("ok") : Code(result.error());
}
std::string Reason(const std::optional<SpawnRejectReason>& reason) {
    if (!reason) return "spawned";
    switch (*reason) {
    case SpawnRejectReason::IdInAllotterRange: return "IdInAllotterRange";
    case SpawnRejectReason::IdSpawnedTwice: return "IdSpawnedTwice";
    case SpawnRejectReason::IdAlive: return "IdAlive";
    }
    return "?";
}

Shield ShieldFromHp(const Hp& hp) { return Shield{hp.v * 10}; }

// ---- Hooks

SystemResult LogHpAdd(HookCtx ctx) {
    Log().push_back("add Hp " + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult LogHpRemove(HookCtx ctx, Query<Read<Hp>> hp) {
    const Hp* value = hp.Get<Hp>(ctx.Entity());
    Log().push_back("remove Hp " + std::to_string(ctx.Entity()) + " v" + (value != nullptr ? std::to_string(value->v) : "-"));
    return Ok();
}
// OnAdd<Mode>: records a Tag for the entity.
SystemResult ModeAddsTag(HookCtx ctx, Commands commands) {
    commands.Insert(ctx.Entity(), Tag{static_cast<std::int32_t>(ctx.Entity())});
    return Ok();
}
SystemResult LogTagAdd(HookCtx ctx) {
    Log().push_back("add Tag " + std::to_string(ctx.Entity()));
    return Ok();
}
// Two hooks that keep recording: Ball added -> Bat in, Ball out; Bat added -> Ball in, Bat out.
SystemResult BallToBat(HookCtx ctx, Commands commands) {
    commands.Insert(ctx.Entity(), Bat{1});
    commands.Remove<Ball>(ctx.Entity());
    return Ok();
}
SystemResult BatToBall(HookCtx ctx, Commands commands) {
    commands.Insert(ctx.Entity(), Ball{1});
    commands.Remove<Bat>(ctx.Entity());
    return Ok();
}

// The data of this file, the WorldTest data and Tick (stages A, B, C), an allotter on request.
void Immediate(Registrar& r, bool allotter = false) {
    Data(r, allotter);
    r.Component<Hp>({.name = "Hp", .zone = Zone::B});
    r.Component<Shield>({.name = "Shield", .zone = Zone::B});
    r.Component<Ball>({.name = "Ball", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Bat>({.name = "Bat", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Resource<Score>({.name = "Score", .zone = Zone::B});
    r.InitialValue<Shield, &ShieldFromHp>();
    r.Hook<Hp, &LogHpAdd>(HookTrigger::OnAdd, {.name = "LogHpAdd", .zone = Zone::B});
    r.Hook<Hp, &LogHpRemove>(HookTrigger::OnRemove, {.name = "LogHpRemove", .zone = Zone::B});
}

std::unique_ptr<Rig> ImmediateRig(std::function<void(Registrar&)> build, bool verification = false) {
    Log().clear();
    return MakeRig(std::move(build), verification);
}

// ---- Exclusive Systems

SystemResult SpawnNow(ExclusiveWorld<> world) {
    Log().push_back("stamp " + std::to_string(Current().stamp));
    auto id = world.Spawn(Hp{5});
    if (!id) return Engine::Base::Err(std::move(id).error());
    auto hp = world.Read<Hp>(*id, [](const Hp& h) { return h.v; });
    auto shield = world.Read<Shield>(*id, [](const Shield& s) { return s.v; });
    if (!hp || !shield) return Ok();
    Log().push_back("spawned " + std::to_string(*id) + " hp " + std::to_string(hp->value_or(-1)) + " shield " +
                    std::to_string(shield->value_or(-1)));
    return Ok();
}
SystemResult SeeAddedHp(Query<Read<Hp>, Added<Hp>> added) {
    Log().push_back("added " + std::to_string(added.Count()));
    return Ok();
}
SystemResult SpawnWithoutAllotter(ExclusiveWorld<> world) {
    auto id = world.Spawn(Hp{5});
    Log().push_back("spawn " + Outcome(id));
    if (!id) return Engine::Base::Err(std::move(id).error());
    return Ok();
}
SystemResult SpawnWithIds(ExclusiveWorld<> world) {
    for (const auto& [id, v] : std::vector<std::pair<EntityId, std::int32_t>>{{7, 7}, {120, 1}, {7, 8}}) {
        auto done = world.SpawnWithId(id, Hp{v});
        if (!done) return Engine::Base::Err(std::move(done).error());
        Log().push_back("spawn " + std::to_string(id) + " " + Reason(*done));
    }
    return Ok();
}
SystemResult EarlySpawns90(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(90, Tag{90});
    return Ok();
}
SystemResult SpawnNinety(ExclusiveWorld<> world) {
    auto done = world.SpawnWithId(90, Hp{9});
    if (!done) return Engine::Base::Err(std::move(done).error());
    Log().push_back("spawn 90 " + Reason(*done));
    return Ok();
}
SystemResult ChangeMembers(ExclusiveWorld<> world) {
    Log().push_back("stamp " + std::to_string(Current().stamp));
    auto step = [&](const std::string& what, const auto& result) {
        Log().push_back(what + " " + (result ? (*result ? "true" : "false") : Code(result.error())));
    };
    step("insert 1", world.Insert<Hp>(1, Hp{10}));
    step("overwrite 1", world.Insert<Hp>(1, Hp{11}));
    step("insert 99", world.Insert<Hp>(99, Hp{0}));
    step("remove 2", world.Remove<Hp>(2));
    step("remove 2 again", world.Remove<Hp>(2));
    step("despawn 3", world.Despawn(3));
    step("despawn 3 again", world.Despawn(3));
    auto hp = world.Read<Hp>(1, [](const Hp& h) { return h.v; });
    auto shield = world.Read<Shield>(1, [](const Shield& s) { return s.v; });
    if (hp && shield) Log().push_back("in call hp " + std::to_string(hp->value_or(-1)) + " shield " + std::to_string(shield->value_or(-1)));
    return Ok();
}
SystemResult ChangeResources(ExclusiveWorld<> world) {
    Log().push_back("stamp " + std::to_string(Current().stamp));
    auto read = [&]() {
        auto score = world.Read<Score>([](const Score& s) { return s.v; });
        return score && *score ? std::to_string(**score) : std::string("absent");
    };
    Log().push_back("before " + read());
    if (auto set = world.SetResource(Score{5}); !set) return Engine::Base::Err(std::move(set).error());
    Log().push_back("after set " + read());
    auto removed = world.RemoveResource<Score>();
    auto again = world.RemoveResource<Score>();
    if (!removed || !again) return Ok();
    Log().push_back(std::string("removed ") + (*removed ? "true" : "false") + " again " + (*again ? "true" : "false") +
                    " now " + read());
    if (auto set = world.SetResource(Score{6}); !set) return Engine::Base::Err(std::move(set).error());
    return Ok();
}

// ---- The hook rounds of an immediate change

SystemResult SeedMode80(SpawnWithId<Bundle<Mode>> spawn) {
    spawn.New(80, Mode{80});
    return Ok();
}
SystemResult EarlyTagsOne(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(1, Tag{11});
    return Ok();
}
SystemResult InsertModeNow(ExclusiveWorld<> world) {
    const std::uint64_t epoch = Current().world->SyncEpoch();
    auto done = world.Insert<Mode>(5, Mode{5});
    if (!done) return Engine::Base::Err(std::move(done).error());
    Log().push_back("epoch +" + std::to_string(Current().world->SyncEpoch() - epoch));
    auto has = [&](EntityId id) {
        auto tag = world.Read<Tag>(id, [](const Tag&) {});
        return tag && *tag ? std::string("yes") : std::string("no");
    };
    Log().push_back("driver sees tag 5 " + has(5) + ", 80 " + has(80) + ", 1 " + has(1));
    return Ok();
}
SystemResult StartsBall(ExclusiveWorld<> world) {
    auto done = world.Insert<Ball>(1, Ball{0});
    Log().push_back("ball " + Outcome(done));
    if (!done) return Engine::Base::Err(std::move(done).error());
    return Ok();
}

// ---- Refusals

SystemResult InsertInsideClosure(ExclusiveWorld<> world) {
    std::string inner = "not called";
    auto r = world.Modify<Hp>(1, [&world, &inner](Hp&) { inner = Outcome(world.Insert<Mode>(1, Mode{1})); });
    Log().push_back("inside " + inner);
    if (!r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
// ExclusiveWorld is the only parameter of a hook too (S-10).
SystemResult HookInsertsNow(ExclusiveWorld<> world) {
    Log().push_back("hook " + Outcome(world.Insert<Tag>(7, Tag{1})));
    return Ok();
}
SystemResult DespawnSix(Despawn despawn) {
    despawn.Entity(6);
    return Ok();
}

// ---- Membership behind the handle (W-3)

void EraseTagOfOne() {
    Detail::ColumnAt(*KeptWorld(), Detail::ComponentIdOf(*KeptWorld(), KeyOf<Tag>())).Erase(1);
}
SystemResult EraseThenInsert(ExclusiveWorld<> world) {
    EraseTagOfOne();
    Log().push_back("insert " + Outcome(world.Insert<Hp>(1, Hp{1})));
    return Ok();
}
SystemResult InsertThenErase(ExclusiveWorld<> world) {
    auto done = world.Insert<Hp>(1, Hp{1});
    if (!done) return Engine::Base::Err(std::move(done).error());
    EraseTagOfOne();
    return Ok();
}

// ---- A nested schedule after an immediate change

SystemResult SpawnThenRunTick(ExclusiveWorld<> world) {
    auto done = world.SpawnWithId(70, Tag{70});
    if (!done) return Engine::Base::Err(std::move(done).error());
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    return Ok();
}
SystemResult LookFor70(Query<Read<Tag>> tags) {
    Log().push_back(std::string("inner 70 ") + (tags.Contains(70) ? "here" : "gone"));
    return Ok();
}

// ---- Verification

SystemResult SendsPing(EventWriter<Ping> pings) {
    pings.Send(Ping{1});
    return Ok();
}
// OnAdd<Mode>: a hook that writes an event while the immediate change applies.
SystemResult ModePings(HookCtx, EventWriter<Ping> pings) {
    pings.Send(Ping{2});
    return Ok();
}
SystemResult WriteThenChange(ExclusiveWorld<> world) {
    if (auto r = world.Modify<Hp>(1, [](Hp& h) { h.v += 1; }); !r) return Engine::Base::Err(std::move(r).error());
    if (auto r = world.Insert<Mode>(1, Mode{1}); !r) return Engine::Base::Err(std::move(r).error());
    if (auto r = world.SetResource(Score{3}); !r) return Engine::Base::Err(std::move(r).error());
    if (auto r = world.Modify<Hp>(1, [](Hp& h) { h.v += 1; }); !r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
Detail::EventBuffer<Ping>& PingBuffer(World& world) {
    return static_cast<Detail::EventBuffer<Ping>&>(Detail::EventsAt(world, Detail::EventIdOf(world, KeyOf<Ping>())));
}
SystemResult TamperPingThenChange(ExclusiveWorld<> world) {
    PingBuffer(*Current().world).records.back().payload.n = 98;
    Log().push_back("insert " + Outcome(world.Insert<Mode>(1, Mode{1})));
    return Ok();
}

// ---- S6: Commands::SetResource / RemoveResource

SystemResult SetsFlagOne(Commands commands) {
    Log().push_back("stamp " + std::to_string(Current().stamp));
    commands.SetResource(Flag{1});
    return Ok();
}
SystemResult SetsFlagTwo(Commands commands) {
    commands.SetResource(Flag{2});
    return Ok();
}
SystemResult SetThenRemoveOut(Commands commands) {
    commands.SetResource(Out{3});
    commands.RemoveResource<Out>();
    return Ok();
}
SystemResult RemoveThenSetScore(Commands commands) {
    commands.RemoveResource<Score>();
    commands.SetResource(Score{4});
    return Ok();
}
SystemResult ReadsFlagBefore(Res<Flag> flag) {
    Log().push_back(std::string("flag before ") + (flag.Present() ? std::to_string(flag->on) : std::string("absent")));
    return Ok();
}
SystemResult ReadsFlagAfter(Res<Flag> flag) {
    Log().push_back("after stamp " + std::to_string(Current().stamp));
    Log().push_back(std::string("flag after ") + (flag.Present() ? std::to_string(flag->on) : std::string("absent")));
    return Ok();
}
// OnAdd<Mode>: a hook's resource record goes into the hook queue (the next window).
SystemResult ModeSetsOut(HookCtx ctx, Commands commands) {
    commands.SetResource(Out{static_cast<std::int64_t>(ctx.Entity())});
    return Ok();
}
SystemResult ReadsOutIn(Res<Out> out) {
    Log().push_back(std::string(Current().name) + " out " + (out.Present() ? std::to_string(out->value) : std::string("absent")));
    return Ok();
}

} // namespace

// ---- S5: the seven functions

TEST_CASE("DI-KS 18: ExclusiveWorld::Spawn takes an id from the allotter; the entity, its initial value and OnAdd are there when it returns") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r, true);
        r.AddSystem<&SpawnNow>(B("Driver", "A"));
        r.AddSystem<&SeeAddedHp>(B("SeeAdded", "B"));
    });
    rig->world().SetResource(RangeAllotter::Of(100, 199));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Logged("spawned 100 hp 5 shield 50"), "[ks-s5-spawn] " << Joined());
    CHECK_MESSAGE(Index("add Hp 100") < Index("spawned 100 hp 5 shield 50"), "[ks-s5-spawn-onadd-now] " << Joined());
    CHECK_MESSAGE(Logged("added 1"), "[ks-s5-spawn-added] " << Joined());
    const auto stamps = rig->world().Stamps<Hp>(100);
    REQUIRE(stamps.has_value());
    CHECK_MESSAGE(stamps->first == LoggedStamp(), "[ks-s5-spawn-stamp] added " << stamps->first << ", call " << LoggedStamp());
    CHECK(rig->world().Alive(100));
}

TEST_CASE("DI-KS 18: ExclusiveWorld::Spawn with no allotter declared is NoAllotter") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&SpawnWithoutAllotter>(B("Driver", "A"));
    });
    const auto result = rig->Run();
    CHECK_MESSAGE(Logged("spawn NoAllotter"), "[ks-s5-no-allotter] " << Joined());
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::NoAllotter), "[ks-s5-no-allotter] " << Text(result));
    CHECK(rig->recorded.size() == 1);
}

TEST_CASE("DI-KS 18: ExclusiveWorld::SpawnWithId spawns now; an id in the allotter range or alive is not spawned, says why and is recorded") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r, true);
        r.AddSystem<&SpawnWithIds>(B("Driver", "A"));
    });
    rig->world().SetResource(RangeAllotter::Of(100, 199));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "add Hp 7 | spawn 7 spawned | spawn 120 IdInAllotterRange | spawn 7 IdAlive",
                  "[ks-s5-spawn-with-id] " << Joined());
    CHECK_MESSAGE(rig->world().Get<Hp>(7)->v == 7, "[ks-s5-spawn-with-id] the second spawn of 7 wrote its value");
    CHECK_MESSAGE(!rig->world().Alive(120), "[ks-s5-spawn-with-id] 120 spawned");
    CHECK_MESSAGE(rig->world().RejectedSpawns() == 2, "[ks-s5-spawn-rejected-recorded] " << rig->world().RejectedSpawns());
    const std::vector<RejectedSpawn> rejected = rig->world().Diagnostics<RejectedSpawn>();
    REQUIRE(rejected.size() == 2);
    CHECK(rejected[0].entity == 120);
    CHECK(rejected[0].reason == SpawnRejectReason::IdInAllotterRange);
    CHECK(rejected[0].emitter == "Driver");
    CHECK_FALSE(rejected[0].fromAllotter);
    CHECK(rejected[1].reason == SpawnRejectReason::IdAlive);
}

TEST_CASE("DI-KS 19: a spawn of the same id this level recorded is dropped as IdAlive at its own sync point") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&EarlySpawns90>(B("Early", "A", {"Driver"}));
        r.AddSystem<&SpawnNinety>(B("Driver", "A"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Logged("spawn 90 spawned"), "[ks-s5-level-spawn-dropped] " << Joined());
    const std::vector<RejectedSpawn> rejected = rig->world().Diagnostics<RejectedSpawn>();
    REQUIRE_MESSAGE(rejected.size() == 1, "[ks-s5-level-spawn-dropped] " << rejected.size());
    CHECK(rejected[0].entity == 90);
    CHECK(rejected[0].reason == SpawnRejectReason::IdAlive);
    CHECK(rejected[0].emitter == "Early");
    CHECK(rig->world().Get<Hp>(90)->v == 9);
    CHECK(rig->world().Get<Tag>(90) == nullptr);
}

TEST_CASE("DI-KS 18: Insert, Remove and Despawn now: values, stamps, OnAdd only for a new cell, OnRemove first; a missing entity is counted") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&ChangeMembers>(B("Driver", "A"));
    });
    rig->world().Insert(1, Mode{1});
    rig->world().Insert(2, Hp{20});
    rig->world().Insert(3, Hp{30});
    rig->world().Insert(3, Mode{3});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const ChangeStamp stamp = LoggedStamp();
    CHECK_MESSAGE(Joined() == "stamp " + std::to_string(stamp) +
                                  " | add Hp 1 | insert 1 true | overwrite 1 true | insert 99 false | remove Hp 2 v20 | "
                                  "remove 2 true | remove 2 again false | remove Hp 3 v30 | despawn 3 true | "
                                  "despawn 3 again false | in call hp 11 shield 100",
                  "[ks-s5-members] " << Joined());
    CHECK_MESSAGE(rig->world().Get<Hp>(1)->v == 11, "[ks-s5-members]");
    CHECK_MESSAGE(rig->world().Get<Shield>(1)->v == 100, "[ks-s5-insert-initial-value]");
    CHECK_MESSAGE(rig->world().Get<Hp>(2) == nullptr, "[ks-s5-members]");
    CHECK_MESSAGE(!rig->world().Alive(3), "[ks-s5-members]");
    CHECK_MESSAGE(rig->world().DroppedMissingEntity() == 1, "[ks-s5-insert-missing-counted] " << rig->world().DroppedMissingEntity());
    const auto stamps = rig->world().Stamps<Hp>(1);
    REQUIRE(stamps.has_value());
    CHECK_MESSAGE(stamps->first == stamp, "[ks-s5-members-stamp] added " << stamps->first << ", call " << stamp);
    // The overwrite came after the first change: the handle took a new stamp in between.
    CHECK_MESSAGE(stamps->second > stamps->first, "[ks-s5-handle-restamped] added " << stamps->first << ", changed " << stamps->second);
}

TEST_CASE("DI-KS 18: SetResource and RemoveResource now") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&ChangeResources>(B("Driver", "A"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "stamp " + std::to_string(LoggedStamp()) +
                                  " | before absent | after set 5 | removed true again false now absent",
                  "[ks-s5-resources] " << Joined());
    REQUIRE(rig->world().Resource<Score>() != nullptr);
    CHECK_MESSAGE(rig->world().Resource<Score>()->v == 6, "[ks-s5-resources]");
    const ChangeStamp changed = Detail::ResourceAt(rig->world(), Detail::ResourceIdOf(rig->world(), KeyOf<Score>())).changed;
    CHECK_MESSAGE(changed > LoggedStamp(), "[ks-s5-resource-stamp] changed " << changed << ", call " << LoggedStamp());
}

// ---- S5: the hook rounds of an immediate change (declaration section 12 item 2, (a))

TEST_CASE("DI-KS 9: what the hooks of an immediate change record is applied before it returns; this level's records and the hook queue wait") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.Hook<Mode, &ModeAddsTag>(HookTrigger::OnAdd, {.name = "ModeAddsTag", .zone = Zone::B});
        r.Hook<Tag, &LogTagAdd>(HookTrigger::OnAdd, {.name = "LogTagAdd", .zone = Zone::B});
        // Sync point A: Mode on 80, so its hook records Tag 80 into Tick's hook queue (applied at sync point B).
        r.AddSystem<&SeedMode80>(B("Seed", "A"));
        // Stage B: a record of this level before the Driver (applied at sync point B).
        r.AddSystem<&EarlyTagsOne>(B("Early", "B", {"Driver"}));
        r.AddSystem<&InsertModeNow>(B("Driver", "B"));
    });
    rig->world().Insert(1, Hp{1});
    rig->world().Insert(5, Hp{5});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Logged("driver sees tag 5 yes, 80 no, 1 no"), "[ks-s5-hook-rounds-now] " << Joined());
    CHECK_MESSAGE(Index("add Tag 5") < Index("driver sees tag 5 yes, 80 no, 1 no"), "[ks-s5-hook-rounds-now] " << Joined());
    // One hook round: its own sync epoch (F-6) and a stamp after the change's.
    CHECK_MESSAGE(Logged("epoch +1"), "[ks-s5-round-epoch] " << Joined());
    const auto mode = rig->world().Stamps<Mode>(5);
    const auto tag = rig->world().Stamps<Tag>(5);
    REQUIRE((mode.has_value() && tag.has_value()));
    CHECK_MESSAGE(tag->first > mode->first, "[ks-s5-round-stamp] Tag added " << tag->first << ", Mode added " << mode->first);
    // Afterwards each waited for its own sync point.
    CHECK(rig->world().Get<Tag>(80) != nullptr);
    CHECK(rig->world().Get<Tag>(1) != nullptr);
    CHECK(rig->world().Get<Tag>(1)->t == 11);
}

TEST_CASE("DI-KS 9: the hook rounds of an immediate change stop at the hook round limit: HookRoundLimit, the World interrupted") {
    SUBCASE("the schedule's own limit comes first") {
        auto rig = ImmediateRig([](Registrar& r) {
            Immediate(r);
            ScheduleOptions loop;
            loop.name = "Loop";
            loop.domain = Domain::Fixed;
            loop.stages = {"L"};
            loop.hookRoundLimit = 2;
            r.Schedule(loop);
            r.Hook<Ball, &BallToBat>(HookTrigger::OnAdd, {.name = "BallToBat", .zone = Zone::B});
            r.Hook<Bat, &BatToBall>(HookTrigger::OnAdd, {.name = "BatToBall", .zone = Zone::B});
            r.AddSystem<&StartsBall>(Sys("Driver", Zone::B, {At("Loop", "L")}));
        });
        rig->world().Insert(1, Hp{1});
        const auto result = rig->Run("Loop");
        CHECK_MESSAGE(Logged("ball HookRoundLimit"), "[ks-s5-round-limit] " << Joined());
        CHECK_MESSAGE(HasCode(result, KernelErrorCode::HookRoundLimit), "[ks-s5-round-limit] " << Text(result));
        CHECK_MESSAGE(Text(result).find("within 2 rounds") != std::string::npos, "[ks-s5-round-limit-schedule] " << Text(result));
        CHECK_MESSAGE(Text(result).find("schedule's declaration") != std::string::npos, "[ks-s5-round-limit-schedule] " << Text(result));
        CHECK(rig->recorded.size() == 1);
        CHECK(rig->world().State() == WorldState::Interrupted);
    }
    SUBCASE("KernelSettings' limit when the schedule declares none") {
        auto rig = ImmediateRig([](Registrar& r) {
            Immediate(r);
            r.Hook<Ball, &BallToBat>(HookTrigger::OnAdd, {.name = "BallToBat", .zone = Zone::B});
            r.Hook<Bat, &BatToBall>(HookTrigger::OnAdd, {.name = "BatToBall", .zone = Zone::B});
            r.AddSystem<&StartsBall>(B("Driver", "A"));
        });
        rig->world().Insert(1, Hp{1});
        KernelSettings settings{false, false};
        settings.hookRoundLimit = 3;
        rig->world().SetResource(settings);
        const auto result = rig->Run();
        CHECK_MESSAGE(HasCode(result, KernelErrorCode::HookRoundLimit), "[ks-s5-round-limit] " << Text(result));
        CHECK_MESSAGE(Text(result).find("within 3 rounds") != std::string::npos, "[ks-s5-round-limit-settings] " << Text(result));
        CHECK(rig->recorded.size() == 1);
    }
}

// ---- S5: refusals (declaration 3.5 point 3)

TEST_CASE("DI-KS 21: an immediate change inside a Read or Modify closure is ClosureChangesMembership; the World is interrupted") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&InsertInsideClosure>(B("Driver", "A"));
    });
    rig->world().Insert(1, Hp{1});
    const auto result = rig->Run();
    CHECK_MESSAGE(Logged("inside ClosureChangesMembership"), "[ks-s5-closure-refused] " << Joined());
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::ClosureChangesMembership), "[ks-s5-closure-refused] " << Text(result));
    CHECK(rig->recorded.size() == 1);
    CHECK(rig->world().State() == WorldState::Interrupted);
    CHECK(rig->world().Get<Mode>(1) == nullptr);
}

TEST_CASE("DI-KS 18: an immediate change through a hook's ExclusiveWorld<> is HookChangesMembership; the World is interrupted") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        // OnRemove of a despawn: the hook runs before the entity's cells go, so no membership has changed yet.
        r.Hook<Tag, &HookInsertsNow>(HookTrigger::OnRemove, {.name = "HookInsertsNow", .zone = Zone::B});
        r.AddSystem<&DespawnSix>(B("Leave", "A"));
    });
    rig->world().Insert(6, Tag{6});
    rig->world().Insert(7, Hp{7});
    const auto result = rig->Run();
    CHECK_MESSAGE(Logged("hook HookChangesMembership"), "[ks-s5-hook-refused] " << Joined());
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::HookChangesMembership), "[ks-s5-hook-refused] " << Text(result));
    CHECK(rig->recorded.size() == 1);
    CHECK(rig->world().Get<Tag>(7) == nullptr);
}

// ---- S5: membership behind the handle (declaration section 12 item 1, (a))

TEST_CASE("DI-KS 18: a membership change behind the handle before an immediate change fails a GYO_ASSERT") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&EraseThenInsert>(B("Driver", "A"));
    });
    rig->world().Insert(1, Tag{1});
    KeptWorld() = &rig->world();
    GYO_CHECK_ASSERTS(rig->Run());
    KeptWorld() = nullptr;
    CHECK_MESSAGE(!Logged("insert ok"), "[ks-s5-detour-before] the change went ahead: " << Joined());
}

TEST_CASE("DI-KS 18: a membership change behind the handle after an immediate change fails a GYO_ASSERT") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&InsertThenErase>(B("Driver", "A"));
    });
    rig->world().Insert(1, Tag{1});
    KeptWorld() = &rig->world();
    GYO_CHECK_ASSERTS(rig->Run());
    KeptWorld() = nullptr;
}

TEST_CASE("DI-KS 18: a nested schedule run after an immediate change sees it") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        ScheduleOptions outer;
        outer.name = "Outer";
        outer.domain = Domain::Fixed;
        outer.stages = {"O"};
        r.Schedule(outer);
        r.AddSystem<&SpawnThenRunTick>(Sys("Driver", Zone::B, {At("Outer", "O")}));
        r.AddSystem<&LookFor70>(B("Look", "A"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run("Outer"));
    CHECK_MESSAGE(Logged("inner 70 here"), "[ks-s5-nested-sees] " << Joined());
}

// ---- S5: access verification (declaration 3.5 point 2)

TEST_CASE("DI-KS 18: verification on: immediate changes and the hooks they run are not reported; the System's writes around them neither") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.Hook<Mode, &ModePings>(HookTrigger::OnAdd, {.name = "ModePings", .zone = Zone::B});
        // A hook round too, so the handle is used after the World's sync epoch moved (W-4).
        r.Hook<Mode, &ModeAddsTag>(HookTrigger::OnAdd, {.name = "ModeAddsTag", .zone = Zone::B});
        r.AddSystem<&WriteThenChange>(B("Driver", "A"));
    }, true);
    rig->world().Insert(1, Hp{1});
    const auto result = rig->Run();
    CHECK_MESSAGE(static_cast<bool>(result), "[ks-s5-verification-quiet] " << Text(result));
    CHECK(rig->world().Get<Hp>(1)->v == 3);
    CHECK(rig->world().Get<Mode>(1) != nullptr);
    CHECK(rig->world().EventPayloads<Ping>().size() == 1);
    CHECK(rig->world().Get<Tag>(1) != nullptr);
}

TEST_CASE("DI-KS 18: verification on: a write the System does not declare, made before an immediate change, is reported first") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&SendsPing>(B("SendsPing", "A"));
        r.AddSystem<&TamperPingThenChange>(B("Driver", "B"));
    }, true);
    rig->world().Insert(1, Hp{1});
    const auto result = rig->Run();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ks-s5-verification-before] the write before the change was not reported");
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::BoundaryChange), "[ks-s5-verification-before] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find("event Ping") != std::string::npos, "[ks-s5-verification-before] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find("'Driver'") != std::string::npos, "[ks-s5-verification-before] " << result.error().Text());
    // Compared before the change, as before a nested run: the change is refused and does not happen.
    CHECK_MESSAGE(Logged("insert BoundaryChange"), "[ks-s5-verification-before-refused] " << Joined());
    CHECK(rig->world().Get<Mode>(1) == nullptr);
    CHECK(rig->recorded.size() == 1);
}

// ---- S6: Commands::SetResource / RemoveResource (declaration 3.6)

TEST_CASE("DI-KS 9: Commands::SetResource and RemoveResource are applied at the sync point's resource step, by (ResourceId, planPos, recSeq)") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.AddSystem<&SetsFlagOne>(B("SetsFlagOne", "A", {"SetsFlagTwo"}));
        r.AddSystem<&SetsFlagTwo>(B("SetsFlagTwo", "A"));
        r.AddSystem<&SetThenRemoveOut>(B("SetThenRemoveOut", "A"));
        r.AddSystem<&RemoveThenSetScore>(B("RemoveThenSetScore", "A"));
        r.AddSystem<&ReadsFlagBefore>(B("ReadsFlagBefore", "A", {}, {"SetsFlagTwo"}));
        r.AddSystem<&ReadsFlagAfter>(B("ReadsFlagAfter", "B"));
    });
    rig->world().SetResource(Out{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Logged("flag before absent"), "[ks-s6-deferred] " << Joined());
    CHECK_MESSAGE(Logged("flag after 2"), "[ks-s6-plan-order] " << Joined());
    CHECK_MESSAGE(rig->world().Resource<Out>() == nullptr, "[ks-s6-record-order] Out is still there");
    REQUIRE_MESSAGE(rig->world().Resource<Score>() != nullptr, "[ks-s6-record-order] Score is absent");
    CHECK(rig->world().Resource<Score>()->v == 4);
    // The sync point's stamp: after every step of stage A, before the first of stage B.
    ChangeStamp after = 0;
    for (const std::string& line : Log()) {
        if (line.rfind("after stamp ", 0) == 0) after = std::stoull(line.substr(12));
    }
    const ChangeStamp changed = Detail::ResourceAt(rig->world(), Detail::ResourceIdOf(rig->world(), KeyOf<Flag>())).changed;
    CHECK_MESSAGE((changed > LoggedStamp() && changed < after),
                  "[ks-s6-stamp] changed " << changed << ", stage A " << LoggedStamp() << ", stage B " << after);
}

TEST_CASE("DI-KS 9: a hook's Commands::SetResource goes into the hook queue: applied at the next sync point") {
    auto rig = ImmediateRig([](Registrar& r) {
        Immediate(r);
        r.Hook<Mode, &ModeSetsOut>(HookTrigger::OnAdd, {.name = "ModeSetsOut", .zone = Zone::B});
        r.AddSystem<&SeedMode80>(B("Seed", "A"));
        r.AddSystem<&ReadsOutIn>(B("InB", "B"));
        r.AddSystem<&ReadsOutIn>(B("InC", "C"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "InB out absent | InC out 80", "[ks-s6-hook-next-window] " << Joined());
}
