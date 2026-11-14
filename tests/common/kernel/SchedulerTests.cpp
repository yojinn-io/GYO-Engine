// Spec J-20 to J-25, J-27 (D-E17 half), J-28 (finish half), J-44, the
// scheduler half of J-24, the Plan fingerprint goldens (declaration judgement 4)
// and the Plan export with its dataflow ratio (judgement 7).
// target/kernel DI-KS 3, 5, 7, 14; spec D.1 to D.3, X-2, X-3, H-1, H-2.

#include "KernelTestSupport.hpp"

#include <array>

namespace {

using namespace KernelTest;

// Zone C columns that allow several writers, so ambiguity cases do not also
// trip D-E2.
struct SharedA { std::int32_t v; };
struct SharedB { std::int32_t v; };
struct SharedC { std::int32_t v; };

void SharedData(Registrar& r) {
    r.Component<SharedA>({.name = "SharedA", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
    r.Component<SharedB>({.name = "SharedB", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
    r.Component<SharedC>({.name = "SharedC", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
}

SystemResult WriteA(Query<Write<SharedA>>) { return Ok(); }
SystemResult ReadA(Query<Read<SharedA>>) { return Ok(); }
SystemResult InsertA(CommandsFor<Inserts<SharedA>, Removes<>>) { return Ok(); }
SystemResult InsertA2(CommandsFor<Inserts<SharedA>, Removes<>>) { return Ok(); }
SystemResult FilterA(Query<Write<SharedB>, With<SharedA>>) { return Ok(); }
SystemResult WriteAWithB(Query<Write<SharedA>, With<SharedB>>) { return Ok(); }
SystemResult WriteAWithoutB(Query<Write<SharedA>, Without<SharedB>>) { return Ok(); }
SystemResult SendEvent(EventWriter<TestEvent>) { return Ok(); }
SystemResult SendEvent2(EventWriter<TestEvent>) { return Ok(); }
SystemResult ReadB(Query<Read<SharedB>>) { return Ok(); }
SystemResult WriteB(Query<Write<SharedB>>) { return Ok(); }
SystemResult WriteC(Query<Write<SharedC>>) { return Ok(); }
SystemResult ReadC(Query<Read<SharedC>>) { return Ok(); }
SystemResult ReadAB(Query<Read<SharedA>, Read<SharedB>>) { return Ok(); }
SystemResult WriteMarker(Query<Write<TestMarker>>) { return Ok(); }
SystemResult ReadMarker(Query<Read<TestMarker>>) { return Ok(); }
SystemResult Nothing() { return Ok(); }
SystemResult ReadPos(Query<Read<TestPos>>) { return Ok(); }
SystemResult SpawnPos(Spawn<Bundle<TestPos>>) { return Ok(); }
SystemResult ReadSettings(Res<KernelSettings>) { return Ok(); }
SystemResult HookWritesA(Query<Write<SharedA>>) { return Ok(); }
SystemResult HookReadsA(Query<Read<SharedA>>) { return Ok(); }
SystemResult ReadDerived(Query<Read<TestDerived>>) { return Ok(); }
SystemResult WriteDerived(Query<Write<TestDerived>>) { return Ok(); }
SystemResult ExtractPos(Extract<Read<TestPos>>) { return Ok(); }
SystemResult WriteInput(ResMut<TestInput>) { return Ok(); }

std::vector<Mount> Sim(std::vector<std::string> before = {}, std::vector<std::string> after = {}) {
    return {At("FixedUpdate", "Simulate", std::move(before), std::move(after))};
}

Built WithShared(std::function<void(Registrar&)> extra, bool rollback = false) {
    return FinishOne([extra = std::move(extra)](Registrar& r) {
        SharedData(r);
        extra(r);
    }, rollback);
}

} // namespace

// ---- J-20 (D-1)

namespace {

void MixedRegistry(Registrar& r) {
    // Golden registry 1: (c) and (b) mixed, a declared order, a (b) tie.
    RegisterCommonData(r);
    SharedData(r);
    r.Schedule(UpdateSchedule());
    r.Schedule(FixedSchedule());
    r.AddSystem<&WriteA>(Sys("StepA", Zone::C, Sim({"StepB"})));
    r.AddSystem<&FilterA>(Sys("StepB", Zone::C, Sim()));
    r.AddSystem<&ReadAB>(Sys("CheckAB", Zone::C, Sim({}, {"StepB"})));
    r.AddSystem<&ReadA>(Sys("ShowA", Zone::B, {At("FixedUpdate", "Present")}));
    r.AddSystem<&WriteMarker>(Sys("MarkLate", Zone::B, {At("Update", "First")}));
    r.AddSystem<&ReadMarker>(Sys("ShowMark", Zone::B, {At("Update", "First")}));
    r.AddSystem<&Nothing>(Sys("Idle", Zone::B, {At("Update", "Second")}));
    // Two (c) Systems with no conflict and no declared order: their order comes only from the tie key
    // (registrationSeq), so a key that depends on container order changes this Plan under the shuffle.
    const auto first = [](Query<Read<SharedC>>) -> SystemResult { return {}; };
    const auto second = [](Query<Read<SharedC>>) -> SystemResult { return {}; };
    r.AddSystem<decltype(first)>(Sys("FreeFirst", Zone::C, {At("FixedUpdate", "Present")}));
    r.AddSystem<decltype(second)>(Sys("FreeSecond", Zone::C, {At("FixedUpdate", "Present")}));
}

void DeferredRegistry(Registrar& r) {
    // Golden registry 2: deferred writes, a named sync point, a spawner.
    RegisterCommonData(r);
    SharedData(r);
    r.Resource<TestAllotter>({.name = "TestAllotter", .zone = Zone::C, .rollback = Rollback::Yes});
    r.Allotter<TestAllotter>();
    r.Schedule(UpdateSchedule());
    ScheduleOptions fixed = FixedSchedule();
    fixed.syncPoints = {{"AfterInsert", "Simulate"}};
    r.Schedule(fixed);
    r.AddSystem<&InsertA>(Sys("InsertA", Zone::C, Sim({"AfterInsert", "InsertA2"})));
    r.AddSystem<&InsertA2>(Sys("InsertA2", Zone::C, Sim({"AfterInsert"})));
    r.AddSystem<&ReadA>(Sys("SeeInserted", Zone::C, Sim({}, {"AfterInsert"})));
    r.AddSystem<&SpawnPos>(Sys("SpawnPos", Zone::C, {At("FixedUpdate", "Present")}));
}

void EventRegistry(Registrar& r) {
    // Golden registry 3: event writers and readers.
    RegisterCommonData(r);
    SharedData(r);
    r.Schedule(UpdateSchedule());
    r.Schedule(FixedSchedule());
    r.AddSystem<&SendEvent>(Sys("SendFirst", Zone::C, Sim({"SendSecond"})));
    r.AddSystem<&SendEvent2>(Sys("SendSecond", Zone::C, Sim()));
    const auto reader = [](EventReader<TestEvent>) -> SystemResult { return {}; };
    r.AddSystem<decltype(reader)>(Sys("ReadEvents", Zone::C, {At("FixedUpdate", "Present")}));
}

struct Golden {
    const char* name;
    void (*build)(Registrar&);
    const char* fixedPlan;
    const char* registry;
};

// Plan fingerprints of FixedUpdate and Registry fingerprints of the three golden
// registries; the same on every L1 platform (declaration judgement 4).
constexpr std::array<Golden, 3> kGoldens{{
    {"mixed", &MixedRegistry, "1:19c809655efc5a2d7887bc522793c9e9b2cd99fd728fd68cb554c11396d6bb5c",
     "1:26223cbafc01a99143197a38b599893e25af955e3c46862568c77b7d8307419a"},
    {"deferred", &DeferredRegistry, "1:a6a8b455af3e5b799e81574a1e0e977f58aec75d8dee1245a9b2600fbca40277",
     "1:5fc7f8765ea715fd6873435c1e55813f85eff911205208698d19725f426828b4"},
    {"events", &EventRegistry, "1:71e9a081a30f3bf3e4cf5ef7eac6cdea2042a81196aea64b534ae9e7ce4c9668",
     "1:1a8883951c850b347d38485ecb93f0cbf99475cbdccb31d21f90d63ac7a6d5c5"},
}};

Built BuildGolden(const Golden& golden, std::uint32_t shuffle = 0) {
    Built built{{}, nullptr, {}};
    built.plugins.push_back(std::make_unique<TestPlugin>("golden", golden.build));
    built.registry = std::make_unique<Registry>(std::vector<Plugin*>{built.plugins.back().get()});
    if (shuffle != 0) SchedulerShuffleProbe::Set(*built.registry, shuffle);
    built.result = built.registry->Finish();
    return built;
}

} // namespace

TEST_CASE("J-20: the Plan is built the same twice and with shuffled internal containers; golden fingerprints") {
    for (const Golden& golden : kGoldens) {
        CAPTURE(golden.name);
        Built first = BuildGolden(golden);
        Built second = BuildGolden(golden);
        KERNEL_REQUIRE_OK(first);
        KERNEL_REQUIRE_OK(second);
        const Plan* plan = first.registry->Frozen().PlanOf("FixedUpdate");
        REQUIRE(plan != nullptr);
        CHECK(plan->fingerprint == second.registry->Frozen().PlanOf("FixedUpdate")->fingerprint);
        for (std::uint32_t seed : {1U, 2U, 7U}) {
            Built shuffled = BuildGolden(golden, seed);
            KERNEL_REQUIRE_OK(shuffled);
            CHECK(shuffled.registry->Frozen().PlanOf("FixedUpdate")->fingerprint == plan->fingerprint);
            CHECK(shuffled.registry->Frozen().PlanOf("Update")->fingerprint ==
                  first.registry->Frozen().PlanOf("Update")->fingerprint);
            CHECK(shuffled.registry->Frozen().RegistryFingerprint() == first.registry->Frozen().RegistryFingerprint());
        }
        MESSAGE(std::string(golden.name) << " FixedUpdate plan " << plan->fingerprint.Text() << " registry "
                            << first.registry->Frozen().RegistryFingerprint().Text());
        CHECK(plan->fingerprint.Text() == golden.fixedPlan);
        CHECK(first.registry->Frozen().RegistryFingerprint().Text() == golden.registry);
    }
}

TEST_CASE("J-20: the golden Plans' step order") {
    Built mixed = BuildGolden(kGoldens[0]);
    KERNEL_REQUIRE_OK(mixed);
    CHECK(SystemSteps(*mixed.registry->Frozen().PlanOf("FixedUpdate")) ==
          std::vector<std::string>{"StepA", "StepB", "CheckAB", "ShowA", "FreeFirst", "FreeSecond"});
    CHECK(SystemSteps(*mixed.registry->Frozen().PlanOf("Update")) ==
          std::vector<std::string>{"MarkLate", "ShowMark", "Idle"});
    Built deferred = BuildGolden(kGoldens[1]);
    KERNEL_REQUIRE_OK(deferred);
    const Plan& plan = *deferred.registry->Frozen().PlanOf("FixedUpdate");
    CHECK(PositionOf(plan, "InsertA") < PositionOf(plan, "InsertA2"));
    CHECK(PositionOf(plan, "InsertA2") < PositionOf(plan, "AfterInsert"));
    CHECK(PositionOf(plan, "AfterInsert") < PositionOf(plan, "SeeInserted"));
    CHECK(PositionOf(plan, "SeeInserted") < PositionOf(plan, "Simulate"));
    // The clock schedule ends with its stage ends and the event trim.
    CHECK(plan.steps.back().kind == StepKind::EventTrim);
}

// ---- J-21 (D-2)

TEST_CASE("J-21: every conflict edge has a fixed order in the Plan") {
    Built built = FinishOne([](Registrar& r) {
        r.AddSystem<&WriteMarker>(Sys("W1", Zone::B, {At("Update", "First")}));
        r.AddSystem<&ReadMarker>(Sys("R1", Zone::B, {At("Update", "First")}));
        const auto w2 = [](Query<Write<TestMarker>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(w2)>(Sys("W2", Zone::B, {At("Update", "First", {"W1"})}));
        const auto r2 = [](Query<Read<TestMarker>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(r2)>(Sys("R2", Zone::B, {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(built);
    const Plan& plan = *built.registry->Frozen().PlanOf("Update");
    // W2 before W1 (declared); every other conflicting pair by registration order.
    CHECK(SystemSteps(plan) == std::vector<std::string>{"W2", "W1", "R1", "R2"});
    // Conflicting pairs: W1-R1, W1-W2, W1-R2, R1-W2, W2-R2. All have distinct positions in order.
    CHECK(built.registry->Frozen().TieBreaks().size() == 2);  // W1-R1 and W1-R2; the rest have a path
}

// ---- J-22 (D-3)

TEST_CASE("J-22: (c) ambiguity fails for immediate writes, deferred-deferred writes and event writes") {
    Built immediate = WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim()));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
    });
    CHECK(KernelTest::ErrorCount(immediate, StartupCode::DE8) == 1);
    REQUIRE_FALSE(static_cast<bool>(immediate.result));
    const StartupError& error = immediate.result.error().errors.front();
    CHECK(error.subjects[0] == "WriteA");
    CHECK(error.subjects[1] == "ReadA");
    CHECK(error.sources.size() == 2);

    Built deferred = WithShared([](Registrar& r) {
        r.AddSystem<&InsertA>(Sys("InsertA", Zone::C, Sim()));
        r.AddSystem<&InsertA2>(Sys("InsertA2", Zone::C, Sim()));
    });
    CHECK(KernelTest::ErrorCount(deferred, StartupCode::DE8) == 1);
    REQUIRE_FALSE(static_cast<bool>(deferred.result));

    Built events = WithShared([](Registrar& r) {
        r.AddSystem<&SendEvent>(Sys("SendEvent", Zone::C, Sim()));
        r.AddSystem<&SendEvent2>(Sys("SendEvent2", Zone::C, Sim()));
    });
    CHECK(KernelTest::ErrorCount(events, StartupCode::DE8) == 1);
    REQUIRE_FALSE(static_cast<bool>(events.result));

    // (b) reading (c) data with no order to its (c) writer is ambiguous too (scheduler section 2).
    Built presentation = WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim()));
        r.AddSystem<&ReadA>(Sys("ShowA", Zone::B, Sim()));
    });
    CHECK(KernelTest::ErrorCount(presentation, StartupCode::DE8) == 1);
}

TEST_CASE("J-22: adding before or after makes the (c) cases pass") {
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim({"ReadA"})));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
    }));
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.AddSystem<&InsertA>(Sys("InsertA", Zone::C, Sim()));
        r.AddSystem<&InsertA2>(Sys("InsertA2", Zone::C, Sim({}, {"InsertA"})));
    }));
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.AddSystem<&SendEvent>(Sys("SendEvent", Zone::C, Sim({"SendEvent2"})));
        r.AddSystem<&SendEvent2>(Sys("SendEvent2", Zone::C, Sim()));
    }));
}

