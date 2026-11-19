// Pushed events (v8-arch-gyo2-time-events step 5; event cursor declaration 3.3,
// 5.2 and section 8 "new: pushed"; time-events declaration 4.2, 4.5; DI-KS 10
// draft): an event type (Registrar::Event) is kept nowhere; a Trigger runs its
// observers (Registrar::Observer). ExclusiveWorld::Trigger and the Host's
// WorldExecutor::Trigger run them now, and what they record is applied before
// the call returns, round after round up to the hook round limit; a
// Commands::Trigger runs them at the last step of the sync point that applies
// it, in record order. Add<T> / Remove<T> observers run per (entity,
// component): OnAdd hooks, then Add observers; Remove observers, then OnRemove
// hooks. An observer is a System (its access derived and verified, a message
// reader with a cursor of its own), holds no ExclusiveWorld, and the observers
// of one trigger run in a deterministic order (by name).
// Each CHECK carries a tag in brackets ("[obs-...]"): the mutants of the step
// expect that tag (docs/architecture/plans/kernel/scripts/mutations.json, ids obs-).
// The entry is J-59 (a new one: the spec has no pushed events entry; J-58 was
// the kernel's Startup, moved to the orchestration layer).

#include "WorldTestSupport.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

struct DoorOpened { std::uint32_t door; };  // event, zone B
struct Rang { std::uint32_t n; };           // event, zone B

std::string Joined() {
    std::string out;
    for (const std::string& line : Log()) out += (out.empty() ? "" : " | ") + line;
    return out;
}

std::size_t LinesStartingWith(const std::string& prefix) {
    std::size_t n = 0;
    for (const std::string& line : Log()) n += line.rfind(prefix, 0) == 0 ? 1U : 0U;
    return n;
}

std::string TargetText(const std::optional<EntityId>& target) {
    return target ? "@" + std::to_string(*target) : std::string();
}

void Events(Registrar& r) {
    r.Event<DoorOpened>({.name = "DoorOpened", .zone = Zone::B});
    r.Event<Rang>({.name = "Rang", .zone = Zone::B});
}

// ---- Observers

SystemResult LogDoor(On<DoorOpened> on) {
    Log().push_back("door:" + std::to_string(on.Event().door) + TargetText(on.Target()));
    return Ok();
}
SystemResult LogDoorA(On<DoorOpened>) {
    Log().push_back("a");
    return Ok();
}
SystemResult LogDoorB(On<DoorOpened>) {
    Log().push_back("b");
    return Ok();
}
SystemResult LogDoorC(On<DoorOpened>) {
    Log().push_back("c");
    return Ok();
}
// Reads what the same sync point applied: a resource set and a cell inserted in its window.
SystemResult LogDoorAndWorld(On<DoorOpened> on, Res<Flag> flag, Query<Read<Tag>> tags) {
    Log().push_back("door:" + std::to_string(on.Event().door) + " flag:" + std::to_string(flag->on) +
                    " tag:" + std::to_string(tags.Contains(1) ? 1 : 0));
    return Ok();
}
// Inserts Mode on the door's target (applied by the round or sync point after it).
SystemResult ModeOnDoor(On<DoorOpened> on, CommandsFor<Inserts<Mode>, Removes<>> commands) {
    Log().push_back("door:" + std::to_string(on.Event().door));
    if (on.Target()) commands.Insert(*on.Target(), Mode{4});
    return Ok();
}
SystemResult ObserveTagAdded(On<Add<Tag>> on) {
    Log().push_back("obs+Tag" + TargetText(on.Target()));
    return Ok();
}
SystemResult ObserveModeAdded(On<Add<Mode>> on, Commands commands) {
    Log().push_back("obs+Mode" + TargetText(on.Target()));
    commands.Trigger(Rang{1});
    return Ok();
}
SystemResult ObserveModeAddedQuiet(On<Add<Mode>> on) {
    Log().push_back("obs+Mode" + TargetText(on.Target()));
    return Ok();
}
// Remove observers read the cells they are told about, and the entity's others.
SystemResult ObserveTagRemoved(On<Remove<Tag>> on, Query<Read<Tag>> tags) {
    Log().push_back("obs-Tag" + TargetText(on.Target()) + " tag:" + std::to_string(tags.Contains(*on.Target()) ? 1 : 0));
    return Ok();
}
SystemResult ObserveModeRemoved(On<Remove<Mode>> on, Query<Read<Tag>> tags) {
    Log().push_back("obs-Mode" + TargetText(on.Target()) + " tag:" + std::to_string(tags.Contains(*on.Target()) ? 1 : 0));
    return Ok();
}
SystemResult LogRang(On<Rang> on) {
    Log().push_back("rang:" + std::to_string(on.Event().n));
    return Ok();
}
// Triggers the next Rang every time: rounds that never empty.
SystemResult Retrigger(On<Rang> on, Commands commands) {
    Log().push_back("rang:" + std::to_string(on.Event().n));
    commands.Trigger(Rang{on.Event().n + 1});
    return Ok();
}
// A reader of its own: reads the Pings written since it last ran.
SystemResult ReadPingsOnDoor(On<DoorOpened>, MessageReader<Ping> pings) {
    std::string line = "obs-read:";
    for (const Ping* ping : pings.Read()) line += " " + std::to_string(ping->n);
    Log().push_back(line);
    return Ok();
}
// Writes Out through a World pointer kept outside, which it does not declare (W-5).
SystemResult WritesUndeclared(On<DoorOpened>) {
    if (Out* out = KeptWorld()->ResourceMut<Out>()) out->value = 9;
    return Ok();
}
SystemResult WritesDeclared(On<DoorOpened>, ResMut<Out> out) {
    out->value = 8;
    return Ok();
}
SystemResult FailingObserver(On<DoorOpened>) {
    return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "the observer failed"));
}
SystemResult ObserverWithLocal(On<DoorOpened>, Local<std::uint32_t>) { return Ok(); }
SystemResult WritesMode(On<DoorOpened>, Query<Write<Mode>>) { return Ok(); }

