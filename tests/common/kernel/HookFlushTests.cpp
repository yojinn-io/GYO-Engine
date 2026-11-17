// Spec J-54 (F-12; batch v8-arch-gyo2-hook-flush, declaration
// docs/architecture/target/v2/hook-flush-declaration.zh-Hant.md section 6
// judgement 1): the hook rounds after the last sync point of a Run.
//
// A hook that records a deferred change at the last sync point of a Run used
// to wait for the next Run of the same schedule ("fixed-step end": a schedule
// run 0 to k times per frame; "run once": a schedule run once never applied
// it). Now the Executor applies the schedule's hook queue right after the last
// sync point, round after round, until it is empty, up to a limit.
//
// Every rig registers the schedule "Early" before "Tick", so the hook queue of
// the schedule that runs is never the first one (a wrong "first queue" fails).

#include "WorldTestSupport.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

struct Bullet { std::int32_t b; };
struct Trail { std::int32_t t; };
struct Chain { std::int32_t left; };
struct Fan { std::int32_t f; };
struct Score { std::int32_t s; };
struct Marker { std::int32_t m; };
struct Req { std::int32_t r; };
struct Filled { std::int32_t f; };
struct Bomb { std::int32_t b; };
struct Fuse { std::int32_t f; };
struct Seed { std::int32_t s; };
struct Depth { std::int32_t d; };

Filled FilledFrom(const Req& request) { return Filled{request.r * 10}; }

struct Limits {
    std::optional<std::uint32_t> tick;
    std::optional<std::uint32_t> early;
    std::vector<std::string> tickStages = {"A", "B", "C"};
    std::string eventTrim;
};

