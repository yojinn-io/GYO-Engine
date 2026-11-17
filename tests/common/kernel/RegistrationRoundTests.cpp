// Registration in rounds (declaration v8-arch-gyo2-kernel-subtract-structure
// 3.2 and 3.3, section 5.3 rows S2 and S3): Register / BuildPlans repeated,
// the pending round, the World attaching to a new version, RemoveSystem, R-E7,
// WorldOptions::dynamicInsert, and the read-only, versioned Registry view.
// target/kernel DI-KS 3, 4, 13, 25, 26.

#include "WorldTestSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) out += (out.empty() ? "" : " ") + part;
    return out;
}

// Types only a later round registers.
struct Later { std::int32_t v; };
struct LaterRes { std::int64_t v; };
struct Other { std::int32_t v; };

const RegistrationOwner kBase{"base", 0};
const RegistrationOwner kCommand{"command", 1};

SystemResult Counting(Query<Read<Tag>, Changed<Tag>> changed, Local<std::int32_t> runs) {
    *runs += 1;
    Log().push_back("old run" + std::to_string(*runs) + " changed:" + std::to_string(changed.Count()));
    return Ok();
}
SystemResult NewSystem(Query<Read<Tag>>) {
    Log().push_back("new");
    return Ok();
}
// A Local value that logs when the kernel drops it (a removed System's state).
struct Tracker {
    Tracker() = default;
    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;
    ~Tracker() { Log().push_back("tracker dropped"); }
    std::int32_t runs = 0;
};
SystemResult Tracked(Query<Read<Tag>>, Local<Tracker> tracker) {
    tracker->runs += 1;
    Log().push_back("tracked run" + std::to_string(tracker->runs));
    return Ok();
}
SystemResult UsesLater(Query<Read<Later>> later) {
    Log().push_back("later:" + std::to_string(later.Count()));
    return Ok();
}
SystemResult UsesUnknown(Query<Read<Other>>) { return Ok(); }
SystemResult Fails(Res<Flag>) { return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "planned failure")); }
SystemResult WritesFlag(ResMut<Flag>) { return Ok(); }
SystemResult ReadsFlag(Res<Flag>) { return Ok(); }
SystemResult Plain(Query<Read<Tag>>) { return Ok(); }
SystemResult OnTag(HookCtx) { return Ok(); }
bool FlagIsSet(Res<Flag> flag) { return flag->on != 0; }
Mode ModeFromLater(const Later& later) { return Mode{later.v}; }
SystemResult ExtractPosOf(Extract<Read<TestPos>>) { return Ok(); }
SystemResult ExtractVelOf(Extract<Read<TestVel>>) { return Ok(); }
SystemResult AnnotatesFlag(ExclusiveWorld<Writes<Flag>>) { return Ok(); }

// The Registry a System calls into (test side: a System captures nothing).
Registry*& KeptRegistry() {
    static Registry* registry = nullptr;
    return registry;
}
SystemResult RegistersFromSystem(Res<Flag>) {
    static_cast<void>(KeptRegistry()->Register(kCommand, [](Registrar& r) {
        r.Component<Later>({.name = "Later", .zone = Zone::B});
    }));
    return Ok();
}
SystemResult BuildsFromSystem(Res<Flag>) {
    static_cast<void>(KeptRegistry()->BuildPlans());
    return Ok();
}

void Base(Registrar& r) {
    Data(r);
    r.AddSystem<&Counting>(B("Counting", "A"));
}

bool Done(const Engine::Base::Result<void, KernelError>& result) { return static_cast<bool>(result); }
bool Done(const Engine::Base::Result<void, StartupReport>& result) { return static_cast<bool>(result); }
std::string Describe(const Engine::Base::Result<void, StartupReport>& result) {
    return result ? std::string("ok") : result.error().detail;
}

// A Registry whose first round is Base, built.
void FirstRound(Registry& registry, void (*build)(Registrar&) = &Base) {
    REQUIRE(Done(registry.Register(kBase, build)));
    const auto built = registry.BuildPlans();
    REQUIRE_MESSAGE(Done(built), Describe(built));
}