// ---- Hooks

SystemResult HookTagAdded(HookCtx ctx) {
    Log().push_back("hook+Tag@" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult HookModeAdded(HookCtx ctx) {
    Log().push_back("hook+Mode@" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult HookTagRemoved(HookCtx ctx) {
    Log().push_back("hook-Tag@" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult HookModeRemoved(HookCtx ctx) {
    Log().push_back("hook-Mode@" + std::to_string(ctx.Entity()));
    return Ok();
}
// A hook that triggers an event now through its ExclusiveWorld<> (refused).
SystemResult HookTriggersNow(ExclusiveWorld<> world) {
    auto done = world.Trigger(Rang{5});
    Log().push_back(done ? "hook trigger ok" : std::string("hook ") + std::string(ToString(done.error().code)));
    return done;
}

// ---- Systems

SystemResult OpenDoorNow(ExclusiveWorld<> world) {
    Log().push_back("before");
    auto done = world.Trigger(DoorOpened{7}, EntityId{42});
    Log().push_back(done ? "after" : "after:" + std::string(ToString(done.error().code)));
    return done;
}
// Triggers DoorOpened{1} on entity 8 now; then reads what the observers' rounds and the level's window did.
SystemResult OpenDoorOnEight(ExclusiveWorld<> world) {
    auto done = world.Trigger(DoorOpened{1}, EntityId{8});
    if (!done) return done;
    const auto mode = world.Read<Mode>(8, [](const Mode&) {});
    const auto tag = world.Read<Tag>(8, [](const Tag&) {});
    if (!mode) return Engine::Base::Err(mode.error());
    if (!tag) return Engine::Base::Err(tag.error());
    Log().push_back(std::string("after mode:") + (*mode ? "1" : "0") + " tag:" + (*tag ? "1" : "0"));
    return Ok();
}
SystemResult RingNow(ExclusiveWorld<> world) { return world.Trigger(Rang{0}); }
// Triggers inside a Read closure (refused).
SystemResult TriggerInClosure(ExclusiveWorld<> world) {
    Engine::Base::Result<void, KernelError> inner = Engine::Base::Result<void, KernelError>{};
    auto read = world.Read<Flag>([&](const Flag&) { inner = world.Trigger(Rang{1}); });
    if (!read) return Engine::Base::Err(std::move(read).error());
    return inner;
}
SystemResult RecordTagOnEight(Commands commands) {
    commands.Insert(8, Tag{3});
    return Ok();
}
SystemResult TriggerTwoAndSet(Commands commands) {
    commands.Trigger(DoorOpened{1});
    commands.Trigger(DoorOpened{2});
    commands.SetResource(Flag{5});
    commands.Insert(1, Tag{9});
    Log().push_back("recorded");
    return Ok();
}
SystemResult TriggerThird(Commands commands) {
    commands.Trigger(DoorOpened{3});
    Log().push_back("recorded-third");
    return Ok();
}
SystemResult TriggerDoor(Commands commands) {
    commands.Trigger(DoorOpened{0});
    return Ok();
}
SystemResult TriggerDoorOnOne(Commands commands) {
    commands.Trigger(DoorOpened{0}, EntityId{1});
    return Ok();
}
SystemResult AlsoWritesMode(Query<Write<Mode>>) { return Ok(); }
// Triggers DoorOpened on the Runs of Tick 1 and 3 only (TickCount).
SystemResult TriggerDoorOnOddRuns(Commands commands, Res<TickCount> ticks) {
    if (ticks->value == 1 || ticks->value == 3) commands.Trigger(DoorOpened{0});
    return Ok();
}
SystemResult WritePing(MessageWriter<Ping> pings, Res<TickCount> ticks) {
    pings.Send(Ping{static_cast<std::uint32_t>(ticks->value)});
    return Ok();
}
SystemResult SpawnTagMode(SpawnWithId<Bundle<Tag, Mode>> spawn) {
    spawn.New(5, Tag{1}, Mode{1});
    return Ok();
}
SystemResult RemoveTagOfFive(CommandsFor<Inserts<>, Removes<Tag>> commands) {
    commands.Remove<Tag>(5);
    return Ok();
}
SystemResult DespawnFive(Despawn despawn) {
    despawn.Entity(5);
    return Ok();
}
SystemResult SpawnSecondTag(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(6, Tag{2});
    return Ok();
}

void LifecycleObservers(Registrar& r) {
    r.Hook<Tag, &HookTagAdded>(HookTrigger::OnAdd, {.name = "hook+Tag", .zone = Zone::B});
    r.Hook<Mode, &HookModeAdded>(HookTrigger::OnAdd, {.name = "hook+Mode", .zone = Zone::B});
    r.Hook<Tag, &HookTagRemoved>(HookTrigger::OnRemove, {.name = "hook-Tag", .zone = Zone::B});
    r.Hook<Mode, &HookModeRemoved>(HookTrigger::OnRemove, {.name = "hook-Mode", .zone = Zone::B});
    r.Observer<Add<Tag>, &ObserveTagAdded>({.name = "obs+Tag", .zone = Zone::B});
    r.Observer<Add<Mode>, &ObserveModeAddedQuiet>({.name = "obs+Mode", .zone = Zone::B});
    r.Observer<Remove<Tag>, &ObserveTagRemoved>({.name = "obs-Tag", .zone = Zone::B});
    r.Observer<Remove<Mode>, &ObserveModeRemoved>({.name = "obs-Mode", .zone = Zone::B});
}

} // namespace

// ---- When observers run

TEST_CASE("J-59: ExclusiveWorld::Trigger runs the observers now, before it returns, with the event and its target") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &LogDoor>({.name = "log-door", .zone = Zone::B});
        r.AddSystem<&OpenDoorNow>(B("OpenDoorNow", "A"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "before | door:7@42 | after", "[obs-now] " << Joined());
}

TEST_CASE("J-59: a Commands::Trigger runs the observers at the sync point's last step, after the resource step, in record order") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &LogDoorAndWorld>({.name = "log-door", .zone = Zone::B});
        r.AddSystem<&TriggerTwoAndSet>(B("TriggerTwoAndSet", "A"));
        r.AddSystem<&TriggerThird>(B("TriggerThird", "A", {}, {"TriggerTwoAndSet"}));
    });
    rig->world().SetResource(Flag{0});
    rig->world().Insert(1, Mode{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // Not when recorded: after both Systems; each sees the resource and the cell of the same sync point.
    CHECK_MESSAGE(Joined() == "recorded | recorded-third | door:1 flag:5 tag:1 | door:2 flag:5 tag:1 | door:3 flag:5 tag:1",
                  "[obs-deferred-last-step] " << Joined());
}

TEST_CASE("J-59: what an observer records at a sync point goes the way of a hook's records: the next sync point applies it") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &ModeOnDoor>({.name = "mode-on-door", .zone = Zone::B});
        r.Observer<Add<Mode>, &ObserveModeAdded>({.name = "obs+Mode", .zone = Zone::B});
        r.Observer<Rang, &LogRang>({.name = "log-rang", .zone = Zone::B});
        r.AddSystem<&TriggerDoorOnOne>(B("TriggerDoorOnOne", "A"));
    });
    rig->world().Insert(1, Tag{0});
    const std::uint64_t epoch = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // Stage A's sync point: DoorOpened (the insert goes to the hook queue); stage B's: the insert and its
    // Add observer (its trigger goes to the hook queue); stage C's: Rang. No hook round is needed.
    CHECK_MESSAGE(Joined() == "door:0 | obs+Mode@1 | rang:1", "[obs-observer-records] " << Joined());
    CHECK_MESSAGE(rig->world().SyncEpoch() == epoch + 3, "[obs-observer-records] three sync points, no round");
}

// ---- Add and Remove

TEST_CASE("J-59: per (entity, component), the OnAdd hooks run before the Add observers") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        LifecycleObservers(r);
        r.AddSystem<&SpawnTagMode>(B("SpawnTagMode", "A"));
        r.AddSystem<&SpawnSecondTag>(B("SpawnSecondTag", "A"));
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "hook+Tag@5 | obs+Tag@5 | hook+Mode@5 | obs+Mode@5 | hook+Tag@6 | obs+Tag@6",
                  "[obs-add-order] " << Joined());
}

