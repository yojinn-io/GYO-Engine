// Cases added after the batch's review (Fable/xhigh, 2026-11-13; report in the
// main checkout's build/evidence/kernel-registry-schedule/review/): defects it
// reproduced (A1 cycle report, A2 registration after Finish, B1 set conditions,
// B2 errors collected across one schedule, B4 plugins' Finish, C1 hooks, C2 one
// X-E1, C3 sealed run conditions) and guard gaps its adversarial check found
// (sets, ExclusiveWorld and effect conflicts, Z6 and hooks, conditions,
// resource writers, Derived, C fingerprint, field sensitivity of the Registry
// fingerprint, J-24 items). Each case is named by its J entry so its ctest
// runs it; the variable or System names are what the mutation catalog expects.

#include "KernelTestSupport.hpp"

#include <type_traits>

namespace {

using namespace KernelTest;

struct SharedA { std::int32_t v; };
struct SharedB { std::int32_t v; };

void SharedData(Registrar& r) {
    r.Component<SharedA>({.name = "SharedA", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
    r.Component<SharedB>({.name = "SharedB", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
}

SystemResult WriteA(Query<Write<SharedA>>) { return Ok(); }
SystemResult ReadA(Query<Read<SharedA>>) { return Ok(); }
SystemResult ReadB(Query<Read<SharedB>>) { return Ok(); }
SystemResult Nothing() { return Ok(); }
SystemResult InsertA(CommandsFor<Inserts<SharedA>, Removes<>>) { return Ok(); }
SystemResult FilterWithA(Query<Write<SharedB>, With<SharedA>>) { return Ok(); }
SystemResult WriteInputRes(ResMut<TestInput>) { return Ok(); }
SystemResult EditWorld(ExclusiveWorld) { return Ok(); }
SystemResult ReadMarker(Query<Read<TestMarker>>) { return Ok(); }
SystemResult ReadMarkerC(Query<Read<TestVel>>) { return Ok(); }
SystemResult WriteRes(ResMut<TestRes>) { return Ok(); }
SystemResult WriteResAgain(ResMut<TestRes>) { return Ok(); }
SystemResult WritePos(Query<Write<TestPos>>) { return Ok(); }
SystemResult ReadTick(CurrentTick, Query<Write<TestHealth>>) { return Ok(); }
SystemResult InsertDerived(CommandsFor<Inserts<TestDerived>, Removes<>>) { return Ok(); }
SystemResult ReadDerived(Query<Read<TestDerived>>) { return Ok(); }
SystemResult WriteDerived(Query<Write<TestDerived>>) { return Ok(); }
SystemResult ExtractPos(Extract<Read<TestPos>>) { return Ok(); }
SystemResult HookWritesHealth(Query<Write<TestHealth>>) { return Ok(); }
SystemResult HookWritesMode(Query<Write<TestMode>>) { return Ok(); }
SystemResult HookSpawns(Spawn<Bundle<TestPos>>) { return Ok(); }
SystemResult HookReadsSettings(Res<KernelSettings>) { return Ok(); }
SystemResult HookKeepsLocal(Local<int>, Query<Read<TestPos>>) { return Ok(); }
bool VelChanged(Query<Changed<TestVel>>) { return true; }
bool SettingsOn(Res<KernelSettings>) { return true; }
bool ResIsSet(Res<TestRes>) { return true; }
bool ReplicatedAny(SetRead<ReplicatedSet>) { return true; }

std::vector<Mount> Sim(std::vector<std::string> before = {}, std::vector<std::string> after = {}) {
    return {At("FixedUpdate", "Simulate", std::move(before), std::move(after))};
}

Mount InSet(Mount mount, std::string set) {
    mount.sets.push_back(std::move(set));
    return mount;
}

} // namespace

// ---- A1: the D-E6 report names the cycle even when it starts downstream of the lowest node

TEST_CASE("J-24: a cycle downstream of an earlier System is reported with its own members") {
    Built built = FinishOne([](Registrar& r) {
        SharedData(r);
        r.AddSystem<&ReadB>(Sys("Down", Zone::C, Sim({}, {"One"})));
        r.AddSystem<&WriteA>(Sys("One", Zone::C, Sim({"Two"})));
        r.AddSystem<&ReadA>(Sys("Two", Zone::C, Sim({"One"})));
    });
    REQUIRE(ErrorCount(built, StartupCode::DE6) == 1);
    const StartupError& error = built.result.error().errors.front();
    std::vector<std::string> cycle = error.subjects;
    std::sort(cycle.begin(), cycle.end());
    CHECK(cycle == std::vector<std::string>{"One", "Two"});
    CHECK(error.message.find("event-trim") == std::string::npos);
}

// ---- A2: no registration after Finish

TEST_CASE("J-19: a Registrar or FinishContext cannot be copied or moved out of its call") {
    // Registrar lives on Finish's stack for one Build call; with copy and move deleted, nothing can keep a
    // usable one past Build (a kept pointer dangles, which is outside the contract). Every registration also
    // asserts the build phase (Registry.cpp) as a second line.
    static_assert(!std::is_copy_constructible_v<Registrar>);
    static_assert(!std::is_move_constructible_v<Registrar>);
    static_assert(!std::is_copy_assignable_v<Registrar>);
    static_assert(!std::is_copy_constructible_v<FinishContext>);
    static_assert(!std::is_move_constructible_v<FinishContext>);
    CHECK(true);
}

// ---- B2: one schedule's errors are collected, not stopped at the first

TEST_CASE("J-24: a missing target and an ambiguity in one schedule are both reported") {
    Built built = FinishOne([](Registrar& r) {
        SharedData(r);
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim({"Gone"})));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
    });
    CHECK(ErrorCount(built, StartupCode::DE4) == 1);
    CHECK(ErrorCount(built, StartupCode::DE8) == 1);
}

TEST_CASE("J-24: an ambiguity and a Derived read with no writer in one schedule are both reported") {
    Built built = FinishOne([](Registrar& r) {
        SharedData(r);
        r.Component<TestDerived>({.name = "TestDerived", .zone = Zone::C, .rollback = Rollback::Derived});
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim()));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
        r.AddSystem<&ReadDerived>(Sys("UseDerived", Zone::C, Sim()));
    }, true);
    CHECK(ErrorCount(built, StartupCode::DE8) == 1);
    CHECK(ErrorCount(built, StartupCode::DE16) == 1);
}

