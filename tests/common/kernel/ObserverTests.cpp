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
struct Knock { std::uint32_t n; };          // event, the fingerprint cases only
struct Chirp { std::uint32_t n; };          // message, zone B, capacity 3

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
// Drops the trigger's Err: the World is interrupted all the same and the Run ends in it (reported once).
SystemResult RingNowIgnoring(ExclusiveWorld<> world) {
    (void)world.Trigger(Rang{0});
    return Ok();
}
// Logs the Mode cell's changed stamp the observer gave it, then writes it after the trigger (a new stamp).
SystemResult TouchModeOnDoor(On<DoorOpened> on, Query<Write<Mode>> modes) {
    if (Mut<Mode> mode = modes.GetMut<Mode>(*on.Target())) mode->m = 1;
    Log().push_back("obs-stamp:" + std::to_string(KeptWorld()->Stamps<Mode>(*on.Target())->second));
    return Ok();
}
SystemResult OpenDoorThenTouch(ExclusiveWorld<> world) {
    Log().push_back("own-stamp:" + std::to_string(KeptWorld()->Stamp()));
    auto done = world.Trigger(DoorOpened{1}, EntityId{8});
    if (!done) return done;
    auto touched = world.Modify<Mode>(8, [](Mode& mode) { mode.m = 2; });
    if (!touched) return Engine::Base::Err(touched.error());
    Log().push_back("after-stamp:" + std::to_string(KeptWorld()->Stamps<Mode>(8)->second));
    return Ok();
}
SystemResult ReadPings(MessageReader<Ping> pings) {
    std::string line = "sys-read:";
    for (const Ping* ping : pings.Read()) line += " " + std::to_string(ping->n);
    Log().push_back(line);
    return Ok();
}
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

// The limit's message names the observers of the last round only (M1, step 7): a converging chain whose
// observers record in hook rounds, and Retrigger, which never converges.
SystemResult TagOnMode(On<Add<Mode>> on, CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(*on.Target(), Tag{1});
    return Ok();
}
SystemResult DoorOnTag(On<Add<Tag>>, Commands commands) {
    commands.Trigger(DoorOpened{9});
    return Ok();
}
SystemResult RangOnTagRemoved(On<Remove<Tag>>, Commands commands) {
    commands.Trigger(Rang{0});
    return Ok();
}
// Run 1: DoorOpened on entity 1 (its chain converges in the hook rounds); Run 2: Rang (never converges).
SystemResult DoorThenRang(Commands commands, Res<TickCount> ticks) {
    if (ticks->value == 1) commands.Trigger(DoorOpened{0}, EntityId{1});
    if (ticks->value == 2) commands.Trigger(Rang{0});
    return Ok();
}
// Two immediate changes: the first one's chain converges in its rounds, the second one's never does.
SystemResult InsertThenRemoveNow(ExclusiveWorld<> world) {
    auto inserted = world.Insert(1, Mode{1});
    if (!inserted) return Engine::Base::Err(std::move(inserted).error());
    auto removed = world.Remove<Tag>(1);
    if (!removed) return Engine::Base::Err(std::move(removed).error());
    return Ok();
}

// ---- Step 7 (the adversarial check of the mutant list): what no test caught

