// Serial Executor guards of the world-exec batch: spec J-6 (Running half),
// J-26, J-29, J-30, J-32b, J-35, J-45, J-50, J-52, J-53, and the three run-time
// halves handed over by registry-schedule (declaration section 3 item 2):
// J-11 full against incremental rebuild (W-11), J-12 Fetch has no order
// dependence (spec B.4 point 7), J-44 the World summary (D-9).
// target/kernel DI-KS 2, 6, 9, 12 (B-12 half), 14; spec E.2, E.5, X-1 to X-5,
// F.2 steps 0 and 2, W-6b, LIM-6.

#include "WorldTestSupport.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) out += (out.empty() ? "" : " ") + part;
    return out;
}

constexpr bool kNdebug =
#if defined(NDEBUG)
    true;
#else
    false;
#endif

// ---- J-6 (W-6, W-6b)

// The Host's code reached while a schedule runs, with no System of this World
// current on the thread (as on a thread a System started itself: spec R-03).
SystemResult ReadWithoutCurrentCall(Query<Read<Tag>>) {
    Detail::SystemContext* saved = Detail::CurrentThreadCall();
    Detail::SetCurrentThreadCall(nullptr);
    struct Restore {
        Detail::SystemContext* call;
        ~Restore() { Detail::SetCurrentThreadCall(call); }
    } restore{saved};
    static_cast<void>(KeptWorld()->Get<Tag>(1));
    return Ok();
}

SystemResult Idle(Query<Read<Tag>>) { return Ok(); }

} // namespace

TEST_CASE("J-6: a World access while Running with no System current fails a GYO_ASSERT; Host writes to zone C state after the first Run too") {
    {
        // Verification off: the always-on GYO_ASSERT alone must stop it.
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ReadWithoutCurrentCall>(B("ReadWithoutCurrentCall", "A"));
        }, false);
        rig->world().Insert(1, Tag{1});
        KeptWorld() = &rig->world();
        GYO_CHECK_ASSERTS(rig->Run());
        KeptWorld() = nullptr;
    }
    {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&Idle>(B("Idle", "A"));
        });
        World& world = rig->world();
        world.Insert(1, Pos{1});  // before the first Run: the initial content
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        GYO_CHECK_ASSERTS(world.Insert(2, Pos{2}));
        GYO_CHECK_ASSERTS(world.GetMut<Pos>(1));
        GYO_CHECK_ASSERTS(world.SetResource(CRes{1}));
        // Boundary (role Input) and zone B data: the Host may write them in Ready.
        world.Insert(1, Boundary{1});
        world.Insert(1, Tag{1});
        world.SetResource(Out{3});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
    }
}

// ---- J-26 (X-1)

namespace {

SystemResult LogFirst(Query<Read<Tag>>) {
    Log().push_back("First");
    return Ok();
}
SystemResult LogSecond(Query<Read<Tag>>) {
    Log().push_back("Second");
    return Ok();
}
SystemResult LogSkipped(Query<Read<Tag>>) {
    Log().push_back("Skipped");
    return Ok();
}
SystemResult LogThird(Query<Read<Tag>>) {
    Log().push_back("Third");
    return Ok();
}
bool Never() { return false; }

} // namespace

TEST_CASE("J-26: the Serial call sequence is the Plan's System steps without those whose condition is false") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&LogThird>(B("Third", "C"));
        r.AddSystem<&LogSecond>(B("Second", "B"));
        SystemOptions skipped = B("Skipped", "B", {}, {"Second"});
        skipped.mounts[0].conditions.push_back(Condition<&Never>("Never"));
        r.AddSystem<&LogSkipped>(std::move(skipped));
        r.AddSystem<&LogFirst>(B("First", "A"));
    });
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    std::vector<std::string> expected;
    for (const std::string& name : SystemSteps(*plan)) {
        if (name != "Skipped") expected.push_back(name);
    }
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(expected == std::vector<std::string>{"First", "Second", "Third"});
    CHECK(Log() == expected);
}

// ---- J-29 (X-4)

namespace {

SystemResult ToggleFlag(ResMut<Flag> flag, CurrentTick tick) {
    flag->on = tick.Tick() == 1 ? 1 : 0;
    return Ok();
}
bool FlagOn(Res<Flag> flag) { return flag->on != 0; }
SystemResult Guarded(Query<Read<Tag>>, CurrentTick tick) {
    Log().push_back("guarded@" + std::to_string(tick.Tick()));
    return Ok();
}
bool CountedCondition(Res<Out>) {
    Log().push_back("condition");
    return true;
}
SystemResult MemberOne(Query<Read<Tag>>) { return Ok(); }
SystemResult MemberTwo(Query<Read<Tag>>) { return Ok(); }

} // namespace

TEST_CASE("J-29: a run condition sees the step before it, is evaluated every run, and once per set member") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&ToggleFlag>(B("ToggleFlag", "A"));
        SystemOptions guarded = B("Guarded", "B");
        guarded.mounts[0].conditions.push_back(Condition<&FlagOn>("FlagOn"));
        r.AddSystem<&Guarded>(std::move(guarded));
        r.Set({.name = "Counted", .schedule = "Tick"}, {Condition<&CountedCondition>("CountedCondition")});
        SystemOptions one = B("MemberOne", "C");
        one.mounts[0].sets = {"Counted"};
        SystemOptions two = B("MemberTwo", "C");
        two.mounts[0].sets = {"Counted"};
        r.AddSystem<&MemberOne>(std::move(one));
        r.AddSystem<&MemberTwo>(std::move(two));
    });
    rig->world().SetResource(Flag{0});
    rig->world().SetResource(Out{0});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Join(Log()) == "guarded@1 condition condition condition condition");
}

