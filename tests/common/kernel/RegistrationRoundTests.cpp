// Registration in rounds (declaration v8-arch-gyo2-kernel-subtract-structure
// 3.2 and 3.3, section 5.3 rows S2 and S3): Register / BuildPlans repeated,
// the pending round, the World attaching to a new version, RemoveSystem, R-E7,
// WorldOptions::dynamicInsert, and the read-only, versioned Registry view.
// target/kernel DI-KS 3, 4, 13, 25, 26.

#include "WorldTestSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
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
struct LaterEvent { std::uint32_t n; };

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
SystemResult Fails(Res<Flag>) { return GYO::Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "planned failure")); }
SystemResult WritesFlag(ResMut<Flag>) { return Ok(); }
SystemResult ReadsFlag(Res<Flag>) { return Ok(); }
SystemResult Plain(Query<Read<Tag>>) { return Ok(); }
SystemResult OnTag(HookCtx) { return Ok(); }
bool FlagIsSet(Res<Flag> flag) { return flag->on != 0; }
Mode ModeFromLater(const Later& later) { return Mode{later.v}; }
Other OtherFromTag(const Tag& tag) { return Other{tag.t}; }
Other OtherFromLater(const Later& later) { return Other{later.v}; }
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
        r.Component<Later>({.name = "Later"});
    }));
    return Ok();
}
SystemResult BuildsFromSystem(Res<Flag>) {
    static_cast<void>(KeptRegistry()->BuildPlans());
    return Ok();
}
SystemResult DiscardsFromSystem(Res<Flag>) {
    KeptRegistry()->DiscardRegistrations();
    return Ok();
}
SystemResult HookRegisters(HookCtx) {
    static_cast<void>(KeptRegistry()->Register(kCommand, [](Registrar&) {}));
    return Ok();
}
SystemResult InsertsMode(CommandsFor<Inserts<Mode>, Removes<>> commands) {
    commands.Insert(1, Mode{1});
    return Ok();
}
// Not a System or hook: user code the kernel calls while a sync point applies.
Mode ModeFromLaterRegisters(const Later& later) {
    static_cast<void>(KeptRegistry()->Register(kCommand, [](Registrar&) {}));
    return Mode{later.v};
}
SystemResult InsertsLater(CommandsFor<Inserts<Later>, Removes<>> commands) {
    commands.Insert(1, Later{1});
    return Ok();
}
// Another Registry's World that a System runs (test side: a System captures nothing).
WorldExecutor*& KeptExecutor() {
    static WorldExecutor* executor = nullptr;
    return executor;
}
SystemResult RunsKeptExecutor(Res<Flag>) {
    static_cast<void>(KeptExecutor()->Run("Tick"));
    return Ok();
}
SystemResult Throws(Res<Flag>) { throw std::runtime_error("planned throw"); }
SystemResult SendsLater(MessageWriter<LaterEvent> out) {
    out.Send(LaterEvent{7});
    return Ok();
}
// A reader that runs before SendsLater and reads nothing: it holds the record past
// the Run's end trim (v8-arch-gyo2-time-events step 3), so the payload is there to count.
SystemResult HoldsLater(MessageReader<LaterEvent>) { return Ok(); }
SystemResult OnLaterAdd(HookCtx, Query<Read<Tag>> tags) {
    Log().push_back("later hook:" + std::to_string(tags.Count()));
    return Ok();
}

void Base(Registrar& r) {
    Data(r);
    r.AddSystem<&Counting>(B("Counting", "A"));
}

bool Done(const GYO::Engine::Base::Result<void, KernelError>& result) { return static_cast<bool>(result); }
bool Done(const GYO::Engine::Base::Result<void, StartupReport>& result) { return static_cast<bool>(result); }
std::string Describe(const GYO::Engine::Base::Result<void, StartupReport>& result) {
    return result ? std::string("ok") : result.error().detail;
}

// A Registry whose first round is Base, built.
void FirstRound(Registry& registry, void (*build)(Registrar&) = &Base) {
    REQUIRE(Done(registry.Register(kBase, build)));
    const auto built = registry.BuildPlans();
    REQUIRE_MESSAGE(Done(built), Describe(built));
}