TEST_CASE("J-22: pairs that are not ambiguous pass (deferred write against immediate read, immediate write, With)") {
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.AddSystem<&InsertA>(Sys("InsertA", Zone::C, Sim()));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
    }));
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.AddSystem<&InsertA>(Sys("InsertA", Zone::C, Sim()));
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim()));
    }));
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.AddSystem<&InsertA>(Sys("InsertA", Zone::C, Sim()));
        r.AddSystem<&FilterA>(Sys("FilterA", Zone::C, Sim()));
    }));
    // Systems in different stages are ordered by the stage end.
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim()));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, {At("FixedUpdate", "Present")}));
    }));
}

// ---- J-23 (D-4, D-10)

TEST_CASE("J-23: (b) ties follow registrationSeq, not names") {
    Built built = FinishOne([](Registrar& r) {
        r.AddSystem<&WriteMarker>(Sys("Zeta", Zone::B, {At("Update", "First")}));
        r.AddSystem<&ReadMarker>(Sys("Alpha", Zone::B, {At("Update", "First")}));
        const auto third = [](Query<Write<TestMarker>>) -> SystemResult { return {}; };
        r.AddSystem<decltype(third)>(Sys("Mid", Zone::B, {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(built);
    CHECK(SystemSteps(*built.registry->Frozen().PlanOf("Update")) == std::vector<std::string>{"Zeta", "Alpha", "Mid"});
    const auto ties = built.registry->Frozen().TieBreaks();
    REQUIRE(ties.size() == 3);
    CHECK(ties[0].first == "Zeta");
    CHECK(ties[0].second == "Alpha");
    // Systems with no conflict are not ties; they still follow registrationSeq.
    Built free = FinishOne([](Registrar& r) {
        r.AddSystem<&Nothing>(Sys("Zeta", Zone::B, {At("Update", "First")}));
        const auto other = []() -> SystemResult { return {}; };
        r.AddSystem<decltype(other)>(Sys("Alpha", Zone::B, {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(free);
    CHECK(SystemSteps(*free.registry->Frozen().PlanOf("Update")) == std::vector<std::string>{"Zeta", "Alpha"});
    CHECK(free.registry->Frozen().TieBreaks().empty());
}

// ---- J-25 (D-8)

TEST_CASE("J-25: With and Without do not remove a conflict") {
    Built built = WithShared([](Registrar& r) {
        r.AddSystem<&WriteAWithB>(Sys("WithB", Zone::C, Sim()));
        r.AddSystem<&WriteAWithoutB>(Sys("WithoutB", Zone::C, Sim()));
    });
    CHECK(KernelTest::ErrorCount(built, StartupCode::DE8) == 1);
    REQUIRE_FALSE(static_cast<bool>(built.result));
}

// ---- J-27 (D-E17 half; LIM-2)

TEST_CASE("J-27: any Parallel schedule fails in v1; a (c) Parallel schedule without a guard also reports X-E1") {
    Built bOnly = FinishOne([](Registrar& r) {
        ScheduleOptions s;
        s.name = "Ui";
        s.domain = Domain::Frame;
        s.stages = {"Draw"};
        s.executor = ExecutorKind::Parallel;
        s.parallelGuardTest = "ui.parallel_equivalence";
        r.Schedule(s);
        r.AddSystem<&ReadMarker>(Sys("Draw", Zone::B, {At("Ui", "Draw")}));
    });
    CHECK(KernelTest::ErrorCount(bOnly, StartupCode::DE17) == 1);
    REQUIRE_FALSE(static_cast<bool>(bOnly.result));
    CHECK_FALSE(KernelTest::HasError(bOnly, StartupCode::XE1));
    Built c = WithShared([](Registrar& r) {
        ScheduleOptions s;
        s.name = "FixedParallel";
        s.domain = Domain::Fixed;
        s.stages = {"Run"};
        s.executor = ExecutorKind::Parallel;
        r.Schedule(s);
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, {At("FixedParallel", "Run")}));
    });
    CHECK(KernelTest::HasError(c, StartupCode::DE17));
    REQUIRE_FALSE(static_cast<bool>(c.result));
    CHECK(KernelTest::ErrorCount(c, StartupCode::XE1) == 1);
    CHECK(KernelTest::ErrorCount(c, StartupCode::DE11) == 1);
}

// ---- J-28 (finish half; X-3, H-3)

TEST_CASE("J-28: a Main System on a role runner fails (X-E2); a named role on another runner fails (X-E3)") {
    const auto roleSchedule = [](Registrar& r) {
        ScheduleOptions s = FixedSchedule();
        s.runner = "sim";
        r.Schedule(s);
        ScheduleOptions u = UpdateSchedule();
        u.runner = "sim";
        r.Schedule(u);
    };
    Built main = Finish({{"test", [&](Registrar& r) {
                              RegisterCommonData(r);
                              roleSchedule(r);
                              SystemOptions o = Sys("NeedsMain", Zone::B, {At("FixedUpdate", "Present")});
                              o.affinity = Affinity::Named(kMainRole);
                              r.AddSystem<&ReadMarker>(o);
                          }}});
    CHECK(KernelTest::ErrorCount(main, StartupCode::XE2) == 1);
    REQUIRE_FALSE(static_cast<bool>(main.result));
    CHECK(KernelTest::ErrorCount(main, StartupCode::DE10) == 1);
    Built named = Finish({{"test", [&](Registrar& r) {
                               RegisterCommonData(r);
                               roleSchedule(r);
                               SystemOptions o = Sys("NeedsAudio", Zone::B, {At("FixedUpdate", "Present")});
                               o.affinity = Affinity::Named("audio");
                               r.AddSystem<&ReadMarker>(o);
                           }}});
    CHECK(KernelTest::ErrorCount(named, StartupCode::XE3) == 1);
    Built resource = Finish({{"test", [&](Registrar& r) {
                                  RegisterCommonData(r);
                                  r.Resource<TestLink>({.name = "Window", .zone = Zone::B, .external = true,
                                                        .affinity = Affinity::Named(kMainRole)});
                                  roleSchedule(r);
                                  const auto touch = [](Res<TestLink>) -> SystemResult { return {}; };
                                  r.AddSystem<decltype(touch)>(Sys("TouchWindow", Zone::B, {At("Update", "First")}));
                              }}});
    CHECK(KernelTest::ErrorCount(resource, StartupCode::XE2) == 1);
    Built fine = Finish({{"test", [&](Registrar& r) {
                              RegisterCommonData(r);
                              roleSchedule(r);
                              SystemOptions o = Sys("OnSim", Zone::B, {At("FixedUpdate", "Present")});
                              o.affinity = Affinity::Named("sim");
                              r.AddSystem<&ReadMarker>(o);
                          }}});
    KERNEL_REQUIRE_OK(fine);
}

// ---- J-44 (D-9)

namespace {

using Registration = void (*)(Registrar&);

// Five (c) Systems; in the ordered set every conflicting pair has a declared order.
constexpr std::array<Registration, 5> kOrdered{{
    [](Registrar& r) { r.AddSystem<&WriteA>(Sys("P0", Zone::C, Sim({"P1", "P3"}))); },
    [](Registrar& r) { r.AddSystem<&FilterA>(Sys("P1", Zone::C, Sim({"P2"}))); },
    [](Registrar& r) { r.AddSystem<&ReadB>(Sys("P2", Zone::C, Sim())); },
    [](Registrar& r) { r.AddSystem<&WriteC>(Sys("P3", Zone::C, Sim({"P4"}))); },
    [](Registrar& r) { r.AddSystem<&ReadAB>(Sys("P4", Zone::C, Sim({}, {"P1"}))); },
}};
// The same five with one order missing (P0 writes A, P4 reads A, no path between them).
constexpr std::array<Registration, 5> kAmbiguous{{
    [](Registrar& r) { r.AddSystem<&WriteA>(Sys("P0", Zone::C, Sim({"P1"}))); },
    [](Registrar& r) { r.AddSystem<&FilterA>(Sys("P1", Zone::C, Sim({"P2"}))); },
    [](Registrar& r) { r.AddSystem<&ReadB>(Sys("P2", Zone::C, Sim())); },
    [](Registrar& r) { r.AddSystem<&WriteC>(Sys("P3", Zone::C, Sim({"P4"}))); },
    [](Registrar& r) { r.AddSystem<&ReadA>(Sys("P4", Zone::C, Sim())); },
}};

Built Permuted(const std::array<Registration, 5>& systems, const std::array<int, 5>& order) {
    return WithShared([&](Registrar& r) {
        for (int index : order) systems[static_cast<std::size_t>(index)](r);
    });
}

} // namespace

TEST_CASE("J-44: 20 registration orders of (c) Systems: either all D-E8, or every conflicting pair keeps its order") {
    std::array<int, 5> order{0, 1, 2, 3, 4};
    std::vector<std::array<int, 5>> orders;
    // 20 permutations: every 6th of the 120, in lexicographic order, so first and last positions both vary.
    int index = 0;
    do {
        if (index % 6 == 0) orders.push_back(order);
        ++index;
    } while (std::next_permutation(order.begin(), order.end()));
    REQUIRE(orders.size() == 20);

    const std::vector<std::pair<std::string, std::string>> conflicting{
        {"P0", "P1"}, {"P0", "P4"}, {"P1", "P2"}, {"P1", "P4"}, {"P3", "P4"}};
    std::vector<bool> reference;
    for (const auto& permutation : orders) {
        Built built = Permuted(kOrdered, permutation);
        KERNEL_REQUIRE_OK(built);
        const Plan& plan = *built.registry->Frozen().PlanOf("FixedUpdate");
        std::vector<bool> firstBeforeSecond;
        for (const auto& [a, b] : conflicting) firstBeforeSecond.push_back(PositionOf(plan, a) < PositionOf(plan, b));
        if (reference.empty()) reference = firstBeforeSecond;
        CHECK(firstBeforeSecond == reference);
    }
    CHECK(reference == std::vector<bool>{true, true, true, true, true});
    for (const auto& permutation : orders) {
        Built built = Permuted(kAmbiguous, permutation);
        CHECK(KernelTest::ErrorCount(built, StartupCode::DE8) >= 1);
        REQUIRE_FALSE(static_cast<bool>(built.result));
    }
}

// ---- J-24 (scheduler and composition codes; D-5: each from a minimal Registry, names both sides)

TEST_CASE("J-24: D-E1 (missing schedule or stage), D-E19 (two mounts in one schedule)") {
    Built missing = FinishOne([](Registrar& r) {
        r.AddSystem<&ReadPos>(Sys("NoSchedule", Zone::C, {At("Nowhere", "Run")}));
        r.AddSystem<&ReadMarker>(Sys("NoStage", Zone::B, {At("Update", "Nowhere")}));
    });
    CHECK(KernelTest::ErrorCount(missing, StartupCode::DE1) == 2);
    Built twice = FinishOne([](Registrar& r) {
        r.AddSystem<&ReadMarker>(Sys("Twice", Zone::B, {At("Update", "First"), At("Update", "Second")}));
    });
    CHECK(KernelTest::ErrorCount(twice, StartupCode::DE19) == 1);
}

TEST_CASE("J-24: D-E3 (Spawn without an allotter), D-E18 (Res of KernelSettings)") {
    Built spawn = FinishOne([](Registrar& r) { r.AddSystem<&SpawnPos>(Sys("SpawnPos", Zone::C, Sim())); });
    CHECK(KernelTest::ErrorCount(spawn, StartupCode::DE3) == 1);
    Built settings = FinishOne([](Registrar& r) {
        r.AddSystem<&ReadSettings>(Sys("ReadSettings", Zone::B, {At("Update", "First")}));
    });
    CHECK(KernelTest::ErrorCount(settings, StartupCode::DE18) == 1);
    const auto writes = [](ResMut<KernelSettings>) -> SystemResult { return {}; };
    Built settingsMut = FinishOne([&](Registrar& r) {
        r.AddSystem<decltype(writes)>(Sys("WriteSettings", Zone::B, {At("Update", "First")}));
    });
    CHECK(KernelTest::ErrorCount(settingsMut, StartupCode::DE18) == 1);
}

TEST_CASE("J-24: D-E4 (missing target), D-E5 (against stage order), D-E6 (cycle)") {
    Built target = WithShared([](Registrar& r) { r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim({"Gone"}))); });
    REQUIRE(KernelTest::ErrorCount(target, StartupCode::DE4) == 1);
    CHECK(target.result.error().errors.front().subjects == std::vector<std::string>{"WriteA", "Gone"});
    Built stage = WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("Early", Zone::C, Sim()));
        r.AddSystem<&ReadA>(Sys("Late", Zone::C, {At("FixedUpdate", "Present", {"Early"})}));
    });
    CHECK(KernelTest::ErrorCount(stage, StartupCode::DE5) == 1);
    Built cycle = WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("One", Zone::C, Sim({"Two"})));
        r.AddSystem<&ReadA>(Sys("Two", Zone::C, Sim({"Three"})));
        r.AddSystem<&ReadB>(Sys("Three", Zone::C, Sim({"One"})));
    });
    REQUIRE(KernelTest::ErrorCount(cycle, StartupCode::DE6) == 1);
    CHECK(cycle.result.error().errors.front().subjects.size() == 3);
}