TEST_CASE("J-59: per (entity, component), the Remove observers run before the OnRemove hooks, while the cell is there") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        LifecycleObservers(r);
        r.AddSystem<&RemoveTagOfFive>(B("RemoveTagOfFive", "A"));
    });
    rig->world().Insert(5, Tag{1});
    rig->world().Insert(5, Mode{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "obs-Tag@5 tag:1 | hook-Tag@5", "[obs-remove-order] " << Joined());
    CHECK_MESSAGE(rig->world().Get<Tag>(5) == nullptr, "[obs-remove-order] the cell is removed after both");
}

TEST_CASE("J-59: a despawn runs every cell's Remove observers and OnRemove hooks, cell by cell, before it destroys the entity") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        LifecycleObservers(r);
        r.AddSystem<&DespawnFive>(B("DespawnFive", "A"));
    });
    rig->world().Insert(5, Tag{1});
    rig->world().Insert(5, Mode{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // The Mode observer still sees the entity's Tag: nothing is destroyed until every cell ran its reactions.
    CHECK_MESSAGE(Joined() == "obs-Tag@5 tag:1 | hook-Tag@5 | obs-Mode@5 tag:1 | hook-Mode@5", "[obs-despawn-order] " << Joined());
    CHECK_MESSAGE(!rig->world().Alive(5), "[obs-despawn-order] the entity is destroyed afterwards");
}

TEST_CASE("J-59: the Host's direct writes in Ready (Insert, Remove, Despawn) run no Add or Remove observer") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        LifecycleObservers(r);
    });
    rig->world().Insert(9, Tag{1});
    rig->world().Insert(9, Mode{1});
    rig->world().Remove<Tag>(9);
    rig->world().Despawn(9);
    CHECK_MESSAGE(Joined().empty(), "[obs-host-direct] " << Joined());
}