// The expression of the GYO_ASSERT that step fails, or empty. Lets a test
// report an assertion with its own label (outside GYO_CHECK_ASSERTS the
// default handler would abort the run).
template <class Step>
std::string AssertionIn(Step&& step) {
    ::GYO::Test::ScopedAssertionHandler scope;
    try {
        std::forward<Step>(step)();
    } catch (const ::GYO::Engine::Base::AssertionFailure& failure) {
        return std::string(failure.expression);
    }
    return {};
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

// The message writer list is derived again at every BuildPlans (authority-rollback declaration 2.2), as the readers.
TEST_CASE("DI-KS 26: a later BuildPlans lists a message type's writer once") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Base(r);
        r.Message<LaterEvent>({.name = "LaterEvent"});
        r.AddSystem<&SendsLater>(B("SendsLater", "A"));
    });
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.AddSystem<&NewSystem>(B("NewSystem", "B")); })));
    REQUIRE(Done(registry.BuildPlans()));
    const auto& writers = registry.Frozen().Messages()[*registry.Frozen().MessageIdOf<LaterEvent>()].writers;
    CHECK_MESSAGE(writers.size() == 1, "[ks-msg-writers-round] " << writers.size());
}

TEST_CASE("DI-KS 26: after BuildPlans, before the next Run, the World API uses the new types") {
    Registry registry;
    FirstRound(registry);
    WorldExecutor executor(registry, "rounds");
    World& world = executor.GetWorld();
    world.Insert(1, Tag{1});
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
        r.Component<Later>({.name = "Later"});
        r.Resource<LaterRes>({.name = "LaterRes"});
        r.AddSystem<&UsesLater>(B("UsesLater", "A"));
    })));
    REQUIRE(Done(registry.BuildPlans()));
    // The World attaches at its next entry (here the World API), not at the next Run:
    // its summary has the new storage, like a World made at the new version.
    World fresh(registry, "fresh");
    fresh.Insert(1, Tag{1});
    CHECK_MESSAGE(SummaryOf(world) == SummaryOf(fresh), "attach at the World API entry (summary)");
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
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later"}); })));
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
        r.Component<Later>({.name = "Later"});
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
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Other>({.name = "Other"}); })));
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
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later"}); })));
    registry.DiscardRegistrations();
    CHECK(Done(registry.BuildPlans()));  // nothing left to build
    CHECK_MESSAGE(registry.Frozen().Version() == 1U, "a discarded round was built");
    CHECK_FALSE(registry.Frozen().ComponentIdOf<Later>().has_value());
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Other>({.name = "Other"}); })));
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
        r.Effect("IO:file");
        ScheduleOptions other;
        other.name = "Other";
        other.stages = {"Only"};
        r.Schedule(other);
        SystemOptions writes = B("WritesFlag", "A");
        writes.effects = {"IO:file"};
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
        r.AddSystem<&Plain>(Sys("OtherPlain", {m}));
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
    });
    WorldExecutor executor(registry, "rounds");
    // kind: what the R-E7 message names first (declaration 3.2 rule 6); each
    // SUBCASE's failure text carries it, so a mutant's expected text is its kind.
    const auto refused = [&](const std::string& kind, auto build) {
        REQUIRE(Done(registry.Register(kBase, build)));
        const auto result = registry.BuildPlans();
        CHECK_MESSAGE(ErrorCount(result, StartupCode::RE7) == 1, "R-E7 count (" << kind << "): " << Describe(result));
        if (!result) {
            const std::vector<StartupError>& errors = result.error().errors;
            const bool named = std::any_of(errors.begin(), errors.end(), [&](const StartupError& error) {
                return error.code == StartupCode::RE7 && error.message.rfind(kind + ": ", 0) == 0;
            });
            CHECK_MESSAGE(named, "R-E7 kind (" << kind << ") not named: " << Describe(result));
        }
        CHECK(registry.Frozen().Version() == 1U);
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    };
    SUBCASE("a hook on a component the previous version has") {
        refused("hook", [](Registrar& r) { r.Hook<Tag, &OnTag>(HookTrigger::OnAdd, {.name = "OnTag"}); });
    }
    SUBCASE("an initial value whose Target the previous version has") {
        refused("InitialValue Target", [](Registrar& r) {
            r.Component<Later>({.name = "Later"});
            r.InitialValue<Mode, &ModeFromLater>();
        });
    }
    SUBCASE("an initial value whose Request the previous version has") {
        // Entities that have Tag already would never get Other (S2 of the segment 2 review).
        refused("InitialValue Request", [](Registrar& r) {
            r.Component<Other>({.name = "Other"});
            r.InitialValue<Other, &OtherFromTag>();
        });
    }
    SUBCASE("a member of a named set the previous version has") {
        refused("set member", [](Registrar& r) { r.AddToSet<ReplicatedSet, Tag>(); });
    }
    SUBCASE("an allotter") {
        refused("allotter", [](Registrar& r) {
            r.Resource<TestRangeAllotter>({.name = "Allotter"});
            r.Allotter<TestRangeAllotter>();
        });
    }
    SUBCASE("every kind in one round") {
        REQUIRE(Done(registry.Register(kBase, [](Registrar& r) {
            r.Hook<Tag, &OnTag>(HookTrigger::OnAdd, {.name = "OnTag"});
            r.Component<Later>({.name = "Later"});
            r.Component<Other>({.name = "Other"});
            r.InitialValue<Mode, &ModeFromLater>();
            r.InitialValue<Other, &OtherFromTag>();
            r.AddToSet<ReplicatedSet, Tag>();
            r.Resource<TestRangeAllotter>({.name = "Allotter"});
            r.Allotter<TestRangeAllotter>();
        })));
        const auto result = registry.BuildPlans();
        const std::size_t count = ErrorCount(result, StartupCode::RE7);
        CHECK_MESSAGE(count == 5, "R-E7 count (all kinds) = " << count << ": " << Describe(result));
        CHECK(registry.Frozen().Version() == 1U);
        KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    }
    SUBCASE("what a later round may: a hook and an initial value of new components, a new named set") {
        REQUIRE(Done(registry.Register(kBase, [](Registrar& r) {
            r.Component<Later>({.name = "Later"});
            r.Component<Other>({.name = "Other"});
            r.Hook<Later, &OnTag>(HookTrigger::OnAdd, {.name = "OnLater"});
            r.InitialValue<Other, &OtherFromLater>();
            r.NamedSet<DigestTag>("Fresh");
            r.AddToSet<DigestTag, Tag>();
        })));
        const auto result = registry.BuildPlans();
        CHECK_MESSAGE(Done(result), Describe(result));
        CHECK(registry.Frozen().Version() == 2U);
        CHECK_MESSAGE(registry.Frozen().InitialValues().size() == 1, "a later round's initial value of new components");
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
            r.Component<Later>({.name = "Later"});
        });
        REQUIRE_MESSAGE(!Done(result), "a World that declares no dynamic insert did not refuse");
        CHECK(result.error().code == KernelErrorCode::DynamicInsertRefused);
        CHECK_FALSE(called);
        // Nothing changed: nothing to build, no pending round (a World may be made).
        CHECK(Done(registry.BuildPlans()));
        CHECK_MESSAGE(registry.Frozen().Version() == 1U, "a refused Register left a round pending");
        WorldExecutor another(registry, "another");
        CHECK_FALSE(registry.Frozen().ComponentIdOf<Later>().has_value());
        KERNEL_REQUIRE_RUN_OK(allowing.Run("Tick"));
    }
    // The refusing World is gone: Register is allowed again.
    REQUIRE_MESSAGE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later"}); })),
                    "Register refused after the refusing World is gone");
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().Version() == 2U);
}

