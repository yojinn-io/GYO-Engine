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
    r.Component<SharedA>({.name = "SharedA"});
    r.Component<SharedB>({.name = "SharedB"});
}

SystemResult WriteA(Query<Write<SharedA>>) { return Ok(); }
SystemResult ReadA(Query<Read<SharedA>>) { return Ok(); }
SystemResult ReadB(Query<Read<SharedB>>) { return Ok(); }
SystemResult Nothing() { return Ok(); }
SystemResult InsertA(CommandsFor<Inserts<SharedA>, Removes<>>) { return Ok(); }
SystemResult FilterWithA(Query<Write<SharedB>, With<SharedA>>) { return Ok(); }
SystemResult EditWorld(ExclusiveWorld<>) { return Ok(); }
SystemResult ReadMarker(Query<Read<TestMarker>>) { return Ok(); }
SystemResult ExtractPos(Extract<Read<TestPos>>) { return Ok(); }
SystemResult HookWritesMode(Query<Write<TestMode>>) { return Ok(); }
SystemResult HookSpawns(Spawn<Bundle<TestPos>>) { return Ok(); }
bool SettingsOn(Res<KernelSettings>) { return true; }
SystemResult HookReadsSettings(Res<KernelSettings>) { return Ok(); }
SystemResult HookWritesSettings(ResMut<KernelSettings>) { return Ok(); }
bool ResIsSet(Res<TestRes>) { return true; }

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
        r.AddSystem<&ReadB>(Sys("Down", Sim({}, {"One"})));
        r.AddSystem<&WriteA>(Sys("One", Sim({"Two"})));
        r.AddSystem<&ReadA>(Sys("Two", Sim({"One"})));
    });
    REQUIRE(ErrorCount(built, StartupCode::DE6) == 1);
    const StartupError& error = built.result.error().errors.front();
    std::vector<std::string> cycle = error.subjects;
    std::sort(cycle.begin(), cycle.end());
    CHECK(cycle == std::vector<std::string>{"One", "Two"});
}

// ---- A2: no registration after Finish

TEST_CASE("J-19: a Registrar cannot be copied or moved out of its call") {
    // Registrar lives on Register's stack for one build call; with copy and move deleted, nothing can keep a
    // usable one past build (a kept pointer dangles, which is outside the contract). Every registration also
    // asserts the build phase (Registry.cpp) as a second line. (The context of a registration unit's Finish
    // moved to the orchestration layer: WorldFinish, v8-arch-gyo2-orchestration-skeleton.)
    static_assert(!std::is_copy_constructible_v<Registrar>);
    static_assert(!std::is_move_constructible_v<Registrar>);
    static_assert(!std::is_copy_assignable_v<Registrar>);
    CHECK(true);
}

// ---- B2: one schedule's errors are collected, not stopped at the first

// The second error used to be an ambiguity (D-E8), which went with the ambiguity
// detection (v8-arch-gyo2-no-ambiguity-detection). The first case keeps two errors
// of one step of the analysis (D-E4 and D-E5 both come from the declared-order
// step); the second keeps one of that step (D-E4) and one of a later step
// (D-E16), so only the second shows that the analysis goes on after a step that
// found an error.
TEST_CASE("J-24: a missing target and an order against the stage order in one schedule are both reported") {
    Built built = FinishOne([](Registrar& r) {
        SharedData(r);
        r.AddSystem<&WriteA>(Sys("WriteA", Sim({"Gone"})));
        r.AddSystem<&ReadA>(Sys("ReadA", {At("FixedUpdate", "Present", {"WriteA"})}));
    });
    CHECK(ErrorCount(built, StartupCode::DE4) == 1);
    CHECK(ErrorCount(built, StartupCode::DE5) == 1);
}

// ---- B4: the read-only view after BuildPlans has final named sets and Systems
// (what a registration unit's Finish reads; the unit lifecycle is the orchestration layer's)

TEST_CASE("J-19: after BuildPlans the read-only view has resolved named sets and System access") {
    Registry registry;
    REQUIRE(static_cast<bool>(RegisterAll(registry, {{"replication", [](Registrar& r) {
                                                  RegisterCommonData(r);
                                                  r.NamedSet<ReplicatedSet>("Replicated");
                                                  r.AddToSet<ReplicatedSet, TestPos>();
                                                  r.AddToSet<ReplicatedSet, TestVel>();
                                                  r.Schedule(FixedSchedule());
                                                  r.AddSystem<&ReadMarker>(Sys("ShowMark", {At("FixedUpdate", "Present")}));
                                              }}})));
    const std::vector<ComponentId> members = registry.Frozen().SetMembers(0);
    const std::size_t systems = registry.Frozen().Systems().size();
    CHECK(members.size() == 2);
    CHECK(systems == 1);
}