// ---- Triggers outside a sync point: the rounds

TEST_CASE("J-59: the Host's Trigger in Ready runs the observers now and applies what they record before it returns") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &ModeOnDoor>({.name = "mode-on-door", .zone = Zone::B});
        r.Hook<Mode, &HookModeAdded>(HookTrigger::OnAdd, {.name = "hook+Mode", .zone = Zone::B});
        r.Observer<Add<Mode>, &ObserveModeAdded>({.name = "obs+Mode", .zone = Zone::B});
        r.Observer<Rang, &LogRang>({.name = "log-rang", .zone = Zone::B});
    });
    rig->world().Insert(3, Tag{0});
    const std::uint64_t epoch = rig->world().SyncEpoch();
    const auto triggered = rig->executor->Trigger(DoorOpened{2}, EntityId{3});
    REQUIRE_MESSAGE(static_cast<bool>(triggered), Text(triggered));
    // Round 1: the Mode insert (its OnAdd hook, its Add observer, which triggers Rang); round 2: Rang.
    CHECK_MESSAGE(Joined() == "door:2 | hook+Mode@3 | obs+Mode@3 | rang:1", "[obs-host-applied] " << Joined());
    CHECK_MESSAGE(rig->world().Get<Mode>(3) != nullptr, "[obs-host-applied] the observer's insert is applied");
    CHECK_MESSAGE(rig->world().SyncEpoch() == epoch + 2, "[obs-host-applied] one sync epoch per round");
    CHECK_MESSAGE(rig->world().State() == WorldState::Ready, "[obs-host-applied] the World is Ready again");
    // The World runs on as usual.
    KERNEL_REQUIRE_RUN_OK(rig->Run());
}