TEST_CASE("DI-KS 26: an Interrupted World does not count as running; one that declares no dynamic insert still refuses") {
    SUBCASE("left by an exception, allows dynamic insert: Register goes on") {
        Registry registry;
        FirstRound(registry, [](Registrar& r) {
            Base(r);
            r.AddSystem<&Throws>(B("Throws", "C"));
        });
        WorldExecutor executor(registry, "interrupted");
        executor.GetWorld().SetResource(Flag{0});
        CHECK_THROWS_AS(static_cast<void>(executor.Run("Tick")), std::runtime_error);
        REQUIRE(executor.GetWorld().State() == WorldState::Interrupted);
        bool registered = false;
        const std::string asserted = AssertionIn([&] {
            registered = Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later"}); }));
        });
        REQUIRE_MESSAGE(asserted.empty(), "an Interrupted World counts as running: " << asserted);
        REQUIRE(registered);
        CHECK(Done(registry.BuildPlans()));
        CHECK(registry.Frozen().Version() == 2U);
    }
    SUBCASE("left by an Err, declares no dynamic insert: Register is still refused") {
        Registry registry;
        FirstRound(registry, [](Registrar& r) {
            Base(r);
            r.AddSystem<&Fails>(B("Fails", "C"));
        });
        WorldExecutor executor(registry, "interrupted", WorldOptions{false});
        executor.GetWorld().SetResource(Flag{0});
        REQUIRE_FALSE(Done(executor.Run("Tick")));
        REQUIRE(executor.GetWorld().State() == WorldState::Interrupted);
        const auto result = registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later"}); });
        REQUIRE_MESSAGE(!Done(result), "an Interrupted World that declares no dynamic insert stopped refusing");
        CHECK(result.error().code == KernelErrorCode::DynamicInsertRefused);
        CHECK(registry.Frozen().Version() == 1U);
    }
}