void Quiet(WorldExecutor& executor, std::vector<KernelError>& recorded) {
    executor.SetErrorHandler([&recorded](const KernelError& error) { recorded.push_back(error); });
}

} // namespace

// ---- S2: Register / BuildPlans in rounds (DI-KS 26, 3, 4, 25)

TEST_CASE("DI-KS 26: a System registered after the first BuildPlans runs from the next Run; old state is kept") {
    Registry registry;
    FirstRound(registry);
    CHECK(registry.Frozen().Version() == 1U);
    WorldExecutor executor(registry, "rounds");
    executor.GetWorld().Insert(1, Tag{1});
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.AddSystem<&NewSystem>(B("NewSystem", "B")); })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().Version() == 2U);
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    // Local<T> and the mount's last-run stamp survive the new version: run3, and no change since run2.
    CHECK_MESSAGE(Join(Log()) == "old run1 changed:1 old run2 changed:0 old run3 changed:0 new",
                  "round state: " << Join(Log()));
}

TEST_CASE("DI-KS 26: after BuildPlans, before the next Run, the World API uses the new types") {
    Registry registry;
    FirstRound(registry);
    WorldExecutor executor(registry, "rounds");
    World& world = executor.GetWorld();
    world.Insert(1, Tag{1});
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
        r.Component<Later>({.name = "Later", .zone = Zone::B});
        r.Resource<LaterRes>({.name = "LaterRes", .zone = Zone::B});
        r.AddSystem<&UsesLater>(B("UsesLater", "A"));
    })));
    REQUIRE(Done(registry.BuildPlans()));
    // The World attaches at its next entry (here the World API), not at the next Run:
    // its summary has the new storage, like a World made at the new version.
    World fresh(registry, "fresh");
    fresh.Insert(1, Tag{1});
    CHECK_MESSAGE(world.Summary() == fresh.Summary(), "attach at the World API entry (summary)");
    world.Insert(1, Later{7});
    world.SetResource(LaterRes{9});
    CHECK_MESSAGE(world.Get<Later>(1) != nullptr, "attach at the World API entry");
    REQUIRE(world.Resource<LaterRes>() != nullptr);
    CHECK(world.Resource<LaterRes>()->v == 9);
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    CHECK(std::find(Log().begin(), Log().end(), "later:1") != Log().end());
}

TEST_CASE("DI-KS 25: what a later round registers is not visible before its BuildPlans") {
    Registry registry;
    FirstRound(registry);
    WorldExecutor executor(registry, "rounds");
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later", .zone = Zone::B}); })));
    CHECK_MESSAGE(!registry.Frozen().ComponentIdOf<Later>().has_value(), "pending round visible in the Registry view");
    CHECK(registry.Frozen().Version() == 1U);
    GYO_CHECK_ASSERTS(executor.GetWorld().Insert(1, Later{1}));  // an unregistered type for the World API
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
}

TEST_CASE("DI-KS 4: a failed BuildPlans drops its round; the World runs the old Plan; ids go on from the old version") {
    Registry registry;
    FirstRound(registry);
    WorldExecutor executor(registry, "rounds");
    const std::size_t components = registry.Frozen().Components().size();
    const std::string plan = registry.Frozen().PlanOf("Tick")->fingerprint.Text();
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
        r.Component<Later>({.name = "Later", .zone = Zone::B});
        r.AddSystem<&UsesUnknown>(B("UsesUnknown", "A"));  // Other is not registered: R-E4
    })));
    const auto failed = registry.BuildPlans();
    REQUIRE_FALSE(Done(failed));
    CHECK(failed.error().Has(StartupCode::RE4));
    CHECK_MESSAGE(registry.Frozen().Version() == 1U, "a failed round raised the version");
    CHECK_MESSAGE(!registry.Frozen().ComponentIdOf<Later>().has_value(), "a failed round was kept");
    CHECK(registry.Frozen().Components().size() == components);
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    CHECK(registry.Frozen().PlanOf("Tick")->fingerprint.Text() == plan);
    CHECK(registry.Frozen().SystemNamed("UsesUnknown") == nullptr);
    // The failed round's id for Later is void: the next round gives the next id from the old version.
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Other>({.name = "Other", .zone = Zone::B}); })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().ComponentIdOf<Other>() == static_cast<ComponentId>(components));
    CHECK_FALSE(registry.Frozen().ComponentIdOf<Later>().has_value());
    CHECK(registry.Frozen().ComponentIdOf<Pos>() == 0U);  // ids of the built version do not change
}