TEST_CASE("J-24: D-E9 (zone C hooks on one trigger conflict)") {
    Built built = WithShared([](Registrar& r) {
        r.Hook<TestPos, &HookWritesA>(HookTrigger::OnAdd, {.name = "WriteOnAdd", .zone = Zone::C});
        r.Hook<TestPos, &HookReadsA>(HookTrigger::OnAdd, {.name = "ReadOnAdd", .zone = Zone::C});
    });
    REQUIRE(KernelTest::ErrorCount(built, StartupCode::DE9) == 1);
    CHECK(built.result.error().errors.front().subjects[0] == "WriteOnAdd");
    KERNEL_REQUIRE_OK(WithShared([](Registrar& r) {
        r.Hook<TestPos, &HookWritesA>(HookTrigger::OnAdd, {.name = "WriteOnAdd", .zone = Zone::C});
        r.Hook<TestPos, &HookReadsA>(HookTrigger::OnRemove, {.name = "ReadOnRemove", .zone = Zone::C});
    }));
}

TEST_CASE("J-24: D-E12 (clock schedules per domain), D-E13 (one World on two threads)") {
    Built twoClocks = FinishOne([](Registrar& r) {
        ScheduleOptions s = UpdateSchedule();
        s.name = "SecondFrame";
        r.Schedule(s);
    });
    CHECK(KernelTest::ErrorCount(twoClocks, StartupCode::DE12) == 1);
    Built noClock = Finish({{"test", [](Registrar& r) {
                                ScheduleOptions s = UpdateSchedule();
                                s.clock = false;
                                r.Schedule(s);
                            }}});
    CHECK(KernelTest::ErrorCount(noClock, StartupCode::DE12) == 1);
    Built threads = FinishOne([](Registrar& r) {
        ScheduleOptions s = UpdateSchedule();
        s.name = "Late";
        s.clock = false;
        s.runner = "other";
        r.Schedule(s);
    });
    CHECK(KernelTest::ErrorCount(threads, StartupCode::DE13) == 1);
}

