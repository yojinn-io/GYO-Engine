// A System holding ExclusiveWorld<...> runs another schedule (K0,
// v8-arch-gyo2-nested-schedule; declaration section 6 judgement 1):
//   J-56  nested runs (spec E.2, E.5, F.2, W-3, W-4, X-11 to X-16, Z5);
//   J-57  ExclusiveWorld's closures and resource access (spec S-13 to S-15);
//   J-58  Startup run by the kernel (spec H-12).
// Each CHECK carries a tag in brackets ("[ns-...]"): the mutants of the batch
// expect that tag (docs/architecture/plans/kernel/scripts/mutations.json, ids ns-).

#include "WorldTestSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using namespace WorldTest;

// ---- Schedules: Frame (Frame clock, ingress stage In), Tick (Fixed clock, WorldTest's),
// Step and Deep (Fixed, no clock), Boot (Startup), Extract (an extract schedule).

ScheduleOptions Schedule(std::string name, Domain domain, bool clock, std::vector<std::string> stages) {
    ScheduleOptions s;
    s.name = std::move(name);
    s.domain = domain;
    s.clock = clock;
    s.stages = std::move(stages);
    return s;
}

ScheduleOptions FrameSchedule() {
    ScheduleOptions s = Schedule("Frame", Domain::Frame, true, {"In", "Drive", "Out"});
    s.ingressStage = "In";
    return s;
}

ScheduleOptions BootSchedule(std::string name = "Boot") {
    ScheduleOptions s;
    s.name = std::move(name);
    s.startup = true;
    s.stages = {"S"};
    return s;
}

// WorldTest's data and Tick, plus Frame, Step and Deep.
void Nest(Registrar& r, bool allotter = false) {
    Data(r, allotter);
    r.Schedule(FrameSchedule());
    r.Schedule(Schedule("Step", Domain::Fixed, false, {"S"}));
    r.Schedule(Schedule("Deep", Domain::Fixed, false, {"D"}));
}

SystemOptions In(std::string name, std::string schedule, std::string stage, Zone zone = Zone::B,
                 std::vector<std::string> before = {}, std::vector<std::string> after = {}) {
    return Sys(std::move(name), zone, {At(std::move(schedule), std::move(stage), std::move(before), std::move(after))});
}

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

std::string Here() {
    const Detail::SystemContext& call = Current();
    return std::string(call.name) + "@" + std::string(call.schedule);
}

// What each driver (by System name) runs, in order.
std::map<std::string, std::vector<std::string>>& Runs() {
    static std::map<std::string, std::vector<std::string>> runs;
    return runs;
}
// A cap on nested runs, so a mutant that lets a schedule run itself does not recurse forever.
int& RunBudget() {
    static int budget = 0;
    return budget;
}

std::unique_ptr<Rig> NestRig(std::function<void(Registrar&)> build, bool verification = true) {
    Log().clear();
    Runs().clear();
    RunBudget() = 16;
    return MakeRig(std::move(build), verification);
}

// ---- Systems

SystemResult LogCall(Query<Read<Tag>>) {
    Log().push_back(Here() + "#" + std::to_string(Current().tick));
    return Ok();
}

// Runs Runs()[its name] in order; passes the first Err on.
SystemResult Drive(ExclusiveWorld<> world) {
    const std::string name(Current().name);
    Log().push_back("drive " + Here());
    for (const std::string& schedule : Runs()[name]) {
        if (--RunBudget() < 0) return Ok();
        if (auto ran = world.RunSchedule(schedule); !ran) return Engine::Base::Err(std::move(ran).error());
    }
    Log().push_back("driven " + Here());
    return Ok();
}

// The same, swallowing every Err (the "the outer System returns Ok" cases).
SystemResult DriveSwallow(ExclusiveWorld<> world) {
    const std::string name(Current().name);
    for (const std::string& schedule : Runs()[name]) {
        if (--RunBudget() < 0) return Ok();
        if (auto ran = world.RunSchedule(schedule); !ran) Log().push_back("swallowed " + std::string(ToString(ran.error().code)));
    }
    return Ok();
}

SystemResult Fails(Query<Read<Tag>>) {
    Log().push_back("fails " + Here());
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "boom"));
}

SystemResult Throws(Query<Read<Tag>>) {
    Log().push_back("throws " + Here());
    throw std::runtime_error("thrown inside");
}

} // namespace

// ---- J-56: running k times, and back in the outer schedule

TEST_CASE("J-56: an exclusive System runs a clock schedule 3 times: its Systems run 3 times, its tick advances 3") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
        r.AddSystem<&LogCall>(In("After", "Frame", "Out"));
    });
    Runs()["Driver"] = {"Tick", "Tick", "Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Joined() == "drive Driver@Frame | InA@Tick#1 | InA@Tick#2 | InA@Tick#3 | driven Driver@Frame | After@Frame#1",
                  "[ns-run-k] " << Joined());
    CHECK_MESSAGE(rig->world().Tick(Domain::Fixed) == 3, "[ns-run-k-tick] " << rig->world().Tick(Domain::Fixed));
    CHECK_MESSAGE(rig->world().State() == WorldState::Ready, "[ns-back-ready]");
}

namespace {
SystemResult DriveThenLookAround(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    Log().push_back(KeptWorld()->State() == WorldState::Running ? "state Running" : "state not Running");
    Log().push_back("current " + std::string(Current().name));
    return Ok();
}
SystemResult DriveThenInsert(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    KeptWorld()->Insert(50, Tag{50});  // a structural World API write while Running
    return Ok();
}
} // namespace

TEST_CASE("J-56: back in the outer System the World is still Running and the call is the outer System's") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&DriveThenLookAround>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    KeptWorld() = &rig->world();
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    KeptWorld() = nullptr;
    CHECK_MESSAGE(Logged("state Running"), "[ns-back-state] " << Joined());
    CHECK_MESSAGE(Logged("current Driver"), "[ns-context] " << Joined());
    CHECK_MESSAGE(rig->world().State() == WorldState::Ready, "[ns-back-ready]");
}

TEST_CASE("J-56: back in the outer System a structural World API write still fails a GYO_ASSERT") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&DriveThenInsert>(In("Driver", "Frame", "Drive"));
    });
    KeptWorld() = &rig->world();
    GYO_CHECK_ASSERTS(rig->Run("Frame"));
    KeptWorld() = nullptr;
}

// ---- J-56: membership

namespace {
SystemResult SpawnSeventy(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(70, Tag{70});
    return Ok();
}
SystemResult DespawnOne(Despawn despawn) {
    if (auto r = despawn.Entity(1); !r) return r;
    return Ok();
}
SystemResult LookForMembers(Query<Read<Tag>> tags) {
    Log().push_back(std::string("70 ") + (tags.Contains(70) ? "here" : "gone") + ", 1 " + (tags.Contains(1) ? "here" : "gone"));
    return Ok();
}
void EraseTagOfOne() {
    Detail::ColumnAt(*KeptWorld(), Detail::ComponentIdOf(*KeptWorld(), KeyOf<Tag>())).Erase(1);
}
SystemResult EraseThenDrive(ExclusiveWorld<> world) {
    EraseTagOfOne();
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    return Ok();
}
SystemResult DriveThenErase(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    EraseTagOfOne();
    return Ok();
}
Product*& KeptProduct() {
    static Product* product = nullptr;
    return product;
}
SystemResult PlainRunsThroughKeptPointer(Query<Read<Tag>>) {
    static_cast<void>(KeptProduct()->Run("Tick"));
    return Ok();
}
} // namespace

TEST_CASE("J-56: the inner schedule spawns and despawns: no assert, and the outer System after it sees the change") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&SpawnSeventy>(In("Spawn70", "Tick", "A"));
        r.AddSystem<&DespawnOne>(In("Despawn1", "Tick", "A"));
        r.AddSystem<&LookForMembers>(In("Look", "Frame", "Drive", Zone::B, {}, {"Driver"}));
    });
    rig->world().Insert(1, Tag{1});
    Runs()["Driver"] = {"Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("70 here, 1 gone"), "[ns-inner-membership] " << Joined());
}

TEST_CASE("J-56: an exclusive System's own membership change behind the World API before the nested run fails a GYO_ASSERT") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&EraseThenDrive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    }, false);
    rig->world().Insert(1, Tag{1});
    KeptWorld() = &rig->world();
    GYO_CHECK_ASSERTS(rig->Run("Frame"));
    KeptWorld() = nullptr;
    CHECK_MESSAGE(!Logged("InA@Tick#1"), "[ns-own-membership-before] the nested run went ahead: " << Joined());
}

TEST_CASE("J-56: an exclusive System's own membership change behind the World API after the nested run fails a GYO_ASSERT") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&DriveThenErase>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    }, false);
    rig->world().Insert(1, Tag{1});
    KeptWorld() = &rig->world();
    GYO_CHECK_ASSERTS(rig->Run("Frame"));
    KeptWorld() = nullptr;
}

TEST_CASE("J-56: a plain System reaching Product::Run through a kept pointer fails a GYO_ASSERT") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&PlainRunsThroughKeptPointer>(In("Plain", "Frame", "Drive"));
    });
    KeptProduct() = rig->product.get();
    GYO_CHECK_ASSERTS(rig->Run("Frame"));
    KeptProduct() = nullptr;
}

// ---- J-56: the outer level's records

namespace {
SystemResult InsertTagOnOne(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(1, Tag{11});
    return Ok();
}
// The same cell as InsertTagOnOne: at the outer sync point the later Plan position wins.
SystemResult InsertTagOnOneLate(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(1, Tag{22});
    return Ok();
}
SystemResult ProbeTagOfOne(Query<Read<Tag>> tags) {
    Log().push_back(std::string("probe 1 ") + (tags.Contains(1) ? "has Tag" : "has no Tag"));
    return Ok();
}
SystemResult SeedMode(SpawnWithId<Bundle<Mode>> spawn) {
    spawn.New(80, Mode{80});
    return Ok();
}
// OnAdd<Mode>: records a Tag into the inner schedule's hook queue, so its hook rounds run.
SystemResult ModeAddsTag(HookCtx ctx, CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Tag{static_cast<std::int32_t>(ctx.Entity())});
    return Ok();
}
// OnAdd<Tag>: where the insert was applied.
SystemResult LogTagAdd(HookCtx ctx) {
    Log().push_back("add " + std::to_string(ctx.Entity()) + "@" + std::string(Current().schedule));
    return Ok();
}
void OuterRecords(Registrar& r, bool nest) {
    Nest(r);
    r.AddSystem<&InsertTagOnOne>(In("Early", "Frame", "Drive", Zone::B, {"Driver"}));
    r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
    r.AddSystem<&InsertTagOnOneLate>(In("Late", "Frame", "Drive", Zone::B, {}, {"Driver"}));
    r.AddSystem<&ProbeTagOfOne>(In("Probe", "Tick", "B"));
    if (nest) r.AddSystem<&SeedMode>(In("Seed", "Tick", "C"));
    r.Hook<Mode, &ModeAddsTag>(HookTrigger::OnAdd, {.name = "ModeAddsTag", .zone = Zone::B});
    r.Hook<Tag, &LogTagAdd>(HookTrigger::OnAdd, {.name = "LogTagAdd", .zone = Zone::B});
}
} // namespace