TEST_CASE("DI-KS 4: DiscardRegistrations drops the round the same way") {
    Registry registry;
    FirstRound(registry);
    WorldExecutor executor(registry, "rounds");
    const std::size_t components = registry.Frozen().Components().size();
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later", .zone = Zone::B}); })));
    registry.DiscardRegistrations();
    CHECK(Done(registry.BuildPlans()));  // nothing left to build
    CHECK_MESSAGE(registry.Frozen().Version() == 1U, "a discarded round was built");
    CHECK_FALSE(registry.Frozen().ComponentIdOf<Later>().has_value());
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Other>({.name = "Other", .zone = Zone::B}); })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().ComponentIdOf<Other>() == static_cast<ComponentId>(components));
}

TEST_CASE("DI-KS 26: BuildPlans with nothing registered does nothing") {
    Registry registry;
    CHECK(Done(registry.BuildPlans()));
    CHECK_FALSE(registry.IsFrozen());
    FirstRound(registry);
    const std::string fingerprint = registry.Frozen().RegistryFingerprint().Text();
    CHECK(Done(registry.BuildPlans()));
    CHECK_MESSAGE(registry.Frozen().Version() == 1U, "an empty BuildPlans raised the version");
    CHECK(registry.Frozen().RegistryFingerprint().Text() == fingerprint);
}

TEST_CASE("DI-KS 3: a second round that adds one unrelated System leaves ties, effects, order and other Plans as they were") {
    const auto first = [](Registrar& r) {
        Data(r);
        r.Effect("Io:file");
        ScheduleOptions other;
        other.name = "Other";
        other.domain = Domain::Frame;
        other.clock = true;
        other.stages = {"Only"};
        r.Schedule(other);
        SystemOptions writes = B("WritesFlag", "A");
        writes.effects = {"Io:file"};
        r.AddSystem<&WritesFlag>(writes);
        r.AddSystem<&ReadsFlag>(B("ReadsFlag", "A"));  // conflicts with WritesFlag, no order: a tie
        r.AddSystem<&Plain>(B("Plain", "B", {}, {}));
    };
    Registry registry;
    REQUIRE(Done(registry.Register(kBase, first)));
    REQUIRE(Done(registry.BuildPlans()));
    const FrozenRegistry& view = registry.Frozen();
    const auto ties = [&] {
        std::vector<std::string> out;
        for (const TieBreak& tie : view.TieBreaks()) out.push_back(tie.schedule + ":" + tie.first + ">" + tie.second);
        return out;
    };
    const std::vector<std::string> tiesBefore = ties();
    REQUIRE(tiesBefore == std::vector<std::string>{"Tick:WritesFlag>ReadsFlag"});
    const std::vector<EffectId> effectsBefore = view.SystemNamed("WritesFlag")->effects;
    REQUIRE(effectsBefore.size() == 1);
    const std::vector<OrderEdge> orderBefore(view.OrderGraph("Tick").begin(), view.OrderGraph("Tick").end());
    REQUIRE_FALSE(orderBefore.empty());
    const std::string tickPlan = view.PlanOf("Tick")->fingerprint.Text();
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
        Mount m = At("Other", "Only");
        r.AddSystem<&Plain>(Sys("OtherPlain", Zone::B, {m}));
    })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK_MESSAGE(ties() == tiesBefore, "ties after the second round: " << Join(ties()));
    CHECK_MESSAGE(view.SystemNamed("WritesFlag")->effects == effectsBefore, "effects added to again");
    const std::vector<OrderEdge> orderAfter(view.OrderGraph("Tick").begin(), view.OrderGraph("Tick").end());
    CHECK_MESSAGE(orderAfter == orderBefore, "the order graph of Tick changed: " << orderAfter.size() << " edges");
    CHECK_MESSAGE(view.PlanOf("Tick")->fingerprint.Text() == tickPlan, "the Plan of an untouched schedule changed");
    CHECK_MESSAGE(view.Plans().size() == 2, "plans listed once: " << view.Plans().size());
}