// ---- J-30 (X-5, W-11)

namespace {
SystemResult SeeAdded(Query<Read<Tag>, Added<Tag>> added) {
    Log().push_back("added:" + std::to_string(added.Count()));
    return Ok();
}
} // namespace

TEST_CASE("J-30: Added sees new cells only; tickStamps keep a tick's first stamp") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&SeeAdded>(B("SeeAdded", "A"));
    });
    rig->world().Insert(1, Tag{1});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    rig->world().GetMut<Tag>(1)->t = 2;  // changed, not added
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Join(Log()) == "added:1 added:0");
    CHECK(rig->world().TickStamp(Domain::Fixed, 1) == std::optional<ChangeStamp>(1));
    CHECK(rig->world().TickStamp(Domain::Fixed, 2) == std::optional<ChangeStamp>(5));
}

namespace {

bool NeverRuns(Res<Flag>) { return false; }
SystemResult SkippedWriter(Query<Write<Tag>>) { return Ok(); }
SystemResult WriteTagOne(Query<Write<Tag>> tags) {
    if (Mut<Tag> tag = tags.GetMut<Tag>(1)) tag->t += 1;
    return Ok();
}
SystemResult ReadTagTwo(Query<Read<Tag>> tags) {
    static_cast<void>(tags.Get<Tag>(2));
    return Ok();
}

std::vector<ChangeStamp> StampsOfTwoRuns() {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        SystemOptions skipped = B("SkippedWriter", "A");
        skipped.mounts[0].conditions.push_back(Condition<&NeverRuns>("NeverRuns"));
        r.AddSystem<&SkippedWriter>(std::move(skipped));
        r.AddSystem<&WriteTagOne>(B("WriteTagOne", "B"));
        r.AddSystem<&ReadTagTwo>(B("ReadTagTwo", "B", {}, {"WriteTagOne"}));
    });
    World& world = rig->world();
    world.SetResource(Flag{0});
    world.Insert(1, Tag{0});
    world.Insert(2, Tag{0});
    std::vector<ChangeStamp> stamps{world.Stamp()};
    for (int run = 0; run < 2; ++run) {
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        stamps.push_back(world.Stamp());
        stamps.push_back(world.Stamps<Tag>(1)->second);
        stamps.push_back(world.Stamps<Tag>(2)->second);
    }
    return stamps;
}

} // namespace

TEST_CASE("J-30: a skipped System takes its stamp; the same history gives the same stamps; writes mark, reads do not") {
    // Plan: SkippedWriter, sync A, WriteTagOne, ReadTagTwo, sync B, sync C = 6 steps a Run.
    const std::vector<ChangeStamp> first = StampsOfTwoRuns();
    // A Host write in Ready carries the stamp the next step takes (1 here).
    CHECK(first == std::vector<ChangeStamp>{0, 6, 3, 1, 12, 9, 1});
    CHECK(StampsOfTwoRuns() == first);
}

// ---- J-32b (F-10, Z5)