TEST_CASE("J-56: the records of the outer level stay there: the inner schedule neither sees nor applies them") {
    auto rig = NestRig([](Registrar& r) { OuterRecords(r, true); });
    rig->world().Insert(1, Mode{1});
    Runs()["Driver"] = {"Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("probe 1 has no Tag"), "[ns-outer-records-unseen] " << Joined());
    // The inner hook rounds ran (80's Tag), and did not take the outer records with them.
    CHECK_MESSAGE(Logged("add 80@Tick"), "[ns-outer-records-rounds] " << Joined());
    CHECK_MESSAGE((Logged("add 1@Frame") && !Logged("add 1@Tick")), "[ns-outer-records-schedule] " << Joined());
    REQUIRE_MESSAGE(rig->world().Get<Tag>(1) != nullptr, "[ns-outer-records-kept] the outer records are gone");
    // Both outer records at the outer sync point: Late's later Plan position wins (F.2 step 5).
    CHECK_MESSAGE(rig->world().Get<Tag>(1)->t == 22, "[ns-outer-records-order] " << rig->world().Get<Tag>(1)->t);
    CHECK_MESSAGE(std::count(Log().begin(), Log().end(), std::string("add 1@Frame")) == 1, "[ns-outer-records-order] " << Joined());
    CHECK_MESSAGE(Index("add 80@Tick") < Index("add 1@Frame"), "[ns-outer-records-order] " << Joined());
    auto control = NestRig([](Registrar& r) { OuterRecords(r, false); });
    control->world().Insert(1, Mode{1});
    KERNEL_REQUIRE_RUN_OK(control->Run("Frame"));
    CHECK_MESSAGE(control->world().Get<Tag>(1)->t == rig->world().Get<Tag>(1)->t,
                  "[ns-outer-records-same] the nested run changed the outer records' result");
}

// ---- J-56: an outer insert meets an inner despawn (Z5 across levels)

namespace {
SystemResult InsertTagOnFive(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(5, Tag{5});
    return Ok();
}
SystemResult InsertVelOnFive(CommandsFor<Inserts<Vel>, Removes<>> commands) {
    commands.Insert(5, Vel{5});
    return Ok();
}
SystemResult SpawnVelFive(SpawnWithId<Bundle<Vel>> spawn) {
    spawn.New(5, Vel{5});
    return Ok();
}
SystemResult DespawnFive(Despawn despawn) {
    if (auto r = despawn.Entity(5); !r) return r;
    return Ok();
}
// OnAdd<Tag> (zone C): records Vel into the schedule's hook queue.
SystemResult TagToVel(HookCtx ctx, CommandsFor<Inserts<Vel>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Vel{9});
    return Ok();
}

void CheckZ5Across(Rig& rig, const std::string& tag, const std::string& where) {
    rig.world().Insert(5, Mode{5});
    const auto result = rig.Run("Frame");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[" << tag << "] the inner despawn was applied: " << Joined());
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::ZoneCDespawn), "[" << tag << "] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find(where) != std::string::npos,
                  "[" << tag << "-where] the message does not name " << where << ": " << result.error().Text());
    CHECK_MESSAGE(rig.world().Alive(5), "[" << tag << "] entity 5 was despawned");
    CHECK(rig.recorded.size() == 1);
}
} // namespace

TEST_CASE("J-56: an outer zone B insert into E and an inner despawn of E: the outer sync point drops the insert and counts it") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&InsertTagOnFive>(In("Early", "Frame", "Drive", Zone::B, {"Driver"}));
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    rig->world().Insert(5, Mode{5});
    Runs()["Driver"] = {"Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(!rig->world().Alive(5), "[ns-z5-b]");
    CHECK_MESSAGE(rig->world().DroppedMissingEntity() == 1, "[ns-z5-b] dropped " << rig->world().DroppedMissingEntity());
}

TEST_CASE("J-56: an outer zone C insert into E and an inner zone B despawn of E: Z5 at the inner sync point, E stays") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&InsertVelOnFive>(In("Early", "Frame", "Drive", Zone::C, {"Driver"}));
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    Runs()["Driver"] = {"Tick"};
    CheckZ5Across(*rig, "ns-z5-outer", "level 0 of 2 (schedule 'Frame')");
}

TEST_CASE("J-56: three levels, the zone C insert in the outermost: Z5 all the same") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&InsertVelOnFive>(In("Early", "Frame", "Drive", Zone::C, {"Driver"}));
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Drive>(In("Driver2", "Step", "S"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    Runs()["Driver"] = {"Step"};
    Runs()["Driver2"] = {"Tick"};
    CheckZ5Across(*rig, "ns-z5-three-levels", "level 0 of 3 (schedule 'Frame')");
}

TEST_CASE("J-56: the zone C insert is in the outer schedule's hook queue: Z5 all the same") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&InsertTagOnFive>(In("Seed", "Frame", "In"));
        r.Hook<Tag, &TagToVel>(HookTrigger::OnAdd, {.name = "TagToVel", .zone = Zone::C});
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    Runs()["Driver"] = {"Tick"};
    CheckZ5Across(*rig, "ns-z5-outer-hook", "hook 'TagToVel' of the outer run at level 0 of 2");
}

TEST_CASE("J-56: the outer zone C call spawns E with SpawnWithId: Z5 all the same") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&SpawnVelFive>(In("Early", "Frame", "Drive", Zone::C, {"Driver"}));
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    Runs()["Driver"] = {"Tick"};
    CheckZ5Across(*rig, "ns-z5-outer-withid", "level 0 of 2 (schedule 'Frame')");
}

// ---- J-56: the outer schedule's hook queue

namespace {
SystemResult InsertTagOnSix(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(6, Tag{6});
    return Ok();
}
// OnAdd<Tag> (zone B): records Mode into the schedule's hook queue.
SystemResult TagToMode(HookCtx ctx, CommandsFor<Inserts<Mode>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Mode{6});
    return Ok();
}
SystemResult LogModeAdd(HookCtx ctx) {
    Log().push_back("mode " + std::to_string(ctx.Entity()) + "@" + std::string(Current().schedule));
    return Ok();
}
SystemResult ProbeModeOfSix(Query<Read<Mode>> modes) {
    Log().push_back(std::string("probe 6 ") + (modes.Contains(6) ? "has Mode" : "has no Mode"));
    return Ok();
}
} // namespace

TEST_CASE("J-56: the outer schedule's hook queue is neither applied nor dropped by the inner schedule") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&InsertTagOnSix>(In("Seed", "Frame", "In"));
        r.Hook<Tag, &TagToMode>(HookTrigger::OnAdd, {.name = "TagToMode", .zone = Zone::B});
        r.Hook<Mode, &LogModeAdd>(HookTrigger::OnAdd, {.name = "LogModeAdd", .zone = Zone::B});
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&ProbeModeOfSix>(In("Probe", "Tick", "C"));
    });
    rig->world().Insert(6, Pos{6});
    Runs()["Driver"] = {"Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("probe 6 has no Mode"), "[ns-outer-hooks] " << Joined());
    CHECK_MESSAGE((Logged("mode 6@Frame") && !Logged("mode 6@Tick")), "[ns-outer-hooks] " << Joined());
    CHECK_MESSAGE(rig->world().Get<Mode>(6) != nullptr, "[ns-outer-hooks] the outer hook record was dropped");
}

// ---- J-56: despawned outside, spawned with the same id inside

namespace {
SystemResult DespawnSeven(Despawn despawn) {
    if (auto r = despawn.Entity(7); !r) return r;
    return Ok();
}
SystemResult SpawnSeven(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(7, Tag{77});
    return Ok();
}
void SameId(Registrar& r) {
    Nest(r);
    r.AddSystem<&DespawnSeven>(In("Leave", "Frame", "Drive", Zone::B, {"Driver"}));
    r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
    r.AddSystem<&SpawnSeven>(In("Join", "Tick", "A"));
}
} // namespace

TEST_CASE("J-56: despawned outside, spawned with the same id inside, the old one alive: the inner spawn is dropped, then the outer despawn") {
    auto rig = NestRig(SameId);
    rig->world().Insert(7, Mode{7});
    Runs()["Driver"] = {"Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    const std::vector<RejectedSpawn> rejected = rig->world().Diagnostics<RejectedSpawn>();
    REQUIRE_MESSAGE(rejected.size() == 1, "[ns-same-id-alive] " << rejected.size());
    CHECK_MESSAGE(rejected[0].reason == SpawnRejectReason::IdAlive, "[ns-same-id-alive]");
    CHECK_MESSAGE(rejected[0].schedule == "Tick", "[ns-same-id-alive] " << rejected[0].schedule);
    CHECK(rig->world().RejectedSpawns() == 1);
    CHECK_MESSAGE(!rig->world().Alive(7), "[ns-same-id-alive] 7 survived the outer despawn");
}

TEST_CASE("J-56: despawned outside, spawned with the same id inside, the old one gone: the outer despawn removes the new one") {
    auto rig = NestRig(SameId);
    Runs()["Driver"] = {"Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(!rig->world().Alive(7), "[ns-same-id-gone] the new 7 survived the outer despawn");
    CHECK(rig->world().Diagnostics<RejectedSpawn>().empty());
}

// ---- J-56: access verification

namespace {
Mode* g_mode = nullptr;
Flag* g_flag = nullptr;
SystemResult WriteSharedAndListed(ExclusiveWorld<Writes<Flag>> world) {
    for (int round = 0; round < 2; ++round) {
        if (auto r = world.Modify<Tag>(1, [](Tag& t) { t.t += 1; }); !r) return Engine::Base::Err(std::move(r).error());
        if (auto r = world.Modify<Flag>([](Flag& f) { f.on += 1; }); !r) return Engine::Base::Err(std::move(r).error());
        if (round == 0) {
            if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
        }
    }
    return Ok();
}
SystemResult TamperModeInside(Query<Read<Tag>>) {
    g_mode->m = 99;
    return Ok();
}
SystemResult TamperModeThenDrive(ExclusiveWorld<> world) {
    g_mode->m = 98;
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    return Ok();
}
SystemResult FlagThroughWorldApi(ExclusiveWorld<>) {
    if (Flag* flag = KeptWorld()->ResourceMut<Flag>()) flag->on = 5;
    return Ok();
}
SystemResult FlagThroughKeptAddress(ExclusiveWorld<>) {
    g_flag->on = 6;
    return Ok();
}
SystemResult DriveThenFlagThroughKeptAddress(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    g_flag->on = 7;
    return Ok();
}
} // namespace

TEST_CASE("J-56: verification on: the exclusive System's writes of Shared data and listed resources around a nested run are not reported") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&WriteSharedAndListed>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    rig->world().Insert(1, Tag{0});
    rig->world().SetResource(Flag{0});
    const auto result = rig->Run("Frame");
    CHECK_MESSAGE(static_cast<bool>(result), "[ns-ver-quiet] " << Text(result));
    CHECK(rig->world().Get<Tag>(1)->t == 2);
    CHECK(rig->world().Resource<Flag>()->on == 2);
}

TEST_CASE("J-56: verification on: an inner System's undeclared write is reported and named") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&TamperModeInside>(In("Tamper", "Tick", "A"));
    });
    rig->world().Insert(1, Mode{1});
    g_mode = const_cast<Mode*>(rig->world().Get<Mode>(1));
    Runs()["Driver"] = {"Tick"};
    const auto result = rig->Run("Frame");
    g_mode = nullptr;
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-ver-inner]");
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::BoundaryChange), "[ns-ver-inner] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find("'Tamper'") != std::string::npos, "[ns-ver-inner] " << result.error().Text());
    CHECK(rig->recorded.size() == 1);
}