TEST_CASE("DI-KS 26: RemoveSystem") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Base(r);
        r.AddSystem<&NewSystem>(B("NewSystem", "B"));
        r.AddSystem<&Tracked>(B("Tracked", "C"));
    });
    WorldExecutor executor(registry, "rounds");
    SUBCASE("a removed System's state is dropped when the Executor attaches") {
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
        REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { CHECK(r.RemoveSystem("Tracked")); })));
        REQUIRE(Done(registry.BuildPlans()));
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
        CHECK_MESSAGE(Join(Log()) == "old run1 changed:0 new tracked run1 tracker dropped old run2 changed:0 new",
                      "removed state: " << Join(Log()));
    }
    SUBCASE("the System no longer runs and leaves a tombstone; its name is free again") {
        bool removed = false;
        bool unknown = true;
        REQUIRE(Done(registry.Register(kCommand, [&](Registrar& r) {
            removed = r.RemoveSystem("NewSystem");
            unknown = r.RemoveSystem("NoSuchSystem");
        })));
        CHECK(removed);
        CHECK_FALSE(unknown);
        REQUIRE(Done(registry.BuildPlans()));
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
        CHECK_MESSAGE(Join(Log()) == "old run1 changed:0 tracked run1", "a removed System ran: " << Join(Log()));
        const FrozenRegistry& view = registry.Frozen();
        REQUIRE(view.Systems().size() == 3);
        CHECK(view.Systems()[1].name == "NewSystem");
        CHECK_MESSAGE(view.Systems()[1].removed, "no tombstone");
        CHECK_FALSE(view.Systems()[0].removed);
        CHECK(view.SystemNamed("NewSystem") == &view.Systems()[1]);
        const auto graph = view.OrderGraph("Tick");
        CHECK_MESSAGE(std::none_of(graph.begin(), graph.end(),
                                   [](const OrderEdge& e) { return e.first == "NewSystem" || e.second == "NewSystem"; }),
                      "the order graph names a removed System");
        // The same name again: a new registrationSeq, the tombstone's is not reused.
        RegistrationSeq again = 0;
        REQUIRE(Done(registry.Register(kCommand, [&](Registrar& r) { again = r.AddSystem<&NewSystem>(B("NewSystem", "A")); })));
        const auto rebuilt = registry.BuildPlans();
        REQUIRE_MESSAGE(Done(rebuilt), Describe(rebuilt));
        CHECK_MESSAGE(again == 3U, "the tombstone's registrationSeq was given again");
        CHECK_FALSE(registry.Frozen().SystemNamed("NewSystem")->removed);
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
        CHECK(Join(Log()) == "old run2 changed:0 new tracked run2");
    }
    SUBCASE("removing and registering one name in the same round") {
        REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
            CHECK(r.RemoveSystem("NewSystem"));
            r.AddSystem<&NewSystem>(B("NewSystem", "A"));
        })));
        const auto rebuilt = registry.BuildPlans();
        CHECK_MESSAGE(Done(rebuilt), Describe(rebuilt));
    }
}

TEST_CASE("DI-KS 26: removing a System another one orders against is D-E4") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Base(r);
        r.AddSystem<&NewSystem>(B("NewSystem", "B"));
        r.AddSystem<&Plain>(B("AfterNew", "B", {}, {"NewSystem"}));
    });
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { CHECK(r.RemoveSystem("NewSystem")); })));
    const auto result = registry.BuildPlans();
    CHECK_MESSAGE(ErrorCount(result, StartupCode::DE4) == 1, "D-E4 count after the removal: " << Describe(result));
    CHECK(registry.Frozen().Version() == 1U);
}