namespace {

SystemResult UntypedIntoZoneC(Commands commands) {
    if (auto r = commands.Insert(1, Pos{1}); !r) return Engine::Base::Err(std::move(r).error().Wrap("UntypedIntoZoneC"));
    return Ok();
}
SystemResult UntypedIntoExclusive(Commands commands) {
    if (auto r = commands.Remove<Mode>(1); !r) return Engine::Base::Err(std::move(r).error().Wrap("UntypedIntoExclusive"));
    return Ok();
}
SystemResult UntypedIntoShared(Commands commands) {
    if (auto r = commands.Insert(1, Tag{5}); !r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult UntypedIntoZoneCShared(Commands commands) {
    if (auto r = commands.Insert(1, CShared{1}); !r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
// Drops the kernel Err and returns another one: both are reported.
SystemResult DroppedBesideOwn(Commands commands) {
    static_cast<void>(commands.Insert(1, Pos{1}));
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "own"));
}
// Drops the Err it got: the Executor reports it all the same.
SystemResult UntypedDropped(Commands commands) {
    static_cast<void>(commands.Insert(1, Pos{1}));
    return Ok();
}
SystemResult ZoneBDespawnsZoneC(Despawn despawn) {
    if (auto r = despawn.Entity(1); !r) return Engine::Base::Err(std::move(r).error().Wrap("ZoneBDespawnsZoneC"));
    return Ok();
}

void CheckErrOnce(Rig& rig, KernelErrorCode code, const char* rule) {
    const auto result = rig.Run();
    REQUIRE_FALSE_MESSAGE(static_cast<bool>(result), "expected Err " << std::string(ToString(code)));
    CHECK_MESSAGE(result.error().Has(code), result.error().Text());
    bool ruled = false;
    for (const KernelError* leaf : result.error().Leaves()) ruled = ruled || leaf->rule == rule;
    CHECK(ruled);
    CHECK(result.error().world == "test-world");
    CHECK(rig.world().State() == WorldState::Interrupted);
    CHECK(rig.recorded.size() == 1);
}

} // namespace

TEST_CASE("J-32b: untyped Commands on zone C or Exclusive data and a zone B despawn of zone C data are Err in every build") {
    MESSAGE("NDEBUG (release-like target): " << std::string(kNdebug ? "defined" : "not defined"));
    SUBCASE("untyped insert into a zone C column") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&UntypedIntoZoneC>(B("UntypedIntoZoneC", "A"));
        }, false);
        rig->world().Insert(1, Tag{0});
        CheckErrOnce(*rig, KernelErrorCode::UntypedWrite, "F-10");
        CHECK(rig->world().Get<Pos>(1) == nullptr);
    }
    SUBCASE("untyped remove from an Exclusive column") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&UntypedIntoExclusive>(B("UntypedIntoExclusive", "A"));
        }, false);
        rig->world().Insert(1, Mode{0});
        CheckErrOnce(*rig, KernelErrorCode::UntypedWrite, "F-10");
        CHECK(rig->world().Get<Mode>(1) != nullptr);
    }
    SUBCASE("a dropped Err is still reported") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&UntypedDropped>(B("UntypedDropped", "A"));
        }, false);
        CheckErrOnce(*rig, KernelErrorCode::UntypedWrite, "F-10");
    }
    SUBCASE("untyped insert into a zone C, Shared column") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&UntypedIntoZoneCShared>(B("UntypedIntoZoneCShared", "A"));
        }, false);
        rig->world().Insert(1, Tag{0});
        CheckErrOnce(*rig, KernelErrorCode::UntypedWrite, "F-10");
    }
    SUBCASE("a dropped Err beside the System's own Err") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&DroppedBesideOwn>(B("DroppedBesideOwn", "A"));
        }, false);
        const auto result = rig->Run();
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Has(KernelErrorCode::UntypedWrite));
        CHECK(result.error().Has(KernelErrorCode::SystemFailed));
    }
    SUBCASE("a zone B despawn of an entity with a zone C column") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ZoneBDespawnsZoneC>(B("ZoneBDespawnsZoneC", "A"));
        }, false);
        rig->world().Insert(1, Pos{0});
        const auto result = rig->Run();
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Has(KernelErrorCode::ZoneCDespawn));
        // Caught when recorded (returned to the System, which wrapped it), not only by the sync point's recheck.
        CHECK_MESSAGE(result.error().Text().find("System 'ZoneBDespawnsZoneC'") != std::string::npos, result.error().Text());
        CHECK(result.error().Text().find("sync point at step") == std::string::npos);
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK(rig->recorded.size() == 1);
        CHECK(rig->world().Alive(1));
    }
    SUBCASE("zone B, Shared: allowed") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&UntypedIntoShared>(B("UntypedIntoShared", "A"));
        }, false);
        rig->world().Insert(1, Mode{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        REQUIRE(rig->world().Get<Tag>(1) != nullptr);
        CHECK(rig->world().Get<Tag>(1)->t == 5);
        CHECK(rig->recorded.empty());
    }
}

// ---- J-35 (E.5, X-9)

namespace {

SystemResult SpawnThenThrow(SpawnWithId<Bundle<Tag>> spawn, DiagnosticsWriter<Note> notes) {
    spawn.New(5, Tag{5});
    notes.Write(Note{1});
    throw std::runtime_error("System threw");
}
SystemResult SpawnThenErr(SpawnWithId<Bundle<Tag>> spawn, DiagnosticsWriter<Note> notes) {
    spawn.New(5, Tag{5});
    notes.Write(Note{2});
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "the System's own Err"));
}

} // namespace

namespace {
SystemResult SpawnTagOne(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(1, Tag{1});
    return Ok();
}
SystemResult HookReturnsErr(HookCtx) {
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "the hook's own Err"));
}
SystemResult HookWritesThroughReadPointer(HookCtx, Query<Read<Pos>> pos) {
    if (const Pos* p = pos.Get<Pos>(2)) const_cast<Pos*>(p)->x = 64;
    return Ok();
}
SystemResult HookDespawnsZoneC(HookCtx ctx, Despawn despawn) {
    return despawn.Entity(ctx.Entity() + 1);
}
} // namespace

TEST_CASE("J-35: a hook returns Err: the World is interrupted and the leaf keeps its code and rule") {
    SUBCASE("a hook returns its own Err") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnTagOne>(B("SpawnTagOne", "A"));
            r.Hook<Tag, &HookReturnsErr>(HookTrigger::OnAdd, {.name = "HookReturnsErr", .zone = Zone::B});
        }, false);
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::SystemFailed));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("hook 'HookReturnsErr'") != std::string::npos);
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK(rig->recorded.size() == 1);
    }
    SUBCASE("a hook writes through a read pointer: caught by the comparison around the hook") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnTagOne>(B("SpawnTagOne", "A"));
            r.Hook<Tag, &HookWritesThroughReadPointer>(HookTrigger::OnAdd, {.name = "HookWritesThroughReadPointer", .zone = Zone::B});
        }, true);
        rig->world().Insert(2, Pos{2});
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::BoundaryChange));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("hook 'HookWritesThroughReadPointer'") != std::string::npos);
    }
    SUBCASE("a zone B hook despawns zone C data: Z5 with its rule") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnTagOne>(B("SpawnTagOne", "A"));
            r.Hook<Tag, &HookDespawnsZoneC>(HookTrigger::OnAdd, {.name = "HookDespawnsZoneC", .zone = Zone::B});
        }, false);
        rig->world().Insert(2, Pos{2});
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::ZoneCDespawn));
        REQUIRE_FALSE(static_cast<bool>(result));
        bool z5 = false;
        for (const KernelError* leaf : result.error().Leaves()) z5 = z5 || leaf->rule == "Z5";
        CHECK(z5);
    }
}