TEST_CASE("J-56: verification on: a forbidden write before the nested run is the exclusive System's, and the inner schedule does not run") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&TamperModeThenDrive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    rig->world().Insert(1, Mode{1});
    g_mode = const_cast<Mode*>(rig->world().Get<Mode>(1));
    const auto result = rig->Run("Frame");
    g_mode = nullptr;
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-ver-before] the write before the nested run was not reported");
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::BoundaryChange), "[ns-ver-before] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find("'Driver'") != std::string::npos, "[ns-ver-before] " << result.error().Text());
    CHECK_MESSAGE(!Logged("InA@Tick#1"), "[ns-ver-before-not-run] the inner schedule ran: " << Joined());
    CHECK(rig->recorded.size() == 1);
}

TEST_CASE("J-56: verification on: an unlisted single-writer resource written through the World API is reported when it happens (W-5)") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&FlagThroughWorldApi>(In("Driver", "Frame", "Drive"));
    });
    rig->world().SetResource(Flag{0});
    KeptWorld() = &rig->world();
    const auto result = rig->Run("Frame");
    KeptWorld() = nullptr;
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::AccessViolation), "[ns-ver-w5] " << Text(result));
}

TEST_CASE("J-56: verification on: an unlisted single-writer resource written through a kept address is reported at the boundary") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&FlagThroughKeptAddress>(In("Driver", "Frame", "Drive"));
    });
    rig->world().SetResource(Flag{0});
    g_flag = rig->world().ResourceMut<Flag>();
    const auto result = rig->Run("Frame");
    g_flag = nullptr;
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::BoundaryChange), "[ns-ver-address] " << Text(result));
}

TEST_CASE("J-56: verification on: the same write through a kept address after a nested run is reported at the boundary") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&DriveThenFlagThroughKeptAddress>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    rig->world().SetResource(Flag{0});
    g_flag = rig->world().ResourceMut<Flag>();
    const auto result = rig->Run("Frame");
    g_flag = nullptr;
    CHECK_MESSAGE(HasCode(result, KernelErrorCode::BoundaryChange), "[ns-ver-address-after] " << Text(result));
    CHECK(Logged("InA@Tick#1"));
}

// ---- J-56: the handle and the stamp after a nested run

namespace {
SystemResult SeeChangedTag(Query<Read<Tag>, Changed<Tag>> changed) {
    Log().push_back("changed " + std::to_string(changed.Count()));
    return Ok();
}
SystemResult LogStamp(Query<Read<Tag>>) {
    Log().push_back("inner stamp " + std::to_string(Current().stamp));
    return Ok();
}
SystemResult DriveWriteDrive(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    if (auto r = world.Modify<Tag>(1, [](Tag& t) { t.t += 1; }); !r) return Engine::Base::Err(std::move(r).error());
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    return Ok();
}
SystemResult FailThenWrite(ExclusiveWorld<> world) {
    auto ran = world.RunSchedule("Tick");
    if (auto r = world.Modify<Tag>(1, [](Tag& t) { t.t = 41; }); !r) return Engine::Base::Err(std::move(r).error());
    return ran;
}
} // namespace

TEST_CASE("J-56: after a nested run the handle is valid and its writes take a new stamp, seen by Changed in the next inner run") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&DriveWriteDrive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&SeeChangedTag>(In("SeeChanged", "Tick", "A"));
        r.AddSystem<&LogStamp>(In("Stamp", "Tick", "C"));
    });
    rig->world().Insert(1, Tag{0});
    const auto result = rig->Run("Frame");
    REQUIRE_MESSAGE(static_cast<bool>(result), "[ns-stamp-handle] " << Text(result));
    // The first inner run sees the Host's insert; the second sees the Driver's write.
    CHECK_MESSAGE((Joined().find("changed 1 | inner stamp") == 0 && Log().size() == 4 && Log()[2] == "changed 1"),
                  "[ns-stamp-changed] " << Joined());
    // Tick's last stamp: the C stage's sync point, right after Stamp's step.
    REQUIRE(Log().size() >= 2);
    const std::string first = Log()[1];
    const ChangeStamp innerLast = std::stoull(first.substr(std::string("inner stamp ").size())) + 1;
    const auto stamps = rig->world().Stamps<Tag>(1);
    REQUIRE(stamps.has_value());
    // The second inner run's stamps are larger still; compare with the first one's last.
    CHECK_MESSAGE(stamps->second > innerLast, "[ns-stamp-after] changed " << stamps->second << ", inner last " << innerLast);
}

TEST_CASE("J-56: after the inner schedule fails the handle still writes, with no stale-handle report") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&FailThenWrite>(In("Driver", "Frame", "Drive"));
        // After Tick's first sync point, so the sync epoch has moved when the handle is used again.
        r.AddSystem<&Fails>(In("Fails", "Tick", "B"));
    });
    rig->world().Insert(1, Tag{0});
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(!result.error().Has(KernelErrorCode::StaleHandle), "[ns-stamp-after-error] " << result.error().Text());
    CHECK_MESSAGE(rig->world().Get<Tag>(1)->t == 41, "[ns-write-after-error]");
    CHECK(rig->recorded.size() == 1);
}

// ---- J-56: errors are reported once

namespace {
SystemResult DriveReturnsOther(ExclusiveWorld<> world) {
    static_cast<void>(world.RunSchedule("Step"));
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "other"));
}
SystemResult RunAgainAfterFailure(ExclusiveWorld<> world) {
    auto first = world.RunSchedule("Step");
    auto second = world.RunSchedule("Tick");
    Log().push_back(std::string("second ") +
                    (second ? "ok"
                            : std::string(ToString(second.error().code)) + " " + second.error().message + " at " +
                                  (second.error().context.empty() ? std::string("?") : second.error().context.back())));
    return first;
}
SystemResult CatchThenAfter(ExclusiveWorld<> world) {
    try {
        static_cast<void>(world.RunSchedule("Step"));
    } catch (const std::runtime_error&) {
        Log().push_back("caught");
    }
    auto again = world.RunSchedule("Step");
    Log().push_back(std::string("again ") + (again ? "ok" : std::string(ToString(again.error().code))));
    return Ok();
}
std::vector<std::string> LeafMessages(const KernelError& error) {
    std::vector<std::string> out;
    for (const KernelError* leaf : error.Leaves()) out.push_back(leaf->message);
    return out;
}
} // namespace

TEST_CASE("J-56: an inner System's Err is reported once, with the inner System, the inner schedule and the outer System") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Fails>(In("Fails", "Step", "S"));
        r.AddSystem<&LogCall>(In("After", "Frame", "Out"));
    });
    Runs()["Driver"] = {"Step"};
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(rig->recorded.size() == 1, "[ns-err-once] the handler was called " << rig->recorded.size() << " times");
    const std::string text = result.error().Text();
    CHECK_MESSAGE((text.find("'Fails' of schedule 'Step'") != std::string::npos && text.find("'Driver' of schedule 'Frame'") != std::string::npos),
                  "[ns-err-context] " << text);
    CHECK(rig->world().State() == WorldState::Interrupted);
    CHECK_MESSAGE(!Logged("After@Frame#1"), "[ns-err-stops] " << Joined());
}

TEST_CASE("J-56: the outer System returns another Err: the inner one is still in the list") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&DriveReturnsOther>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Fails>(In("Fails", "Step", "S"));
    });
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    const std::vector<std::string> leaves = LeafMessages(result.error());
    CHECK_MESSAGE((std::count(leaves.begin(), leaves.end(), "boom") == 1 && std::count(leaves.begin(), leaves.end(), "other") == 1),
                  "[ns-err-other] " << result.error().Text());
    CHECK(rig->recorded.size() == 1);
}

TEST_CASE("J-56: after an interruption RunSchedule runs nothing and returns the left error") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&RunAgainAfterFailure>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Fails>(In("Fails", "Step", "S"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(!Logged("InA@Tick#1"), "[ns-again] Tick ran after the interruption: " << Joined());
    CHECK_MESSAGE(Logged("second SystemFailed boom at Executor: step 0 'Fails' of schedule 'Step'"), "[ns-again] " << Joined());
    CHECK_MESSAGE(rig->recorded.size() == 1, "[ns-again] the handler was called " << rig->recorded.size() << " times");
}

TEST_CASE("J-56: an exception from the inner schedule caught by the outer System: the outer schedule stops, the handler gets NestedRunAborted once") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&CatchThenAfter>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Throws>(In("Throws", "Step", "S"));
        r.AddSystem<&LogCall>(In("After", "Frame", "Out"));
    });
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(Logged("caught"), "[ns-throw] " << Joined());
    CHECK_MESSAGE(!Logged("After@Frame#1"), "[ns-throw] the outer schedule went on: " << Joined());
    CHECK_MESSAGE((rig->recorded.size() == 1 && rig->recorded[0].Has(KernelErrorCode::NestedRunAborted)),
                  "[ns-throw] " << rig->recorded.size() << " " << (rig->recorded.empty() ? "" : rig->recorded[0].Text()));
    CHECK(rig->world().State() == WorldState::Interrupted);
}

TEST_CASE("J-56: after a caught exception the same System's RunSchedule returns the left error, not 'running already'") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&CatchThenAfter>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Throws>(In("Throws", "Step", "S"));
    });
    static_cast<void>(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("again NestedRunAborted"), "[ns-throw-again] " << Joined());
}

TEST_CASE("J-56: the left error is the innermost one") {
    // A runs Step, Step's driver runs Deep, Deep fails; A's driver then runs Tick and gets the left error.
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&RunAgainAfterFailure>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Drive>(In("Driver2", "Step", "S"));
        r.AddSystem<&Fails>(In("Fails", "Deep", "D"));
    });
    Runs()["Driver2"] = {"Deep"};
    static_cast<void>(rig->Run("Frame"));
    // Left by Deep's level, not re-wrapped by Step's.
    CHECK_MESSAGE(Logged("second SystemFailed boom at Executor: step 0 'Fails' of schedule 'Deep'"), "[ns-left-innermost] " << Joined());
    REQUIRE(rig->recorded.size() == 1);
    // The reported error names all three levels; the left one stops at Deep's executor.
    const std::string text = rig->recorded[0].Text();
    CHECK_MESSAGE((text.find("of schedule 'Deep'") != std::string::npos && text.find("of schedule 'Step'") != std::string::npos),
                  "[ns-left-innermost] " << text);
}

// ---- J-56: refused calls (each swallowed by the outer System: still reported once, the World interrupted)