TEST_CASE("J-24: D-E14 (extract source not frozen or not set)") {
    const auto build = [](Registrar& r) {
        r.Component<TestMarker>({.name = "TestMarker", .zone = Zone::B});
        ScheduleOptions s;
        s.name = "Extract";
        s.domain = Domain::Frame;
        s.clock = true;
        s.extract = true;
        s.stages = {"Copy"};
        r.Schedule(s);
        r.AddSystem<&ExtractPos>(Sys("ExtractPos", Zone::B, {At("Extract", "Copy")}));
    };
    Built unset = Finish({{"render", build}});
    CHECK(KernelTest::ErrorCount(unset, StartupCode::DE14) == 1);
    TestPlugin sourcePlugin("source", [](Registrar& r) {
        r.Component<TestPos>({.name = "TestPos", .zone = Zone::C});
        r.Schedule(FixedSchedule());
    });
    Registry source({&sourcePlugin});
    TestPlugin renderPlugin("render", build);
    Registry render({&renderPlugin});
    render.SetExtractSource(source);  // source not finished yet
    const auto result = render.Finish();
    CHECK(KernelTest::ErrorCount(result, StartupCode::DE14) == 1);
}

TEST_CASE("J-24: D-E15 (ingress without NotInRollback), D-E16 (Derived read before any writer)") {
    const auto ingress = [](bool notInRollback) {
        return [notInRollback](Registrar& r) {
            r.Resource<TestInput>({.name = "TestInput", .zone = Zone::C, .role = Role::Input});
            Mount m = At("FixedUpdate", "Ingress");
            m.notInRollback = notInRollback;
            SystemOptions o = Sys("StageInput", Zone::B, {m});
            o.ingress = true;
            r.AddSystem<&WriteInput>(o);
        };
    };
    CHECK(KernelTest::ErrorCount(FinishOne(ingress(false), true), StartupCode::DE15) == 1);
    KERNEL_REQUIRE_OK(FinishOne(ingress(true), true));
    const auto derived = [](bool writerFirst) {
        return [writerFirst](Registrar& r) {
            r.Component<TestDerived>({.name = "TestDerived", .zone = Zone::C, .rollback = Rollback::Derived});
            r.AddSystem<&WriteDerived>(Sys("Recompute", Zone::C, Sim(writerFirst ? std::vector<std::string>{"UseDerived"}
                                                                                   : std::vector<std::string>{})));
            r.AddSystem<&ReadDerived>(Sys("UseDerived", Zone::C, Sim(writerFirst ? std::vector<std::string>{}
                                                                                   : std::vector<std::string>{"Recompute"})));
        };
    };
    CHECK(KernelTest::ErrorCount(FinishOne(derived(false), true), StartupCode::DE16) == 1);
    KERNEL_REQUIRE_OK(FinishOne(derived(true), true));
}