TEST_CASE("J-59: the Host's Trigger stops at KernelSettings::hookRoundLimit: HookRoundLimit, reported once, the World interrupted") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<Rang, &Retrigger>({.name = "retrigger", .zone = Zone::B});
    });
    rig->world().SetResource(KernelSettings{true, false, 3});
    const auto triggered = rig->executor->Trigger(Rang{0});
    CHECK_MESSAGE(HasCode(triggered, KernelErrorCode::HookRoundLimit), "[obs-host-limit] " << Text(triggered));
    // The trigger and three rounds ran; a fourth round is the error.
    CHECK_MESSAGE(LinesStartingWith("rang:") == 4, "[obs-host-limit] " << Joined());
    CHECK_MESSAGE(Text(triggered).find("observer 'retrigger'") != std::string::npos, "[obs-host-limit] " << Text(triggered));
    CHECK_MESSAGE(rig->recorded.size() == 1, "[obs-host-limit] reported once");
    CHECK_MESSAGE(rig->world().State() == WorldState::Interrupted, "[obs-host-limit] the World is interrupted");
}

TEST_CASE("J-59: the Host's Trigger with verification on: an undeclared write is reported; a change outside the World API before it too") {
    SUBCASE("an observer's undeclared write") {
        Log().clear();
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            Events(r);
            r.Observer<DoorOpened, &WritesUndeclared>({.name = "undeclared", .zone = Zone::B});
        });
        rig->world().SetResource(Out{0});
        KeptWorld() = &rig->world();
        const auto triggered = rig->executor->Trigger(DoorOpened{1});
        KeptWorld() = nullptr;
        CHECK_MESSAGE(HasCode(triggered, KernelErrorCode::AccessViolation), "[obs-host-verification] " << Text(triggered));
        CHECK_MESSAGE(rig->recorded.size() == 1, "[obs-host-verification] reported once");
        CHECK_MESSAGE(rig->world().State() == WorldState::Interrupted, "[obs-host-verification] interrupted");
    }
    SUBCASE("a change outside the World API since the last Run") {
        Log().clear();
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            Events(r);
            r.Observer<DoorOpened, &LogDoor>({.name = "log-door", .zone = Zone::B});
        });
        rig->world().Insert(1, Tag{1});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        const Tag* kept = rig->world().Get<Tag>(1);
        const_cast<Tag*>(kept)->t = 42;  // not through the World API
        const auto triggered = rig->executor->Trigger(DoorOpened{1});
        CHECK_MESSAGE(HasCode(triggered, KernelErrorCode::PreCallChange), "[obs-host-precall] " << Text(triggered));
        CHECK_MESSAGE(Joined().empty(), "[obs-host-precall] no observer ran: " << Joined());
    }
    SUBCASE("a clean trigger leaves nothing for the next Run's Ready window") {
        Log().clear();
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            Events(r);
            r.Observer<DoorOpened, &WritesDeclared>({.name = "declared", .zone = Zone::B});
            r.Observer<DoorOpened, &ModeOnDoor>({.name = "mode-on-door", .zone = Zone::B});
        });
        rig->world().SetResource(Out{0});
        rig->world().Insert(3, Tag{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        const auto triggered = rig->executor->Trigger(DoorOpened{1}, EntityId{3});
        REQUIRE_MESSAGE(static_cast<bool>(triggered), Text(triggered));
        CHECK_MESSAGE(rig->world().Resource<Out>()->value == 8, "[obs-host-verification-clean]");
        const auto ran = rig->Run();
        CHECK_MESSAGE(static_cast<bool>(ran), "[obs-host-verification-clean] " << Text(ran));
    }
}

TEST_CASE("J-59: ExclusiveWorld::Trigger applies the observers' records before it returns; the level's window waits") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &ModeOnDoor>({.name = "mode-on-door", .zone = Zone::B});
        r.Observer<Add<Mode>, &ObserveModeAddedQuiet>({.name = "obs+Mode", .zone = Zone::B});
        r.AddSystem<&RecordTagOnEight>(B("RecordTagOnEight", "A"));
        r.AddSystem<&OpenDoorOnEight>(B("OpenDoorOnEight", "A", {}, {"RecordTagOnEight"}));
    });
    rig->world().Insert(8, Pos{0});
    const std::uint64_t epoch = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "door:1 | obs+Mode@8 | after mode:1 tag:0", "[obs-exclusive-rounds] " << Joined());
    // The trigger's round, then the stage's sync point (which applied the Tag) and the other stages'.
    CHECK_MESSAGE(rig->world().Get<Tag>(8) != nullptr, "[obs-exclusive-rounds] the level's insert is applied at its sync point");
    CHECK_MESSAGE(rig->world().SyncEpoch() == epoch + 1 + 3, "[obs-exclusive-rounds] one sync epoch for the round");
}

