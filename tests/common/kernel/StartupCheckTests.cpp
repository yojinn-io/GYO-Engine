// Startup checks kept from ZoneRuleTests.cpp when the zone rules, the writer
// count and the digest set left the kernel (v8-arch-gyo2-authority-rollback
// step 2, declaration 9.2): J-14's hand-written Diagnostics half (spec S-8,
// kept: the Diagnostics effect comes only from DiagnosticsWriter<T>), J-37
// (a message is used in any schedule) and J-47's named set across plugins.

#include "KernelTestSupport.hpp"

namespace {

using namespace GYO::Test::Kernel;

SystemResult ReadPos(Query<Read<TestPos>>) { return Ok(); }
SystemResult TracePos(DiagnosticsWriter<TestRecord>, Query<Read<TestPos>>) { return Ok(); }
SystemResult WriteEvent(MessageWriter<TestEvent>) { return Ok(); }
SystemResult ReadEvent(MessageReader<TestEvent>) { return Ok(); }
SystemResult ReadFrameEvent(MessageReader<TestFrameEvent>) { return Ok(); }
SystemResult ReadSet(SetRead<ReplicatedSet>) { return Ok(); }

std::vector<Mount> Simulate() { return {At("FixedUpdate", "Simulate")}; }

} // namespace

TEST_CASE("J-14: DiagnosticsWriter passes; hand-written Diagnostics fails") {
    Built diagnostics = FinishOne([](Registrar& r) { r.AddSystem<&TracePos>(Sys("TracePos", Simulate())); });
    KERNEL_REQUIRE_OK(diagnostics);
    CHECK(diagnostics.registry->Frozen().SystemNamed("TracePos")->access.diagnostics);
    Built handWritten = FinishOne([](Registrar& r) {
        SystemOptions o = Sys("ReadPos", Simulate());
        o.effects = {"Diagnostics"};
        r.AddSystem<&ReadPos>(o);
    });
    CHECK(HasRule(handWritten, StartupCode::DE7, "S-8"));
}

TEST_CASE("J-37: a message is used in any schedule") {
    Built otherSchedule = FinishOne([](Registrar& r) {
        r.AddSystem<&ReadEvent>(Sys("ReadEvent", {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(otherSchedule);
    Built oneOff = FinishOne([](Registrar& r) {
        ScheduleOptions s;
        s.name = "FixedLate";
        s.stages = {"Late"};
        r.Schedule(s);
        r.AddSystem<&WriteEvent>(Sys("WriteEvent", {At("FixedLate", "Late")}));
    });
    KERNEL_REQUIRE_OK(oneOff);
    Built inClock = FinishOne([](Registrar& r) {
        r.AddSystem<&WriteEvent>(Sys("WriteEvent", Simulate()));
        r.AddSystem<&ReadEvent>(Sys("ReadEvent", {At("FixedUpdate", "Present")}));
        r.AddSystem<&ReadFrameEvent>(Sys("ReadFrameEvent", {At("Update", "First")}));
    });
    KERNEL_REQUIRE_OK(inClock);
}

TEST_CASE("J-47: two plugins add to one named set; SetRead reads both; removing one plugin leaves one") {
    const auto owner = [](Registrar& r) {
        RegisterCommonData(r);
        r.NamedSet<ReplicatedSet>("Replicated");
        r.AddToSet<ReplicatedSet, TestPos>();
        r.Schedule(UpdateSchedule());
        r.Schedule(FixedSchedule());
    };
    const auto other = [](Registrar& r) {
        r.Component<TestDerived>({.name = "TestDerived"});
        r.AddToSet<ReplicatedSet, TestDerived>();
    };
    const auto reader = [](Registrar& r) { r.AddSystem<&ReadSet>(Sys("ReadSet", Simulate())); };
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