SystemResult Idle(Local<std::uint32_t>) { return Ok(); }
bool FlagSet(Res<Flag> flag) { return flag->on != 0; }
SystemResult TriggerDoorFromRunTwo(Commands commands, Res<TickCount> ticks) {
    if (ticks->value >= 2) commands.Trigger(DoorOpened{0});
    return Ok();
}
SystemResult ChirpReader(MessageReader<Chirp> chirps) {
    std::string line = "chirps:";
    for (const Chirp* chirp : chirps.Read()) line += " " + std::to_string(chirp->n);
    Log().push_back(line);
    return Ok();
}
SystemResult WriteFiveChirpsOnce(MessageWriter<Chirp> chirps, Local<std::uint32_t> runs) {
    if (++*runs != 1) return Ok();
    for (std::uint32_t n = 1; n <= 5; ++n) chirps.Send(Chirp{n});
    return Ok();
}
SystemResult ChirpTwiceOnDoor(On<DoorOpened>, MessageWriter<Chirp> chirps) {
    chirps.Send(Chirp{100});
    chirps.Send(Chirp{101});
    return Ok();
}
SystemResult ReadsCRes(On<DoorOpened>, Res<CRes>) { return Ok(); }
SystemResult ReadsPos(On<DoorOpened>, Query<Read<Pos>>) { return Ok(); }
SystemResult LogChangedModes(On<DoorOpened>, Query<Read<Mode>, Changed<Mode>> changed) {
    std::string line = "changed:";
    for (EntityId id : changed.Entities()) line += " " + std::to_string(id);
    Log().push_back(line);
    return Ok();
}
SystemResult TouchModeTwoOnRunTwo(Query<Write<Mode>> modes, Res<TickCount> ticks) {
    if (ticks->value != 2) return Ok();
    if (Mut<Mode> mode = modes.GetMut<Mode>(2)) mode->m = 7;
    return Ok();
}
// Writes the target's Tag (its stamp is the observer's) and records a Mode insert (applied by the round).
SystemResult StampTagThenInsertMode(On<DoorOpened> on, Query<Write<Tag>> tags, CommandsFor<Inserts<Mode>, Removes<>> commands) {
    if (Mut<Tag> tag = tags.GetMut<Tag>(*on.Target())) tag->t = 2;
    commands.Insert(*on.Target(), Mode{1});
    return Ok();
}
// Drops the first trigger's Err (the World is interrupted) and triggers again.
SystemResult RingTwiceIgnoringFirst(ExclusiveWorld<> world) {
    (void)world.Trigger(Rang{0});
    auto second = world.Trigger(Rang{100});
    Log().push_back(second ? std::string("second ok") : "second " + std::string(ToString(second.error().code)));
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
    const auto triggered = rig->Trigger(DoorOpened{2}, EntityId{3});
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
    const auto triggered = rig->Trigger(Rang{0});
    CHECK_MESSAGE(HasCode(triggered, KernelErrorCode::HookRoundLimit), "[obs-host-limit] " << Text(triggered));
    // The trigger and three rounds ran; a fourth round is the error. (Each check prints its value: the mutants of
    // this case each expect their own, step 7.)
    CHECK_MESSAGE(LinesStartingWith("rang:") == 4, "[obs-host-limit-rounds] " << LinesStartingWith("rang:") << " | " << Joined());
    CHECK_MESSAGE(Text(triggered).find("observer 'retrigger'") != std::string::npos, "[obs-host-limit] " << Text(triggered));
    CHECK_MESSAGE(rig->recorded.size() == 1, "[obs-host-limit-once] reported " << rig->recorded.size());
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
        const auto triggered = rig->Trigger(DoorOpened{1});
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
        const auto triggered = rig->Trigger(DoorOpened{1});
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
        const auto triggered = rig->Trigger(DoorOpened{1}, EntityId{3});
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

TEST_CASE("J-59: the trigger's rounds use the schedule's declared hook round limit before the World's") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        ScheduleOptions limited;
        limited.name = "Limited";
        limited.stages = {"A"};
        limited.hookRoundLimit = 1;
        r.Schedule(limited);
        r.Observer<Rang, &Retrigger>({.name = "retrigger", .zone = Zone::B});
        r.AddSystem<&RingNow>(Sys("RingNow", Zone::B, {At("Limited", "A")}));
    });
    rig->world().SetResource(KernelSettings{true, false, 5});
    const auto ran = rig->Run("Limited");
    CHECK_MESSAGE(HasCode(ran, KernelErrorCode::HookRoundLimit), "[obs-exclusive-limit-schedule] " << Text(ran));
    CHECK_MESSAGE(LinesStartingWith("rang:") == 2, "[obs-exclusive-limit-schedule] " << Joined());
}

TEST_CASE("J-59: the hook rounds of a Run stop at the limit naming the observers of the last round, not those of earlier rounds or Runs") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &ModeOnDoor>({.name = "mode-on-door", .zone = Zone::B});
        r.Observer<Add<Mode>, &TagOnMode>({.name = "tag-on-mode", .zone = Zone::B});
        r.Observer<Add<Tag>, &DoorOnTag>({.name = "door-on-tag", .zone = Zone::B});
        r.Observer<Rang, &Retrigger>({.name = "retrigger", .zone = Zone::B});
        r.AddSystem<&DoorThenRang>(B("DoorThenRang", "C"));
    });
    rig->world().SetResource(KernelSettings{true, false, 3});
    rig->world().Insert(1, Pos{0});
    // Run 1: the last sync point delivers DoorOpened; its rounds apply Mode (tag-on-mode records), Tag
    // (door-on-tag records) and DoorOpened again (Mode is there: nothing more). Converges.
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Get<Tag>(1) != nullptr);
    // Run 2: Rang at the last sync point; retrigger records in every round.
    const auto ran = rig->Run();
    REQUIRE_MESSAGE(HasCode(ran, KernelErrorCode::HookRoundLimit), Text(ran));
    CHECK_MESSAGE(Text(ran).find("recorded in round 3 by observer 'retrigger'") != std::string::npos,
                  "[obs-run-limit-names] " << Text(ran));
    CHECK_MESSAGE((Text(ran).find("tag-on-mode") == std::string::npos && Text(ran).find("door-on-tag") == std::string::npos),
                  "[obs-run-limit-last-round] " << Text(ran));
}