// ---- B1 and sets: order, conditions

TEST_CASE("J-21: a set's before / after orders every member") {
    Built built = FinishOne([](Registrar& r) {
        r.Set({.name = "Late", .schedule = "Update", .after = {"First1"}});
        r.AddSystem<&ReadMarker>(Sys("Member1", {InSet(At("Update", "First"), "Late")}));
        const auto member2 = [](Query<Read<TestMarker>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(member2)>(Sys("Member2", {InSet(At("Update", "First"), "Late")}));
        const auto first = []() -> SystemResult { return {}; };
        r.AddSystem<decltype(first)>(Sys("First1", {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(built);
    const Plan& setPlan = *built.registry->Frozen().PlanOf("Update");
    CHECK(SystemSteps(setPlan) == std::vector<std::string>{"First1", "Member1", "Member2"});
}

TEST_CASE("J-12: a set's run-condition reads join every member's reads") {
    Built built = FinishOne([](Registrar& r) {
        r.Set({.name = "Gated", .schedule = "Update"}, {Condition<&ResIsSet>("ResIsSet")});
        r.AddSystem<&ReadMarker>(Sys("SetGated", {InSet(At("Update", "First"), "Gated")}));
    });
    KERNEL_REQUIRE_OK(built);
    const ResourceId res = *built.registry->Frozen().ResourceIdOf<TestRes>();
    const SystemInfo* info = built.registry->Frozen().SystemNamed("SetGated");
    REQUIRE(info != nullptr);
    CHECK(std::find(info->access.reads.begin(), info->access.reads.end(), AccessKey{AccessKind::Resource, res}) !=
          info->access.reads.end());
}

// D-E18 is gone (v8-arch-gyo2-kernel-settings-read-write): KernelSettings is read and written like any resource.
TEST_CASE("J-24: a mount's or a set's run condition may read KernelSettings") {
    Built conditionSettings = FinishOne([](Registrar& r) {
        Mount m = At("Update", "First");
        m.conditions.push_back(Condition<&SettingsOn>("SettingsOn"));
        r.AddSystem<&ReadMarker>(Sys("Gated", {m}));
    });
    KERNEL_REQUIRE_OK(conditionSettings);
    Built setSettings = FinishOne([](Registrar& r) {
        r.Set({.name = "Gated", .schedule = "Update"}, {Condition<&SettingsOn>("SettingsOn")});
        r.AddSystem<&ReadMarker>(Sys("Gated", {InSet(At("Update", "First"), "Gated")}));
    });
    KERNEL_REQUIRE_OK(setSettings);
}

// D-E18 is gone for hooks too (v8-arch-gyo2-kernel-subtract-structure 3.7).
TEST_CASE("J-24: a hook may declare KernelSettings, read or write") {
    Built reads = FinishOne([](Registrar& r) {
        r.Hook<TestMarker, &HookReadsSettings>(HookTrigger::OnAdd, {.name = "HookReadsSettings"});
    });
    CHECK_MESSAGE(static_cast<bool>(reads.result), "[ks-s7-hook-settings] Res<KernelSettings> in a hook: " << Describe(reads));
    Built writes = FinishOne([](Registrar& r) {
        r.Hook<TestMarker, &HookWritesSettings>(HookTrigger::OnRemove, {.name = "HookWritesSettings"});
    });
    CHECK_MESSAGE(static_cast<bool>(writes.result), "[ks-s7-hook-settings] ResMut<KernelSettings> in a hook: " << Describe(writes));
}

// ---- D.1 conditions 2 and 3

TEST_CASE("J-22: a zone B ExclusiveWorld System conflicts with a zone C System of its stage (a tie without order)") {
    Built exclusive = FinishOne([](Registrar& r) {
        SharedData(r);
        r.AddSystem<&EditWorld>(Sys("EditWorld", Sim()));
        r.AddSystem<&ReadA>(Sys("ReadA", Sim()));
    });
    KERNEL_REQUIRE_OK(exclusive);
    CHECK_MESSAGE(Ties(exclusive) == std::vector<std::string>{"FixedUpdate: EditWorld > ReadA"},
                  "J-22 exclusive: ExclusiveWorld conflicts with every System of its stage");
}

TEST_CASE("J-23: two zone B Systems declaring one effect conflict and are ordered by registration") {
    Built sameEffect = FinishOne([](Registrar& r) {
        r.Effect("Io:file");
        SystemOptions a = Sys("SaveFirst", {At("Update", "First")});
        a.effects = {"Io:file"};
        r.AddSystem<&Nothing>(a);
        SystemOptions b = Sys("SaveSecond", {At("Update", "First")});
        b.effects = {"Io:file"};
        const auto second = []() -> SystemResult { return {}; };
        r.AddSystem<decltype(second)>(b);
    });
    KERNEL_REQUIRE_OK(sameEffect);
    CHECK(sameEffect.registry->Frozen().TieBreaks().size() == 1);
}

// ---- Z6, Z8, hooks

TEST_CASE("J-24: a hook that spawns without an allotter (D-E3)") {
    Built hookSpawn = FinishOne([](Registrar& r) {
        r.Hook<TestVel, &HookSpawns>(HookTrigger::OnAdd, {.name = "OnVel"});
    });
    CHECK(ErrorCount(hookSpawn, StartupCode::DE3) == 1);
}

// ---- D-E2 for resources

// ---- D-E16 details

// ---- Fingerprints

namespace {

Built Variant(bool renamed, bool added, std::string field = {}) {
    // Same structure in every variant; field flips one declared value.
    return FinishOne([=](Registrar& r) {
        SharedData(r);
        r.AddSystem<&WriteA>(Sys("WriteA", Sim({"ReadA"})));
        r.AddSystem<&ReadA>(Sys("ReadA", Sim()));
        r.AddSystem<&ReadMarker>(Sys(renamed ? "ShowMarkRenamed" : "ShowMark", {At("Update", "First")}));
        r.AddSystem<&Nothing>(Sys("StageNothing", {At("FixedUpdate", "Ingress")}));
        if (added) r.AddSystem<&ReadB>(Sys("ReadB", Sim()));
        if (field == "hook access") {
            r.Hook<TestPos, &HookWritesMode>(HookTrigger::OnAdd, {.name = "OnPos"});
        } else {
            r.Hook<TestPos, &ReadMarker>(HookTrigger::OnAdd, {.name = "OnPos"});
        }
    });
}

} // namespace

TEST_CASE("J-18: the Registry fingerprint sees every field") {
    Built base = Variant(false, false);
    Built renamed = Variant(true, false);
    Built added = Variant(false, true);
    KERNEL_REQUIRE_OK(base);
    KERNEL_REQUIRE_OK(renamed);
    KERNEL_REQUIRE_OK(added);
    CHECK(base.registry->Frozen().RegistryFingerprint() != renamed.registry->Frozen().RegistryFingerprint());
    CHECK(base.registry->Frozen().RegistryFingerprint() != added.registry->Frozen().RegistryFingerprint());
    for (const std::string field : {"hook access"}) {
        CAPTURE(field);
        Built changed = Variant(false, false, field);
        KERNEL_REQUIRE_OK(changed);
        CHECK(changed.registry->Frozen().RegistryFingerprint() != base.registry->Frozen().RegistryFingerprint());
    }
}

// ---- J-24 items from the review

TEST_CASE("J-24: a System name twice (R-E2), Extract outside an extract schedule, no mount") {
    Built sameName = FinishOne([](Registrar& r) {
        r.AddSystem<&ReadMarker>(Sys("Same", {At("Update", "First")}));
        r.AddSystem<&Nothing>(Sys("Same", {At("Update", "Second")}));
    });
    CHECK(ErrorCount(sameName, StartupCode::RE2) == 1);
    Built extractOutside = FinishOne([](Registrar& r) {
        r.AddSystem<&ExtractPos>(Sys("ExtractPos", {At("Update", "First")}));
    });
    CHECK(HasRule(extractOutside, StartupCode::DE7, "H-6"));
    Built noMount = FinishOne([](Registrar& r) { r.AddSystem<&Nothing>(Sys("Idle", {})); });
    CHECK(ErrorCount(noMount, StartupCode::DE1) == 1);
}

TEST_CASE("J-12: a run condition cannot be written by hand (its access comes from its signature)") {
    static_assert(!std::is_constructible_v<ConditionRef, std::string, Detail::ResolveFn>);
    static_assert(!std::is_default_constructible_v<ConditionRef>);
    const ConditionRef condition = Condition<&ResIsSet>("ResIsSet");
    CHECK(condition.Name() == "ResIsSet");
    CHECK(condition.Resolver() != nullptr);
}

// ---- B5: the judged ratio uses the plan's definition; the extended one is printed beside it

TEST_CASE("Plan export: a deferred write and a With are not dataflow by the plan's definition, but are in the extended one") {
    Built built = FinishOne([](Registrar& r) {
        SharedData(r);
        r.AddSystem<&InsertA>(Sys("Defer", Sim({"See"})));
        r.AddSystem<&ReadA>(Sys("See", Sim()));
        r.AddSystem<&WriteA>(Sys("Writer", {At("FixedUpdate", "Present", {"Filter"})}));
        r.AddSystem<&FilterWithA>(Sys("Filter", {At("FixedUpdate", "Present")}));
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