TEST_CASE("J-35: a throw, a System Err or a kernel-level Err interrupt the World: records dropped, diagnostics kept, no further Run") {
    SUBCASE("a System throws") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnThenThrow>(B("SpawnThenThrow", "A"));
        }, false);
        CHECK_THROWS_AS(static_cast<void>(rig->Run()), std::runtime_error);
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK_FALSE(rig->world().Alive(5));
        REQUIRE(rig->world().Diagnostics<Note>().size() == 1);
        CHECK(rig->world().Diagnostics<Note>()[0].code == 1);
        GYO_CHECK_ASSERTS(rig->Run());
    }
    SUBCASE("a System returns Err") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnThenErr>(B("SpawnThenErr", "A"));
        }, false);
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::SystemFailed));
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK(rig->recorded.size() == 1);
        CHECK_FALSE(rig->world().Alive(5));
        REQUIRE(rig->world().Diagnostics<Note>().size() == 1);
        CHECK(rig->world().Diagnostics<Note>()[0].code == 2);
        GYO_CHECK_ASSERTS(rig->Run());
    }
    SUBCASE("the comparison before the first step finds a Host write through a kept pointer") {
        // The first stage is empty: the first step is a sync point, whose snapshot must not hide the write.
        auto build = [](Registrar& r) {
            Data(r);
            r.AddSystem<&Idle>(B("Idle", "B"));
        };
        auto rig = MakeRig(build, true);
        rig->world().Insert(1, Tag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        const Tag* kept = rig->world().Get<Tag>(1);
        const_cast<Tag*>(kept)->t = 42;  // not through the World API
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::PreCallChange));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("Ready window") != std::string::npos);
        CHECK(rig->world().State() == WorldState::Interrupted);
        GYO_CHECK_ASSERTS(rig->Run());
        GYO_CHECK_ASSERTS(rig->world().Insert(2, Tag{2}));  // an Interrupted World takes no Host write
        // Restore arrives with rollback-extract; rebuilding the World (a new App) is the other way back.
        App rebuilt(*rig->built.registry, "rebuilt");
        rebuilt.SetErrorHandler([](const KernelError&) {});
        KERNEL_REQUIRE_RUN_OK(rebuilt.Run("Tick"));
    }
}

// ---- J-45 (S-13)

namespace {

SystemResult ExclusiveWritesZoneC(ExclusiveWorld world) {
    auto r = world.GetMut<Pos>(1);
    if (!r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult ExclusiveWritesExclusive(ExclusiveWorld world) {
    auto r = world.GetMut<Mode>(1);
    if (!r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult ExclusiveSpawns(ExclusiveWorld world) {
    auto r = world.Spawn();
    if (!r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult ExclusiveWritesZoneCShared(ExclusiveWorld world) {
    auto r = world.GetMut<CShared>(1);
    if (!r) return Engine::Base::Err(std::move(r).error());
    return Ok();
}
SystemResult ExclusiveAllowed(ExclusiveWorld world) {
    const Pos* pos = world.Get<Pos>(1);
    auto tag = world.GetMut<Tag>(1);
    if (!tag) return Engine::Base::Err(std::move(tag).error());
    if (pos != nullptr && *tag) tag->Get().t = pos->x + 1;
    return Ok();
}

std::unique_ptr<Rig> ExclusiveRig(void (*add)(Registrar&), bool verification) {
    auto rig = MakeRig([add](Registrar& r) {
        Data(r);
        add(r);
    }, verification);
    rig->world().Insert(1, Pos{7});
    rig->world().Insert(1, Mode{1});
    rig->world().Insert(1, Tag{0});
    return rig;
}

} // namespace

TEST_CASE("J-45: ExclusiveWorld writes only zone B Shared data; anything else, and a spawn, is Err in every build") {
    MESSAGE("NDEBUG (release-like target): " << std::string(kNdebug ? "defined" : "not defined"));
    SUBCASE("zone C holding it: Finish fails") {
        const Built built = Finish({{"test", [](Registrar& r) {
                                         Data(r);
                                         r.AddSystem<&ExclusiveAllowed>(C("ExclusiveAllowed", "A"));
                                     }}});
        CHECK_FALSE(static_cast<bool>(built.result));
    }
    SUBCASE("zone C write") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveWritesZoneC>(B("X", "A")); }, false);
        CheckErrOnce(*rig, KernelErrorCode::ExclusiveWorldWrite, "S-13");
    }
    SUBCASE("Exclusive write") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveWritesExclusive>(B("X", "A")); }, false);
        CheckErrOnce(*rig, KernelErrorCode::ExclusiveWorldWrite, "S-13");
    }
    SUBCASE("zone C, Shared write") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveWritesZoneCShared>(B("X", "A")); }, false);
        rig->world().Insert(1, CShared{0});
        CheckErrOnce(*rig, KernelErrorCode::ExclusiveWorldWrite, "S-13");
    }
    SUBCASE("spawn") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveSpawns>(B("X", "A")); }, false);
        CheckErrOnce(*rig, KernelErrorCode::ExclusiveWorldWrite, "S-13");
    }
    SUBCASE("zone B Shared write and any read pass, also with verification on") {
        auto rig = ExclusiveRig([](Registrar& r) { r.AddSystem<&ExclusiveAllowed>(B("X", "A")); }, true);
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(rig->world().Get<Tag>(1)->t == 8);
    }
}