TEST_CASE("J-59: the hook rounds of an immediate change stop at the limit naming the observers of its last round, not of an earlier change") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &LogDoor>({.name = "log-door", .zone = Zone::B});
        r.Observer<Add<Mode>, &TagOnMode>({.name = "tag-on-mode", .zone = Zone::B});
        r.Observer<Add<Tag>, &DoorOnTag>({.name = "door-on-tag", .zone = Zone::B});
        r.Observer<Remove<Tag>, &RangOnTagRemoved>({.name = "rang-on-untag", .zone = Zone::B});
        r.Observer<Rang, &Retrigger>({.name = "retrigger", .zone = Zone::B});
        r.AddSystem<&InsertThenRemoveNow>(B("InsertThenRemoveNow", "A"));
    });
    rig->world().SetResource(KernelSettings{true, false, 3});
    rig->world().Insert(1, Pos{0});
    // The insert's rounds: Tag (door-on-tag records in round 1), DoorOpened. The removal's: Rang, round after round.
    const auto ran = rig->Run();
    REQUIRE_MESSAGE(HasCode(ran, KernelErrorCode::HookRoundLimit), Text(ran));
    CHECK_MESSAGE(LinesStartingWith("door:9") == 1, "[obs-immediate-limit-names] the first change converged: " << Joined());
    CHECK_MESSAGE(Text(ran).find("recorded by observer 'retrigger'") != std::string::npos, "[obs-immediate-limit-names] " << Text(ran));
    CHECK_MESSAGE(Text(ran).find("door-on-tag") == std::string::npos, "[obs-immediate-limit-last-round] " << Text(ran));
}

TEST_CASE("J-59: an ExclusiveWorld::Trigger error the System drops interrupts the World and ends the Run in it, reported once") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<Rang, &Retrigger>({.name = "retrigger", .zone = Zone::B});
        r.AddSystem<&RingNowIgnoring>(B("RingNowIgnoring", "A"));
    });
    rig->world().SetResource(KernelSettings{true, false, 2});
    const auto ran = rig->Run();
    CHECK_MESSAGE(HasCode(ran, KernelErrorCode::HookRoundLimit), "[obs-exclusive-err-left] " << Text(ran));
    CHECK_MESSAGE(rig->recorded.size() == 1, "[obs-exclusive-err-left] reported once");
    CHECK_MESSAGE(rig->world().State() == WorldState::Interrupted, "[obs-exclusive-err-left] interrupted");
}

TEST_CASE("J-59: an observer of a trigger outside a sync point takes a stamp of its own; the exclusive System a new one after it") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &TouchModeOnDoor>({.name = "touch-mode", .zone = Zone::B});
        r.AddSystem<&OpenDoorThenTouch>(B("OpenDoorThenTouch", "A"));
    }, false);
    rig->world().Insert(8, Mode{0});
    KeptWorld() = &rig->world();
    const auto ran = rig->Run();
    KeptWorld() = nullptr;
    REQUIRE_MESSAGE(static_cast<bool>(ran), Text(ran));
    REQUIRE(Log().size() == 3);
    const auto stampIn = [](const std::string& line) { return std::stoull(line.substr(line.find(':') + 1)); };
    const std::uint64_t own = stampIn(Log()[0]);
    const std::uint64_t observer = stampIn(Log()[1]);
    const std::uint64_t after = stampIn(Log()[2]);
    CHECK_MESSAGE(observer > own, "[obs-trigger-stamp] the observer's stamp is its own: " << Joined());
    CHECK_MESSAGE(after > observer, "[obs-trigger-stamp-renewed] the System writes after with a new stamp: " << Joined());
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
        // Registered first: System 0 mount 0 is a reader of Ping too, whose cursor is not the observer's.
        r.AddSystem<&ReadPings>(B("ReadPings", "A", {}, {"WritePing"}));
        r.AddSystem<&WritePing>(B("WritePing", "A"));
        r.AddSystem<&TriggerDoorOnOddRuns>(B("TriggerDoorOnOddRuns", "A", {}, {"WritePing"}));
    });
    for (int run = 0; run < 3; ++run) KERNEL_REQUIRE_RUN_OK(rig->Run());
    // The observer: Run 1 reads Ping 1; it does not run in Run 2; Run 3 reads 2 and 3 (its cursor waited and
    // held the record back). The System reads each Run's.
    CHECK_MESSAGE(Joined() == "sys-read: 1 | obs-read: 1 | sys-read: 2 | sys-read: 3 | obs-read: 2 3", "[obs-reader] " << Joined());
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
    CHECK_MESSAGE(added.zone == Zone::B, "[obs-registry-zone]");
    CHECK_MESSAGE(added.source.find("ObserverTests.cpp") != std::string::npos, "[obs-registry-source] " << added.source);
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