TEST_CASE("DI-KS 26: a later round may not register what only the first round may (R-E7); the World goes on") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Base(r);
        r.NamedSet<ReplicatedSet>("Replicated");
        ScheduleOptions boot;
        boot.name = "Boot";
        boot.startup = true;
        boot.stages = {"Only"};
        r.Schedule(boot);
    });
    WorldExecutor executor(registry, "rounds");
    std::vector<KernelError> recorded;
    Quiet(executor, recorded);
    const auto refused = [&](auto build) {
        REQUIRE(Done(registry.Register(kBase, build)));
        const auto result = registry.BuildPlans();
        CHECK_MESSAGE(ErrorCount(result, StartupCode::RE7) == 1, "R-E7 count: " << Describe(result));
        CHECK(registry.Frozen().Version() == 1U);
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    };
    SUBCASE("a hook on a component the previous version has") {
        refused([](Registrar& r) { r.Hook<Tag, &OnTag>(HookTrigger::OnAdd, {.name = "OnTag", .zone = Zone::B}); });
    }
    SUBCASE("an initial value of a component the previous version has") {
        refused([](Registrar& r) {
            r.Component<Later>({.name = "Later", .zone = Zone::B});
            r.InitialValue<Mode, &ModeFromLater>();
        });
    }
    SUBCASE("a member of a named set the previous version has") {
        refused([](Registrar& r) { r.AddToSet<ReplicatedSet, Tag>(); });
    }
    SUBCASE("a Startup schedule") {
        refused([](Registrar& r) {
            ScheduleOptions second;
            second.name = "Boot2";
            second.startup = true;
            second.stages = {"Only"};
            r.Schedule(second);
        });
    }
    SUBCASE("a System mounted in the Startup schedule") {
        refused([](Registrar& r) { r.AddSystem<&Plain>(Sys("BootPlain", Zone::B, {At("Boot", "Only")})); });
    }
    SUBCASE("an allotter") {
        refused([](Registrar& r) {
            r.Resource<RangeAllotter>({.name = "Allotter", .zone = Zone::B});
            r.Allotter<RangeAllotter>();
        });
    }
    SUBCASE("the digest set") {
        refused([](Registrar& r) { r.DigestSet<ReplicatedSet>(); });
    }
    SUBCASE("explicit zones") {
        refused([](Registrar& r) { r.RequireExplicitZones(); });
    }
    SUBCASE("what a later round may: a hook and an initial value of new components, a new named set") {
        REQUIRE(Done(registry.Register(kBase, [](Registrar& r) {
            r.Component<Later>({.name = "Later", .zone = Zone::B});
            r.Component<Other>({.name = "Other", .zone = Zone::B});
            r.Hook<Later, &OnTag>(HookTrigger::OnAdd, {.name = "OnLater", .zone = Zone::B});
            r.NamedSet<DigestTag>("Fresh");
            r.AddToSet<DigestTag, Tag>();
        })));
        const auto result = registry.BuildPlans();
        CHECK_MESSAGE(Done(result), Describe(result));
    }
}

TEST_CASE("DI-KS 26: a World that declares no dynamic insert refuses Register for its whole Registry") {
    Registry registry;
    FirstRound(registry);
    WorldExecutor allowing(registry, "allowing");
    CHECK(allowing.GetWorld().Options().dynamicInsert);
    {
        WorldExecutor refusing(registry, "refusing", WorldOptions{false});
        CHECK_FALSE(refusing.GetWorld().Options().dynamicInsert);
        bool called = false;
        const auto result = registry.Register(kCommand, [&](Registrar& r) {
            called = true;
            r.Component<Later>({.name = "Later", .zone = Zone::B});
        });
        REQUIRE_MESSAGE(!Done(result), "a World that declares no dynamic insert did not refuse");
        CHECK(result.error().code == KernelErrorCode::DynamicInsertRefused);
        CHECK_FALSE(called);
        // Nothing changed: no pending round (a World may be made), nothing to build.
        WorldExecutor another(registry, "another");
        CHECK(Done(registry.BuildPlans()));
        CHECK(registry.Frozen().Version() == 1U);
        CHECK_FALSE(registry.Frozen().ComponentIdOf<Later>().has_value());
        KERNEL_REQUIRE_RUN_OK(allowing.Run("Tick"));
    }
    // The refusing World is gone: Register is allowed again.
    REQUIRE_MESSAGE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later", .zone = Zone::B}); })),
                    "Register refused after the refusing World is gone");
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().Version() == 2U);
}