// ---- J-50 (LIM-6, W-6b, D-E18)

namespace {

SystemResult ReadsSettings(Res<KernelSettings>) { return Ok(); }
SystemResult ReadUndeclared(Query<Read<Tag>>) {
    static_cast<void>(KeptWorld()->Get<Pos>(1));
    return Ok();
}

} // namespace

TEST_CASE("J-50: KernelSettings is read when the first Run starts and cannot change after it; no System declares it") {
    CHECK(HasError(Finish({{"test", [](Registrar& r) {
                               Data(r);
                               r.AddSystem<&ReadsSettings>(B("ReadsSettings", "A"));
                           }}}),
                   StartupCode::DE18));

    {
        // After a clean first Run the World is Ready: only the KernelSettings rule stops the write.
        auto clean = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&Idle>(B("Idle", "A"));
        }, false);
        KERNEL_REQUIRE_RUN_OK(clean->Run());
        CHECK(clean->world().State() == WorldState::Ready);
        GYO_CHECK_ASSERTS(clean->world().SetResource(KernelSettings{true, false}));
    }

    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&ReadUndeclared>(B("ReadUndeclared", "A"));
    }, false);
    // The Ready window: the last value before the first Run counts.
    rig->world().SetResource(KernelSettings{false, false});
    rig->world().SetResource(KernelSettings{true, false});
    rig->world().Insert(1, Pos{1});
    KeptWorld() = &rig->world();
    const auto result = rig->Run();
    KeptWorld() = nullptr;
    CHECK(HasCode(result, KernelErrorCode::AccessViolation));
    CHECK(rig->app->Verification().accessVerification);

    // breakOnKernelError: the returning break hook is called where the error is made, then the Err goes on.
    for (const bool breakOn : {true, false}) {
        auto breaking = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ReadUndeclared>(B("ReadUndeclared", "A"));
        });
        breaking->world().SetResource(KernelSettings{true, breakOn});
        int breaks = 0;
        breaking->world().SetBreakHook([&breaks](const KernelError&) { ++breaks; });
        breaking->world().Insert(1, Pos{1});
        KeptWorld() = &breaking->world();
        const auto broken = breaking->Run();
        KeptWorld() = nullptr;
        CHECK(HasCode(broken, KernelErrorCode::AccessViolation));
        CHECK_MESSAGE((breaks > 0) == breakOn, "breakOnKernelError " << breakOn << ", break hook calls " << breaks);
        CHECK(breaking->app->Verification().breakOnKernelError == breakOn);
    }
}

// ---- J-52 (F.2 steps 0 and 2, B-12)

namespace {

// Entity 1 has only zone B data when the despawn is recorded; a zone C System
// inserts a zone C column into it in the same window.
SystemResult ZoneBDespawns(Despawn despawn) {
    if (auto r = despawn.Entity(1); !r) return r;
    if (auto r = despawn.Entity(0); !r) return r;  // a smaller, legal despawn: not applied either
    return Ok();
}
SystemResult ZoneCInserts(CommandsFor<Inserts<Pos>, Removes<>> commands) {
    commands.Insert(1, Pos{1});
    return Ok();
}
// A zone B hook that despawns the entity whose Tag was added (when only zone B data is on it).
SystemResult HookDespawns(HookCtx ctx, Despawn despawn) {
    return despawn.Entity(ctx.Entity());
}
SystemResult InsertTagOnce(CommandsFor<Inserts<Tag>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() == 1) commands.Insert(1, Tag{1});
    return Ok();
}
SystemResult ZoneCInsertsAtTwo(CommandsFor<Inserts<Pos>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() == 2) commands.Insert(1, Pos{2});
    return Ok();
}

// Entity 1 has only zone B data when the zone B despawn is recorded; a zone C
// hook recorded a zone C insert into it at the sync point before, applied at
// step 0 of the next: at step 2 the entity has a zone C column.
SystemResult SpawnVelOnce(SpawnWithId<Bundle<Vel>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(2, Vel{2});
    return Ok();
}
SystemResult ZoneCHookInsertsPos(HookCtx, CommandsFor<Inserts<Pos>, Removes<>> commands) {
    commands.Insert(1, Pos{1});
    return Ok();
}
SystemResult ZoneBDespawnsAtTwo(Despawn despawn, CurrentTick tick) {
    if (tick.Tick() == 2) return despawn.Entity(1);
    return Ok();
}

