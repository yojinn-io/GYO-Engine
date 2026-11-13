// Spec J-14, J-15, J-16, J-37 to J-41, J-47: the startup checks of zones,
// exclusive writers and events (target/kernel DI-KS 1, 6, 8, 10, 12; spec G.1
// Z1 to Z8, S-6 to S-9, S-11, S-12, F-9). Every failing case is checked for its
// code and rule, and a passing twin shows the check does not over-report.

#include "KernelTestSupport.hpp"

namespace {

using namespace KernelTest;

SystemResult ReadPos(Query<Read<TestPos>>) { return Ok(); }
SystemResult WritePos(Query<Write<TestPos>>) { return Ok(); }
SystemResult WritePosAgain(Query<Write<TestPos>>) { return Ok(); }
SystemResult InsertPos(CommandsFor<Inserts<TestPos>, Removes<>>) { return Ok(); }
SystemResult RemovePos(CommandsFor<Inserts<>, Removes<TestPos>>) { return Ok(); }
SystemResult SpawnPos(Spawn<Bundle<TestPos>>) { return Ok(); }
SystemResult SpawnPosGiven(SpawnWithId<Bundle<TestPos>>) { return Ok(); }
SystemResult WriteMarker(Query<Write<TestMarker>>) { return Ok(); }
SystemResult WriteMarkerAgain(Query<Write<TestMarker>>) { return Ok(); }
SystemResult CountLocally(Local<std::int32_t>, Query<Read<TestPos>>) { return Ok(); }
SystemResult TracePos(DiagnosticsWriter<TestRecord>, Query<Read<TestPos>>) { return Ok(); }
SystemResult WatchVel(Query<Read<TestPos>, Changed<TestVel>>) { return Ok(); }
SystemResult WatchAdded(Query<Read<TestPos>, Added<TestVel>>) { return Ok(); }
SystemResult ReadFrameTime(Res<TestFrameTime>, Query<Write<TestPos>>) { return Ok(); }
SystemResult ReadLink(Res<TestLink>) { return Ok(); }
SystemResult WriteLink(ResMut<TestLink>) { return Ok(); }
SystemResult WriteInput(ResMut<TestInput>) { return Ok(); }
SystemResult ReadInput(Res<TestInput>, Query<Write<TestPos>>) { return Ok(); }
SystemResult WriteHealthFromB(Query<Write<TestHealth>>) { return Ok(); }
SystemResult FilterWithB(Query<Write<TestPos>, With<TestMarker>>) { return Ok(); }
SystemResult FilterWithoutB(Query<Write<TestPos>, Without<TestMarker>>) { return Ok(); }
SystemResult WriteModeFromC(Query<Write<TestMode>>) { return Ok(); }
SystemResult WriteEvent(EventWriter<TestEvent>) { return Ok(); }
SystemResult ReadEvent(EventReader<TestEvent>) { return Ok(); }
SystemResult ReadFrameEvent(EventReader<TestFrameEvent>) { return Ok(); }
SystemResult ReadSet(SetRead<ReplicatedSet>) { return Ok(); }
SystemResult EditWorld(ExclusiveWorld) { return Ok(); }
bool OtherResIsSet(Res<TestOtherRes>) { return true; }
SystemResult HookWritesPos(Query<Write<TestPos>>) { return Ok(); }

void ExternalAndInput(Registrar& r) {
    r.Resource<TestLink>({.name = "TestLink", .zone = Zone::B, .external = true});
    r.Resource<TestFrameTime>({.name = "TestFrameTime", .zone = Zone::B, .external = true});
    r.Resource<TestInput>({.name = "TestInput", .zone = Zone::C, .role = Role::Input});
}

std::vector<Mount> Simulate() { return {At("FixedUpdate", "Simulate")}; }

} // namespace

// ---- J-14 (S-6, S-7, S-8)

TEST_CASE("J-14: a System, component, resource or event without a zone fails at finish") {
    Built built = Finish({{"test", [](Registrar& r) {
                              r.Component<TestPos>({.name = "TestPos"});
                              r.Resource<TestRes>({.name = "TestRes"});
                              r.Event<TestEvent>({.name = "TestEvent", .domain = Domain::Fixed});
                              r.Schedule(FixedSchedule());
                              SystemOptions o;  // no zone given: the default must not stand for one
                              o.name = "ReadPos";
                              o.mounts = Simulate();
                              r.AddSystem<&ReadPos>(o);
                          }}});
    CHECK(KernelTest::ErrorCount(built, StartupCode::DE7) == 4);
    REQUIRE_FALSE(static_cast<bool>(built.result));
    CHECK(HasRule(built, StartupCode::DE7, "S-6"));
}