TEST_CASE("DI-KS 26: an Interrupted World does not block Register") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Base(r);
        r.AddSystem<&Fails>(B("Fails", "C"));
    });
    WorldExecutor executor(registry, "interrupted");
    std::vector<KernelError> recorded;
    Quiet(executor, recorded);
    executor.GetWorld().SetResource(Flag{0});
    REQUIRE_FALSE(Done(executor.Run("Tick")));
    REQUIRE(executor.GetWorld().State() == WorldState::Interrupted);
    REQUIRE_MESSAGE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later", .zone = Zone::B}); })),
                    "an Interrupted World blocked Register");
    CHECK(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().Version() == 2U);
}

TEST_CASE("DI-KS 26: a System of a World made from the Registry may not register or build; another Registry's may") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Base(r);
        r.AddSystem<&RegistersFromSystem>(B("RegistersFromSystem", "A"));
    });
    SUBCASE("Register from its own World's System") {
        WorldExecutor executor(registry, "own");
        std::vector<KernelError> recorded;
        Quiet(executor, recorded);
        executor.GetWorld().SetResource(Flag{0});
        KeptRegistry() = &registry;
        GYO_CHECK_ASSERTS(executor.Run("Tick"));
    }
    SUBCASE("BuildPlans from its own World's System") {
        Registry builds;
        FirstRound(builds, [](Registrar& r) {
            Data(r);
            r.AddSystem<&BuildsFromSystem>(B("BuildsFromSystem", "A"));
        });
        WorldExecutor executor(builds, "own");
        executor.GetWorld().SetResource(Flag{0});
        KeptRegistry() = &builds;
        GYO_CHECK_ASSERTS(executor.Run("Tick"));
    }
    SUBCASE("another Registry") {
        Registry target;
        FirstRound(target);
        WorldExecutor executor(registry, "other");
        executor.GetWorld().SetResource(Flag{0});
        KeptRegistry() = &target;
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
        REQUIRE(Done(target.BuildPlans()));
        CHECK(target.Frozen().ComponentIdOf<Later>().has_value());
    }
    KeptRegistry() = nullptr;
}

TEST_CASE("DI-KS 26: Register inside a build callable, and a World made while a round is pending, are asserts") {
    Registry registry;
    FirstRound(registry);
    GYO_CHECK_ASSERTS(registry.Register(kCommand, [&](Registrar&) {
        static_cast<void>(registry.Register(kCommand, [](Registrar&) {}));
    }));
    // The outer round stays pending (the inner call asserted): making a World now asserts too.
    GYO_CHECK_ASSERTS(World(registry, "late"));
    registry.DiscardRegistrations();
    World fine(registry, "fine");
    CHECK(fine.State() == WorldState::Ready);
}

TEST_CASE("DI-KS 26: after Finish, Register adds a round; Plugin::Finish is not called again") {
    TestPlugin plugin("base", [](Registrar& r) { Base(r); });
    Registry registry({&plugin});
    REQUIRE(Done(registry.Finish()));
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.AddSystem<&NewSystem>(B("NewSystem", "B")); })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().Version() == 2U);
    CHECK(plugin.builds == 1);
    CHECK_MESSAGE(plugin.finishes == 1, "Plugin::Finish ran again");
    // The owner list keeps the plugin first and adds the new owner after it.
    CHECK(registry.Frozen().SystemNamed("NewSystem")->owner == "command");
    CHECK(registry.Frozen().SystemNamed("Counting")->owner == "base");
}