SystemResult ZoneCRespawns(SpawnWithId<Bundle<Pos>> spawn) {
    spawn.New(1, Pos{1});
    return Ok();
}
bool FlagIsOn(Res<Flag> flag) { return flag->on != 0; }
SystemResult SeeChangesWhenOn(Query<Read<Tag>, Changed<Tag>> changed) {
    Log().push_back("changed:" + std::to_string(changed.Count()));
    return Ok();
}

// Two mounts: Tick (A) and Other (B). Changed and Local are per mount.
SystemResult SeeChanges(Query<Read<Tag>, Changed<Tag>> changed, Local<std::int32_t> runs, CurrentTick tick) {
    *runs += 1;
    Log().push_back(std::string(tick.Clock() == Domain::Fixed ? "A" : "B") + " run" + std::to_string(*runs) + " changed:" +
                    std::to_string(changed.Count()));
    return Ok();
}

} // namespace

TEST_CASE("J-52: the Z5 recheck at the sync point; Changed and Local are per mount") {
    SUBCASE("a zone C insert in the same window: kernel-level Err, no despawn applied") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ZoneBDespawns>(B("ZoneBDespawns", "A"));
            r.AddSystem<&ZoneCInserts>(C("ZoneCInserts", "A"));
        }, false);
        rig->world().Insert(0, Tag{0});
        rig->world().Insert(1, Tag{1});
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::ZoneCDespawn));
        CHECK(rig->world().State() == WorldState::Interrupted);
        CHECK(rig->recorded.size() == 1);
        CHECK(rig->world().Alive(0));
        CHECK(rig->world().Alive(1));
    }
    SUBCASE("a zone B hook's despawn and a zone C insert in the hook's next window") {
        ScheduleOptions one;
        one.name = "Tick";
        one.domain = Domain::Fixed;
        one.clock = true;
        one.stages = {"A"};
        auto rig = MakeRig([one](Registrar& r) {
            r.Component<Pos>({.name = "Pos", .zone = Zone::C, .rollback = Rollback::Yes});
            r.Component<Tag>({.name = "Tag", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
            r.Component<Mode>({.name = "Mode", .zone = Zone::B});
            r.Schedule(one);
            r.AddSystem<&InsertTagOnce>(B("InsertTagOnce", "A"));
            r.AddSystem<&ZoneCInsertsAtTwo>(C("ZoneCInsertsAtTwo", "A"));
            r.Hook<Tag, &HookDespawns>(HookTrigger::OnAdd, {.name = "HookDespawns", .zone = Zone::B});
        }, false);
        rig->world().Insert(1, Mode{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // tick 1: Tag added at the only sync point; the hook records despawn(1)
        CHECK(rig->world().Alive(1));
        const auto result = rig->Run();  // tick 2: a zone C insert into 1 in the window; step 0 rechecks
        CHECK(HasCode(result, KernelErrorCode::ZoneCDespawn));
        CHECK(rig->world().Alive(1));
        CHECK(rig->world().Get<Pos>(1) == nullptr);
    }
    SUBCASE("a zone C SpawnWithId in the same window") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&ZoneBDespawns>(B("ZoneBDespawns", "A"));
            r.AddSystem<&ZoneCRespawns>(C("ZoneCRespawns", "A"));
        }, false);
        rig->world().Insert(0, Tag{0});
        rig->world().Insert(1, Tag{1});
        const auto result = rig->Run();
        CHECK(HasCode(result, KernelErrorCode::ZoneCDespawn));
        CHECK(rig->world().Alive(0));
    }
    SUBCASE("a skipped mount keeps its last-run stamp") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            SystemOptions options = B("SeeChangesWhenOn", "A");
            options.mounts[0].conditions.push_back(Condition<&FlagIsOn>("FlagIsOn"));
            r.AddSystem<&SeeChangesWhenOn>(std::move(options));
        });
        rig->world().SetResource(Flag{0});
        rig->world().Insert(1, Tag{1});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // skipped
        rig->world().SetResource(Flag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // runs: the insert made before the skipped run is still a change
        CHECK(Join(Log()) == "changed:1");
    }
    SUBCASE("a zone C column applied at step 0 is seen by step 2's recheck") {
        ScheduleOptions one;
        one.name = "Tick";
        one.domain = Domain::Fixed;
        one.clock = true;
        one.stages = {"A"};
        auto rig = MakeRig([one](Registrar& r) {
            r.Component<Pos>({.name = "Pos", .zone = Zone::C, .rollback = Rollback::Yes});
            r.Component<Vel>({.name = "Vel", .zone = Zone::C, .rollback = Rollback::Yes});
            r.Component<Tag>({.name = "Tag", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
            r.Schedule(one);
            r.AddSystem<&SpawnVelOnce>(C("SpawnVelOnce", "A"));
            r.AddSystem<&ZoneBDespawnsAtTwo>(B("ZoneBDespawnsAtTwo", "A"));
            r.Hook<Vel, &ZoneCHookInsertsPos>(HookTrigger::OnAdd, {.name = "ZoneCHookInsertsPos", .zone = Zone::C});
        }, false);
        rig->world().Insert(1, Tag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // tick 1: Vel on 2; the zone C hook records Pos into 1
        CHECK(rig->world().Get<Pos>(1) == nullptr);
        const auto result = rig->Run();  // tick 2: despawn(1) recorded while 1 has only Tag
        CHECK(HasCode(result, KernelErrorCode::ZoneCDespawn));
        CHECK(rig->world().Alive(1));
    }
    SUBCASE("two mounts keep their own last-run stamp and Local") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            ScheduleOptions other;
            other.name = "Other";
            other.domain = Domain::Frame;
            other.clock = true;
            other.stages = {"B"};
            r.Schedule(other);
            SystemOptions options = Sys("SeeChanges", Zone::B, {At("Tick", "A"), At("Other", "B")});
            r.AddSystem<&SeeChanges>(std::move(options));
        });
        World& world = rig->world();
        world.Insert(1, Tag{0});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));   // A run1: sees the Host's insert
        world.GetMut<Tag>(1)->t = 5;               // after A, before B
        KERNEL_REQUIRE_RUN_OK(rig->Run("Other"));  // B run1: sees the insert and the write
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));   // A run2: still sees the write
        KERNEL_REQUIRE_RUN_OK(rig->Run("Tick"));   // A run3: nothing new
        CHECK(Join(Log()) == "A run1 changed:1 B run1 changed:1 A run2 changed:1 A run3 changed:0");
    }
}