TEST_CASE("J-24: several errors of different codes come back in one report") {
    Built built = WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("WriteA", Zone::C, Sim({"Gone"})));
        r.AddSystem<&ReadA>(Sys("ReadA", Zone::C, Sim()));
        r.AddSystem<&SpawnPos>(Sys("SpawnPos", Zone::C, Sim()));
        r.AddSystem<&ReadSettings>(Sys("ReadSettings", Zone::B, {At("Update", "First")}));
        r.RequireExplicitZones();
        r.AddSystem<&ReadMarker>(Sys("NoZone", Zone::Unset, {At("Update", "First")}));
    });
    REQUIRE_FALSE(static_cast<bool>(built.result));
    const StartupReport& report = built.result.error();
    CHECK(report.Has(StartupCode::DE3));
    CHECK(report.Has(StartupCode::DE4));
    CHECK(report.Has(StartupCode::DE7));
    CHECK(report.Has(StartupCode::DE18));
    CHECK(report.errors.size() >= 4);
    CHECK(report.detail.find("D-E4") != std::string::npos);
}

// ---- Plan export and the dataflow ratio (migration plan section 3.5; judgement 7)

TEST_CASE("Plan export: 5 before/after edges, 2 without dataflow, prints 40%") {
    Built built = WithShared([](Registrar& r) {
        r.AddSystem<&WriteA>(Sys("E1", Zone::C, Sim({"E2", "E4"})));  // E1->E2 dataflow (A read by E2), E1->E4 none
        r.AddSystem<&ReadA>(Sys("E2", Zone::C, Sim()));
        r.AddSystem<&WriteB>(Sys("E3", Zone::C, Sim({"E5"}, {"E2"})));  // E2->E3 none (E2 writes nothing), E3->E5 dataflow
        r.AddSystem<&WriteC>(Sys("E4", Zone::C, Sim({"E6"})));  // E4->E6 dataflow (C read by E6)
        r.AddSystem<&ReadB>(Sys("E5", Zone::C, Sim({}, {"E3"})));  // E3->E5 declared from both ends: one edge
        r.AddSystem<&ReadC>(Sys("E6", Zone::C, Sim()));
    });
    KERNEL_REQUIRE_OK(built);
    const Plan& plan = *built.registry->Frozen().PlanOf("FixedUpdate");
    CHECK(plan.declaredEdges.size() == 5);
    CHECK(plan.NonDataflowEdges() == 2);
    const std::string report = built.registry->Frozen().ExportPlans();
    CHECK(report.find("non_dataflow_edges 2/5 40%") != std::string::npos);
    CHECK(report.find("edge E1 -> E4 non-dataflow") != std::string::npos);
    CHECK(report.find("edge E2 -> E3 non-dataflow") != std::string::npos);
    CHECK(report.find("edge E1 -> E2 dataflow") != std::string::npos);
    CHECK(report.find("gyo.kernel.plans 1") == 0);
    CHECK(report.find("fingerprint 1:") != std::string::npos);
}