TEST_CASE("DI-KS 13: an extract source registers again, then the Registry that reads it builds again") {
    const auto extractSchedule = [](Registrar& r) {
        ScheduleOptions s;
        s.name = "Extract";
        s.domain = Domain::Frame;
        s.clock = true;
        s.extract = true;
        s.stages = {"Copy"};
        r.Schedule(s);
    };
    Registry source;
    REQUIRE(Done(source.Register(kBase, [](Registrar& r) {
        r.Component<TestPos>({.name = "TestPos", .zone = Zone::C});
        r.Schedule(FixedSchedule());
    })));
    REQUIRE(Done(source.BuildPlans()));
    Registry render;
    render.SetExtractSource(source);
    REQUIRE(Done(render.Register(kBase, [extractSchedule](Registrar& r) {
        extractSchedule(r);
        r.AddSystem<&ExtractPosOf>(Sys("ExtractPos", Zone::B, {At("Extract", "Copy")}));
    })));
    REQUIRE(Done(render.BuildPlans()));
    // TestVel is not in the source yet.
    REQUIRE(Done(render.Register(kCommand, [](Registrar& r) {
        r.AddSystem<&ExtractVelOf>(Sys("ExtractVel", Zone::B, {At("Extract", "Copy")}));
    })));
    CHECK(ErrorCount(render.BuildPlans(), StartupCode::RE4) == 1);
    REQUIRE(Done(source.Register(kCommand, [](Registrar& r) { r.Component<TestVel>({.name = "TestVel", .zone = Zone::C}); })));
    REQUIRE(Done(source.BuildPlans()));
    REQUIRE(Done(render.Register(kCommand, [](Registrar& r) {
        r.AddSystem<&ExtractVelOf>(Sys("ExtractVel", Zone::B, {At("Extract", "Copy")}));
    })));
    const auto rebuilt = render.BuildPlans();
    REQUIRE_MESSAGE(Done(rebuilt), Describe(rebuilt));
    const SystemInfo* vel = render.Frozen().SystemNamed("ExtractVel");
    REQUIRE(vel != nullptr);
    const std::vector<AccessKey> expectedReads{{AccessKind::Component, *source.Frozen().ComponentIdOf<TestVel>()}};
    CHECK_MESSAGE(vel->access.extractReads == expectedReads, "extract reads of the rebuilt source");
}

// ---- S3: the read-only, versioned view (DI-KS 25)

TEST_CASE("DI-KS 25: the version is 0 before and inside Plugin::Finish, 1 after, +1 per success, kept on failure") {
    std::uint64_t inFinish = 99;
    TestPlugin plugin("base", [](Registrar& r) { Base(r); }, {},
                      [&](const FrozenRegistry& view, FinishContext&) { inFinish = view.Version(); });
    Registry registry({&plugin});
    REQUIRE(Done(registry.Finish()));
    CHECK_MESSAGE(inFinish == 0U, "DI-KS 25 version inside Plugin::Finish");
    const FrozenRegistry& view = registry.Frozen();
    CHECK(view.Version() == 1U);
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.AddSystem<&UsesUnknown>(B("UsesUnknown", "A")); })));
    REQUIRE_FALSE(Done(registry.BuildPlans()));
    CHECK_MESSAGE(view.Version() == 1U, "DI-KS 25 version after a failed round");
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.AddSystem<&NewSystem>(B("NewSystem", "A")); })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK_MESSAGE(view.Version() == 2U, "DI-KS 25 version after a later round");
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.AddSystem<&Plain>(B("Plain", "A")); })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(view.Version() == 3U);
}