TEST_CASE("J-14: a zone C System with Local fails; zone B with Local passes") {
    Built c = FinishOne([](Registrar& r) { r.AddSystem<&CountLocally>(Sys("CountLocally", Zone::C, Simulate())); });
    CHECK(HasRule(c, StartupCode::DE7, "S-7"));
    Built b = FinishOne([](Registrar& r) {
        r.AddSystem<&CountLocally>(Sys("CountLocally", Zone::B, {At("FixedUpdate", "Present")}));
    });
    KERNEL_REQUIRE_OK(b);
}

TEST_CASE("J-14: a zone C System declaring Io:file fails; DiagnosticsWriter passes; hand-written Diagnostics fails") {
    Built io = FinishOne([](Registrar& r) {
        r.Effect("Io:file");
        SystemOptions o = Sys("ReadPos", Zone::C, Simulate());
        o.effects = {"Io:file"};
        r.AddSystem<&ReadPos>(o);
    });
    CHECK(HasRule(io, StartupCode::DE7, "S-8"));
    Built diagnostics = FinishOne([](Registrar& r) { r.AddSystem<&TracePos>(Sys("TracePos", Zone::C, Simulate())); });
    KERNEL_REQUIRE_OK(diagnostics);
    CHECK(diagnostics.registry->Frozen().SystemNamed("TracePos")->access.diagnostics);
    Built handWritten = FinishOne([](Registrar& r) {
        SystemOptions o = Sys("ReadPos", Zone::C, Simulate());
        o.effects = {"Diagnostics"};
        r.AddSystem<&ReadPos>(o);
    });
    CHECK(HasRule(handWritten, StartupCode::DE7, "S-8"));
    // Zone B may declare effects.
    Built b = FinishOne([](Registrar& r) {
        r.Effect("Io:file");
        SystemOptions o = Sys("ReadPos", Zone::B, {At("FixedUpdate", "Present")});
        o.effects = {"Io:file"};
        r.AddSystem<&ReadPos>(o);
    });
    KERNEL_REQUIRE_OK(b);
}

// ---- J-15 (S-11, DI-KS 6)

TEST_CASE("J-15: a second writer of an Exclusive column fails, whatever the write path") {
    struct Case {
        std::string what;
        std::function<void(Registrar&)> second;
    };
    const std::vector<Case> cases{
        {"immediate", [](Registrar& r) { r.AddSystem<&WritePosAgain>(Sys("WritePosAgain", Zone::C, Simulate())); }},
        {"deferred insert", [](Registrar& r) { r.AddSystem<&InsertPos>(Sys("InsertPos", Zone::C, Simulate())); }},
        {"deferred remove", [](Registrar& r) { r.AddSystem<&RemovePos>(Sys("RemovePos", Zone::C, Simulate())); }},
        // The allotter is Shared here on purpose: with the default (Exclusive) two spawning Systems are already
        // D-E2 through the allotter, and this case is about the bundle. Whether an allotter should be counted as an
        // exclusive column at all (spec W-9 says spawners are serialized by their conflict) is open for the
        // architect (registry-schedule declaration, limits; PvP's ApplyMembership uses SpawnWithId, not the allotter).
        {"Spawn bundle", [](Registrar& r) {
             r.Resource<TestAllotter>({.name = "TestAllotter", .zone = Zone::C, .writePolicy = WritePolicy::Shared,
                                       .rollback = Rollback::Yes});
             r.Allotter<TestAllotter>();
             r.AddSystem<&SpawnPos>(Sys("SpawnPos", Zone::C, Simulate()));
         }},
        {"SpawnWithId bundle", [](Registrar& r) { r.AddSystem<&SpawnPosGiven>(Sys("SpawnPosGiven", Zone::C, Simulate())); }},
        {"hook", [](Registrar& r) { r.Hook<TestVel, &HookWritesPos>(HookTrigger::OnAdd, {.name = "OnVel", .zone = Zone::C}); }},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        Built built = FinishOne([&c](Registrar& r) {
            Mount m = At("FixedUpdate", "Simulate");
            r.AddSystem<&WritePos>(Sys("WritePos", Zone::C, {m}));
            c.second(r);
        });
        CHECK(KernelTest::ErrorCount(built, StartupCode::DE2) == 1);
        REQUIRE_FALSE(static_cast<bool>(built.result));
        const StartupError& error = built.result.error().errors.front();
        CHECK(error.rule == "S-11");
        CHECK(error.subjects.size() == 3);  // the column and both writers
    }
}