namespace {
SystemResult RunInsideClosure(ExclusiveWorld<> world) {
    auto r = world.Read<Tag>(1, [&world](const Tag&) {
        auto ran = world.RunSchedule("Tick");
        Log().push_back(std::string("inside ") + (ran ? "ok" : std::string(ToString(ran.error().code))));
    });
    static_cast<void>(r);
    return Ok();
}
SystemResult DespawnSix(Despawn despawn) {
    if (auto r = despawn.Entity(6); !r) return r;
    return Ok();
}
SystemResult HookRunsSchedule(ExclusiveWorld<> world) {
    auto ran = world.RunSchedule("Tick");
    Log().push_back(std::string("hook ") + (ran ? "ok" : std::string(ToString(ran.error().code))));
    return Ok();
}
struct Refusal {
    const char* name;
    std::vector<std::string> runs;
    KernelErrorCode code;
    const char* rule;
};

void CheckSwallowed(Rig& rig, KernelErrorCode code, const char* rule, const std::string& tag) {
    const auto result = rig.Run("Frame");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[" << tag << "] a swallowed refusal let the World go on");
    CHECK_MESSAGE(result.error().Has(code), "[" << tag << "] " << result.error().Text());
    if (std::string_view(rule).empty() == false) {
        bool ruled = false;
        for (const KernelError* leaf : result.error().Leaves()) ruled = ruled || leaf->rule == rule;
        CHECK_MESSAGE(ruled, "[" << tag << "] rule " << rule << ": " << result.error().Text());
    }
    CHECK_MESSAGE(rig.world().State() == WorldState::Interrupted, "[" << tag << "]");
    CHECK_MESSAGE(rig.recorded.size() == 1, "[" << tag << "] the handler was called " << rig.recorded.size() << " times");
    CHECK_MESSAGE(!Logged("After@Frame#1"), "[" << tag << "] the outer schedule went on: " << Joined());
}

void SwallowRig(Registrar& r) {
    Nest(r);
    r.AddSystem<&DriveSwallow>(In("Driver", "Frame", "Drive"));
    r.AddSystem<&Fails>(In("Fails", "Step", "S"));
    r.AddSystem<&LogCall>(In("After", "Frame", "Out"));
    ScheduleOptions extract = Schedule("Extract", Domain::Frame, false, {"E"});
    extract.extract = true;
    r.Schedule(extract);
}
} // namespace

TEST_CASE("J-56: refused or failed and swallowed: an inner Err") {
    auto rig = NestRig(SwallowRig);
    Runs()["Driver"] = {"Step"};
    CheckSwallowed(*rig, KernelErrorCode::SystemFailed, "", "ns-swallow-inner");
}

TEST_CASE("J-56: refused and swallowed: the running schedule itself") {
    auto rig = NestRig(SwallowRig);
    Runs()["Driver"] = {"Frame"};
    CheckSwallowed(*rig, KernelErrorCode::ScheduleRunning, "X-11", "ns-self");
}

TEST_CASE("J-56: refused and swallowed: an outer level's schedule (A runs B, B runs A)") {
    auto rig = NestRig([](Registrar& r) {
        SwallowRig(r);
        r.AddSystem<&DriveSwallow>(In("Driver2", "Step", "S", Zone::B, {"Fails"}));
    });
    Runs()["Driver"] = {"Step"};
    Runs()["Driver2"] = {"Frame"};
    // Driver2 runs before Fails and swallows; the World is interrupted, so Fails does not run.
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::ScheduleRunning), "[ns-self-outer] " << result.error().Text());
    CHECK_MESSAGE(!Logged("fails Fails@Step"), "[ns-self-outer] " << Joined());
    CHECK(rig->recorded.size() == 1);
}

TEST_CASE("J-56: refused and swallowed: an unknown schedule") {
    auto rig = NestRig(SwallowRig);
    Runs()["Driver"] = {"NoSuchSchedule"};
    CheckSwallowed(*rig, KernelErrorCode::UnknownSchedule, "X-12", "ns-unknown");
}

TEST_CASE("J-56: refused and swallowed: an extract schedule") {
    auto rig = NestRig(SwallowRig);
    Runs()["Driver"] = {"Extract"};
    CheckSwallowed(*rig, KernelErrorCode::ExtractNotNestable, "X-15", "ns-extract");
}

TEST_CASE("J-56: refused and swallowed: RunSchedule inside a Read closure") {
    auto rig = NestRig([](Registrar& r) {
        SwallowRig(r);
        r.AddSystem<&RunInsideClosure>(In("Closure", "Frame", "Drive", Zone::B, {}, {"Driver"}));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    rig->world().Insert(1, Tag{1});
    CheckSwallowed(*rig, KernelErrorCode::ClosureRunsSchedule, "X-14", "ns-closure-run");
    CHECK_MESSAGE((Logged("inside ClosureRunsSchedule") && !Logged("InA@Tick#1")), "[ns-closure-run] " << Joined());
}

TEST_CASE("J-56: refused and swallowed: RunSchedule in a hook") {
    auto rig = NestRig([](Registrar& r) {
        SwallowRig(r);
        // OnRemove of a despawn: the hook runs before the entity's cells go, so no membership has changed yet.
        r.AddSystem<&DespawnSix>(In("Leave", "Frame", "In"));
        r.Hook<Tag, &HookRunsSchedule>(HookTrigger::OnRemove, {.name = "HookRuns", .zone = Zone::B});
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    rig->world().Insert(6, Tag{6});
    CheckSwallowed(*rig, KernelErrorCode::HookRunsSchedule, "X-13", "ns-hook-refused");
    CHECK_MESSAGE((Logged("hook HookRunsSchedule") && !Logged("InA@Tick#1")), "[ns-hook-refused] " << Joined());
}

TEST_CASE("J-56: refused and swallowed: Startup, and another schedule while Startup runs") {
    {
        auto rig = NestRig([](Registrar& r) {
            SwallowRig(r);
            r.Schedule(BootSchedule());
        });
        Runs()["Driver"] = {"Boot"};
        CheckSwallowed(*rig, KernelErrorCode::StartupNotNestable, "H-12", "ns-swallow-startup");
    }
    {
        auto rig = NestRig([](Registrar& r) {
            SwallowRig(r);
            r.Schedule(BootSchedule());
            r.AddSystem<&DriveSwallow>(In("BootDriver", "Boot", "S"));
        });
        Runs()["BootDriver"] = {"Tick"};
        const auto result = rig->Run("Frame");
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK_MESSAGE(result.error().Has(KernelErrorCode::StartupRunning), "[ns-swallow-startup-running] " << result.error().Text());
        CHECK_MESSAGE(result.error().Text().find("[H-13]") != std::string::npos, "[ns-swallow-startup-running] rule: " << result.error().Text());
        CHECK(rig->recorded.size() == 1);
        CHECK(rig->world().State() == WorldState::Interrupted);
    }
}

// ---- J-56: two levels, hooks inside, events inside

namespace {
SystemResult LogAfterDrive(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    Log().push_back("step driver back " + Here());
    return Ok();
}
SystemResult SeedNinety(SpawnWithId<Bundle<Mode>> spawn) {
    spawn.New(90, Mode{90});
    return Ok();
}
SystemResult ReadTagOfNinety(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    auto tag = world.Read<Tag>(90, [](const Tag& t) { return t.t; });
    if (!tag) return Engine::Base::Err(std::move(tag).error());
    Log().push_back(*tag ? "90 has Tag " + std::to_string(**tag) : std::string("90 has no Tag"));
    return Ok();
}
SystemResult HookLogsHere(HookCtx) {
    Log().push_back("hook " + Here());
    return Ok();
}
SystemResult SendPing(EventWriter<Ping> writer, CurrentTick tick) {
    writer.Send(Ping{static_cast<std::uint32_t>(tick.Tick())});
    return Ok();
}
} // namespace

TEST_CASE("J-56: two levels: A runs B, B's exclusive System runs C; order and ticks") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogAfterDrive>(In("Driver2", "Step", "S"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
        r.AddSystem<&LogCall>(In("After", "Frame", "Out"));
    });
    Runs()["Driver"] = {"Step", "Step"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Joined() == "drive Driver@Frame | InA@Tick#1 | step driver back Driver2@Step | InA@Tick#2 | step driver back Driver2@Step | "
                              "driven Driver@Frame | After@Frame#1",
                  "[ns-two-levels] " << Joined());
    CHECK(rig->world().Tick(Domain::Fixed) == 2);
    CHECK(rig->world().Tick(Domain::Frame) == 1);
}

TEST_CASE("J-56: the inner schedule's hooks run, and their records are applied when it ends") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ReadTagOfNinety>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&SeedNinety>(In("Seed", "Tick", "A"));
        r.Hook<Mode, &ModeAddsTag>(HookTrigger::OnAdd, {.name = "ModeAddsTag", .zone = Zone::B});
        r.Hook<Mode, &HookLogsHere>(HookTrigger::OnAdd, {.name = "HookLogs", .zone = Zone::B});
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("hook HookLogs@Tick"), "[ns-hook-inner] " << Joined());
    CHECK_MESSAGE(Logged("90 has Tag 90"), "[ns-hook-inner] " << Joined());
}

TEST_CASE("J-56: events written inside are trimmed by the inner clock schedule's ticks") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&SendPing>(In("Ping", "Tick", "A"));
    });
    Runs()["Driver"] = {"Tick", "Tick", "Tick"};
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    const std::vector<Ping> pings = rig->world().EventPayloads<Ping>();
    CHECK_MESSAGE((pings.size() == 1 && pings[0].n == 3), "[ns-events] " << pings.size());
}

// ---- J-57: closures

namespace {

template <class F>
concept ReadAccepts = requires(ExclusiveWorld<>& world, F f) { world.Read<Tag>(EntityId{1}, f); };
template <class F>
concept ModifyAccepts = requires(ExclusiveWorld<>& world, F f) { world.Modify<Tag>(EntityId{1}, f); };
template <class F>
concept ResourceReadAccepts = requires(ExclusiveWorld<>& world, F f) { world.Read<Out>(f); };
template <class W>
concept HasGet = requires(W& world) { world.template Get<Tag>(EntityId{1}); };
template <class W>
concept HasGetMut = requires(W& world) { world.template GetMut<Tag>(EntityId{1}); };

struct Copied {
    std::int32_t a;
    std::int32_t b;
};

const std::int32_t kTwo[2] = {1, 2};
std::vector<std::int32_t> g_values{1, 2, 3};

SystemResult ReadsCopies(ExclusiveWorld<> world) {
    auto value = world.Read<Tag>(1, [](const Tag& t) { return t.t; });
    auto text = world.Read<Tag>(1, [](const Tag& t) { return std::to_string(t.t); });
    auto list = world.Read<Tag>(1, [](const Tag& t) { return std::vector<std::int32_t>{t.t, t.t}; });
    auto maybe = world.Read<Tag>(1, [](const Tag& t) { return std::optional<std::int32_t>(t.t); });
    auto whole = world.Read<Tag>(1, [](const Tag& t) { return t; });
    auto pair = world.Read<Tag>(1, [](const Tag& t) { return std::pair<std::int32_t, std::string>(t.t, "x"); });
    bool called = false;
    auto done = world.Read<Tag>(1, [&called](const Tag&) { called = true; });
    if (!value || !text || !list || !maybe || !whole || !pair || !done) return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "read"));
    Log().push_back("value " + std::to_string(**value) + " text " + **text + " list " + std::to_string((*list)->size()) +
                    " maybe " + std::to_string(***maybe) + " whole " + std::to_string((*whole)->t) + " pair " +
                    std::to_string((*pair)->first) + " done " + (*done && called ? "yes" : "no"));
    return Ok();
}
SystemResult ReadsAbsent(ExclusiveWorld<> world) {
    bool called = false;
    auto value = world.Read<Tag>(2, [&called](const Tag& t) {
        called = true;
        return t.t;
    });
    auto written = world.Modify<Tag>(2, [&called](Tag& t) {
        called = true;
        t.t = 1;
    });
    if (!value || !written) return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "absent"));
    Log().push_back(std::string("absent ") + (*value ? "value" : "nothing") + (*written ? " written" : " not written") +
                    (called ? " called" : " not called"));
    return Ok();
}
SystemResult ClosureInClosure(ExclusiveWorld<> world) {
    auto sum = world.Read<Mode>(1, [&world](const Mode& mode) {
        auto tag = world.Read<Tag>(1, [](const Tag& t) { return t.t; });
        return mode.m + (tag && *tag ? **tag : 1000);
    });
    if (!sum || !*sum) return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "nested"));
    Log().push_back("sum " + std::to_string(**sum));
    return Ok();
}

} // namespace