// ---- Step 7: the cases the adversarial check of the mutant list found missing

TEST_CASE("J-59: removing a System (L5) keeps the cursor of the observer with the same index") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.AddSystem<&Idle>(B("Removable", "A"));  // System 0, removed below
        r.Observer<DoorOpened, &ReadPingsOnDoor>({.name = "read-pings", .zone = Zone::B});  // observer 0
        r.AddSystem<&WritePing>(B("WritePing", "A"));
        SystemOptions slow = B("Slow", "B");
        slow.mounts[0].conditions = {Condition<&FlagSet>("FlagSet")};
        r.AddSystem<&ReadPings>(std::move(slow));
        r.AddSystem<&TriggerDoorFromRunTwo>(B("TriggerDoorFromRunTwo", "A", {}, {"WritePing"}));
    });
    rig->world().SetResource(Flag{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // Ping 1: Slow reads it; the observer has not run
    rig->world().SetResource(Flag{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // Ping 2: Slow skipped holds it; the observer reads it
    REQUIRE(static_cast<bool>(rig->built.registry->Register(RegistrationOwner{"later", 9}, [](Registrar& r) {
        CHECK(r.RemoveSystem("Removable"));
    })));
    REQUIRE(static_cast<bool>(rig->built.registry->BuildPlans()));
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // Ping 3: the observer's cursor stayed, so 3 only (2 is still kept for Slow)
    CHECK_MESSAGE(Joined() == "sys-read: 1 | obs-read: 2 | obs-read: 3", "[obs-cursor-kept-on-removal] " << Joined());
}

TEST_CASE("J-59: the Host's Trigger records its own DroppedMessages, not into the last Run's") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Message<Chirp>({.name = "Chirp", .zone = Zone::B, .capacity = 3});
        r.AddSystem<&ChirpReader>(B("ChirpReader", "A"));
        r.AddSystem<&WriteFiveChirpsOnce>(B("WriteChirps", "B"));
        r.Observer<DoorOpened, &ChirpTwiceOnDoor>({.name = "chirp-on-door", .zone = Zone::B});
    });
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // five into three: 1 and 2 dropped; the reader holds 3 4 5
    const auto triggered = rig->Trigger(DoorOpened{1});
    REQUIRE_MESSAGE(static_cast<bool>(triggered), Text(triggered));  // 100 and 101: 3 and 4 dropped
    const std::vector<DroppedMessages> dropped = rig->world().TakeDiagnostics<DroppedMessages>();
    REQUIRE_MESSAGE(dropped.size() == 2, "[obs-host-trigger-dropped-record] " << dropped.size() << " records"
                                                                            << (dropped.empty() ? std::string() : " count " + std::to_string(dropped[0].count)));
    CHECK_MESSAGE((dropped[1].count == 2 && dropped[1].firstSeq == 3 && dropped[1].lastSeq == 4),
                  "[obs-host-trigger-dropped-record] " << dropped[1].count << " " << dropped[1].firstSeq << ".." << dropped[1].lastSeq);
}