// ---- J-53 (E.2 step 5)

namespace {

const Tag* g_keptTag = nullptr;

SystemResult KeepTag(Query<Read<Tag>> tags) {
    g_keptTag = tags.Get<Tag>(1);
    return Ok();
}
// Returns Ok after writing through a kept pointer it never declared.
SystemResult WriteKeptAndReturnOk(Query<Read<Pos>>) {
    if (g_keptTag != nullptr) const_cast<Tag*>(g_keptTag)->t = 77;
    return Ok();
}
// A false condition that read undeclared data through a kept World.
bool FalseAfterUndeclaredRead(Res<Flag>) {
    static_cast<void>(KeptWorld()->Get<Pos>(1));
    return false;
}
SystemResult NeverCalled(Query<Read<Tag>>) {
    Log().push_back("called");
    return Ok();
}

} // namespace

TEST_CASE("J-53: a System that returns Ok with a boundary change, and a false condition with a violation, are Err") {
    SUBCASE("Ok with a change after the call") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&KeepTag>(B("KeepTag", "A"));
            r.AddSystem<&WriteKeptAndReturnOk>(B("WriteKeptAndReturnOk", "B"));
        });
        rig->world().Insert(1, Tag{1});
        const auto result = rig->Run();
        g_keptTag = nullptr;
        CHECK(HasCode(result, KernelErrorCode::BoundaryChange));
        REQUIRE_FALSE(static_cast<bool>(result));
        CHECK(result.error().Text().find("WriteKeptAndReturnOk") != std::string::npos);
        CHECK(result.error().Text().find("schedule 'Tick'") != std::string::npos);
        CHECK(rig->world().State() == WorldState::Interrupted);
    }
    SUBCASE("a false condition that recorded a violation") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            SystemOptions options = B("NeverCalled", "A");
            options.mounts[0].conditions.push_back(Condition<&FalseAfterUndeclaredRead>("FalseAfterUndeclaredRead"));
            r.AddSystem<&NeverCalled>(std::move(options));
        });
        rig->world().SetResource(Flag{0});
        rig->world().Insert(1, Pos{1});
        KeptWorld() = &rig->world();
        Log().clear();
        const auto result = rig->Run();
        KeptWorld() = nullptr;
        CHECK(Log().empty());
        CHECK(HasCode(result, KernelErrorCode::AccessViolation));
        CHECK(rig->recorded.size() == 1);
    }
}

// ---- J-11 run-time half: full against incremental rebuild (W-11)

namespace {

std::map<EntityId, std::int32_t>& Mirror() {
    static std::map<EntityId, std::int32_t> mirror;
    return mirror;
}

// Writes a few cells each tick through Mut (the only way to take a value for writing).
SystemResult MoveSome(Query<Write<Pos>> positions, CurrentTick tick) {
    for (EntityId id : positions.Entities()) {
        if ((id + tick.Tick()) % 3 == 0) {
            if (Mut<Pos> pos = positions.GetMut<Pos>(id)) pos->x += static_cast<std::int32_t>(tick.Tick());
        }
    }
    return Ok();
}
// Keeps a mirror up to date from the changes only.
SystemResult MirrorChanges(Query<Read<Pos>, Changed<Pos>> changed) {
    changed.ForEach([](EntityId id, const Pos& pos) { Mirror()[id] = pos.x; });
    return Ok();
}

} // namespace

TEST_CASE("J-11: a mirror rebuilt from Changed equals a full copy (no missed change stamp)") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&MoveSome>(C("MoveSome", "A"));
        r.AddSystem<&MirrorChanges>(B("MirrorChanges", "B"));
    });
    for (EntityId id = 1; id <= 12; ++id) rig->world().Insert(id, Pos{static_cast<std::int32_t>(id)});
    Mirror().clear();
    for (int tick = 0; tick < 6; ++tick) {
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        std::map<EntityId, std::int32_t> full;
        for (EntityId id : rig->world().Keys<Pos>()) full[id] = rig->world().Get<Pos>(id)->x;
        CHECK(Mirror() == full);
    }
}