TEST_CASE("DI-KS 26: while a World made from the Registry runs, nothing may register, build or discard; another Registry may") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Base(r);
        r.AddSystem<&RegistersFromSystem>(B("RegistersFromSystem", "A"));
    });
    SUBCASE("Register from its own World's System") {
        WorldExecutor registersOwn(registry, "own");
        registersOwn.GetWorld().SetResource(Flag{0});
        KeptRegistry() = &registry;
        GYO_CHECK_ASSERTS(registersOwn.Run("Tick"));
    }
    SUBCASE("BuildPlans from its own World's System") {
        Registry builds;
        FirstRound(builds, [](Registrar& r) {
            Data(r);
            r.AddSystem<&BuildsFromSystem>(B("BuildsFromSystem", "A"));
        });
        WorldExecutor buildsOwn(builds, "own");
        buildsOwn.GetWorld().SetResource(Flag{0});
        KeptRegistry() = &builds;
        GYO_CHECK_ASSERTS(buildsOwn.Run("Tick"));
    }
    SUBCASE("DiscardRegistrations from its own World's System") {
        Registry discards;
        FirstRound(discards, [](Registrar& r) {
            Data(r);
            r.AddSystem<&DiscardsFromSystem>(B("DiscardsFromSystem", "A"));
        });
        WorldExecutor discardsOwn(discards, "own");
        discardsOwn.GetWorld().SetResource(Flag{0});
        KeptRegistry() = &discards;
        GYO_CHECK_ASSERTS(discardsOwn.Run("Tick"));
    }
    SUBCASE("Register from its own World's hook") {
        Registry hooked;
        FirstRound(hooked, [](Registrar& r) {
            Data(r);
            r.Hook<Mode, &HookRegisters>(HookTrigger::OnAdd, {.name = "HookRegisters"});
            r.AddSystem<&InsertsMode>(B("InsertsMode", "A"));
        });
        WorldExecutor hookOwn(hooked, "own");
        hookOwn.GetWorld().Insert(1, Tag{1});
        KeptRegistry() = &hooked;
        GYO_CHECK_ASSERTS(hookOwn.Run("Tick"));
    }
    SUBCASE("Register from an initial value function at a sync point") {
        // Not a System or hook: the review's S1 (the Executor holds the initial values while it applies).
        Registry filled;
        FirstRound(filled, [](Registrar& r) {
            Data(r);
            r.Component<Later>({.name = "Later"});
            r.InitialValue<Mode, &ModeFromLaterRegisters>();
            r.AddSystem<&InsertsLater>(B("InsertsLater", "A"));
        });
        WorldExecutor initialValueOwn(filled, "own");
        initialValueOwn.GetWorld().Insert(1, Tag{1});
        KeptRegistry() = &filled;
        GYO_CHECK_ASSERTS(initialValueOwn.Run("Tick"));
    }
    SUBCASE("Register from another Registry's World, run by its own World's System") {
        // The review's M1: the innermost call is the other World's System; the outer World is running.
        Registry outer;
        FirstRound(outer, [](Registrar& r) {
            Data(r);
            r.AddSystem<&RunsKeptExecutor>(B("RunsKeptExecutor", "A"));
            r.AddSystem<&Plain>(B("Plain", "C"));
        });
        WorldExecutor nestedOuter(outer, "outer");
        WorldExecutor inner(registry, "inner");
        nestedOuter.GetWorld().SetResource(Flag{0});
        inner.GetWorld().SetResource(Flag{0});
        KeptRegistry() = &outer;
        KeptExecutor() = &inner;
        GYO_CHECK_ASSERTS(nestedOuter.Run("Tick"));
        CHECK_MESSAGE(outer.Frozen().Version() == 1U, "the outer Registry was rebuilt while its World ran");
        KeptExecutor() = nullptr;
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

TEST_CASE("DI-KS 26: BuildPlans and DiscardRegistrations inside a build callable are asserts") {
    Registry buildInside;
    FirstRound(buildInside);
    GYO_CHECK_ASSERTS(buildInside.Register(kCommand, [&](Registrar&) { static_cast<void>(buildInside.BuildPlans()); }));
    buildInside.DiscardRegistrations();
    Registry discardInside;
    FirstRound(discardInside);
    GYO_CHECK_ASSERTS(discardInside.Register(kCommand, [&](Registrar&) { discardInside.DiscardRegistrations(); }));
    discardInside.DiscardRegistrations();
    // The first round too.
    Registry firstBuildInside;
    GYO_CHECK_ASSERTS(firstBuildInside.Register(kBase, [&](Registrar&) { static_cast<void>(firstBuildInside.BuildPlans()); }));
}

TEST_CASE("DI-KS 26: a build callable that throws leaves its registrations in the round") {
    Registry registry;
    FirstRound(registry);
    CHECK_THROWS_AS(static_cast<void>(registry.Register(kCommand, [](Registrar& r) {
        r.Component<Later>({.name = "Later"});
        throw std::runtime_error("planned throw");
    })), std::runtime_error);
    // The round is pending (a World may not be made) and goes on with the next Register.
    GYO_CHECK_ASSERTS(World(registry, "late"));
    bool registered = false;
    const std::string asserted = AssertionIn([&] {
        registered = Done(registry.Register(kCommand, [](Registrar& r) { r.Resource<LaterRes>({.name = "LaterRes"}); }));
    });
    REQUIRE_MESSAGE(asserted.empty(), "a build that threw left its Registrar open: " << asserted);
    REQUIRE(registered);
    const auto built = registry.BuildPlans();
    REQUIRE_MESSAGE(Done(built), Describe(built));
    CHECK(registry.Frozen().ComponentIdOf<Later>().has_value());
    CHECK(registry.Frozen().ResourceIdOf<LaterRes>().has_value());
    CHECK(registry.Frozen().Version() == 2U);
}

TEST_CASE("DI-KS 4: DiscardRegistrations in the first round drops what was registered") {
    Registry registry;
    REQUIRE(Done(registry.Register(kBase, &Base)));
    registry.DiscardRegistrations();
    CHECK(Done(registry.BuildPlans()));
    CHECK_MESSAGE(!registry.IsFrozen(), "a first-round discard kept the round");
    REQUIRE(Done(registry.Register(kBase, &Base)));
    const auto built = registry.BuildPlans();
    REQUIRE_MESSAGE(Done(built), Describe(built));
    CHECK(registry.Frozen().Version() == 1U);
    CHECK(registry.Frozen().ComponentIdOf<Pos>() == 0U);
}

TEST_CASE("DI-KS 26: after a failed first round, Register is an assert") {
    Registry failedFirst;
    REQUIRE(Done(failedFirst.Register(kBase, [](Registrar& r) {
        Data(r);
        r.AddSystem<&UsesUnknown>(B("UsesUnknown", "A"));
    })));
    REQUIRE_FALSE(Done(failedFirst.BuildPlans()));
    GYO_CHECK_ASSERTS(failedFirst.Register(kCommand, [](Registrar&) {}));
}

TEST_CASE("DI-KS 26: an existing World attaches every table a later round adds (verification on)") {
    Registry registry;
    FirstRound(registry);
    WorldExecutor executor(registry, "attach");
    World& world = executor.GetWorld();
    world.SetResource(KernelSettings{true, false});
    world.Insert(1, Tag{1});
    KERNEL_REQUIRE_RUN_OK(executor.Run("Tick"));
    Log().clear();
    // Each SUBCASE adds one kind of table, so a table the World misses fails only its own label.
    SUBCASE("message buffers") {
        REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
            r.Message<LaterEvent>({.name = "LaterEvent"});
            r.AddSystem<&SendsLater>(B("SendsLater", "A"));
            r.AddSystem<&HoldsLater>(B("HoldsLater", "A", {"SendsLater"}));
        })));
        REQUIRE(Done(registry.BuildPlans()));
        std::string ran;
        std::size_t payloads = 0;
        const std::string asserted = AssertionIn([&] {
            ran = Text(executor.Run("Tick"));
            payloads = world.MessagePayloads<LaterEvent>().size();
        });
        REQUIRE_MESSAGE(asserted.empty(), "attach (message buffers): " << asserted);
        CHECK_MESSAGE(ran == "ok", "attach (message buffers): " << ran);
        CHECK_MESSAGE(payloads == 1, "attach (message buffers): payloads " << payloads);
    }
    SUBCASE("verification exclusions") {
        // ExcludeFromVerification is for a World that has not run: one made before the round.
        WorldExecutor unrun(registry, "unrun");
        REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.Component<Later>({.name = "Later"}); })));
        REQUIRE(Done(registry.BuildPlans()));
        std::vector<std::string> excluded;
        const std::string asserted = AssertionIn([&] {
            unrun.GetWorld().ExcludeFromVerification<Later>();
            excluded = unrun.Verification().excludedColumns;
        });
        REQUIRE_MESSAGE(asserted.empty(), "attach (verification exclusions): " << asserted);
        CHECK_MESSAGE(excluded == std::vector<std::string>{"Later"}, "attach (verification exclusions): " << Join(excluded));
    }
    SUBCASE("hook access") {
        REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
            r.Component<Later>({.name = "Later"});
            r.Hook<Later, &OnLaterAdd>(HookTrigger::OnAdd, {.name = "OnLaterAdd"});
            r.AddSystem<&InsertsLater>(B("InsertsLater", "A"));
        })));
        REQUIRE(Done(registry.BuildPlans()));
        std::string ran;
        const std::string asserted = AssertionIn([&] { ran = Text(executor.Run("Tick")); });
        REQUIRE_MESSAGE(asserted.empty(), "attach (hook access): " << asserted);
        CHECK_MESSAGE(ran == "ok", "attach (hook access): " << ran);
        CHECK_MESSAGE(std::find(Log().begin(), Log().end(), "later hook:1") != Log().end(),
                      "attach (hook access): the hook did not run: " << Join(Log()));
    }
    SUBCASE("hook queues") {
        REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) {
            ScheduleOptions late;
            late.name = "Late";
            late.stages = {"Only"};
            r.Schedule(late);
            r.AddSystem<&NewSystem>(Sys("LateSystem", {At("Late", "Only")}));
        })));
        REQUIRE(Done(registry.BuildPlans()));
        std::string late;
        std::string tick;
        const std::string asserted = AssertionIn([&] {
            late = Text(executor.Run("Late"));
            tick = Text(executor.Run("Tick"));
        });
        REQUIRE_MESSAGE(asserted.empty(), "attach (hook queues): " << asserted);
        CHECK_MESSAGE(late == "ok", "attach (hook queues): " << late);
        CHECK(tick == "ok");
        CHECK_MESSAGE(Join(Log()) == "new old run2 changed:0", "attach (hook queues): " << Join(Log()));
    }
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