TEST_CASE("J-57: a closure returns a copy: a value, a string, a vector, an optional, a struct, a pair of copies, or nothing") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ReadsCopies>(In("Driver", "Frame", "Drive"));
    });
    rig->world().Insert(1, Tag{7});
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("value 7 text 7 list 2 maybe 7 whole 7 pair 7 done yes"), "[ns-closure-copies] " << Joined());
    // std::optional is not a view today; C++26 makes it one (declaration section 9): this is the case that notices.
    CHECK_MESSAGE(ReadAccepts<decltype([](const Tag& t) { return std::optional<std::int32_t>(t.t); })>,
                  "[ns-closure-optional] std::optional<int> is refused: the standard library counts it as a view");
    CHECK(ReadAccepts<decltype([](const Tag& t) { return std::string(1, static_cast<char>('a' + t.t)); })>);
    CHECK(ReadAccepts<decltype([](const Tag&) { return std::vector<std::int32_t>{}; })>);
}

TEST_CASE("J-57: a closure may not return what points into the World (pointer, reference, Mut, reference_wrapper, view, span, iterator; inside pair and tuple)") {
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) { return &t; })>, "[ns-closure-pointer]");
    // A reference is checked on the constraint itself: with the clause gone, a requires-expression on Read
    // would have to form Result<optional<const Tag&>>, a hard error rather than a false.
    CHECK_MESSAGE(!(Detail::ExclusiveClosure<decltype([](const Tag& t) -> const Tag& { return t; }), const Tag&>),
                  "[ns-closure-reference]");
    CHECK_MESSAGE(!(Detail::ExclusiveClosure<decltype([](Tag& t) -> Tag& { return t; }), Tag&>), "[ns-closure-reference] Modify");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) { return Mut<Tag>(); })>, "[ns-closure-mut]");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) { return std::cref(t); })>, "[ns-closure-refwrap]");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) { return std::string_view("x"); })>, "[ns-closure-view] string_view");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) { return std::ranges::ref_view(g_values); })>, "[ns-closure-view] ref_view");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) { return std::span<const std::int32_t>(kTwo); })>, "[ns-closure-view] span");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) { return std::span<const std::int32_t, 2>(kTwo); })>,
                  "[ns-closure-span] fixed-extent span");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) { return g_values.begin(); })>, "[ns-closure-iterator]");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) { return std::tie(t); })>, "[ns-closure-pair-tuple] std::tie");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) { return std::pair<const Tag*, std::int32_t>(&t, 1); })>,
                  "[ns-closure-pair-tuple] pair with a pointer");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) { return std::tuple<std::int32_t, const Tag*>(1, &t); })>,
                  "[ns-closure-pair-tuple] tuple with a pointer");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) {
                      return std::pair<std::pair<const Tag*, std::int32_t>, std::int32_t>({&t, 1}, 2);
                  })>,
                  "[ns-closure-recursion] a pair inside a pair");
    CHECK_MESSAGE(!ResourceReadAccepts<decltype([](const Out& o) { return &o; })>, "[ns-closure-pointer] resource");
    // Accepted, for contrast: the requires-expression is not false for every closure.
    CHECK(ReadAccepts<decltype([](const Tag& t) { return t.t; })>);
    CHECK(ResourceReadAccepts<decltype([](const Out& o) { return o.value; })>);
}

TEST_CASE("J-57: ExclusiveWorld has no Get and no GetMut") {
    CHECK_MESSAGE(!HasGet<ExclusiveWorld<>>, "[ns-no-get]");
    CHECK_MESSAGE(!HasGetMut<ExclusiveWorld<>>, "[ns-no-get]");
    CHECK_MESSAGE(!HasGet<ExclusiveWorld<Writes<Flag>>>, "[ns-no-get]");
}

TEST_CASE("J-57: an entity without the component: the closure is not called and the result says nothing") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ReadsAbsent>(In("Driver", "Frame", "Drive"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("absent nothing not written not called"), "[ns-absent] " << Joined());
}

TEST_CASE("J-57: a closure inside a closure reads another component") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ClosureInClosure>(In("Driver", "Frame", "Drive"));
    });
    rig->world().Insert(1, Mode{3});
    rig->world().Insert(1, Tag{4});
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("sum 7"), "[ns-closure-nested] " << Joined());
}

// ---- J-57: resources

namespace {
struct Pinned { std::int32_t value; };  // resource, zone B, affinity "Render"

SystemResult ReadsResources(ExclusiveWorld<> world) {
    auto out = world.Read<Out>([](const Out& o) { return o.value; });
    auto flag = world.Read<Flag>([](const Flag& f) { return f.on; });
    auto c = world.Read<CRes>([](const CRes& v) { return v.value; });
    if (!out || !flag || !c) return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "read"));
    Log().push_back("out " + (*out ? std::to_string(**out) : std::string("absent")) + " flag " +
                    (*flag ? std::to_string(**flag) : std::string("absent")) + " c " +
                    (*c ? std::to_string(**c) : std::string("absent")));
    return Ok();
}
SystemResult ReadsAbsentResource(ExclusiveWorld<> world) {
    bool called = false;
    auto out = world.Read<Out>([&called](const Out& o) {
        called = true;
        return o.value;
    });
    if (!out) return Engine::Base::Err(std::move(out).error());
    Log().push_back(std::string("absent out ") + (*out ? "value" : "nothing") + (called ? " called" : " not called"));
    return Ok();
}
SystemResult ReadsPinned(ExclusiveWorld<> world) {
    auto pinned = world.Read<Pinned>([](const Pinned& p) { return p.value; });
    if (!pinned) return Engine::Base::Err(std::move(pinned).error());
    return Ok();
}
SystemResult ReadsSettings(ExclusiveWorld<> world) {
    auto settings = world.Read<KernelSettings>([](const KernelSettings& s) { return s.accessVerification; });
    if (!settings) return Engine::Base::Err(std::move(settings).error());
    return Ok();
}
SystemResult ModifiesSettings(ExclusiveWorld<> world) {
    auto settings = world.Modify<KernelSettings>([](KernelSettings& s) { s.accessVerification = false; });
    if (!settings) return Engine::Base::Err(std::move(settings).error());
    return Ok();
}
SystemResult WritesListedFlag(ExclusiveWorld<Writes<Flag>> world) {
    auto flag = world.Modify<Flag>([](Flag& f) { f.on = 11; });
    auto out = world.Modify<Out>([](Out& o) { o.value = 12; });
    if (!flag || !out) return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "listed"));
    return Ok();
}
SystemResult WritesSharedOut(ExclusiveWorld<> world) {
    auto out = world.Modify<Out>([](Out& o) { o.value = 13; });
    if (!out) return Engine::Base::Err(std::move(out).error());
    return Ok();
}
SystemResult WritesUnlistedFlag(ExclusiveWorld<> world) {
    auto flag = world.Modify<Flag>([](Flag& f) { f.on = 14; });
    if (!flag) return Engine::Base::Err(std::move(flag).error());
    return Ok();
}
SystemResult AlsoWritesFlag(ResMut<Flag> flag) {
    if (flag.Present()) flag->on += 1;
    return Ok();
}
SystemResult ListsZoneC(ExclusiveWorld<Writes<CRes>>) { return Ok(); }
SystemResult ListsSettings(ExclusiveWorld<Writes<KernelSettings>>) { return Ok(); }
SystemResult ListedAndMore(ExclusiveWorld<Writes<Flag>>, Res<Out>) { return Ok(); }
SystemResult HookWithWrites(ExclusiveWorld<Writes<Flag>>) { return Ok(); }
SystemResult Ingress(ExclusiveWorld<>) { return Ok(); }

void ResourceRig(Registrar& r) {
    Nest(r);
    r.Resource<Pinned>({.name = "Pinned", .zone = Zone::B, .affinity = Affinity::Named("Render")});
}
} // namespace