TEST_CASE("J-59: ExclusiveWorld::Trigger stops at the hook round limit: HookRoundLimit, reported once, the World interrupted") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<Rang, &Retrigger>({.name = "retrigger", .zone = Zone::B});
        r.AddSystem<&RingNow>(B("RingNow", "A"));
    });
    rig->world().SetResource(KernelSettings{true, false, 2});
    const auto ran = rig->Run();
    CHECK_MESSAGE(HasCode(ran, KernelErrorCode::HookRoundLimit), "[obs-exclusive-limit] " << Text(ran));
    CHECK_MESSAGE(LinesStartingWith("rang:") == 3, "[obs-exclusive-limit] " << Joined());
    CHECK_MESSAGE(rig->recorded.size() == 1, "[obs-exclusive-limit] reported once");
    CHECK_MESSAGE(rig->world().State() == WorldState::Interrupted, "[obs-exclusive-limit] the World is interrupted");
}

TEST_CASE("J-59: a hook's ExclusiveWorld<> triggering now is HookChangesMembership; the World is interrupted") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Hook<Tag, &HookTriggersNow>(HookTrigger::OnAdd, {.name = "hook-triggers", .zone = Zone::B});
        r.Observer<Rang, &LogRang>({.name = "log-rang", .zone = Zone::B});
        r.AddSystem<&SpawnSecondTag>(B("SpawnSecondTag", "A"));
    });
    const auto ran = rig->Run();
    CHECK_MESSAGE(HasCode(ran, KernelErrorCode::HookChangesMembership), "[obs-hook-trigger-refused] " << Text(ran));
    CHECK_MESSAGE(LinesStartingWith("rang:") == 0, "[obs-hook-trigger-refused] no observer ran: " << Joined());
    CHECK_MESSAGE(rig->world().State() == WorldState::Interrupted, "[obs-hook-trigger-refused] interrupted");
}

TEST_CASE("J-59: ExclusiveWorld::Trigger inside a Read or Modify closure is ClosureChangesMembership; the World is interrupted") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<Rang, &LogRang>({.name = "log-rang", .zone = Zone::B});
        r.AddSystem<&TriggerInClosure>(B("TriggerInClosure", "A"));
    });
    rig->world().SetResource(Flag{1});
    const auto ran = rig->Run();
    CHECK_MESSAGE(HasCode(ran, KernelErrorCode::ClosureChangesMembership), "[obs-closure-trigger-refused] " << Text(ran));
    CHECK_MESSAGE(LinesStartingWith("rang:") == 0, "[obs-closure-trigger-refused] no observer ran: " << Joined());
}

TEST_CASE("J-59: an observer's Err stops the Run, named as the observer of its event") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &FailingObserver>({.name = "failing", .zone = Zone::B});
        r.AddSystem<&TriggerDoor>(B("TriggerDoor", "A"));
    });
    const auto ran = rig->Run();
    CHECK_MESSAGE(HasCode(ran, KernelErrorCode::SystemFailed), "[obs-err] " << Text(ran));
    CHECK_MESSAGE(Text(ran).find("observer 'failing' of event 'DoorOpened' at sync point step") != std::string::npos,
                  "[obs-err] " << Text(ran));
    CHECK_MESSAGE(rig->recorded.size() == 1, "[obs-err] reported once");
}

// ---- The order of the observers of one trigger

TEST_CASE("J-59: the observers of one trigger run in the same order for the same registrations, whatever the registration order") {
    std::vector<std::string> orders;
    for (const bool reversed : {false, true}) {
        Log().clear();
        auto rig = MakeRig([reversed](Registrar& r) {
            Data(r);
            Events(r);
            if (reversed) {
                r.Observer<DoorOpened, &LogDoorC>({.name = "c", .zone = Zone::B});
                r.Observer<DoorOpened, &LogDoorB>({.name = "b", .zone = Zone::B});
                r.Observer<DoorOpened, &LogDoorA>({.name = "a", .zone = Zone::B});
            } else {
                r.Observer<DoorOpened, &LogDoorB>({.name = "b", .zone = Zone::B});
                r.Observer<DoorOpened, &LogDoorA>({.name = "a", .zone = Zone::B});
                r.Observer<DoorOpened, &LogDoorC>({.name = "c", .zone = Zone::B});
            }
            r.AddSystem<&TriggerDoor>(B("TriggerDoor", "A"));
        });
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        orders.push_back(Joined());
    }
    // Deterministic (the same Registry twice gives the same order) and not the registration order.
    CHECK_MESSAGE(orders[0] == orders[1], "[obs-order] " << orders[0] << " / " << orders[1]);
    CHECK_MESSAGE(orders[0] == "a | b | c", "[obs-order] " << orders[0]);
}