TEST_CASE("DI-KS 26: Register adds a later round; the owner list keeps the first owner first") {
    Registry registry;
    REQUIRE(Done(registry.Register(kBase, &Base)));
    REQUIRE(Done(registry.BuildPlans()));
    REQUIRE(Done(registry.Register(kCommand, [](Registrar& r) { r.AddSystem<&NewSystem>(B("NewSystem", "B")); })));
    REQUIRE(Done(registry.BuildPlans()));
    CHECK(registry.Frozen().Version() == 2U);
    // The owner list keeps the first owner first and adds the new owner after it.
    CHECK(registry.Frozen().SystemNamed("NewSystem")->owner == "command");
    CHECK(registry.Frozen().SystemNamed("Counting")->owner == "base");
}

TEST_CASE("DI-KS 13: an extract source registers again, then the Registry that reads it builds again") {
    const auto extractSchedule = [](Registrar& r) {
        ScheduleOptions s;
        s.name = "Extract";
        s.extract = true;
        s.stages = {"Copy"};
        r.Schedule(s);
    };
    Registry source;
    REQUIRE(Done(source.Register(kBase, [](Registrar& r) {
        r.Component<TestPos>({.name = "TestPos"});
        r.Schedule(FixedSchedule());
    })));
    REQUIRE(Done(source.BuildPlans()));
    Registry render;
    render.SetExtractSource(source);
    REQUIRE(Done(render.Register(kBase, [extractSchedule](Registrar& r) {
        extractSchedule(r);
        r.AddSystem<&ExtractPosOf>(Sys("ExtractPos", {At("Extract", "Copy")}));
    })));
    REQUIRE(Done(render.BuildPlans()));
    // TestVel is not in the source yet.
    REQUIRE(Done(render.Register(kCommand, [](Registrar& r) {
        r.AddSystem<&ExtractVelOf>(Sys("ExtractVel", {At("Extract", "Copy")}));
    })));
    CHECK(ErrorCount(render.BuildPlans(), StartupCode::RE4) == 1);
    REQUIRE(Done(source.Register(kCommand, [](Registrar& r) { r.Component<TestVel>({.name = "TestVel"}); })));
    REQUIRE(Done(source.BuildPlans()));
    REQUIRE(Done(render.Register(kCommand, [](Registrar& r) {
        r.AddSystem<&ExtractVelOf>(Sys("ExtractVel", {At("Extract", "Copy")}));
    })));
    const auto rebuilt = render.BuildPlans();
    REQUIRE_MESSAGE(Done(rebuilt), Describe(rebuilt));
    const SystemInfo* vel = render.Frozen().SystemNamed("ExtractVel");
    REQUIRE(vel != nullptr);
    const std::vector<AccessKey> expectedReads{{AccessKind::Component, *source.Frozen().ComponentIdOf<TestVel>()}};
    CHECK_MESSAGE(vel->access.extractReads == expectedReads, "extract reads of the rebuilt source");
}