TEST_CASE("J-57: resources: any is read; one absent is not; KernelSettings is Err") {
    {
        auto rig = NestRig([](Registrar& r) {
            ResourceRig(r);
            r.AddSystem<&ReadsResources>(In("Driver", "Frame", "Drive"));
        });
        rig->world().SetResource(Out{1});
        rig->world().SetResource(Flag{2});
        rig->world().SetResource(CRes{3});
        KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
        CHECK_MESSAGE(Logged("out 1 flag 2 c 3"), "[ns-res-read] " << Joined());
    }
    {
        auto rig = NestRig([](Registrar& r) {
            ResourceRig(r);
            r.AddSystem<&ReadsAbsentResource>(In("Driver", "Frame", "Drive"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
        CHECK_MESSAGE(Logged("absent out nothing not called"), "[ns-res-absent] " << Joined());
    }
}

TEST_CASE("J-57: resources: one whose affinity the runner does not satisfy is Err, the World interrupted, the message names the System and the resource") {
    auto rig = NestRig([](Registrar& r) {
        ResourceRig(r);
        r.AddSystem<&ReadsPinned>(In("ReadsPinned", "Frame", "Drive"));
    });
    rig->world().SetResource(Pinned{1});
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-res-affinity] read");
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::UnreadableResource), "[ns-res-affinity] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find("[S-16]") != std::string::npos, "[ns-res-affinity] rule: " << result.error().Text());
    const std::string text = result.error().Text();
    CHECK_MESSAGE((text.find("'Pinned'") != std::string::npos && text.find("'ReadsPinned'") != std::string::npos &&
                      text.find("Render") != std::string::npos),
                  "[ns-res-affinity] " << text);
    CHECK(rig->world().State() == WorldState::Interrupted);
}

TEST_CASE("J-57: resources: a listed single-writer resource and a Shared one are written; an unlisted single-writer one is Err") {
    {
        auto rig = NestRig([](Registrar& r) {
            ResourceRig(r);
            r.AddSystem<&WritesListedFlag>(In("Driver", "Frame", "Drive"));
        });
        rig->world().SetResource(Flag{0});
        rig->world().SetResource(Out{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
        CHECK_MESSAGE((rig->world().Resource<Flag>()->on == 11 && rig->world().Resource<Out>()->value == 12), "[ns-res-listed]");
    }
    {
        auto rig = NestRig([](Registrar& r) {
            ResourceRig(r);
            r.AddSystem<&WritesSharedOut>(In("Driver", "Frame", "Drive"));
        });
        rig->world().SetResource(Out{0});
        const auto result = rig->Run("Frame");
        CHECK_MESSAGE(static_cast<bool>(result), "[ns-res-shared] " << Text(result));
        CHECK(rig->world().Resource<Out>()->value == 13);
    }
    {
        auto rig = NestRig([](Registrar& r) {
            ResourceRig(r);
            r.AddSystem<&WritesUnlistedFlag>(In("Driver", "Frame", "Drive"));
        });
        rig->world().SetResource(Flag{0});
        const auto result = rig->Run("Frame");
        REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-res-unlisted] an unlisted single-writer resource was written");
        CHECK_MESSAGE(result.error().Has(KernelErrorCode::UndeclaredResourceWrite), "[ns-res-unlisted] " << result.error().Text());
        CHECK(rig->world().Resource<Flag>()->on == 0);
    }
}

TEST_CASE("J-57: declarations: a listed zone C resource is Z2, KernelSettings is D-E18, ingress is S-17, a hook with Writes is S-18") {
    const Built zoneC = Finish({{"test", [](Registrar& r) {
                                    Nest(r);
                                    r.AddSystem<&ListsZoneC>(In("ListsZoneC", "Frame", "Drive"));
                                }}});
    CHECK_MESSAGE(HasRule(zoneC, StartupCode::DE7, "Z2"), "[ns-decl-zone-c] " << Describe(zoneC));
    const Built settings = Finish({{"test", [](Registrar& r) {
                                       Nest(r);
                                       r.AddSystem<&ListsSettings>(In("ListsSettings", "Frame", "Drive"));
                                   }}});
    CHECK_MESSAGE(HasError(settings, StartupCode::DE18), "[ns-decl-settings] " << Describe(settings));
    const Built ingress = Finish({{"test", [](Registrar& r) {
                                      Nest(r);
                                      SystemOptions options = In("Ingress", "Frame", "In");
                                      options.ingress = true;
                                      r.AddSystem<&Ingress>(options);
                                  }}});
    CHECK_MESSAGE(HasRule(ingress, StartupCode::DE7, "S-17"), "[ns-decl-ingress] " << Describe(ingress));
    const Built inStage = Finish({{"test", [](Registrar& r) {
                                      Nest(r);
                                      r.AddSystem<&Ingress>(In("NotIngress", "Frame", "In"));
                                  }}});
    CHECK_MESSAGE(static_cast<bool>(inStage.result), "[ns-decl-ingress-stage] " << Describe(inStage));
    const Built hook = Finish({{"test", [](Registrar& r) {
                                   Nest(r);
                                   r.Hook<Tag, &HookWithWrites>(HookTrigger::OnAdd, {.name = "HookWithWrites", .zone = Zone::B});
                               }}});
    CHECK_MESSAGE(HasRule(hook, StartupCode::DE7, "S-18"), "[ns-decl-hook] " << Describe(hook));
}

TEST_CASE("J-10: ExclusiveWorld<Writes<...>> is the only parameter too") {
    CHECK_MESSAGE(SystemRejectionOfFunction<&ListedAndMore>() == Rejection::ExclusiveWorldNotAlone, "[ns-decl-alone]");
    CHECK(SystemRejectionOfFunction<&WritesListedFlag>() == Rejection::None);
}

TEST_CASE("J-15: a resource an ExclusiveWorld lists has a second writer: D-E2 names both") {
    const Built built = Finish({{"test", [](Registrar& r) {
                                    Nest(r);
                                    r.AddSystem<&WritesListedFlag>(In("ListsFlag", "Frame", "Drive"));
                                    r.AddSystem<&AlsoWritesFlag>(In("AlsoWritesFlag", "Tick", "A"));
                                }}});
    REQUIRE_MESSAGE(HasError(built, StartupCode::DE2), "[ns-res-counted] " << Describe(built));
    bool both = false;
    for (const StartupError& error : built.result.error().errors) {
        if (!error.Is(StartupCode::DE2)) continue;
        const auto& s = error.subjects;
        both = both || (std::find(s.begin(), s.end(), "ListsFlag") != s.end() && std::find(s.begin(), s.end(), "AlsoWritesFlag") != s.end());
    }
    CHECK_MESSAGE(both, "[ns-res-counted] " << Describe(built));
}

TEST_CASE("J-50: KernelSettings read or written through ExclusiveWorld is Err at run time") {
    for (const bool modify : {false, true}) {
        CAPTURE(modify);
        auto rig = NestRig([modify](Registrar& r) {
            Nest(r);
            if (modify) {
                r.AddSystem<&ModifiesSettings>(In("Driver", "Frame", "Drive"));
            } else {
                r.AddSystem<&ReadsSettings>(In("Driver", "Frame", "Drive"));
            }
        });
        const auto result = rig->Run("Frame");
        REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-res-settings] KernelSettings was reached");
        CHECK_MESSAGE(result.error().Has(KernelErrorCode::UnreadableResource), "[ns-res-settings] " << result.error().Text());
        CHECK_MESSAGE(result.error().Text().find("[S-15]") != std::string::npos, "[ns-res-settings] rule: " << result.error().Text());
        CHECK(rig->world().State() == WorldState::Interrupted);
    }
}

// ---- J-58: Startup run by the kernel

namespace {
SystemResult BootNote(Query<Read<Tag>>) {
    Log().push_back("boot " + Here() + " fixed tick " + std::to_string(KeptWorld() != nullptr ? KeptWorld()->Tick(Domain::Fixed) : 99));
    return Ok();
}
SystemResult FirstStamp(Query<Read<Tag>>) {
    Log().push_back("first stamp " + std::to_string(Current().stamp));
    return Ok();
}
SystemResult BootSeedsMode(SpawnWithId<Bundle<Mode>> spawn) {
    spawn.New(30, Mode{30});
    return Ok();
}
SystemResult BootFails(Query<Read<Tag>>) {
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "boot failed"));
}
void BootRig(Registrar& r) {
    Nest(r);
    r.Schedule(BootSchedule());
    r.AddSystem<&BootNote>(In("BootNote", "Boot", "S"));
    r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
}
} // namespace

TEST_CASE("J-58: the kernel runs Startup once, before the first System of the first other schedule") {
    {
        auto rig = NestRig(BootRig);
        KeptWorld() = &rig->world();
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
        KeptWorld() = nullptr;
        CHECK_MESSAGE(std::count_if(Log().begin(), Log().end(), [](const std::string& l) { return l.rfind("boot ", 0) == 0; }) == 1,
                      "[ns-st-once] " << Joined());
        CHECK_MESSAGE((!Log().empty() && Log().front().rfind("boot BootNote@Boot", 0) == 0), "[ns-st-first] " << Joined());
        CHECK_MESSAGE((Logged("InA@Tick#1") && Logged("InA@Tick#2")), "[ns-st-auto] " << Joined());
    }
    {
        // No Startup: nothing happens first.
        auto rig = NestRig([](Registrar& r) {
            Nest(r);
            r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
        CHECK(Joined() == "InA@Tick#1");
    }
}

TEST_CASE("J-58: Startup runs before the tick advances: its Systems read the old tick, the first tick's stamp is the schedule's own") {
    auto rig = NestRig([](Registrar& r) {
        BootRig(r);
        r.AddSystem<&FirstStamp>(In("FirstStamp", "Tick", "A", Zone::B, {"InA"}));
    });
    KeptWorld() = &rig->world();
    KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
    KeptWorld() = nullptr;
    CHECK_MESSAGE(Logged("boot BootNote@Boot fixed tick 0"), "[ns-st-tick] " << Joined());
    const auto first = std::find_if(Log().begin(), Log().end(), [](const std::string& l) { return l.rfind("first stamp ", 0) == 0; });
    REQUIRE(first != Log().end());
    const ChangeStamp stamp = std::stoull(first->substr(std::string("first stamp ").size()));
    const std::optional<ChangeStamp> tickStamp = rig->world().TickStamp(Domain::Fixed, 1);
    REQUIRE(tickStamp.has_value());
    CHECK_MESSAGE(*tickStamp == stamp, "[ns-st-stamp] tick 1 starts at " << *tickStamp << ", the schedule's first step took " << stamp);
}

TEST_CASE("J-58: the hook records of Startup's last sync point are applied by its own hook rounds; verification on, no false report") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.Schedule(BootSchedule());
        r.AddSystem<&BootSeedsMode>(In("BootSeeds", "Boot", "S"));
        r.Hook<Mode, &ModeAddsTag>(HookTrigger::OnAdd, {.name = "ModeAddsTag", .zone = Zone::B});
        r.Hook<Tag, &LogTagAdd>(HookTrigger::OnAdd, {.name = "LogTagAdd", .zone = Zone::B});
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    const auto result = rig->Run("Tick");
    CHECK_MESSAGE(static_cast<bool>(result), "[ns-st-verify] " << Text(result));
    CHECK_MESSAGE(Logged("add 30@Boot"), "[ns-st-rounds] " << Joined());
    CHECK_MESSAGE(rig->world().Get<Tag>(30) != nullptr, "[ns-st-rounds]");
}

TEST_CASE("J-58: the main program runs Startup first: it runs once in all") {
    auto rig = NestRig(BootRig);
    KERNEL_REQUIRE_RUN_OK(rig->Run("Boot"));
    KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
    CHECK_MESSAGE(std::count_if(Log().begin(), Log().end(), [](const std::string& l) { return l.rfind("boot ", 0) == 0; }) == 1,
                  "[ns-st-main-first] " << Joined());
}

TEST_CASE("J-58: a System runs Startup: Err, after Startup ran and in the Run that triggered it") {
    for (const bool explicitFirst : {true, false}) {
        CAPTURE(explicitFirst);
        auto rig = NestRig([](Registrar& r) {
            BootRig(r);
            r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        });
        Runs()["Driver"] = {"Boot"};
        if (explicitFirst) KERNEL_REQUIRE_RUN_OK(rig->Run("Boot"));
        const auto result = rig->Run("Frame");
        REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-st-nested] a System ran Startup");
        CHECK_MESSAGE(result.error().Has(KernelErrorCode::StartupNotNestable), "[ns-st-nested] " << result.error().Text());
        CHECK_MESSAGE(std::count_if(Log().begin(), Log().end(), [](const std::string& l) { return l.rfind("boot ", 0) == 0; }) == 1,
                      "[ns-st-nested] " << Joined());
    }
}

TEST_CASE("J-58: an exclusive System of Startup runs another schedule: Err naming it and the schedule") {
    auto rig = NestRig([](Registrar& r) {
        BootRig(r);
        r.AddSystem<&Drive>(In("BootDriver", "Boot", "S"));
    });
    Runs()["BootDriver"] = {"Tick"};
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-st-running] Startup ran another schedule");
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::StartupRunning), "[ns-st-running] " << result.error().Text());
    const std::string text = result.error().Text();
    CHECK_MESSAGE((text.find("'BootDriver'") != std::string::npos && text.find("'Tick'") != std::string::npos), "[ns-st-running] " << text);
    CHECK_MESSAGE(!Logged("InA@Tick#1"), "[ns-st-running] " << Joined());
}