TEST_CASE("DI-KS 25: hooks, initial values, schedules, sets, effects and annotated writes in the view") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Data(r);
        r.Component<Later>({.name = "Later", .zone = Zone::B});
        r.Effect("Io:file");
        r.InsertStage("Tick", "A", StagePlace::After, "AB");
        r.Hook<Tag, &OnTag>(HookTrigger::OnRemove, {.name = "OnTagRemoved", .zone = Zone::B});
        r.InitialValue<Mode, &ModeFromLater>();
        r.Set({.name = "Grp", .schedule = "Tick", .before = {"Plain"}, .after = {}},
              {Condition<&FlagIsSet>("FlagIsSet")});
        SystemOptions writes = B("WritesFlag", "A");
        writes.effects = {"Io:file"};
        r.AddSystem<&WritesFlag>(writes);
        r.AddSystem<&AnnotatesFlag>(B("AnnotatesFlag", "C"));
        r.AddSystem<&Plain>(B("Plain", "A"));
    });
    const FrozenRegistry& view = registry.Frozen();
    REQUIRE_MESSAGE(view.Hooks().size() == 1, "DI-KS 25 hooks listed");
    const HookInfo& hook = view.Hooks()[0];
    CHECK(hook.name == "OnTagRemoved");
    CHECK(hook.trigger == HookTrigger::OnRemove);
    CHECK(hook.component == *view.ComponentIdOf<Tag>());
    CHECK(hook.owner == "base");
    CHECK(hook.source.find("RegistrationRoundTests.cpp") != std::string::npos);
    REQUIRE_MESSAGE(view.InitialValues().size() == 1, "DI-KS 25 initial values listed");
    CHECK(view.InitialValues()[0].target == *view.ComponentIdOf<Mode>());
    CHECK(view.InitialValues()[0].request == *view.ComponentIdOf<Later>());
    CHECK(view.InitialValues()[0].owner == "base");
    REQUIRE_MESSAGE(view.Schedules().size() == 1, "DI-KS 25 schedules listed");
    CHECK(view.Schedules()[0].options.name == "Tick");
    CHECK(view.Schedules()[0].options.stages == std::vector<std::string>{"A", "AB", "B", "C"});
    REQUIRE_MESSAGE(view.Sets().size() == 1, "DI-KS 25 sets listed");
    CHECK(view.Sets()[0].options.name == "Grp");
    CHECK(view.Sets()[0].options.before == std::vector<std::string>{"Plain"});
    CHECK_MESSAGE(view.Sets()[0].conditions == std::vector<std::string>{"FlagIsSet"}, "DI-KS 25 set conditions");
    CHECK_MESSAGE(view.SystemNamed("WritesFlag")->effects.size() == 1, "DI-KS 25 System effects");
    CHECK(view.SystemNamed("Plain")->effects.empty());
    CHECK(view.SystemNamed("AnnotatesFlag")->access.annotatedWrites ==
          std::vector<ResourceId>{*view.ResourceIdOf<Flag>()});
}

TEST_CASE("DI-KS 25: the order graph lists System-to-System edges with their source") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Data(r);
        ScheduleOptions tick = TickSchedule();
        tick.name = "Ordered";
        tick.domain = Domain::Frame;
        tick.syncPoints = {SyncPointOptions{"Mid", "C"}};
        r.Schedule(tick);
        const auto at = [](std::string name, std::string stage, std::vector<std::string> before = {},
                           std::vector<std::string> after = {}, std::vector<std::string> sets = {}) {
            Mount m = At("Ordered", std::move(stage), std::move(before), std::move(after));
            m.sets = std::move(sets);
            return Sys(std::move(name), Zone::B, {m});
        };
        r.Set({.name = "Grp", .schedule = "Ordered", .before = {"Z"}, .after = {}});
        r.AddSystem<&WritesFlag>(at("X", "A"));
        r.AddSystem<&ReadsFlag>(at("Y", "A", {}, {}, {"Grp"}));  // conflicts with X, no order: a tie
        r.AddSystem<&Plain>(at("Z", "A", {}, {"X"}));
        r.AddSystem<&Plain>(at("W", "B"));
        r.AddSystem<&Plain>(at("U", "C", {"Mid"}));
        r.AddSystem<&Plain>(at("V", "C", {}, {"Mid"}));
    });
    const auto graph = registry.Frozen().OrderGraph("Ordered");
    const auto has = [&](const char* first, const char* second, OrderSource source) {
        return std::find(graph.begin(), graph.end(), OrderEdge{first, second, source}) != graph.end();
    };
    const auto any = [&](const char* first, const char* second) {
        return std::any_of(graph.begin(), graph.end(),
                           [&](const OrderEdge& e) { return e.first == first && e.second == second; });
    };
    CHECK_MESSAGE(has("X", "Y", OrderSource::TieBreak), "tie edge");
    CHECK_MESSAGE(has("X", "Z", OrderSource::Declared), "declared System edge");
    CHECK_MESSAGE(has("Y", "Z", OrderSource::Declared), "set edge");
    CHECK_MESSAGE(has("Z", "W", OrderSource::Stage), "stage edge");
    CHECK_MESSAGE(has("U", "V", OrderSource::SyncPoint), "sync point edge");
    CHECK_FALSE(any("Y", "X"));
    CHECK_FALSE(any("V", "U"));
    CHECK_FALSE(any("W", "Z"));
    CHECK(registry.Frozen().OrderGraph("NoSuchSchedule").empty());
}