TEST_CASE("J-18: the event types, an observer's zone and its access are in the Registry fingerprint; zone C ones in the C fingerprint") {
    auto make = [](int variant) {
        return Finish({{"test", [variant](Registrar& r) {
                            Data(r);
                            Events(r);
                            switch (variant) {
                            case 1: r.Event<Knock>({.name = "Knock", .zone = Zone::B}); break;
                            case 2: r.Event<Knock>({.name = "Knock", .zone = Zone::C}); break;
                            case 3: r.Observer<DoorOpened, &LogDoorA>({.name = "o", .zone = Zone::B}); break;
                            case 4: r.Observer<DoorOpened, &LogDoorA>({.name = "o", .zone = Zone::C}); break;
                            case 5: r.Observer<DoorOpened, &WritesDeclared>({.name = "o", .zone = Zone::B}); break;
                            case 6: r.Observer<DoorOpened, &ReadsCRes>({.name = "o", .zone = Zone::C}); break;
                            case 7: r.Observer<DoorOpened, &ReadsPos>({.name = "o", .zone = Zone::C}); break;
                            default: break;
                            }
                        }}});
    };
    std::vector<Built> built;
    for (int variant = 0; variant <= 7; ++variant) {
        built.push_back(make(variant));
        KERNEL_REQUIRE_OK(built.back());
    }
    const auto fp = [&](int v) { return built[v].registry->Frozen().RegistryFingerprint().Text(); };
    const auto cfp = [&](int v) { return built[v].registry->Frozen().CFingerprint().Text(); };
    CHECK_MESSAGE(fp(0) != fp(1), "[obs-fp-events] an event type is not in the Registry fingerprint");
    CHECK(cfp(0) == cfp(1));  // a zone B event type is not in the C fingerprint
    CHECK_MESSAGE(cfp(0) != cfp(2), "[obs-cfp-events] a zone C event type is not in the C fingerprint");
    CHECK_MESSAGE(fp(3) != fp(4), "[obs-fp-observer-zone] an observer's zone is not in the Registry fingerprint");
    CHECK_MESSAGE(fp(3) != fp(5), "[obs-fp-observer-access] an observer's access is not in the Registry fingerprint");
    CHECK_MESSAGE(cfp(6) != cfp(7), "[obs-cfp-observer-access] a zone C observer's access is not in the C fingerprint");
}

TEST_CASE("J-59: a zone C observer's access follows the zone rules: a write of zone B data is a startup error (DI-KS 8)") {
    auto built = Finish({{"test", [](Registrar& r) {
                              Data(r);
                              Events(r);
                              r.Observer<DoorOpened, &WritesDeclared>({.name = "c-writes-b", .zone = Zone::C});
                          }}});
    CHECK_MESSAGE(!built.result, "[obs-zone-access] " << Describe(built));
    CHECK_MESSAGE(Describe(built).find("c-writes-b") != std::string::npos, "[obs-zone-access] " << Describe(built));
}

TEST_CASE("J-59: an observer's Changed<T> sees what changed since the observer last ran") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &LogChangedModes>({.name = "changed-modes", .zone = Zone::B});
        r.AddSystem<&TouchModeTwoOnRunTwo>(B("TouchModeTwoOnRunTwo", "A"));
        r.AddSystem<&TriggerDoor>(B("TriggerDoor", "B"));
    });
    rig->world().Insert(1, Mode{0});
    rig->world().Insert(2, Mode{0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "changed: 1 2 | changed: 2", "[obs-last-run] " << Joined());
}

TEST_CASE("J-59: each round of the Host's Trigger takes a new stamp: the round's writes are newer than the observer's") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<DoorOpened, &StampTagThenInsertMode>({.name = "stamp-tag", .zone = Zone::B});
    });
    rig->world().Insert(3, Tag{0});
    const auto triggered = rig->Trigger(DoorOpened{2}, EntityId{3});
    REQUIRE_MESSAGE(static_cast<bool>(triggered), Text(triggered));
    const auto tag = rig->world().Stamps<Tag>(3);
    const auto mode = rig->world().Stamps<Mode>(3);
    REQUIRE((tag.has_value() && mode.has_value()));
    CHECK_MESSAGE(mode->second > tag->second, "[obs-round-new-stamp] observer " << tag->second << " round " << mode->second);
}

TEST_CASE("J-59: a second ExclusiveWorld::Trigger after a dropped Err returns that Err again and runs no observer") {
    Log().clear();
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        Events(r);
        r.Observer<Rang, &Retrigger>({.name = "retrigger", .zone = Zone::B});
        r.AddSystem<&RingTwiceIgnoringFirst>(B("RingTwiceIgnoringFirst", "A"));
    });
    rig->world().SetResource(KernelSettings{true, false, 2});
    const auto ran = rig->Run();
    CHECK(HasCode(ran, KernelErrorCode::HookRoundLimit));
    CHECK_MESSAGE((LinesStartingWith("rang:100") == 0 && Log().back() == "second HookRoundLimit"), "[obs-trigger-interrupted] " << Joined());
}