TEST_CASE("J-58: two Startup schedules are D-E21 (H-12)") {
    const Built built = Finish({{"test", [](Registrar& r) {
                                    Nest(r);
                                    r.Schedule(BootSchedule("Boot"));
                                    r.Schedule(BootSchedule("Boot2"));
                                }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE21, "H-12"), "[ns-st-two] " << Describe(built));
    CHECK_MESSAGE(Codes(built) == std::vector<std::string>{"D-E21"}, "[ns-st-two] " << Describe(built));
}

TEST_CASE("J-58: the main program runs Startup a second time: GYO_ASSERT") {
    auto rig = NestRig(BootRig);
    KERNEL_REQUIRE_RUN_OK(rig->Run("Boot"));
    GYO_CHECK_ASSERTS(rig->Run("Boot"));
}

TEST_CASE("J-58: the main program runs Startup after the kernel ran it: GYO_ASSERT") {
    auto rig = NestRig(BootRig);
    KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));
    GYO_CHECK_ASSERTS(rig->Run("Boot"));
}

TEST_CASE("J-58: Startup returns Err: the triggering Run returns it, no System of that schedule runs, its tick does not advance") {
    auto rig = NestRig([](Registrar& r) {
        BootRig(r);
        r.AddSystem<&BootFails>(In("BootFails", "Boot", "S", Zone::B, {}, {"BootNote"}));
    });
    const auto result = rig->Run("Tick");
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-st-error] Startup's Err was not returned");
    CHECK(result.error().Text().find("boot failed") != std::string::npos);
    CHECK(rig->world().State() == WorldState::Interrupted);
    CHECK_MESSAGE(!Logged("InA@Tick#1"), "[ns-st-error-x] " << Joined());
    CHECK_MESSAGE(rig->world().Tick(Domain::Fixed) == 0, "[ns-st-error-tick] " << rig->world().Tick(Domain::Fixed));
    CHECK_MESSAGE(rig->recorded.size() == 1, "[ns-st-error-once] the handler was called " << rig->recorded.size() << " times");
}

TEST_CASE("J-58: which schedules trigger Startup: a clock schedule, a schedule without a clock, and never a nested run") {
    {
        auto rig = NestRig([](Registrar& r) {
            BootRig(r);
            r.AddSystem<&LogCall>(In("InStep", "Step", "S"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run("Step"));
        CHECK_MESSAGE((Log().size() == 2 && Log()[0].rfind("boot ", 0) == 0 && Log()[1] == "InStep@Step#0"), "[ns-st-trigger] " << Joined());
    }
    {
        // Step advances no tick; its exclusive System runs Tick, a clock schedule: Startup ran before Step's first System.
        auto rig = NestRig([](Registrar& r) {
            BootRig(r);
            r.AddSystem<&Drive>(In("Driver", "Step", "S"));
        });
        Runs()["Driver"] = {"Tick"};
        const auto result = rig->Run("Step");
        REQUIRE_MESSAGE(static_cast<bool>(result), "[ns-st-trigger-nested] " << Text(result));
        CHECK_MESSAGE(Joined().rfind("boot BootNote@Boot fixed tick 99 | drive Driver@Step | InA@Tick#1", 0) == 0,
                      "[ns-st-trigger-nested] " << Joined());
    }
}

// ---- Added after the adversarial check of the mutant list (Fable/high,
// build/evidence/gyo2-nested-schedule/mutant-list-check-fable.md)

namespace {
SystemResult SpawnSeventyOne(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(71, Tag{71});
    return Ok();
}
// Writes Mode (which the outer exclusive System does not declare) and throws before its boundary comparison.
SystemResult ThrowsInB(Query<Write<Mode>> modes) {
    if (Mut<Mode> mode = modes.GetMut<Mode>(1)) mode->m = 55;
    throw std::runtime_error("thrown after a sync point");
}
SystemResult CatchAndReturnOk(ExclusiveWorld<> world) {
    try {
        static_cast<void>(world.RunSchedule("Tick"));
    } catch (const std::runtime_error&) {
        Log().push_back("caught");
    }
    // The handle after the exception: valid, and its write takes a new stamp.
    auto written = world.Modify<Tag>(71, [](Tag& t) { t.t = 710; });
    Log().push_back(std::string("write after catch ") + (written ? "ok" : std::string(ToString(written.error().code))));
    return Ok();
}
// Tick: stage A spawns 71 (a membership change at its sync point), stage B fails or throws.
void FailsAfterSync(Registrar& r, bool throws) {
    Nest(r);
    r.AddSystem<&SpawnSeventyOne>(In("Spawn71", "Tick", "A"));
    if (throws) {
        r.AddSystem<&CatchAndReturnOk>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&ThrowsInB>(In("Throws", "Tick", "B"));
    } else {
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&Fails>(In("Fails", "Tick", "B"));
    }
}
} // namespace

TEST_CASE("J-56: the inner schedule fails after a sync point changed membership: reported once, no assert, nothing blamed on the outer System") {
    auto rig = NestRig([](Registrar& r) { FailsAfterSync(r, false); });
    Runs()["Driver"] = {"Tick"};
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(rig->recorded.size() == 1, "[ns-fail-after-sync]");
}

TEST_CASE("J-56: the inner schedule throws after a sync point changed membership, the outer System catches: NestedRunAborted, no assert, nothing blamed") {
    auto rig = NestRig([](Registrar& r) { FailsAfterSync(r, true); });
    rig->world().Insert(1, Mode{1});
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE((rig->recorded.size() == 1 && result.error().Has(KernelErrorCode::NestedRunAborted)),
                  "[ns-throw-after-sync] " << result.error().Text());
    CHECK_MESSAGE(!result.error().Has(KernelErrorCode::BoundaryChange), "[ns-throw-after-sync-blame] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find("schedule 'Tick'") != std::string::npos, "[ns-throw-after-sync] " << result.error().Text());
    CHECK_MESSAGE(!result.error().Has(KernelErrorCode::StaleHandle), "[ns-throw-handle] " << result.error().Text());
    CHECK_MESSAGE(Logged("write after catch ok"), "[ns-throw-handle] " << Joined());
}

TEST_CASE("J-56: refused, swallowed, then a valid schedule in the same System: it does not run and the left error comes back") {
    auto rig = NestRig(SwallowRig);
    rig->world().Insert(1, Tag{1});
    Runs()["Driver"] = {"NoSuchSchedule", "Step"};
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(!Logged("fails Fails@Step"), "[ns-refusal-interrupts] Step ran after a refusal: " << Joined());
    CHECK_MESSAGE(std::count(Log().begin(), Log().end(), std::string("swallowed UnknownSchedule")) == 2, "[ns-refusal-interrupts] " << Joined());
    // The left error is listed once, not once per call that got it back.
    CHECK_MESSAGE(result.error().Leaves().size() == 1, "[ns-left-listed-once] " << result.error().Text());
}

TEST_CASE("J-56: B runs B (a level that is not the outermost runs itself): refused") {
    auto rig = NestRig([](Registrar& r) {
        SwallowRig(r);
        r.AddSystem<&DriveSwallow>(In("Driver2", "Step", "S", Zone::B, {"Fails"}));
    });
    Runs()["Driver"] = {"Step"};
    Runs()["Driver2"] = {"Step"};
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::ScheduleRunning), "[ns-self-middle] " << result.error().Text());
}

namespace {
SystemResult InsertVelOnFiveThenDrive(CommandsFor<Inserts<Vel>, Removes<>> commands) {
    commands.Insert(5, Vel{5});
    return Ok();
}
SystemResult InsertVelOnFour(CommandsFor<Inserts<Vel>, Removes<>> commands) {
    commands.Insert(4, Vel{4});
    return Ok();
}
} // namespace

TEST_CASE("J-56: three levels, the zone C insert in the middle level: Z5 all the same") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&InsertVelOnFiveThenDrive>(In("Middle", "Step", "S", Zone::C, {"Driver2"}));
        r.AddSystem<&Drive>(In("Driver2", "Step", "S"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    Runs()["Driver"] = {"Step"};
    Runs()["Driver2"] = {"Tick"};
    CheckZ5Across(*rig, "ns-z5-middle-level", "level 1 of 3 (schedule 'Step')");
}

TEST_CASE("J-56: an outer zone C insert into another entity does not stop an inner despawn") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&InsertVelOnFour>(In("Early", "Frame", "Drive", Zone::C, {"Driver"}));
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    rig->world().Insert(4, Mode{4});
    rig->world().Insert(5, Mode{5});
    Runs()["Driver"] = {"Tick"};
    const auto result = rig->Run("Frame");
    CHECK_MESSAGE(static_cast<bool>(result), "[ns-z5-other-entity] " << Text(result));
    CHECK(!rig->world().Alive(5));
}

namespace {
bool g_called = false;
SystemResult WritesRefused(ExclusiveWorld<> world) {
    g_called = false;
    auto pos = world.Modify<Pos>(1, [](Pos& p) { g_called = true; p.x = 100; });
    auto mode = world.Modify<Mode>(1, [](Mode& m) { g_called = true; m.m = 100; });
    auto cres = world.Modify<CRes>([](CRes& c) { g_called = true; c.value = 100; });
    auto flag = world.Modify<Flag>([](Flag& f) { g_called = true; f.on = 100; });
    auto settings = world.Read<KernelSettings>([](const KernelSettings&) { g_called = true; return 0; });
    auto pinned = world.Modify<Pinned>([](Pinned& p) { g_called = true; p.value = 100; });
    Log().push_back(std::string("codes ") + std::string(pos ? "ok" : ToString(pos.error().code)) + " " +
                    std::string(mode ? "ok" : ToString(mode.error().code)) + " " + std::string(cres ? "ok" : ToString(cres.error().code)) +
                    " " + std::string(flag ? "ok" : ToString(flag.error().code)) + " " +
                    std::string(settings ? "ok" : ToString(settings.error().code)) + " " +
                    std::string(pinned ? "ok" : ToString(pinned.error().code)) + (g_called ? " called" : " not called"));
    return Ok();
}
// Called with const T& by Read; the non-const overload must never be chosen.
struct WhichOverload {
    int operator()(const Tag&) const { return 1; }
    int operator()(Tag&) const { return 2; }
    int operator()(const Out&) const { return 1; }
    int operator()(Out&) const { return 2; }
};
SystemResult ReadsThroughConst(ExclusiveWorld<> world) {
    auto component = world.Read<Tag>(1, WhichOverload{});
    auto resource = world.Read<Out>(WhichOverload{});
    if (!component || !resource || !*component || !*resource) return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "read"));
    Log().push_back("overloads " + std::to_string(**component) + std::to_string(**resource));
    return Ok();
}
SystemResult ClosureAfterClosure(ExclusiveWorld<> world) {
    auto outer = world.Read<Tag>(1, [&world](const Tag&) {
        auto inner = world.Read<Tag>(1, [](const Tag& t) { return t.t; });
        static_cast<void>(inner);
        auto ran = world.RunSchedule("Tick");
        return ran ? std::string("ok") : std::string(ToString(ran.error().code));
    });
    Log().push_back("after inner closure " + ((outer && *outer) ? **outer : std::string("?")));
    return Ok();
}
SystemResult ResourceStampAround(ExclusiveWorld<> world) {
    if (auto ran = world.RunSchedule("Tick"); !ran) return Engine::Base::Err(std::move(ran).error());
    if (auto r = world.Modify<Out>([](Out& o) { o.value += 1; }); !r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult IngressDeclared(ExclusiveWorld<Writes<Flag>>) { return Ok(); }
} // namespace

TEST_CASE("J-57: refused writes and reads do not call the closure and change nothing") {
    auto rig = NestRig([](Registrar& r) {
        ResourceRig(r);
        r.AddSystem<&WritesRefused>(In("Driver", "Frame", "Drive"));
    });
    rig->world().Insert(1, Pos{1});
    rig->world().Insert(1, Mode{1});
    rig->world().SetResource(CRes{1});
    rig->world().SetResource(Flag{1});
    rig->world().SetResource(Pinned{1});
    static_cast<void>(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("codes ExclusiveWorldWrite ExclusiveWorldWrite ExclusiveWorldWrite UndeclaredResourceWrite UnreadableResource "
                         "UnreadableResource not called"),
                  "[ns-refused-untouched] " << Joined());
    CHECK_MESSAGE((rig->world().Get<Pos>(1)->x == 1 && rig->world().Get<Mode>(1)->m == 1 && rig->world().Resource<CRes>()->value == 1 &&
                      rig->world().Resource<Flag>()->on == 1 && rig->world().Resource<Pinned>()->value == 1),
                  "[ns-refused-untouched] a refused write changed a value");
}

TEST_CASE("J-57: Read gives the closure a const reference, for a component and a resource") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ReadsThroughConst>(In("Driver", "Frame", "Drive"));
    });
    rig->world().Insert(1, Tag{1});
    rig->world().SetResource(Out{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("overloads 11"), "[ns-read-const] " << Joined());
}