// ---- An observer is a System

TEST_CASE("J-59: an observer is a message reader with a cursor of its own: it reads what was written since it last ran") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &ReadPingsOnDoor>({.name = "read-pings", .zone = Zone::B});
        r.AddSystem<&WritePing>(B("WritePing", "A"));
        r.AddSystem<&TriggerDoorOnOddRuns>(B("TriggerDoorOnOddRuns", "A", {}, {"WritePing"}));
    });
    for (int run = 0; run < 3; ++run) KERNEL_REQUIRE_RUN_OK(rig->Run());
    // Run 1 reads Ping 1; it does not run in Run 2; Run 3 reads 2 and 3 (its cursor waited, held back the record).
    CHECK_MESSAGE(Joined() == "obs-read: 1 | obs-read: 2 3", "[obs-reader] " << Joined());
    const auto& readers = rig->built.registry->Frozen().Messages()[*rig->built.registry->Frozen().MessageIdOf<Ping>()].readers;
    bool listed = false;
    for (const MessageReaderInfo& reader : readers) listed = listed || (reader.observer && reader.system == "read-pings");
    CHECK_MESSAGE(listed, "[obs-reader] the Registry lists the observer as a reader");
}

TEST_CASE("J-59: an observer's access is verified: a write it does not declare is reported, a declared one is not") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &WritesDeclared>({.name = "declared", .zone = Zone::B});
        r.Observer<DoorOpened, &WritesUndeclared>({.name = "undeclared", .zone = Zone::B});
        r.AddSystem<&TriggerDoor>(B("TriggerDoor", "A"));
    });
    rig->world().SetResource(Out{0});
    KeptWorld() = &rig->world();
    const auto ran = rig->Run();
    KeptWorld() = nullptr;
    CHECK_MESSAGE(HasCode(ran, KernelErrorCode::AccessViolation), "[obs-access] " << Text(ran));
    CHECK_MESSAGE(Text(ran).find("observer 'undeclared'") != std::string::npos, "[obs-access] " << Text(ran));
    CHECK_MESSAGE(Text(ran).find("observer 'declared'") == std::string::npos, "[obs-access] " << Text(ran));
}

// ---- Registration

TEST_CASE("J-59: the Registry lists the event types and the observers (trigger, id, access, owner)") {
    auto built = Finish({{"test", [](Registrar& r) {
                              Data(r);
                              Events(r);
                              r.Observer<Add<Mode>, &ObserveModeAddedQuiet>({.name = "obs+Mode", .zone = Zone::B});
                              r.Observer<DoorOpened, &WritesDeclared>({.name = "declared", .zone = Zone::B});
                          }}});
    KERNEL_REQUIRE_OK(built);
    const FrozenRegistry& frozen = built.registry->Frozen();
    REQUIRE(frozen.Events().size() == 2);
    CHECK_MESSAGE(frozen.Events()[0].name == "DoorOpened", "[obs-registry]");
    REQUIRE(frozen.Observers().size() == 2);
    const ObserverInfo& added = frozen.Observers()[0];
    CHECK_MESSAGE((added.trigger == ObserverTrigger::Add && added.id == *frozen.ComponentIdOf<Mode>() && added.owner == "test"),
                  "[obs-registry]");
    const ObserverInfo& declared = frozen.Observers()[1];
    CHECK_MESSAGE((declared.trigger == ObserverTrigger::Event && declared.id == *frozen.EventIdOf<DoorOpened>()), "[obs-registry]");
    const AccessKey out{AccessKind::Resource, *frozen.ResourceIdOf<Out>()};
    CHECK_MESSAGE(std::find(declared.access.writes.begin(), declared.access.writes.end(), out) != declared.access.writes.end(),
                  "[obs-registry] the observer's write is derived from its signature");
}