// Early (Frame clock) first, then the shared test data, then Tick (Fixed clock).
void Base(Registrar& r, const Limits& limits = {}) {
    ScheduleOptions early;
    early.name = "Early";
    early.domain = Domain::Frame;
    early.clock = true;
    early.stages = {"X"};
    early.hookRoundLimit = limits.early;
    r.Schedule(early);
    r.Component<Pos>({.name = "Pos", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<Vel>({.name = "Vel", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<Seed>({.name = "Seed", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Component<Tag>({.name = "Tag", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Bullet>({.name = "Bullet", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Trail>({.name = "Trail", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Chain>({.name = "Chain", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Fan>({.name = "Fan", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Score>({.name = "Score", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Marker>({.name = "Marker", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Req>({.name = "Req", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Filled>({.name = "Filled", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Bomb>({.name = "Bomb", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Fuse>({.name = "Fuse", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Resource<Depth>({.name = "Depth", .zone = Zone::B});
    r.Event<Ping>({.name = "Ping", .zone = Zone::B, .domain = Domain::Fixed});
    ScheduleOptions tick;
    tick.name = "Tick";
    tick.domain = Domain::Fixed;
    tick.clock = true;
    tick.stages = limits.tickStages;
    tick.eventTrim = limits.eventTrim;
    tick.hookRoundLimit = limits.tick;
    r.Schedule(tick);
}

SystemOptions Put(std::string name, std::string schedule, std::string stage, Zone zone = Zone::B) {
    return Sys(std::move(name), zone, {KernelTest::At(std::move(schedule), std::move(stage))});
}

// ---- Systems

SystemResult SpawnBullet(SpawnWithId<Bundle<Bullet>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(1, Bullet{1});
    return Ok();
}
SystemResult SpawnBulletOnce(SpawnWithId<Bundle<Bullet>> spawn) {
    spawn.New(1, Bullet{1});
    return Ok();
}
SystemResult CountTrails(Query<Read<Trail>> trails) {
    Log().push_back("trails:" + std::to_string(trails.Count()));
    return Ok();
}
SystemResult CountTrailsB(Query<Read<Trail>> trails) {
    Log().push_back("B:" + std::to_string(trails.Count()));
    return Ok();
}
SystemResult CountTrailsC(Query<Read<Trail>> trails) {
    Log().push_back("C:" + std::to_string(trails.Count()));
    return Ok();
}
SystemResult MarkTwo(CommandsFor<Inserts<Marker>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() == 1) commands.Insert(2, Marker{2});
    return Ok();
}
// Spawns a chain head whose hook rounds go Depth deep, on every tick (ids never repeat).
SystemResult SpawnChain(SpawnWithId<Bundle<Chain>> spawn, Res<Depth> depth, CurrentTick tick) {
    if (depth->d >= 0) spawn.New(static_cast<EntityId>(tick.Tick()) * 1000000U, Chain{depth->d});
    return Ok();
}
SystemResult SpawnFan(SpawnWithId<Bundle<Fan>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(1, Fan{1});
    return Ok();
}
SystemResult WritePing(EventWriter<Ping> pings, CurrentTick tick) {
    if (tick.Tick() == 1) pings.Send(Ping{11});
    return Ok();
}
SystemResult SpawnBombAtTwo(SpawnWithId<Bundle<Bomb>> spawn, CurrentTick tick) {
    if (tick.Tick() == 2) spawn.New(7, Bomb{7});
    return Ok();
}
SystemResult FailAtB() {
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "stage B fails"));
}
SystemResult ZoneCReadsPos(Query<Read<Pos>>) { return Ok(); }
SystemResult EarlyReader(EventReader<Ping> pings, CurrentTick tick) {
    std::string line = "early@" + std::to_string(tick.Tick()) + ":";
    for (const Ping* ping : pings.Read()) line += " " + std::to_string(ping->n);
    Log().push_back(line);
    return Ok();
}
SystemResult LateReader(EventReader<Ping> pings, CurrentTick tick) {
    std::string line = "late@" + std::to_string(tick.Tick()) + ":";
    for (const Ping* ping : pings.Read()) line += " " + std::to_string(ping->n);
    Log().push_back(line);
    return Ok();
}
SystemResult ZoneCSpawnsSeed(SpawnWithId<Bundle<Seed>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(3, Seed{3});
    return Ok();
}

// ---- Hooks

SystemResult AddTrail(HookCtx ctx, CommandsFor<Inserts<Trail>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Trail{7});
    return Ok();
}
SystemResult Grow(HookCtx ctx, Query<Read<Chain>> chains, SpawnWithId<Bundle<Chain>> spawn) {
    const Chain* chain = chains.Get<Chain>(ctx.Entity());
    if (chain != nullptr && chain->left > 0) spawn.New(ctx.Entity() * 4 + 1, Chain{chain->left - 1});
    return Ok();
}
SystemResult FanP(HookCtx ctx, SpawnWithId<Bundle<Fan>> spawn) {
    spawn.New(ctx.Entity() * 4 + 1, Fan{1});
    return Ok();
}
SystemResult FanQ(HookCtx ctx, SpawnWithId<Bundle<Fan>> spawn) {
    spawn.New(ctx.Entity() * 4 + 2, Fan{2});
    spawn.New(ctx.Entity() * 4 + 3, Fan{3});
    return Ok();
}
SystemResult ArmBomb(HookCtx ctx, CommandsFor<Inserts<Fuse>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Fuse{1});
    return Ok();
}
SystemResult Explode(HookCtx) {
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "the fuse's own Err"));
}
SystemResult ThrowOnFuse(HookCtx) {
    throw std::runtime_error("the fuse throws");
}
SystemResult PingOnTrail(HookCtx, EventWriter<Ping> pings) {
    pings.Send(Ping{99});
    return Ok();
}
// A record made on the System path while the World applies the hook rounds (spec F-5).
SystemResult TrailOnSystemPath(HookCtx ctx) {
    Detail::SystemContext fake;
    fake.world = KeptWorld();
    fake.hook = false;
    std::vector<Detail::Record> queue;
    std::uint32_t seq = 0;
    fake.queue = &queue;
    fake.recSeq = &seq;
    Detail::Push(fake, Detail::Record::Of(Detail::RecordKind::Despawn, ctx.Entity()));
    return Ok();
}
SystemResult BulletDespawnsSix(HookCtx, Despawn despawn) {
    despawn.Entity(6);
    return Ok();
}
SystemResult ZoneCSeedInsertsVelIntoTwo(HookCtx, CommandsFor<Inserts<Vel>, Removes<>> commands) {
    commands.Insert(2, Vel{2});
    return Ok();
}
SystemResult ZoneCVelInsertsPosIntoSix(HookCtx, CommandsFor<Inserts<Pos>, Removes<>> commands) {
    commands.Insert(6, Pos{6});
    return Ok();
}
SystemResult BulletAsksFilled(HookCtx ctx, CommandsFor<Inserts<Req>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Req{4});
    return Ok();
}
SystemResult FilledAddsScore(HookCtx ctx, CommandsFor<Inserts<Score>, Removes<>> commands) {
    Log().push_back("filled:" + std::to_string(ctx.Entity()));
    commands.Insert(ctx.Entity(), Score{1});
    return Ok();
}
SystemResult BulletDespawnsThree(HookCtx, Despawn despawn) {
    despawn.Entity(3);
    return Ok();
}
SystemResult BulletRemovesScoreOfFour(HookCtx, CommandsFor<Inserts<>, Removes<Score>> commands) {
    commands.Remove<Score>(4);
    return Ok();
}
SystemResult LogScoreRemoved(HookCtx ctx) {
    Log().push_back("removed:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult LogScoreAdded(HookCtx ctx) {
    Log().push_back("added:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult ScoreOne(HookCtx ctx, CommandsFor<Inserts<Score>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Score{1});
    return Ok();
}
SystemResult ScoreTwo(HookCtx ctx, CommandsFor<Inserts<Score>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Score{2});
    return Ok();
}
SystemResult BulletSwapsNine(HookCtx, CommandsFor<Inserts<Marker>, Removes<Score>> commands) {
    commands.Remove<Score>(9);
    commands.Insert(9, Marker{9});
    return Ok();
}
SystemResult MarkerAddsScore(HookCtx ctx, CommandsFor<Inserts<Score>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Score{6});
    return Ok();
}

// A rig over Base plus build; KernelSettings with the given product default when set.
std::unique_ptr<Rig> MakeFlushRig(std::function<void(Registrar&)> build, Limits limits = {}, bool verification = true,
                                  std::optional<std::uint32_t> productLimit = std::nullopt, bool breakOn = false) {
    auto rig = MakeRig(
        [build = std::move(build), limits](Registrar& r) {
            Base(r, limits);
            build(r);
        },
        verification);
    if (rig->executor) {
        KernelSettings settings{verification, breakOn};
        if (productLimit) settings.hookRoundLimit = *productLimit;
        rig->world().SetResource(settings);
    }
    return rig;
}

void ChainSystems(Registrar& r) {
    r.AddSystem<&SpawnChain>(Put("SpawnChain", "Tick", "C"));
    r.Hook<Chain, &Grow>(HookTrigger::OnAdd, {.name = "Grow", .zone = Zone::B});
}

// One Run of Tick with a chain of the given depth (rounds needed: depth).
Engine::Base::Result<void, KernelError> RunChain(Rig& rig, std::int32_t depth) {
    rig.world().SetResource(Depth{depth});
    return rig.Run();
}

std::uint64_t StampsPerRun(const Plan& plan) {
    std::uint64_t n = 0;
    for (const Step& step : plan.steps) n += step.kind != StepKind::EventTrim ? 1U : 0U;
    return n;
}

std::string JoinLog() {
    std::string out;
    for (const std::string& part : Log()) out += (out.empty() ? "" : " ") + part;
    return out;
}

std::uint64_t SyncPointsOf(const Plan& plan) {
    std::uint64_t n = 0;
    for (const Step& step : plan.steps) n += step.kind == StepKind::SyncPoint ? 1U : 0U;
    return n;
}

std::size_t Occurrences(const std::string& text, const std::string& part) {
    std::size_t n = 0;
    for (std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size())) ++n;
    return n;
}

} // namespace

TEST_CASE("J-54: fixed-step end: a hook's records at the last sync point are applied before Run returns") {
    for (const bool verification : {true, false}) {
        CAPTURE(verification);
        auto rig = MakeFlushRig(
            [](Registrar& r) {
                r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
                r.AddSystem<&CountTrails>(Put("CountTrails", "Early", "X"));
                r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
            },
            {}, verification);
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
        CHECK_MESSAGE(rig->world().Get<Trail>(1) != nullptr, "J-54: the trail waited for the next Run of Tick");
        KERNEL_REQUIRE_RUN_OK(rig->Run("Early"));
        CHECK(JoinLog() == "trails:1");
    }
}

TEST_CASE("J-54: run once: a startup schedule's hook records at its last sync point are applied") {
    for (const bool verification : {true, false}) {
        CAPTURE(verification);
        auto rig = MakeFlushRig(
            [](Registrar& r) {
                ScheduleOptions once;
                once.name = "Startup";
                once.startup = true;
                once.stages = {"S"};
                r.Schedule(once);
                r.AddSystem<&SpawnBulletOnce>(Put("SpawnBulletOnce", "Startup", "S"));
                r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
            },
            {}, verification);
        KERNEL_REQUIRE_RUN_OK(rig->Run("Startup"));
        CHECK_MESSAGE(rig->world().Get<Trail>(1) != nullptr, "J-54: a run-once schedule never applied its hook records");
    }
}

TEST_CASE("J-54: a chain of three rounds: each round takes a stamp and a sync epoch") {
    auto rig = MakeFlushRig(ChainSystems);
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    const ChangeStamp stampBefore = rig->world().Stamp();
    const std::uint64_t epochBefore = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(RunChain(*rig, 3));
    CHECK_MESSAGE(rig->world().Stamp() == stampBefore + StampsPerRun(*plan) + 3, "J-54: a round takes no stamp of its own");
    CHECK_MESSAGE(rig->world().SyncEpoch() == epochBefore + SyncPointsOf(*plan) + 3, "J-54: a round does not add a sync epoch");
    const ChangeStamp lastSync = stampBefore + StampsPerRun(*plan);
    const std::vector<EntityId> ids = {1000000, 4000001, 16000005, 64000021};
    for (std::size_t i = 0; i < ids.size(); ++i) {
        CAPTURE(i);
        const auto stamps = rig->world().Stamps<Chain>(ids[i]);
        REQUIRE(stamps);
        CHECK(stamps->first == lastSync + i);
    }
}

TEST_CASE("J-54: the default limit is 8 rounds; a round that triggers a hook recording nothing does not count") {
    auto ok = MakeFlushRig(ChainSystems);
    KERNEL_REQUIRE_RUN_OK(RunChain(*ok, 8));  // round 8 applies Chain{0}: Grow runs and records nothing
    auto over = MakeFlushRig(ChainSystems, {}, false);
    const auto result = RunChain(*over, 9);
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::HookRoundLimit), "J-54: a chain of 9 rounds passed the default limit");
}

TEST_CASE("J-54: the product's default (KernelSettings) changes the limit") {
    auto ok = MakeFlushRig(ChainSystems, {}, true, 2U);
    KERNEL_REQUIRE_RUN_OK(RunChain(*ok, 2));
    auto over = MakeFlushRig(ChainSystems, {}, false, 2U);
    CHECK_MESSAGE(HasCode(RunChain(*over, 3), KernelErrorCode::HookRoundLimit), "J-54: KernelSettings::hookRoundLimit not read");
}

TEST_CASE("J-54: a schedule's own limit overrides the product's default, larger or smaller") {
    SUBCASE("larger: the schedule's 5 over the product's 2; a schedule without one uses the product's") {
        Limits limits;
        limits.tick = 5U;
        auto ok = MakeFlushRig(ChainSystems, limits, true, 2U);
        CHECK_MESSAGE(static_cast<bool>(RunChain(*ok, 5)), "J-54: the schedule's limit of 5 not used (depth 5 failed)");
        auto over = MakeFlushRig(ChainSystems, limits, false, 2U);
        CHECK_MESSAGE(HasCode(RunChain(*over, 6), KernelErrorCode::HookRoundLimit), "J-54: the schedule's limit of 5 not used");
        auto early = MakeFlushRig(
            [](Registrar& r) {
                r.AddSystem<&SpawnChain>(Put("SpawnChain", "Early", "X"));
                r.Hook<Chain, &Grow>(HookTrigger::OnAdd, {.name = "Grow", .zone = Zone::B});
            },
            limits, false, 2U);
        early->world().SetResource(Depth{3});
        CHECK_MESSAGE(HasCode(early->Run("Early"), KernelErrorCode::HookRoundLimit),
                      "J-54: a schedule with no limit of its own did not use the product's 2");
    }
    SUBCASE("smaller: the schedule's 1 under the product's 8") {
        Limits limits;
        limits.tick = 1U;
        auto ok = MakeFlushRig(ChainSystems, limits);
        KERNEL_REQUIRE_RUN_OK(RunChain(*ok, 1));
        auto over = MakeFlushRig(ChainSystems, limits, false);
        CHECK_MESSAGE(HasCode(RunChain(*over, 2), KernelErrorCode::HookRoundLimit),
                      "J-54: the smaller of the two limits was not the schedule's");
    }
}

TEST_CASE("J-54: the product's default is per World") {
    auto two = MakeFlushRig(ChainSystems, {}, false, 2U);
    WorldExecutor other(*two->built.registry, "other-world");
    std::vector<KernelError> otherRecorded;
    other.SetErrorHandler([&otherRecorded](const KernelError& error) { otherRecorded.push_back(error); });
    other.GetWorld().SetResource(KernelSettings{false, false});
    other.GetWorld().SetResource(Depth{0});
    KERNEL_REQUIRE_RUN_OK(other.Run("Tick"));  // the other World reads its KernelSettings first
    CHECK(HasCode(RunChain(*two, 3), KernelErrorCode::HookRoundLimit));
    other.GetWorld().SetResource(Depth{3});
    const auto result = other.Run("Tick");
    CHECK_MESSAGE(static_cast<bool>(result), "J-54: another World's KernelSettings changed this World's limit: " << Text(result));
}

TEST_CASE("J-54: the rounds are counted from 1 in every Run") {
    auto rig = MakeFlushRig(ChainSystems, {}, true, 2U);
    for (int run = 0; run < 3; ++run) {
        CAPTURE(run);
        KERNEL_REQUIRE_RUN_OK(RunChain(*rig, 2));
    }
}

TEST_CASE("J-54: KernelSettings is read when each Run starts; a limit of 0 is rejected") {
    SUBCASE("KernelSettings written after the first Run is read when the next Run starts (J-50)") {
        auto rig = MakeFlushRig(ChainSystems);
        KERNEL_REQUIRE_RUN_OK(RunChain(*rig, 3));
        rig->world().SetResource(KernelSettings{true, false, 2});
        CHECK_MESSAGE(HasCode(RunChain(*rig, 3), KernelErrorCode::HookRoundLimit),
                      "J-54 next Run: a limit the Host wrote after the first Run was not read by the next Run");
    }
    SUBCASE("a limit of 0 written between two Runs fails a GYO_ASSERT when the next Run starts") {
        auto rig = MakeFlushRig(ChainSystems);
        KERNEL_REQUIRE_RUN_OK(RunChain(*rig, 0));
        rig->world().SetResource(KernelSettings{true, false, 0});
        GYO_CHECK_ASSERTS(RunChain(*rig, 0));
    }
    SUBCASE("a schedule's limit of 0 is D-E20") {
        Limits limits;
        limits.tick = 0U;
        const Built built = Finish({{"test", [limits](Registrar& r) { Base(r, limits); }}});
        CHECK(HasError(built, StartupCode::DE20));
        CHECK(HasRule(built, StartupCode::DE20, "F-12"));
    }
    SUBCASE("a product default of 0 fails a GYO_ASSERT at the first Run") {
        auto rig = MakeFlushRig(ChainSystems, {}, true, 0U);
        GYO_CHECK_ASSERTS(RunChain(*rig, 0));
    }
}

TEST_CASE("J-54: the schedule's limit is in the Plan and C fingerprints; versions") {
    auto build = [](Limits limits) {
        return Finish({{"test", [limits](Registrar& r) {
                            Base(r, limits);
                            r.AddSystem<&ZoneCReadsPos>(Put("ZoneCReadsPos", "Tick", "A", Zone::C));
                        }}});
    };
    auto planOf = [](const Built& built, const char* schedule) { return built.registry->Frozen().PlanOf(schedule)->fingerprint; };
    const Built none = build({});
    KERNEL_REQUIRE_OK(none);
    Limits tick5;
    tick5.tick = 5U;
    Limits tick6;
    tick6.tick = 6U;
    Limits tick8;
    tick8.tick = 8U;
    Limits early5;
    early5.early = 5U;
    const Built t5 = build(tick5);
    const Built t6 = build(tick6);
    const Built t8 = build(tick8);
    const Built e5 = build(early5);
    // Tick runs a zone C System: its limit is in all three.
    CHECK(planOf(t5, "Tick") != planOf(none, "Tick"));
    CHECK(t5.registry->Frozen().CFingerprint() != none.registry->Frozen().CFingerprint());
    CHECK(t5.registry->Frozen().RegistryFingerprint() != none.registry->Frozen().RegistryFingerprint());
    // Early runs only zone B: Plan and Registry, not C.
    CHECK(planOf(e5, "Early") != planOf(none, "Early"));
    CHECK(e5.registry->Frozen().RegistryFingerprint() != none.registry->Frozen().RegistryFingerprint());
    CHECK(e5.registry->Frozen().CFingerprint() == none.registry->Frozen().CFingerprint());
    // The value, not only its presence; "not declared" is not the default value.
    CHECK(planOf(t5, "Tick") != planOf(t6, "Tick"));
    CHECK(t5.registry->Frozen().CFingerprint() != t6.registry->Frozen().CFingerprint());
    CHECK(planOf(t8, "Tick") != planOf(none, "Tick"));
    CHECK(planOf(none, "Tick").version == 5U);
    CHECK(none.registry->Frozen().CFingerprint().version == 7U);
    CHECK(none.registry->Frozen().RegistryFingerprint().version == 3U);
    const std::string report = t5.registry->Frozen().ExportPlans();
    CHECK(report.find("gyo.kernel.plans 3") == 0);
    CHECK(report.find("  hook-round-limit 5\n") != std::string::npos);
    CHECK(report.find("  hook-round-limit default\n") != std::string::npos);
}

TEST_CASE("J-54: at the limit: Err F-12 once, the World interrupted, the rest not applied, the hooks named") {
    for (const bool breakOn : {true, false}) {
        CAPTURE(breakOn);
        auto rig = MakeFlushRig(
            [](Registrar& r) {
                r.AddSystem<&SpawnFan>(Put("SpawnFan", "Tick", "C"));
                r.Hook<Fan, &FanP>(HookTrigger::OnAdd, {.name = "FanP", .zone = Zone::B});
                r.Hook<Fan, &FanQ>(HookTrigger::OnAdd, {.name = "FanQ", .zone = Zone::B});
            },
            {}, false, 3U, breakOn);
        int breaks = 0;
        rig->world().SetBreakHook([&breaks](const KernelError&) { ++breaks; });
        const auto result = rig->Run();
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().code == KernelErrorCode::HookRoundLimit);
        CHECK(result.error().rule == "F-12");
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK_MESSAGE(rig->recorded.size() == 1, "J-54: the handler ran " << rig->recorded.size() << " times");
        CHECK_MESSAGE((breaks > 0) == breakOn, "J-54: break hook calls " << breaks);
        const std::string& message = result.error().message;
        CHECK(message.find("schedule 'Tick'") != std::string::npos);
        CHECK(message.find("within 3 rounds") != std::string::npos);
        CHECK(message.find("World's KernelSettings") != std::string::npos);
        CHECK(message.find("81 records remain") != std::string::npos);
        CHECK_MESSAGE(message.find("by hook 'FanP', 'FanQ'") != std::string::npos, message);
        CHECK(Occurrences(message, "'FanQ'") == 1);
        const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
        REQUIRE(plan != nullptr);
        const std::string at = "at sync point step " + std::to_string(PositionOf(*plan, "C"));
        CHECK_MESSAGE(result.error().Text().find(at) != std::string::npos, result.error().Text());
        // The head and three rounds of 3, 9 and 27 fans; the 81 of round 4 not applied.
        CHECK(rig->world().Entities().size() == 40);
        GYO_CHECK_ASSERTS(rig->Run());
    }
}

TEST_CASE("J-54: an Err in a round: the rest of the Run is not done, the applied rounds stay, the handler runs once") {
    auto rig = MakeFlushRig(
        [](Registrar& r) {
            r.AddSystem<&WritePing>(Put("WritePing", "Tick", "A"));
            r.AddSystem<&SpawnBombAtTwo>(Put("SpawnBombAtTwo", "Tick", "C"));
            r.Hook<Bomb, &ArmBomb>(HookTrigger::OnAdd, {.name = "ArmBomb", .zone = Zone::B});
            r.Hook<Fuse, &Explode>(HookTrigger::OnAdd, {.name = "Explode", .zone = Zone::B});
        },
        {}, false);
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // tick 1 writes a Ping
    const auto result = rig->Run();     // tick 2: the Fuse's OnAdd in round 1 returns Err
    CHECK(HasCode(result, KernelErrorCode::SystemFailed));
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK(result.error().Text().find("hook round 1") != std::string::npos);
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    const std::string hookAt = "hook 'Explode' at sync point step " + std::to_string(PositionOf(*plan, "C"));
    CHECK_MESSAGE(result.error().Text().find(hookAt) != std::string::npos, "J-54: the round's hook is not at the last sync point");
    CHECK(rig->recorded.size() == 1);
    CHECK(rig->world().State() == WorldState::Interrupted);
    CHECK_MESSAGE(rig->world().EventPayloads<Ping>().size() == 1, "J-54: the event trim ran after an Err in a round");
    CHECK_MESSAGE(rig->world().Get<Fuse>(7) != nullptr, "J-54: an applied round was undone");
}

TEST_CASE("J-54: an event trim at an earlier sync point: the rounds still follow the last sync point") {
    Limits limits;
    limits.eventTrim = "A";
    auto rig = MakeFlushRig(
        [](Registrar& r) {
            r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "A"));
            r.AddSystem<&CountTrailsB>(Put("CountTrailsB", "Tick", "B"));
            r.AddSystem<&SpawnChain>(Put("SpawnChain", "Tick", "C"));
            r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
            r.Hook<Chain, &Grow>(HookTrigger::OnAdd, {.name = "Grow", .zone = Zone::B});
        },
        limits);
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(RunChain(*rig, 2));
    CHECK(JoinLog() == "B:0");  // the mid-Plan hook's record waits for stage B's sync point
    CHECK(rig->world().Get<Trail>(1) != nullptr);
    CHECK_MESSAGE(rig->world().Get<Chain>(16000005) != nullptr, "J-54: the rounds did not follow the last sync point");
    KERNEL_REQUIRE_RUN_OK(RunChain(*rig, 0));  // the queue was left empty
}

TEST_CASE("J-54: mid-Plan hook records still wait for the next sync point; no round, no extra stamp") {
    auto rig = MakeFlushRig([](Registrar& r) {
        r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "A"));
        r.AddSystem<&CountTrailsB>(Put("CountTrailsB", "Tick", "B"));
        r.AddSystem<&MarkTwo>(Put("MarkTwo", "Tick", "B"));
        r.AddSystem<&CountTrailsC>(Put("CountTrailsC", "Tick", "C"));
        r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
    });
    rig->world().Insert(2, Tag{2});
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    const ChangeStamp stampBefore = rig->world().Stamp();
    const std::uint64_t epochBefore = rig->world().SyncEpoch();
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(JoinLog() == "B:0 C:1");
    const auto trail = rig->world().Stamps<Trail>(1);
    const auto marker = rig->world().Stamps<Marker>(2);
    REQUIRE(trail);
    REQUIRE(marker);
    CHECK_MESSAGE(trail->first == marker->first, "J-54: the mid-Plan hook record was not applied at stage B's sync point");
    CHECK(rig->world().Stamp() == stampBefore + StampsPerRun(*plan));
    CHECK(rig->world().SyncEpoch() == epochBefore + 3);
}

TEST_CASE("J-54: a round is applied while the World is Applying (F-5)") {
    auto rig = MakeFlushRig([](Registrar& r) {
        r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
        r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
        r.Hook<Trail, &TrailOnSystemPath>(HookTrigger::OnAdd, {.name = "TrailOnSystemPath", .zone = Zone::B});
    });
    KeptWorld() = &rig->world();
    GYO_CHECK_ASSERTS(rig->Run());
    KeptWorld() = nullptr;
}

TEST_CASE("J-54: a round is a whole application") {
    SUBCASE("an initial value request in a round is filled there; its target's OnAdd records into the next round") {
        auto rig = MakeFlushRig([](Registrar& r) {
            r.InitialValue<Filled, &FilledFrom>();
            r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
            r.Hook<Bullet, &BulletAsksFilled>(HookTrigger::OnAdd, {.name = "BulletAsksFilled", .zone = Zone::B});
            r.Hook<Filled, &FilledAddsScore>(HookTrigger::OnAdd, {.name = "FilledAddsScore", .zone = Zone::B});
        });
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        REQUIRE(rig->world().Get<Filled>(1) != nullptr);
        CHECK(rig->world().Get<Filled>(1)->f == 40);
        CHECK(JoinLog() == "filled:1");
        CHECK(rig->world().Get<Score>(1) != nullptr);
        const auto filled = rig->world().Stamps<Filled>(1);
        const auto score = rig->world().Stamps<Score>(1);
        REQUIRE(filled);
        REQUIRE(score);
        CHECK(score->first == filled->first + 1);
    }
    SUBCASE("a despawn and a remove in a round run OnRemove") {
        auto rig = MakeFlushRig([](Registrar& r) {
            r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
            r.Hook<Bullet, &BulletDespawnsThree>(HookTrigger::OnAdd, {.name = "BulletDespawnsThree", .zone = Zone::B});
            r.Hook<Bullet, &BulletRemovesScoreOfFour>(HookTrigger::OnAdd, {.name = "BulletRemovesScoreOfFour", .zone = Zone::B});
            r.Hook<Score, &LogScoreRemoved>(HookTrigger::OnRemove, {.name = "LogScoreRemoved", .zone = Zone::B});
        });
        rig->world().Insert(3, Score{3});
        rig->world().Insert(4, Score{4});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(JoinLog() == "removed:3 removed:4");
        CHECK_FALSE(rig->world().Alive(3));
        CHECK(rig->world().Alive(4));
        CHECK(rig->world().Get<Score>(4) == nullptr);
    }
}

TEST_CASE("J-54: two hooks insert one cell in one round: the later record wins") {
    auto rig = MakeFlushRig([](Registrar& r) {
        r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
        r.Hook<Bullet, &ScoreOne>(HookTrigger::OnAdd, {.name = "ScoreOne", .zone = Zone::B});
        r.Hook<Bullet, &ScoreTwo>(HookTrigger::OnAdd, {.name = "ScoreTwo", .zone = Zone::B});
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Get<Score>(1) != nullptr);
    CHECK(rig->world().Get<Score>(1)->s == 2);
}

TEST_CASE("J-54: rounds are separate applications") {
    SUBCASE("a remove in round 1 and an insert of the same cell in round 2 both take effect") {
        auto rig = MakeFlushRig([](Registrar& r) {
            r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
            r.Hook<Bullet, &BulletSwapsNine>(HookTrigger::OnAdd, {.name = "BulletSwapsNine", .zone = Zone::B});
            r.Hook<Marker, &MarkerAddsScore>(HookTrigger::OnAdd, {.name = "MarkerAddsScore", .zone = Zone::B});
            r.Hook<Score, &LogScoreRemoved>(HookTrigger::OnRemove, {.name = "LogScoreRemoved", .zone = Zone::B});
            r.Hook<Score, &LogScoreAdded>(HookTrigger::OnAdd, {.name = "LogScoreAdded", .zone = Zone::B});
        });
        rig->world().Insert(9, Score{5});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(JoinLog() == "removed:9 added:9");
        REQUIRE(rig->world().Get<Score>(9) != nullptr);
        CHECK(rig->world().Get<Score>(9)->s == 6);
    }
    SUBCASE("a zone B despawn in round 1 and a zone C insert in round 2: dropped and counted") {
        auto rig = MakeFlushRig(
            [](Registrar& r) {
                r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
                r.AddSystem<&ZoneCSpawnsSeed>(Put("ZoneCSpawnsSeed", "Tick", "C", Zone::C));
                r.Hook<Bullet, &BulletDespawnsSix>(HookTrigger::OnAdd, {.name = "BulletDespawnsSix", .zone = Zone::B});
                r.Hook<Seed, &ZoneCSeedInsertsVelIntoTwo>(HookTrigger::OnAdd, {.name = "ZoneCSeedInsertsVelIntoTwo", .zone = Zone::C});
                r.Hook<Vel, &ZoneCVelInsertsPosIntoSix>(HookTrigger::OnAdd, {.name = "ZoneCVelInsertsPosIntoSix", .zone = Zone::C});
            },
            {}, false);
        rig->world().Insert(6, Tag{6});
        rig->world().Insert(2, Tag{2});
        const std::uint64_t dropped = rig->world().DroppedMissingEntity();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK_FALSE(rig->world().Alive(6));
        CHECK(rig->world().Get<Vel>(2) != nullptr);
        CHECK(rig->world().DroppedMissingEntity() == dropped + 1);
    }
}

TEST_CASE("J-54: with verification on, the snapshots are taken again after the rounds") {
    // Startup has no event trim after its last sync point, so nothing else takes the snapshots.
    auto rig = MakeFlushRig([](Registrar& r) {
        ScheduleOptions once;
        once.name = "Startup";
        once.startup = true;
        once.stages = {"S"};
        r.Schedule(once);
        r.AddSystem<&SpawnBulletOnce>(Put("SpawnBulletOnce", "Startup", "S"));
        r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run("Startup"));  // round 1 adds Trail; no hook runs after it
    const auto result = rig->Run("Early");
    CHECK_MESSAGE(static_cast<bool>(result), "J-54: the round's change was blamed on the Ready window: " << Text(result));
}

TEST_CASE("J-54: an event a hook writes in a round is read once by every reader next tick") {
    auto rig = MakeFlushRig([](Registrar& r) {
        r.AddSystem<&EarlyReader>(Put("EarlyReader", "Tick", "A"));
        r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
        r.AddSystem<&LateReader>(Put("LateReader", "Tick", "C"));
        r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
        r.Hook<Trail, &PingOnTrail>(HookTrigger::OnAdd, {.name = "PingOnTrail", .zone = Zone::B});
    });
    Log().clear();
    for (int tick = 0; tick < 3; ++tick) KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(JoinLog() == "early@1: late@1: early@2: 99 late@2: 99 early@3: late@3:");
}

TEST_CASE("J-54: a hook that throws in a round: the exception goes on and the World is interrupted") {
    auto rig = MakeFlushRig(
        [](Registrar& r) {
            r.AddSystem<&SpawnBombAtTwo>(Put("SpawnBombAtTwo", "Tick", "C"));
            r.Hook<Bomb, &ArmBomb>(HookTrigger::OnAdd, {.name = "ArmBomb", .zone = Zone::B});
            r.Hook<Fuse, &ThrowOnFuse>(HookTrigger::OnAdd, {.name = "ThrowOnFuse", .zone = Zone::B});
        },
        {}, false);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_THROWS_AS(static_cast<void>(rig->Run()), std::runtime_error);
    CHECK(rig->world().State() == WorldState::Interrupted);
    GYO_CHECK_ASSERTS(rig->Run());
}

TEST_CASE("J-54: an Err before the last sync point: no round, the queued hook record is dropped") {
    auto rig = MakeFlushRig(
        [](Registrar& r) {
            r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "A"));
            r.AddSystem<&FailAtB>(Put("FailAtB", "Tick", "B"));
            r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
        },
        {}, false);
    const auto result = rig->Run();
    CHECK(HasCode(result, KernelErrorCode::SystemFailed));
    CHECK(rig->world().Get<Trail>(1) == nullptr);
    CHECK(rig->recorded.size() == 1);
}

// ---- Added after the adversarial check of the mutant list (Fable, high): each case
// names the mutants (hf-*) that survived the cases above and that it kills.

namespace {

SystemResult TrailAddsScoreAndMarker(HookCtx ctx, CommandsFor<Inserts<Score, Marker>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Score{1});
    commands.Insert(ctx.Entity(), Marker{1});
    return Ok();
}
SystemResult QuietOnMarker(HookCtx) { return Ok(); }
SystemResult MarkerAddsBomb(HookCtx ctx, CommandsFor<Inserts<Bomb>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Bomb{1});
    return Ok();
}
SystemResult ScoreAddsFuse(HookCtx ctx, CommandsFor<Inserts<Fuse>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Fuse{1});
    return Ok();
}
SystemResult TrailAddsFuse(HookCtx ctx, CommandsFor<Inserts<Fuse>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Fuse{1});
    return Ok();
}

} // namespace

// hf-limit-source-always-settings, hf-limit-msg-round-number, hf-limit-layer-round-zero, hf-recorders-not-cleared,
// hf-recorders-every-called-hook, hf-limit-names-first-call-order, hf-tostring-hookroundlimit, hf-limit-break-hook-twice.
TEST_CASE("J-54: at a schedule's own limit: whose limit, which round, only the last round's recording hooks in registration order") {
    Limits limits;
    limits.tick = 2U;
    auto rig = MakeFlushRig(
        [](Registrar& r) {
            r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
            // Records at the last sync point, before round 1.
            r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
            // Round 2 (the last): Score's hook is called before Marker's (ComponentId order), but is registered after it.
            r.Hook<Marker, &QuietOnMarker>(HookTrigger::OnAdd, {.name = "QuietOnMarker", .zone = Zone::B});
            r.Hook<Marker, &MarkerAddsBomb>(HookTrigger::OnAdd, {.name = "MarkerAddsBomb", .zone = Zone::B});
            r.Hook<Score, &ScoreAddsFuse>(HookTrigger::OnAdd, {.name = "ScoreAddsFuse", .zone = Zone::B});
            // Round 1 only.
            r.Hook<Trail, &TrailAddsScoreAndMarker>(HookTrigger::OnAdd, {.name = "TrailAddsScoreAndMarker", .zone = Zone::B});
        },
        limits, false, 8U, true);
    int breaks = 0;
    rig->world().SetBreakHook([&breaks](const KernelError&) { ++breaks; });
    const auto result = rig->Run();
    REQUIRE_FALSE(static_cast<bool>(result));
    REQUIRE(result.error().code == KernelErrorCode::HookRoundLimit);
    const std::string& message = result.error().message;
    CHECK_MESSAGE(message.find("within 2 rounds (the limit of the schedule's declaration)") != std::string::npos,
                  "J-54: the limit's source is not the schedule's declaration: " << message);
    CHECK_MESSAGE(message.find("2 records remain, recorded in round 2 by hook ") != std::string::npos,
                  "J-54: the limit's message names another round: " << message);
    CHECK_MESSAGE(message.find("by hook 'MarkerAddsBomb', 'ScoreAddsFuse'") != std::string::npos,
                  "J-54: the last round's recording hooks are not in registration order: " << message);
    CHECK_MESSAGE(message.find("QuietOnMarker") == std::string::npos, "J-54: a hook that recorded nothing is named: " << message);
    CHECK_MESSAGE(message.find("TrailAddsScoreAndMarker") == std::string::npos, "J-54: a hook of an earlier round is named: " << message);
    const std::string text = result.error().Text();
    CHECK_MESSAGE(text.find("HookRoundLimit [F-12]: ") == 0, "J-54: the error code's text: " << text);
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    CHECK_MESSAGE(text.find("hook round 2 at sync point step " + std::to_string(PositionOf(*plan, "C"))) != std::string::npos,
                  "J-54: the limit's layer names another round: " << text);
    CHECK_MESSAGE(breaks == 1, "J-54: the limit's Err called the break hook " << breaks << " times");
}

// hf-round-err-layer-always-one.
TEST_CASE("J-54: an Err in round 2 says round 2; round 1 stays") {
    auto rig = MakeFlushRig(
        [](Registrar& r) {
            r.AddSystem<&SpawnBullet>(Put("SpawnBullet", "Tick", "C"));
            r.Hook<Bullet, &AddTrail>(HookTrigger::OnAdd, {.name = "AddTrail", .zone = Zone::B});
            r.Hook<Trail, &TrailAddsFuse>(HookTrigger::OnAdd, {.name = "TrailAddsFuse", .zone = Zone::B});
            r.Hook<Fuse, &Explode>(HookTrigger::OnAdd, {.name = "Explode", .zone = Zone::B});
        },
        {}, false);
    const auto result = rig->Run();
    CHECK(HasCode(result, KernelErrorCode::SystemFailed));
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(result.error().Text().find("hook round 2 at sync point step") != std::string::npos,
                  "J-54: an Err in round 2 names another round: " << result.error().Text());
    CHECK(rig->world().Get<Trail>(1) != nullptr);
}

// hf-settings-absent-limit-one.
TEST_CASE("J-54: a World whose KernelSettings was removed before the first Run uses the default limit of 8") {
    auto ok = MakeFlushRig(ChainSystems, {}, false);
    ok->world().RemoveResource<KernelSettings>();
    const auto eight = RunChain(*ok, 8);
    CHECK_MESSAGE(static_cast<bool>(eight), "J-54: a World without KernelSettings did not use the default limit of 8: " << Text(eight));
    auto over = MakeFlushRig(ChainSystems, {}, false);
    over->world().RemoveResource<KernelSettings>();
    CHECK_MESSAGE(HasCode(RunChain(*over, 9), KernelErrorCode::HookRoundLimit), "J-54: a World without KernelSettings has no limit of 8");
}

TEST_CASE("J-54: KernelSettings removed between two Runs: the next Run uses the default limit of 8") {
    auto rig = MakeFlushRig(ChainSystems, {}, true, 2U);
    KERNEL_REQUIRE_RUN_OK(RunChain(*rig, 2));
    rig->world().RemoveResource<KernelSettings>();
    const auto eight = RunChain(*rig, 8);
    CHECK_MESSAGE(static_cast<bool>(eight), "J-54 removed: the Run after KernelSettings was removed did not use the default limit of 8: "
                                                << Text(eight));
}

// hf-c-fp-eight-as-unset, hf-plan-fp-value-shifted, hf-c-fp-value-shifted, hf-registry-fp-schedule-field-when-declared,
// hf-export-limit-line-moved. The three texts are fixed values (the same on the L1 platforms: fixed-width types only).
TEST_CASE("J-54: a declared limit's fingerprints are fixed values; a declared 8 is not 'not declared' in the C fingerprint") {
    auto build = [](Limits limits) {
        return Finish({{"test", [limits](Registrar& r) {
                            Base(r, limits);
                            r.AddSystem<&ZoneCReadsPos>(Put("ZoneCReadsPos", "Tick", "A", Zone::C));
                        }}});
    };
    const Built none = build({});
    KERNEL_REQUIRE_OK(none);
    Limits tick5;
    tick5.tick = 5U;
    Limits tick8;
    tick8.tick = 8U;
    const Built t5 = build(tick5);
    const Built t8 = build(tick8);
    CHECK_MESSAGE(t8.registry->Frozen().CFingerprint() != none.registry->Frozen().CFingerprint(),
                  "J-54: the C fingerprint hashes a declared 8 as 'not declared'");
    CHECK_MESSAGE(t5.registry->Frozen().PlanOf("Tick")->fingerprint.Text() == "5:aacb80017464cdd1b9b347c30581bc53aaf9ad52cfe3263188cc784ce790314f",
                  "J-54: Plan fingerprint of a declared limit of 5: " << t5.registry->Frozen().PlanOf("Tick")->fingerprint.Text());
    CHECK_MESSAGE(t5.registry->Frozen().CFingerprint().Text() == "7:ca0e06b26a8b5cae94109cf60826a18ea2d85ceb1b0a92cbac2b7fd3ac07d2a7",
                  "J-54: C fingerprint of a declared limit of 5: " << t5.registry->Frozen().CFingerprint().Text());
    CHECK_MESSAGE(t5.registry->Frozen().RegistryFingerprint().Text() == "3:0e09213517429bb4326f8462d3419688ecf23334836d8a87d3e42e2721dfd441",
                  "J-54: Registry fingerprint of a declared limit of 5: " << t5.registry->Frozen().RegistryFingerprint().Text());
    const std::string report = t5.registry->Frozen().ExportPlans();
    CHECK_MESSAGE(report.find("  hook-round-limit 5\n  fingerprint 5:") != std::string::npos,
                  "J-54: the export's hook-round-limit line is not right before the fingerprint line");
}

// hf-tostring-de20, hf-de20-no-names.
TEST_CASE("J-54: D-E20 prints as D-E20 and names the schedule") {
    Limits limits;
    limits.tick = 0U;
    const Built built = Finish({{"test", [limits](Registrar& r) { Base(r, limits); }}});
    CHECK_MESSAGE(Codes(built) == std::vector<std::string>{"D-E20"}, "J-54: D-E20's text: " << Describe(built));
    REQUIRE_FALSE(static_cast<bool>(built.result));
    REQUIRE(built.result.error().errors.size() == 1);
    CHECK_MESSAGE(built.result.error().errors.front().subjects == std::vector<std::string>{"Tick"}, "J-54: D-E20 does not name the schedule");
}