TEST_CASE("J-57: after an inner closure ends, RunSchedule in the outer closure is still refused") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ClosureAfterClosure>(In("Driver", "Frame", "Drive"));
    });
    rig->world().Insert(1, Tag{1});
    static_cast<void>(rig->Run("Frame"));
    CHECK_MESSAGE(Logged("after inner closure ClosureRunsSchedule"), "[ns-closure-depth] " << Joined());
}

namespace {
// A closure ends by an exception the System catches: RunSchedule runs afterwards.
SystemResult ClosureThrows(ExclusiveWorld<> world) {
    try {
        static_cast<void>(world.Read<Tag>(1, [](const Tag&) -> int { throw std::runtime_error("from the closure"); }));
    } catch (const std::runtime_error&) {
        Log().push_back("closure threw");
    }
    auto ran = world.RunSchedule("Tick");
    Log().push_back(std::string("then ") + (ran ? "ok" : std::string(ToString(ran.error().code))));
    return Ok();
}
} // namespace

TEST_CASE("J-57: a closure that throws, caught by the System: RunSchedule runs afterwards") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ClosureThrows>(In("Driver", "Frame", "Drive"));
    });
    rig->world().Insert(1, Tag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    CHECK_MESSAGE((Logged("closure threw") && Logged("then ok")), "[ns-closure-throw] " << Joined());
}

TEST_CASE("J-57: what points into the World is refused also when const, and through a void pointer") {
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) { return static_cast<const void*>(&t); })>, "[ns-closure-void-pointer]");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) -> const std::string_view { return "x"; })>, "[ns-closure-const] string_view");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag&) -> const Mut<Tag> { return Mut<Tag>(); })>, "[ns-closure-const] Mut");
    CHECK_MESSAGE(!ReadAccepts<decltype([](const Tag& t) -> const std::pair<const Tag*, std::int32_t> { return {&t, 1}; })>,
                  "[ns-closure-const] pair");
    // The trait itself, so a mutant cannot hide behind the overload set.
    CHECK_MESSAGE(Detail::kPointsIntoWorld<const Tag&>, "[ns-closure-reference] trait");
    CHECK_MESSAGE(Detail::kPointsIntoWorld<Tag*>, "[ns-closure-pointer] trait");
    CHECK_MESSAGE(!Detail::kPointsIntoWorld<std::optional<std::int32_t>>, "[ns-closure-optional] trait");
}

TEST_CASE("J-57: a resource Modify between two nested runs takes the new stamp") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&ResourceStampAround>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&LogStamp>(In("Stamp", "Tick", "C"));
    });
    rig->world().Insert(1, Tag{1});
    rig->world().SetResource(Out{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run("Frame"));
    REQUIRE(Log().size() == 1);
    const ChangeStamp innerLast = std::stoull(Log()[0].substr(std::string("inner stamp ").size())) + 1;
    const ChangeStamp changed =
        Detail::ResourceAt(rig->world(), Detail::ResourceIdOf(rig->world(), KeyOf<Out>())).changed;
    CHECK_MESSAGE(changed > innerLast, "[ns-stamp-resource] changed " << changed << ", inner last " << innerLast);
}

TEST_CASE("J-57: declarations: ExclusiveWorld<Writes<...>> with the ingress flag is S-17 too") {
    const Built built = Finish({{"test", [](Registrar& r) {
                                    Nest(r);
                                    SystemOptions options = In("IngressDeclared", "Frame", "In");
                                    options.ingress = true;
                                    r.AddSystem<&IngressDeclared>(options);
                                }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "S-17"), "[ns-decl-ingress] declared: " << Describe(built));
}

// ---- Added after the line-by-line review (Opus/xhigh, build/evidence/gyo2-nested-schedule/impl-review-opus-xhigh.md)

namespace {
SystemResult InsertTagOnSixInside(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(6, Tag{6});
    return Ok();
}
SystemResult InsertTagOnFiveB(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(5, Tag{5});
    return Ok();
}
SystemResult BootTampers(Query<Read<Tag>>) {
    g_mode->m = 9;  // not declared: verification on in Startup too (KernelSettings read when Startup starts)
    return Ok();
}
SystemResult DespawnSixInside(Despawn despawn) {
    if (auto r = despawn.Entity(6); !r) return r;
    return Ok();
}
} // namespace

TEST_CASE("J-56: the inner sync point fails after its step 0 applied hook records (no hook ran): nothing blamed on the outer System") {
    // Tick A inserts Tag on 6; OnAdd<Tag> records Mode into Tick's hook queue; Tick B's sync point applies it at
    // step 0 (no hook on Mode, so no snapshot is taken there), then fails Z5 against the outer zone C insert into 5.
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&InsertVelOnFive>(In("Early", "Frame", "Drive", Zone::C, {"Driver"}));
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&InsertTagOnSixInside>(In("Seed", "Tick", "A"));
        r.Hook<Tag, &TagToMode>(HookTrigger::OnAdd, {.name = "TagToMode", .zone = Zone::B});
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "B"));
    });
    rig->world().Insert(5, Pos{5});
    rig->world().Insert(6, Pos{6});
    Runs()["Driver"] = {"Tick"};
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::ZoneCDespawn), "[ns-fail-after-sync-blame] " << result.error().Text());
    CHECK_MESSAGE(!result.error().Has(KernelErrorCode::BoundaryChange), "[ns-fail-after-sync-blame] " << result.error().Text());
    CHECK(rig->recorded.size() == 1);
}

TEST_CASE("J-56: three levels, the zone C insert in the middle level's hook queue: Z5 all the same") {
    // Mid: stage S1 inserts Tag on 5; its sync point runs the zone C hook TagToVel, whose record waits in Mid's hook
    // queue for the next sync point; stage S2's exclusive System runs Tick, which despawns 5.
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.Schedule(Schedule("Mid", Domain::Fixed, false, {"S1", "S2"}));
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&InsertTagOnFiveB>(In("Seed", "Mid", "S1"));
        r.Hook<Tag, &TagToVel>(HookTrigger::OnAdd, {.name = "TagToVel", .zone = Zone::C});
        r.AddSystem<&Drive>(In("Driver2", "Mid", "S2"));
        r.AddSystem<&DespawnFive>(In("Kill", "Tick", "A"));
    });
    Runs()["Driver"] = {"Mid"};
    Runs()["Driver2"] = {"Tick"};
    CheckZ5Across(*rig, "ns-z5-middle-hook", "hook 'TagToVel' of the outer run at level 1 of 3 (schedule 'Mid')");
}

TEST_CASE("J-58: verification is on while Startup runs: an undeclared write in Startup is reported, naming its System") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.Schedule(BootSchedule());
        r.AddSystem<&BootTampers>(In("BootTampers", "Boot", "S"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    rig->world().Insert(1, Mode{1});
    g_mode = const_cast<Mode*>(rig->world().Get<Mode>(1));
    const auto result = rig->Run("Tick");
    g_mode = nullptr;
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "[ns-st-verification] Startup's undeclared write was not reported");
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::BoundaryChange), "[ns-st-verification] " << result.error().Text());
    CHECK_MESSAGE(result.error().Text().find("'BootTampers'") != std::string::npos, "[ns-st-verification] " << result.error().Text());
    CHECK(!Logged("InA@Tick#1"));
}

TEST_CASE("J-56: a hook of the inner schedule runs a schedule: refused, reported once, listed once") {
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.AddSystem<&Drive>(In("Driver", "Frame", "Drive"));
        r.AddSystem<&DespawnSixInside>(In("Leave", "Tick", "A"));
        r.Hook<Tag, &HookRunsSchedule>(HookTrigger::OnRemove, {.name = "HookRuns", .zone = Zone::B});
    });
    rig->world().Insert(6, Tag{6});
    Runs()["Driver"] = {"Tick"};
    const auto result = rig->Run("Frame");
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(result.error().Has(KernelErrorCode::HookRunsSchedule), "[ns-hook-refused-inner] " << result.error().Text());
    CHECK_MESSAGE(result.error().Leaves().size() == 1, "[ns-hook-refused-inner] " << result.error().Text());
    CHECK_MESSAGE(rig->recorded.size() == 1, "[ns-hook-refused-inner] the handler was called " << rig->recorded.size() << " times");
}

namespace {
SystemResult BootSeedsModeOnly(SpawnWithId<Bundle<Mode>> spawn) {
    spawn.New(31, Mode{31});
    return Ok();
}
} // namespace

TEST_CASE("J-58: Startup's sync point changes membership with no hook: verification on, the triggering Run reports nothing") {
    // No hook runs at Startup's sync point, so only the Executor's snapshots after it keep Tick's Ready window quiet.
    auto rig = NestRig([](Registrar& r) {
        Nest(r);
        r.Schedule(BootSchedule());
        r.AddSystem<&BootSeedsModeOnly>(In("BootSeeds", "Boot", "S"));
        r.AddSystem<&LogCall>(In("InA", "Tick", "A"));
    });
    const auto result = rig->Run("Tick");
    CHECK_MESSAGE(static_cast<bool>(result), "[ns-st-verify-plain] " << Text(result));
    CHECK(rig->world().Get<Mode>(31) != nullptr);
}