TEST_CASE("J-18: an observer changes the Registry fingerprint; a zone C observer the C fingerprint, which does not hold their order") {
    auto make = [](bool observer, bool zoneC, bool reversed) {
        return Finish({{"test", [=](Registrar& r) {
                            Data(r);
                            Events(r);
                            if (!observer) return;
                            const Zone zone = zoneC ? Zone::C : Zone::B;
                            if (reversed) {
                                r.Observer<DoorOpened, &LogDoorB>({.name = "b", .zone = zone});
                                r.Observer<DoorOpened, &LogDoorA>({.name = "a", .zone = zone});
                            } else {
                                r.Observer<DoorOpened, &LogDoorA>({.name = "a", .zone = zone});
                                r.Observer<DoorOpened, &LogDoorB>({.name = "b", .zone = zone});
                            }
                        }}});
    };
    auto none = make(false, false, false);
    auto zoneB = make(true, false, false);
    auto zoneC = make(true, true, false);
    auto zoneCReversed = make(true, true, true);
    KERNEL_REQUIRE_OK(none);
    KERNEL_REQUIRE_OK(zoneB);
    KERNEL_REQUIRE_OK(zoneC);
    KERNEL_REQUIRE_OK(zoneCReversed);
    const auto fp = [](const Built& b) { return b.registry->Frozen().RegistryFingerprint().Text(); };
    const auto cfp = [](const Built& b) { return b.registry->Frozen().CFingerprint().Text(); };
    CHECK_MESSAGE(fp(none) != fp(zoneB), "[obs-fingerprint] a zone B observer changes the Registry fingerprint");
    CHECK_MESSAGE(cfp(none) == cfp(zoneB), "[obs-fingerprint] a zone B observer is not in the C fingerprint");
    CHECK_MESSAGE(cfp(none) != cfp(zoneC), "[obs-fingerprint] a zone C observer is in the C fingerprint");
    CHECK_MESSAGE(cfp(zoneC) == cfp(zoneCReversed), "[obs-fingerprint] the C fingerprint holds no order of observers");
}

TEST_CASE("J-59: observer startup errors: a name twice, an unregistered event, zone C with Local, an Exclusive column's second writer") {
    auto twice = Finish({{"test", [](Registrar& r) {
                              Data(r);
                              Events(r);
                              r.Observer<DoorOpened, &LogDoorA>({.name = "same", .zone = Zone::B});
                              r.Observer<DoorOpened, &LogDoorB>({.name = "same", .zone = Zone::B});
                          }}});
    CHECK_MESSAGE(HasRule(twice, StartupCode::RE2, "B.1"), "[obs-startup-name] " << Describe(twice));
    auto unregistered = Finish({{"test", [](Registrar& r) {
                                     Data(r);
                                     r.Observer<DoorOpened, &LogDoorA>({.name = "a", .zone = Zone::B});
                                 }}});
    CHECK_MESSAGE(HasRule(unregistered, StartupCode::RE4, "R-6"), "[obs-startup-unregistered] " << Describe(unregistered));
    auto local = Finish({{"test", [](Registrar& r) {
                              Data(r);
                              Events(r);
                              r.Observer<DoorOpened, &ObserverWithLocal>({.name = "local", .zone = Zone::C});
                          }}});
    CHECK_MESSAGE(HasRule(local, StartupCode::DE7, "S-7"), "[obs-startup-zone] " << Describe(local));
    auto writers = Finish({{"test", [](Registrar& r) {
                                Data(r);
                                Events(r);
                                r.Observer<DoorOpened, &WritesMode>({.name = "writes-mode", .zone = Zone::B});
                                r.AddSystem<&AlsoWritesMode>(B("AlsoWritesMode", "A"));
                            }}});
    CHECK_MESSAGE(HasError(writers, StartupCode::DE2), "[obs-startup-writers] " << Describe(writers));
    auto unset = Finish({{"test", [](Registrar& r) {
                              r.RequireExplicitZones();
                              r.Event<DoorOpened>({.name = "DoorOpened"});
                              r.Observer<DoorOpened, &LogDoorA>({.name = "a"});
                          }}});
    CHECK_MESSAGE(ErrorCount(unset, StartupCode::DE7) == 2, "[obs-startup-zone-unset] " << Describe(unset));
}

TEST_CASE("J-59: a later round may not add an Add or Remove observer to a component the previous version has (R-E7)") {
    auto registry = std::make_unique<Registry>();
    auto first = RegisterAll(*registry, {{"test", [](Registrar& r) {
                                              Data(r);
                                              Events(r);
                                          }}});
    REQUIRE(static_cast<bool>(first));
    auto later = RegisterAll(*registry, {{"later", [](Registrar& r) {
                                              r.Observer<Add<Tag>, &ObserveTagAdded>({.name = "obs+Tag", .zone = Zone::B});
                                          }}});
    CHECK_MESSAGE(ErrorCount(later, StartupCode::RE7) == 1, "[obs-later-round] " << (later ? "ok" : later.error().detail));
    auto event = RegisterAll(*registry, {{"later", [](Registrar& r) {
                                              r.Observer<DoorOpened, &LogDoorA>({.name = "a", .zone = Zone::B});
                                          }}});
    CHECK_MESSAGE(static_cast<bool>(event), "[obs-later-round] an observer of an event type may come later");
}