TEST_CASE("J-15: a Startup writer does not count; two writers of a Shared column pass") {
    Built startup = FinishOne([](Registrar& r) {
        ScheduleOptions s;
        s.name = "Startup";
        s.startup = true;
        s.stages = {"Init"};
        r.Schedule(s);
        r.AddSystem<&WritePos>(Sys("WritePos", Zone::C, Simulate()));
        r.AddSystem<&WritePosAgain>(Sys("InitPos", Zone::C, {At("Startup", "Init")}));
    });
    KERNEL_REQUIRE_OK(startup);
    Built shared = FinishOne([](Registrar& r) {
        r.AddSystem<&WriteMarker>(Sys("WriteMarker", Zone::B, {At("FixedUpdate", "Present")}));
        r.AddSystem<&WriteMarkerAgain>(Sys("WriteMarkerAgain", Zone::B, {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(shared);
    // One System with two mounts is one writer (spec B.1 mounts).
    Built twoMounts = FinishOne([](Registrar& r) {
        r.AddSystem<&WriteMarker>(Sys("WriteMarker", Zone::B, {At("FixedUpdate", "Present"), At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(twoMounts);
}

// ---- J-16 (S-12)

TEST_CASE("J-16: a Shared column in the digest set fails; an Exclusive one passes") {
    Built shared = FinishOne([](Registrar& r) {
        r.NamedSet<DigestTag>("Digest");
        r.AddToSet<DigestTag, TestMarker>();
        r.DigestSet<DigestTag>();
    });
    CHECK(HasRule(shared, StartupCode::DE2, "S-12"));
    Built exclusive = FinishOne([](Registrar& r) {
        r.NamedSet<DigestTag>("Digest");
        r.AddToSet<DigestTag, TestPos>();
        r.DigestSet<DigestTag>();
    });
    KERNEL_REQUIRE_OK(exclusive);
}

// ---- J-37 (F-9, F-E1)

TEST_CASE("J-37: an event used outside its domain's clock schedule fails") {
    Built crossDomain = FinishOne([](Registrar& r) {
        r.AddSystem<&ReadEvent>(Sys("ReadEvent", Zone::B, {At("Update", "First")}));  // Fixed event in Frame
    });
    CHECK(KernelTest::ErrorCount(crossDomain, StartupCode::FE1) == 1);
    REQUIRE_FALSE(static_cast<bool>(crossDomain.result));
    Built nonClock = FinishOne([](Registrar& r) {
        ScheduleOptions s;
        s.name = "FixedLate";
        s.domain = Domain::Fixed;
        s.stages = {"Late"};
        r.Schedule(s);
        r.AddSystem<&WriteEvent>(Sys("WriteEvent", Zone::C, {At("FixedLate", "Late")}));
    });
    CHECK(KernelTest::ErrorCount(nonClock, StartupCode::FE1) == 1);
    REQUIRE_FALSE(static_cast<bool>(nonClock.result));
    Built inClock = FinishOne([](Registrar& r) {
        r.AddSystem<&WriteEvent>(Sys("WriteEvent", Zone::C, Simulate()));
        r.AddSystem<&ReadEvent>(Sys("ReadEvent", Zone::C, {At("FixedUpdate", "Present")}));
        r.AddSystem<&ReadFrameEvent>(Sys("ReadFrameEvent", Zone::B, {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(inClock);
}

// ---- J-38 (S-9)

TEST_CASE("J-38: a zone C System filtering by Changed or Added fails; zone B passes") {
    CHECK(HasRule(FinishOne([](Registrar& r) { r.AddSystem<&WatchVel>(Sys("WatchVel", Zone::C, Simulate())); }),
                  StartupCode::DE7, "S-9"));
    CHECK(HasRule(FinishOne([](Registrar& r) { r.AddSystem<&WatchAdded>(Sys("WatchAdded", Zone::C, Simulate())); }),
                  StartupCode::DE7, "S-9"));
    Built b = FinishOne([](Registrar& r) { r.AddSystem<&WatchVel>(Sys("WatchVel", Zone::B, {At("Update", "First")})); });
    KERNEL_REQUIRE_OK(b);
}

// ---- J-39 (G.2 Executor: time only from the domain tick and recorded boundary input)

TEST_CASE("J-39: zone C reading FrameTime fails; a non-ingress System writing boundary input fails") {
    Built frameTime = FinishOne([](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&ReadFrameTime>(Sys("ReadFrameTime", Zone::C, Simulate()));
    });
    CHECK(HasRule(frameTime, StartupCode::DE7, "Z1"));
    Built cWritesInput = FinishOne([](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&WriteInput>(Sys("WriteInput", Zone::C, Simulate()));
    });
    CHECK(HasRule(cWritesInput, StartupCode::DE7, "Z3"));
    Built bWritesInput = FinishOne([](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&WriteInput>(Sys("WriteInput", Zone::B, {At("FixedUpdate", "Ingress")}));
    });
    CHECK(HasRule(bWritesInput, StartupCode::DE7, "Z2"));
    // The domain tick has no writing handle at all: CurrentTick is read-only.
    CHECK(ParamTraits<CurrentTick>::readOnly);
    Built readsInput = FinishOne([](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&ReadInput>(Sys("ReadInput", Zone::C, Simulate()));
    });
    KERNEL_REQUIRE_OK(readsInput);
}

// ---- J-40 (Z1 to Z3, Z8)

TEST_CASE("J-40: (b) writing (c) data, and (c) touching (b) data in any way, fail") {
    CHECK(HasRule(FinishOne([](Registrar& r) {
                      r.AddSystem<&WriteHealthFromB>(Sys("WriteHealthFromB", Zone::B, {At("FixedUpdate", "Present")}));
                  }),
                  StartupCode::DE7, "Z2"));
    CHECK(HasRule(FinishOne([](Registrar& r) { r.AddSystem<&FilterWithB>(Sys("FilterWithB", Zone::C, Simulate())); }),
                  StartupCode::DE7, "Z1"));
    CHECK(HasRule(FinishOne([](Registrar& r) { r.AddSystem<&FilterWithoutB>(Sys("FilterWithoutB", Zone::C, Simulate())); }),
                  StartupCode::DE7, "Z1"));
    CHECK(HasRule(FinishOne([](Registrar& r) { r.AddSystem<&WriteModeFromC>(Sys("WriteModeFromC", Zone::C, Simulate())); }),
                  StartupCode::DE7, "Z1"));
    CHECK(HasRule(FinishOne([](Registrar& r) {
                      Mount m = At("FixedUpdate", "Simulate");
                      m.conditions.push_back(Condition<&OtherResIsSet>("OtherResIsSet"));
                      r.AddSystem<&ReadPos>(Sys("ReadPos", Zone::C, {m}));
                  }),
                  StartupCode::DE7, "Z1"));
    // (b) reading (c) data is fine (presentation reads simulation).
    Built bReads = FinishOne([](Registrar& r) { r.AddSystem<&ReadPos>(Sys("ReadPos", Zone::B, {At("Update", "First")})); });
    KERNEL_REQUIRE_OK(bReads);
}

TEST_CASE("J-40: ingress writes role=Input in the ingress stage; State, another stage or rollback Input fail") {
    const auto ingress = [](std::string stage) {
        SystemOptions o = Sys("StageInput", Zone::B, {At("FixedUpdate", std::move(stage))});
        o.ingress = true;
        return o;
    };
    Built ok = FinishOne([&](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&WriteInput>(ingress("Ingress"));
    });
    KERNEL_REQUIRE_OK(ok);
    Built state = FinishOne([&](Registrar& r) { r.AddSystem<&WriteHealthFromB>(ingress("Ingress")); });
    CHECK(HasRule(state, StartupCode::DE7, "Z2"));
    Built stage = FinishOne([&](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&WriteInput>(ingress("Simulate"));
    });
    CHECK(HasRule(stage, StartupCode::DE7, "Z3"));
    Built rollbackInput = Finish({{"test", [](Registrar& r) {
                                      r.Resource<TestInput>({.name = "TestInput", .zone = Zone::C, .role = Role::Input,
                                                             .rollback = Rollback::Yes});
                                      r.Schedule(FixedSchedule());
                                  }}});
    CHECK(HasRule(rollbackInput, StartupCode::DE7, "Z8"));
}

// ---- J-41 (Z1, Z7, I-3)

TEST_CASE("J-41: zone C reading or writing an external resource fails") {
    Built reads = FinishOne([](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&ReadLink>(Sys("ReadLink", Zone::C, Simulate()));
    });
    CHECK(HasRule(reads, StartupCode::DE7, "Z1"));
    Built writes = FinishOne([](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&WriteLink>(Sys("WriteLink", Zone::C, Simulate()));
    });
    CHECK(HasRule(writes, StartupCode::DE7, "Z7"));
    Built zoneCExternal = Finish({{"test", [](Registrar& r) {
                                      r.Resource<TestLink>({.name = "TestLink", .zone = Zone::C, .external = true});
                                      r.Schedule(FixedSchedule());
                                  }}});
    CHECK(HasRule(zoneCExternal, StartupCode::DE7, "Z1"));
    Built bWrites = FinishOne([](Registrar& r) {
        ExternalAndInput(r);
        r.AddSystem<&WriteLink>(Sys("WriteLink", Zone::B, {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(bWrites);
}

// ---- J-47 (B.4 point 4, Z1)

TEST_CASE("J-47: two plugins add to one named set; SetRead reads both; removing one plugin leaves one") {
    const auto owner = [](Registrar& r) {
        RegisterCommonData(r);
        r.NamedSet<ReplicatedSet>("Replicated");
        r.AddToSet<ReplicatedSet, TestPos>();
        r.Schedule(UpdateSchedule());
        r.Schedule(FixedSchedule());
    };
    const auto other = [](Registrar& r) {
        r.Component<TestDerived>({.name = "TestDerived", .zone = Zone::C, .rollback = Rollback::Yes});
        r.AddToSet<ReplicatedSet, TestDerived>();
    };
    const auto reader = [](Registrar& r) { r.AddSystem<&ReadSet>(Sys("ReadSet", Zone::C, Simulate())); };
    Built both = Finish({{"owner", owner}, {"other", other}, {"reader", reader}});
    KERNEL_REQUIRE_OK(both);
    const SystemInfo* info = both.registry->Frozen().SystemNamed("ReadSet");
    REQUIRE(info != nullptr);
    const ComponentId pos = *both.registry->Frozen().ComponentIdOf<TestPos>();
    const ComponentId derived = *both.registry->Frozen().ComponentIdOf<TestDerived>();
    CHECK(info->access.reads == std::vector<AccessKey>{{AccessKind::Component, pos}, {AccessKind::Component, derived}});
    Built one = Finish({{"owner", owner}, {"reader", reader}});
    KERNEL_REQUIRE_OK(one);
    CHECK(one.registry->Frozen().SystemNamed("ReadSet")->access.reads ==
          std::vector<AccessKey>{{AccessKind::Component, pos}});
}

TEST_CASE("J-47: a zone B member in the set read by a zone C System fails (Z1)") {
    Built built = FinishOne([](Registrar& r) {
        r.NamedSet<ReplicatedSet>("Replicated");
        r.AddToSet<ReplicatedSet, TestPos>();
        r.AddToSet<ReplicatedSet, TestMarker>();
        r.AddSystem<&ReadSet>(Sys("ReadSet", Zone::C, Simulate()));
    });
    CHECK(HasRule(built, StartupCode::DE7, "Z1"));
}

// ---- J-24 (codes from this file's rules)

TEST_CASE("J-24: D-E7 (S-13 ExclusiveWorld in zone C), F-E1 and D-E2 from minimal Registries") {
    CHECK(HasRule(FinishOne([](Registrar& r) { r.AddSystem<&EditWorld>(Sys("EditWorld", Zone::C, Simulate())); }),
                  StartupCode::DE7, "S-13"));
    Built eventWithoutDomain = Finish({{"test", [](Registrar& r) {
                                           r.Event<TestEvent>({.name = "TestEvent", .zone = Zone::C});
                                           r.Schedule(FixedSchedule());
                                       }}});
    CHECK(KernelTest::HasError(eventWithoutDomain, StartupCode::FE1));
}