// ---- J-12 run-time half: Fetch has no order dependence (spec B.4 point 7)

namespace {

SystemResult ResThenQuery(ResMut<Out> out, Query<Write<Tag>> tags) {
    for (EntityId id : tags.Entities()) {
        if (Mut<Tag> tag = tags.GetMut<Tag>(id)) tag->t += 1;
    }
    out->value += 1;
    return Ok();
}
SystemResult QueryThenRes(Query<Write<Mode>> modes, ResMut<Flag> flag) {
    for (EntityId id : modes.Entities()) {
        if (Mut<Mode> mode = modes.GetMut<Mode>(id)) mode->m += 1;
    }
    flag->on += 1;
    return Ok();
}

} // namespace

TEST_CASE("J-12: the same parameters in two orders give the same result and the same stamps") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&ResThenQuery>(B("ResThenQuery", "A"));
        r.AddSystem<&QueryThenRes>(B("QueryThenRes", "B"));
    });
    World& world = rig->world();
    world.SetResource(Out{0});
    world.SetResource(Flag{0});
    world.Insert(1, Tag{0});
    world.Insert(1, Mode{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // Plan: ResThenQuery (stamp 1), sync A (2), QueryThenRes (3), sync B (4), sync C (5).
    CHECK(world.Get<Tag>(1)->t == 1);
    CHECK(world.Get<Mode>(1)->m == 1);
    CHECK(world.Resource<Out>()->value == 1);
    CHECK(world.Resource<Flag>()->on == 1);
    CHECK(world.Stamps<Tag>(1)->second == 1);
    CHECK(world.Stamps<Mode>(1)->second == 3);
}

// ---- J-44 run-time half: the World summary under 20 registration orders (D-9)

namespace {

struct Ca { std::int32_t v; };
struct Cb { std::int32_t v; };
struct Cc { std::int32_t v; };

SystemResult P0(Query<Write<Ca>> a) {
    for (EntityId id : a.Entities()) a.GetMut<Ca>(id)->v += 1;
    return Ok();
}
SystemResult P1(Query<Read<Ca>, Write<Cb>> q) {
    q.ForEach([](EntityId, const Ca& a, Mut<Cb> b) { b->v = b.Read().v * 2 + a.v; });
    return Ok();
}
SystemResult P2(Query<Read<Cb>, Write<Cc>> q) {
    q.ForEach([](EntityId, const Cb& b, Mut<Cc> c) { c->v = c.Read().v + b.v; });
    return Ok();
}
SystemResult P3(Query<Read<Cc>> q) {
    static_cast<void>(q.Count());
    return Ok();
}
SystemResult P4(Query<Read<Ca>, Read<Cc>> q) {
    static_cast<void>(q.Count());
    return Ok();
}

using Registration = void (*)(Registrar&);
std::vector<Mount> Sim(std::vector<std::string> before = {}, std::vector<std::string> after = {}) {
    return {At("Tick", "A", std::move(before), std::move(after))};
}
constexpr std::array<Registration, 5> kSystems{{
    [](Registrar& r) { r.AddSystem<&P0>(Sys("P0", Zone::C, Sim({"P1", "P4"}))); },
    [](Registrar& r) { r.AddSystem<&P1>(Sys("P1", Zone::C, Sim({"P2"}))); },
    [](Registrar& r) { r.AddSystem<&P2>(Sys("P2", Zone::C, Sim({"P3", "P4"}))); },
    [](Registrar& r) { r.AddSystem<&P3>(Sys("P3", Zone::C, Sim())); },
    [](Registrar& r) { r.AddSystem<&P4>(Sys("P4", Zone::C, Sim())); },
}};

std::string SummaryOf(const std::array<int, 5>& order) {
    auto rig = MakeRig([&order](Registrar& r) {
        r.Component<Ca>({.name = "Ca", .zone = Zone::C, .rollback = Rollback::Yes});
        r.Component<Cb>({.name = "Cb", .zone = Zone::C, .rollback = Rollback::Yes});
        r.Component<Cc>({.name = "Cc", .zone = Zone::C, .rollback = Rollback::Yes});
        r.Schedule(TickSchedule());
        for (int index : order) kSystems[static_cast<std::size_t>(index)](r);
    });
    for (EntityId id = 1; id <= 4; ++id) {
        rig->world().Insert(id, Ca{static_cast<std::int32_t>(id)});
        rig->world().Insert(id, Cb{1});
        rig->world().Insert(id, Cc{0});
    }
    for (int tick = 0; tick < 3; ++tick) {
        const auto result = rig->Run();
        REQUIRE_MESSAGE(static_cast<bool>(result), Text(result));
    }
    return rig->world().Summary().Text();
}

} // namespace

TEST_CASE("J-44: 20 registration orders of (c) Systems give the same World summary") {
    std::array<int, 5> order{0, 1, 2, 3, 4};
    std::vector<std::array<int, 5>> orders;
    int index = 0;
    do {
        if (index % 6 == 0) orders.push_back(order);
        ++index;
    } while (std::next_permutation(order.begin(), order.end()));
    REQUIRE(orders.size() == 20);
    const std::string first = SummaryOf(orders.front());
    for (const auto& each : orders) CHECK(SummaryOf(each) == first);
}