TEST_CASE("DI-KS 13: SetExtractSource after the first BuildPlans is an assert") {
    Registry source;
    FirstRound(source);
    Registry builtReader;
    FirstRound(builtReader);
    GYO_CHECK_ASSERTS(builtReader.SetExtractSource(source));
}

// ---- S3: the read-only, versioned view (DI-KS 25)

// What a registration unit's Finish reads (version 1, after the Plans) is the
// orchestration layer's test since v8-arch-gyo2-orchestration-skeleton.
TEST_CASE("DI-KS 25: the version is 1 after the first BuildPlans, +1 per success, kept on failure") {
    Registry registry;
    REQUIRE(Done(registry.Register(kBase, &Base)));
    REQUIRE(Done(registry.BuildPlans()));
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
        r.Component<Later>({.name = "Later"});
        r.Effect("IO:file");
        r.InsertStage("Tick", "A", StagePlace::After, "AB");
        r.Hook<Tag, &OnTag>(HookTrigger::OnRemove, {.name = "OnTagRemoved"});
        r.InitialValue<Mode, &ModeFromLater>();
        r.Set({.name = "Grp", .schedule = "Tick", .before = {"Plain"}, .after = {}},
              {Condition<&FlagIsSet>("FlagIsSet")});
        SystemOptions writes = B("WritesFlag", "A");
        writes.effects = {"IO:file"};
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
          (std::vector<AccessKey>{{AccessKind::Resource, *view.ResourceIdOf<Flag>()}}));
}

TEST_CASE("DI-KS 25: the order graph lists System-to-System edges with their source") {
    Registry registry;
    FirstRound(registry, [](Registrar& r) {
        Data(r);
        ScheduleOptions tick = TickSchedule();
        tick.name = "Ordered";
        tick.syncPoints = {SyncPointOptions{"Mid", "C"}};
        r.Schedule(tick);
        const auto at = [](std::string name, std::string stage, std::vector<std::string> before = {},
                           std::vector<std::string> after = {}, std::vector<std::string> sets = {}) {
            Mount m = At("Ordered", std::move(stage), std::move(before), std::move(after));
            m.sets = std::move(sets);
            return Sys(std::move(name), {m});
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