// ---- B4: plugins' Finish reads final named sets and Systems

TEST_CASE("J-19: a plugin's Finish sees resolved named sets and System access") {
    std::vector<ComponentId> members;
    std::size_t systems = 0;
    TestPlugin plugin("replication", [](Registrar& r) {
        RegisterCommonData(r);
        r.NamedSet<ReplicatedSet>("Replicated");
        r.AddToSet<ReplicatedSet, TestPos>();
        r.AddToSet<ReplicatedSet, TestVel>();
        r.Schedule(FixedSchedule());
        r.AddSystem<&ReadMarker>(Sys("ShowMark", Zone::B, {At("FixedUpdate", "Present")}));
    }, {}, [&](const FrozenRegistry& frozen, FinishContext&) {
        members = frozen.SetMembers(0);
        systems = frozen.Systems().size();
    });
    Registry registry({&plugin});
    REQUIRE(static_cast<bool>(registry.Finish()));
    CHECK(members.size() == 2);
    CHECK(systems == 1);
}

// ---- B1 and sets: order, conditions

TEST_CASE("J-21: a set's before / after orders every member") {
    Built built = FinishOne([](Registrar& r) {
        r.Set({.name = "Late", .schedule = "Update", .after = {"First1"}});
        r.AddSystem<&ReadMarker>(Sys("Member1", Zone::B, {InSet(At("Update", "First"), "Late")}));
        const auto member2 = [](Query<Read<TestMarker>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(member2)>(Sys("Member2", Zone::B, {InSet(At("Update", "First"), "Late")}));
        const auto first = []() -> SystemResult { return {}; };
        r.AddSystem<decltype(first)>(Sys("First1", Zone::B, {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(built);
    const Plan& setPlan = *built.registry->Frozen().PlanOf("Update");
    CHECK(SystemSteps(setPlan) == std::vector<std::string>{"First1", "Member1", "Member2"});
}

TEST_CASE("J-12: a set's run-condition reads join every member's reads") {
    Built built = FinishOne([](Registrar& r) {
        r.Set({.name = "Gated", .schedule = "Update"}, {Condition<&ResIsSet>("ResIsSet")});
        r.AddSystem<&ReadMarker>(Sys("SetGated", Zone::B, {InSet(At("Update", "First"), "Gated")}));
    });
    KERNEL_REQUIRE_OK(built);
    const ResourceId res = *built.registry->Frozen().ResourceIdOf<TestRes>();
    const SystemInfo* info = built.registry->Frozen().SystemNamed("SetGated");
    REQUIRE(info != nullptr);
    CHECK(std::find(info->access.reads.begin(), info->access.reads.end(), AccessKey{AccessKind::Resource, res}) !=
          info->access.reads.end());
}

TEST_CASE("J-38: a Changed filter in a mount's or a set's run condition makes a zone C System fail (S-9)") {
    Built conditionChanged = FinishOne([](Registrar& r) {
        Mount m = At("FixedUpdate", "Simulate");
        m.conditions.push_back(Condition<&VelChanged>("VelChanged"));
        r.AddSystem<&WritePos>(Sys("GatedPos", Zone::C, {m}));
    });
    CHECK(HasRule(conditionChanged, StartupCode::DE7, "S-9"));
    Built setChanged = FinishOne([](Registrar& r) {
        r.Set({.name = "Gated", .schedule = "FixedUpdate"}, {Condition<&VelChanged>("VelChanged")});
        r.AddSystem<&WritePos>(Sys("GatedPos", Zone::C, {InSet(At("FixedUpdate", "Simulate"), "Gated")}));
    });
    CHECK(HasRule(setChanged, StartupCode::DE7, "S-9"));
}

TEST_CASE("J-24: KernelSettings read by a mount's or a set's run condition is D-E18") {
    Built conditionSettings = FinishOne([](Registrar& r) {
        Mount m = At("Update", "First");
        m.conditions.push_back(Condition<&SettingsOn>("SettingsOn"));
        r.AddSystem<&ReadMarker>(Sys("Gated", Zone::B, {m}));
    });
    CHECK(ErrorCount(conditionSettings, StartupCode::DE18) == 1);
    Built setSettings = FinishOne([](Registrar& r) {
        r.Set({.name = "Gated", .schedule = "Update"}, {Condition<&SettingsOn>("SettingsOn")});
        r.AddSystem<&ReadMarker>(Sys("Gated", Zone::B, {InSet(At("Update", "First"), "Gated")}));
    });
    CHECK(ErrorCount(setSettings, StartupCode::DE18) == 1);
}

TEST_CASE("J-47: a set's run condition reading a named set with a zone B member makes a zone C member fail (Z1)") {
    Built built = FinishOne([](Registrar& r) {
        r.NamedSet<ReplicatedSet>("Replicated");
        r.AddToSet<ReplicatedSet, TestMarker>();
        r.Set({.name = "Gated", .schedule = "FixedUpdate"}, {Condition<&ReplicatedAny>("ReplicatedAny")});
        r.AddSystem<&WritePos>(Sys("GatedPos", Zone::C, {InSet(At("FixedUpdate", "Simulate"), "Gated")}));
    });
    CHECK(HasRule(built, StartupCode::DE7, "Z1"));
}

// ---- D.1 conditions 2 and 3

TEST_CASE("J-22: a zone B ExclusiveWorld System conflicts with a zone C System of its stage (D-E8 without order)") {
    Built exclusive = FinishOne([](Registrar& r) {
        SharedData(r);
        r.AddSystem<&EditWorld>(Sys("EditWorld", Zone::B, Sim()));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
    });
    CHECK(ErrorCount(exclusive, StartupCode::DE8) == 1);
}

TEST_CASE("J-23: two zone B Systems declaring one effect conflict and are ordered by registration") {
    Built sameEffect = FinishOne([](Registrar& r) {
        r.Effect("Io:file");
        SystemOptions a = Sys("SaveFirst", Zone::B, {At("Update", "First")});
        a.effects = {"Io:file"};
        r.AddSystem<&Nothing>(a);
        SystemOptions b = Sys("SaveSecond", Zone::B, {At("Update", "First")});
        b.effects = {"Io:file"};
        const auto second = []() -> SystemResult { return {}; };
        r.AddSystem<decltype(second)>(b);
    });
    KERNEL_REQUIRE_OK(sameEffect);
    CHECK(sameEffect.registry->Frozen().TieBreaks().size() == 1);
}

// ---- Z6, Z8, hooks

TEST_CASE("J-40: Z6 for Systems and hooks, Z2 for a zone B hook, Z8 for a component") {
    Built z6 = FinishOne([](Registrar& r) {
        r.Component<TestDerived>({.name = "TestDerived", .zone = Zone::C});  // rollback No
        r.AddSystem<&WriteDerived>(Sys("WriteDerived", Zone::C, Sim()));
    }, true);
    CHECK(HasRule(z6, StartupCode::DE7, "Z6"));
    Built hookNoRollback = FinishOne([](Registrar& r) {
        r.Component<TestDerived>({.name = "TestDerived", .zone = Zone::C, .writePolicy = WritePolicy::Shared});
        r.Hook<TestPos, &WriteDerived>(HookTrigger::OnAdd, {.name = "OnPos", .zone = Zone::C});
    }, true);
    CHECK(HasRule(hookNoRollback, StartupCode::DE7, "Z6"));
    Built hookWritesC = FinishOne([](Registrar& r) {
        r.Hook<TestMarker, &HookWritesHealth>(HookTrigger::OnAdd, {.name = "OnMarker", .zone = Zone::B});
    });
    CHECK(HasRule(hookWritesC, StartupCode::DE7, "Z2"));
    Built rollbackInputColumn = Finish({{"test", [](Registrar& r) {
                                            r.Component<TestInput>({.name = "TestInput", .zone = Zone::C,
                                                                    .role = Role::Input, .rollback = Rollback::Yes});
                                            r.Schedule(FixedSchedule());
                                        }}});
    CHECK(HasRule(rollbackInputColumn, StartupCode::DE7, "Z8"));
}

TEST_CASE("J-14: a hook without a zone fails; a zone C hook with Local fails") {
    Built hookNoZone = FinishOne([](Registrar& r) {
        r.Hook<TestMarker, &HookWritesMode>(HookTrigger::OnAdd, {.name = "OnMarker"});
    });
    CHECK(HasRule(hookNoZone, StartupCode::DE7, "S-6"));
    Built hookLocal = FinishOne([](Registrar& r) {
        r.Hook<TestVel, &HookKeepsLocal>(HookTrigger::OnAdd, {.name = "OnVel", .zone = Zone::C});
    });
    CHECK(HasRule(hookLocal, StartupCode::DE7, "S-7"));
}

TEST_CASE("J-24: a hook that spawns without an allotter (D-E3) or reads KernelSettings (D-E18)") {
    Built hookSpawn = FinishOne([](Registrar& r) {
        r.Hook<TestVel, &HookSpawns>(HookTrigger::OnAdd, {.name = "OnVel", .zone = Zone::C});
    });
    CHECK(ErrorCount(hookSpawn, StartupCode::DE3) == 1);
    Built hookSettings = FinishOne([](Registrar& r) {
        r.Hook<TestMarker, &HookReadsSettings>(HookTrigger::OnAdd, {.name = "OnMarker", .zone = Zone::B});
    });
    CHECK(ErrorCount(hookSettings, StartupCode::DE18) == 1);
}

TEST_CASE("J-39: a zone C System may read the domain tick (it is the (c) time source)") {
    Built readsTick = FinishOne([](Registrar& r) { r.AddSystem<&ReadTick>(Sys("ReadTick", Zone::C, Sim())); });
    KERNEL_REQUIRE_OK(readsTick);
}

// ---- D-E2 for resources

TEST_CASE("J-15: a second writer of an Exclusive resource fails") {
    Built resource = FinishOne([](Registrar& r) {
        r.AddSystem<&WriteRes>(Sys("WriteRes", Zone::C, Sim()));
        r.AddSystem<&WriteResAgain>(Sys("WriteResAgain", Zone::C, {At("FixedUpdate", "Present")}));
    });
    CHECK(ErrorCount(resource, StartupCode::DE2) == 1);
}

// ---- D-E16 details

TEST_CASE("J-24: Derived needs an immediate writer (a deferred one is not enough), only in rollback schedules") {
    Built deferredOnly = FinishOne([](Registrar& r) {
        r.Component<TestDerived>({.name = "TestDerived", .zone = Zone::C, .rollback = Rollback::Derived});
        r.AddSystem<&InsertDerived>(Sys("InsertDerived", Zone::C, Sim({"UseDerived"})));
        r.AddSystem<&ReadDerived>(Sys("UseDerived", Zone::C, Sim()));
    }, true);
    CHECK(ErrorCount(deferredOnly, StartupCode::DE16) == 1);
    Built derivedNoRollback = FinishOne([](Registrar& r) {
        r.Component<TestDerived>({.name = "TestDerived", .zone = Zone::C, .rollback = Rollback::Derived});
        r.AddSystem<&ReadDerived>(Sys("UseDerived", Zone::C, Sim()));
    }, false);
    KERNEL_REQUIRE_OK(derivedNoRollback);
}

// ---- Fingerprints

namespace {

Built Variant(bool zoneBChange, bool zoneCChange, std::string field = {}) {
    // Same structure in every variant; field flips one declared value.
    return FinishOne([=](Registrar& r) {
        SharedData(r);
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim({"ReadA"})));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
        r.AddSystem<&ReadMarker>(Sys(zoneBChange ? "ShowMarkRenamed" : "ShowMark", Zone::B, {At("Update", "First")}));
        SystemOptions stage = Sys("StageNothing", Zone::B, {At("FixedUpdate", "Ingress")});
        stage.ingress = field == "ingress";
        r.AddSystem<&Nothing>(stage);
        if (zoneCChange) r.AddSystem<&ReadB>(Sys("ReadB", Zone::C, Sim()));
        if (field == "hook access") {
            r.Hook<TestPos, &HookWritesMode>(HookTrigger::OnAdd, {.name = "OnPos", .zone = Zone::B});
        } else {
            r.Hook<TestPos, &ReadMarker>(HookTrigger::OnAdd, {.name = "OnPos", .zone = Zone::B});
        }
    }, field == "rollbackEnabled");
}

} // namespace

TEST_CASE("J-18: the C fingerprint changes with the zone C part only; the Registry fingerprint sees every field") {
    Built base = Variant(false, false);
    Built withZoneBOnlyChange = Variant(true, false);
    Built withZoneCChange = Variant(false, true);
    KERNEL_REQUIRE_OK(base);
    KERNEL_REQUIRE_OK(withZoneBOnlyChange);
    KERNEL_REQUIRE_OK(withZoneCChange);
    CHECK(base.registry->Frozen().CFingerprint() == withZoneBOnlyChange.registry->Frozen().CFingerprint());
    CHECK(base.registry->Frozen().CFingerprint() != withZoneCChange.registry->Frozen().CFingerprint());
    CHECK(base.registry->Frozen().RegistryFingerprint() != withZoneBOnlyChange.registry->Frozen().RegistryFingerprint());
    for (const std::string field : {"rollbackEnabled", "hook access", "ingress"}) {
        CAPTURE(field);
        Built changed = Variant(false, false, field);
        KERNEL_REQUIRE_OK(changed);
        CHECK(changed.registry->Frozen().RegistryFingerprint() != base.registry->Frozen().RegistryFingerprint());
    }
}

// ---- J-24 items from the review

TEST_CASE("J-24: a System name twice (R-E2), an unknown eventTrim (D-E1), Extract outside an extract schedule, no mount") {
    Built sameName = FinishOne([](Registrar& r) {
        r.AddSystem<&ReadMarker>(Sys("Same", Zone::B, {At("Update", "First")}));
        r.AddSystem<&Nothing>(Sys("Same", Zone::B, {At("Update", "Second")}));
    });
    CHECK(ErrorCount(sameName, StartupCode::RE2) == 1);
    Built unknownTrim = Finish({{"test", [](Registrar& r) {
                                    ScheduleOptions s = FixedSchedule();
                                    s.eventTrim = "Nowhere";
                                    r.Schedule(s);
                                }}});
    CHECK(ErrorCount(unknownTrim, StartupCode::DE1) == 1);
    Built extractOutside = FinishOne([](Registrar& r) {
        r.AddSystem<&ExtractPos>(Sys("ExtractPos", Zone::B, {At("Update", "First")}));
    });
    CHECK(HasRule(extractOutside, StartupCode::DE7, "H-6"));
    Built noMount = FinishOne([](Registrar& r) { r.AddSystem<&Nothing>(Sys("Idle", Zone::B, {})); });
    CHECK(ErrorCount(noMount, StartupCode::DE1) == 1);
}

TEST_CASE("J-27: a (c) Parallel schedule without a guard is one X-E1, however many zone C Systems it holds") {
    Built built = FinishOne([](Registrar& r) {
        SharedData(r);
        ScheduleOptions s;
        s.name = "FixedParallel";
        s.domain = Domain::Fixed;
        s.stages = {"Run"};
        s.executor = ExecutorKind::Parallel;
        r.Schedule(s);
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, {At("FixedParallel", "Run")}));
        r.AddSystem<&ReadB>(Sys("ReadB", Zone::C, {At("FixedParallel", "Run")}));
        const auto third = [](Query<Read<SharedA>, Read<SharedB>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(third)>(Sys("ReadAB", Zone::C, {At("FixedParallel", "Run")}));
    });
    CHECK(ErrorCount(built, StartupCode::XE1) == 1);
}

TEST_CASE("J-12: a run condition cannot be written by hand (its access comes from its signature)") {
    static_assert(!std::is_constructible_v<ConditionRef, std::string, Detail::ResolveFn>);
    static_assert(!std::is_default_constructible_v<ConditionRef>);
    const ConditionRef condition = Condition<&ResIsSet>("ResIsSet");
    CHECK(condition.Name() == "ResIsSet");
    CHECK(condition.Resolver() != nullptr);
}

// ---- B3: the event trim follows a named sync point immediately

TEST_CASE("J-20: an event trim at a named sync point comes right after it, before what is ordered after it") {
    Built built = Finish({{"test", [](Registrar& r) {
                              SharedData(r);
                              ScheduleOptions s = FixedSchedule();
                              s.syncPoints = {{"Mid", "Simulate"}};
                              s.eventTrim = "Mid";
                              r.Schedule(s);
                              r.AddSystem<&WriteA>(Sys("Before", Zone::C, Sim({"Mid"})));
                              r.AddSystem<&ReadA>(Sys("After", Zone::C, Sim({}, {"Mid"})));
                          }}});
    KERNEL_REQUIRE_OK(built);
    const Plan& plan = *built.registry->Frozen().PlanOf("FixedUpdate");
    const std::size_t mid = PositionOf(plan, "Mid");
    const std::size_t trim = PositionOf(plan, "event-trim");
    CHECK(PositionOf(plan, "Before") < mid);
    CHECK(trim == mid + 1);
    CHECK(trim < PositionOf(plan, "After"));
    CHECK(trim < PositionOf(plan, "Simulate"));
}

// ---- B5: the judged ratio uses the plan's definition; the extended one is printed beside it

TEST_CASE("Plan export: a deferred write and a With are not dataflow by the plan's definition, but are in the extended one") {
    Built built = FinishOne([](Registrar& r) {
        SharedData(r);
        r.AddSystem<&InsertA>(Sys("Defer", Zone::C, Sim({"See"})));
        r.AddSystem<&ReadA>(Sys("See", Zone::C, Sim()));
        r.AddSystem<&WriteA>(Sys("Writer", Zone::C, {At("FixedUpdate", "Present", {"Filter"})}));
        r.AddSystem<&FilterWithA>(Sys("Filter", Zone::C, {At("FixedUpdate", "Present")}));
    });
    KERNEL_REQUIRE_OK(built);
    const Plan& plan = *built.registry->Frozen().PlanOf("FixedUpdate");
    REQUIRE(plan.declaredEdges.size() == 2);
    CHECK(plan.NonDataflowEdges() == 2);
    CHECK(plan.NonDataflowEdgesExtended() == 0);
    const std::string report = built.registry->Frozen().ExportPlans();
    CHECK(report.find("non_dataflow_edges 2/2 100%") != std::string::npos);
    CHECK(report.find("non_dataflow_edges_extended 0/2 0%") != std::string::npos);
    CHECK(report.find("edge Defer -> See non-dataflow extended=dataflow") != std::string::npos);
}

// ---- C4: the ingress stage is the schedule's first stage (Z3)

TEST_CASE("J-40: an ingress stage that is not the schedule's first stage fails (Z3)") {
    Built ingressNotFirst = Finish({{"test", [](Registrar& r) {
                                        ScheduleOptions s = FixedSchedule();
                                        s.ingressStage = "Present";
                                        r.Schedule(s);
                                    }}});
    CHECK(HasRule(ingressNotFirst, StartupCode::DE7, "Z3"));
    Built inserted = Finish({{"test", [](Registrar& r) {
                                 r.Schedule(FixedSchedule());
                                 r.InsertStage("FixedUpdate", "Ingress", StagePlace::Before, "Earlier");
                             }}});
    CHECK(HasRule(inserted, StartupCode::DE7, "Z3"));
    (void)&WriteInputRes;
}

// ---- B6: what the C fingerprint covers (format 2)

namespace {

Built CVariant(const std::string& change) {
    // The zone C declarations stay the same in every variant but the one named.
    return Finish({{"test", [change](Registrar& r) {
                        RegisterCommonData(r);
                        SharedData(r);
                        r.Schedule(UpdateSchedule());
                        ScheduleOptions s = FixedSchedule(change == "rollback");
                        s.syncPoints = {{"Mid", "Simulate"}};
                        r.Schedule(s);
                        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim({"ReadA", "Mid"})));
                        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
                        // "sync": a zone B System after Mid and before ReadA moves Mid between the two zone C steps
                        // (otherwise Mid runs after both: sync points run as late as possible).
                        if (change == "sync") {
                            const auto gate = []() -> SystemResult { return {}; };
                            r.AddSystem<decltype(gate)>(Sys("Gate", Zone::B, Sim({"ReadA"}, {"Mid"})));
                        }
                        // "hook": the zone C hook's access changes (the hook count stays the same).
                        if (change == "hook") {
                            r.Hook<TestPos, &HookWritesHealth>(HookTrigger::OnAdd, {.name = "OnPosC", .zone = Zone::C});
                        } else {
                            r.Hook<TestPos, &ReadMarkerC>(HookTrigger::OnAdd, {.name = "OnPosC", .zone = Zone::C});
                        }
                    }}});
}

} // namespace

TEST_CASE("J-18: the C fingerprint sees zone C hooks, the schedule's rollback and sync points between zone C steps") {
    Built base = CVariant("");
    KERNEL_REQUIRE_OK(base);
    CHECK(base.registry->Frozen().CFingerprint().version == 2U);
    // The "sync" variant really moves Mid between the zone C steps.
    const Plan& plain = *base.registry->Frozen().PlanOf("FixedUpdate");
    CHECK(PositionOf(plain, "Mid") > PositionOf(plain, "ReadA"));
    for (const std::string change : {"hook", "rollback", "sync"}) {
        CAPTURE(change);
        Built changed = CVariant(change);
        KERNEL_REQUIRE_OK(changed);
        CHECK(changed.registry->Frozen().CFingerprint() != base.registry->Frozen().CFingerprint());
    }
}
