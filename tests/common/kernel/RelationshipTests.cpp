// Relationships: ChildOf and Children (kernel DI-KS 29 to 33; declaration
// docs/architecture/target/v2/kernel-relationship-declaration.zh-Hant.md,
// approved by the user 2026-11-23: Q1 (A') attach order, Q2 kept in the apply
// steps, Q3 a rewired child is not despawned with its parent, Q4 depth first).
// Each case's tag in brackets is the declaration's section 12.1 tag; the
// mutants of section 12.3 expect these tags.
//
// Reads across entities (a child's event reading its parent's Children) go
// through observers, which are Systems; a hook reads only its own entity
// (spec F.4).

#include "WorldTestSupport.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace GYO::Kernel;
using GYO::Test::TestRangeAllotter;
using GYO::Test::Kernel::At;
using GYO::Test::Kernel::Finish;
using GYO::Test::Kernel::Log;
using GYO::Test::Kernel::MakeRig;
using GYO::Test::Kernel::Ok;
using GYO::Test::Kernel::Rig;
using GYO::Test::Kernel::Sys;
using GYO::Test::Kernel::TickCount;

namespace {

struct Item {
    std::int32_t v;
    friend bool operator==(const Item&, const Item&) = default;
};
struct Mark { std::int32_t from; };      // an initial value from ChildOf
struct Crowd { std::int32_t first; };    // an initial value from Children
inline void HashValue(Hasher& h, const Item& v) { HashValue(h, v.v); }
inline void HashValue(Hasher& h, const Mark& v) { HashValue(h, v.from); }
inline void HashValue(Hasher& h, const Crowd& v) { HashValue(h, v.first); }

Mark MarkFromChildOf(const ChildOf& c) { return Mark{static_cast<std::int32_t>(c.parent)}; }
Crowd CrowdFromChildren(const Children& c) { return Crowd{static_cast<std::int32_t>(c[0])}; }
ChildOf ChildOfFromMark(const Mark& m) { return ChildOf{static_cast<EntityId>(m.from)}; }

// What a driver System does on one Run.
struct Act {
    Commands& commands;
    SpawnWithId<Bundle<Item>>& spawn;
    SpawnWithId<Bundle<ChildOf>>& spawnChild;
    Spawn<Bundle<Item>>& spawnFresh;
    Despawn& despawn;
};
using Script = std::map<std::uint64_t, std::function<void(Act&)>>;
Script& Early() { static Script s; return s; }    // stage A
Script& Second() { static Script s; return s; }   // stage A, after Early's System
Script& Late() { static Script s; return s; }     // stage C (the last sync point)
using ExclusiveScript = std::map<std::uint64_t, std::function<SystemResult(ExclusiveWorld<>&)>>;
ExclusiveScript& Exclusive() { static ExclusiveScript s; return s; }  // stage B

SystemResult Play(Script& script, std::uint64_t tick, Act& act) {
    const auto found = script.find(tick);
    if (found != script.end()) found->second(act);
    return Ok();
}

SystemResult DriveEarly(Commands commands, SpawnWithId<Bundle<Item>> spawn, SpawnWithId<Bundle<ChildOf>> spawnChild,
                        Spawn<Bundle<Item>> spawnFresh, Despawn despawn, Res<TickCount> tick) {
    Act act{commands, spawn, spawnChild, spawnFresh, despawn};
    return Play(Early(), tick->value, act);
}
SystemResult DriveSecond(Commands commands, SpawnWithId<Bundle<Item>> spawn, SpawnWithId<Bundle<ChildOf>> spawnChild,
                         Spawn<Bundle<Item>> spawnFresh, Despawn despawn, Res<TickCount> tick) {
    Act act{commands, spawn, spawnChild, spawnFresh, despawn};
    return Play(Second(), tick->value, act);
}
SystemResult DriveLate(Commands commands, SpawnWithId<Bundle<Item>> spawn, SpawnWithId<Bundle<ChildOf>> spawnChild,
                       Spawn<Bundle<Item>> spawnFresh, Despawn despawn, Res<TickCount> tick) {
    Act act{commands, spawn, spawnChild, spawnFresh, despawn};
    return Play(Late(), tick->value, act);
}
SystemResult DriveExclusive(ExclusiveWorld<> world) {
    std::uint64_t tick = 0;
    (void)world.Read<TickCount>([&](const TickCount& t) { tick = t.value; });
    const auto found = Exclusive().find(tick);
    if (found == Exclusive().end()) return Ok();
    return found->second(world);
}

ScheduleOptions Tick() {
    ScheduleOptions s;
    s.name = "Tick";
    s.stages = {"A", "B", "C"};
    return s;
}

SystemOptions In(std::string name, std::string stage, std::vector<std::string> after = {}) {
    return Sys(std::move(name), {At("Tick", std::move(stage), {}, std::move(after))});
}

void Data(Registrar& r) {
    r.Component<Item>({.name = "Item"});
    r.Resource<TestRangeAllotter>({.name = "Allotter"});
    r.Allotter<TestRangeAllotter>();
    r.Resource<TickCount>({.name = "TickCount"});
    r.Schedule(Tick());
    r.AddSystem<&DriveEarly>(In("DriveEarly", "A"));
    r.AddSystem<&DriveSecond>(In("DriveSecond", "A", {"DriveEarly"}));
    r.AddSystem<&DriveExclusive>(In("DriveExclusive", "B"));
    r.AddSystem<&DriveLate>(In("DriveLate", "C"));
}

std::unique_ptr<Rig> Start(std::function<void(Registrar&)> more = {}) {
    Early().clear();
    Second().clear();
    Late().clear();
    Exclusive().clear();
    Log().clear();
    auto rig = MakeRig([more](Registrar& r) {
        Data(r);
        if (more) more(r);
    });
    if (rig->executor) rig->world().SetResource(TestRangeAllotter::Of(1000, 1999));
    return rig;
}

std::vector<EntityId> Kids(World& world, EntityId parent) {
    const Children* children = world.Get<Children>(parent);
    if (children == nullptr) return {};
    return {children->begin(), children->end()};
}

std::optional<EntityId> ParentOf(World& world, EntityId child) {
    const ChildOf* of = world.Get<ChildOf>(child);
    if (of == nullptr) return std::nullopt;
    return of->parent;
}

void Items(Act& act, std::initializer_list<EntityId> ids) {
    for (EntityId id : ids) act.spawn.New(id, Item{static_cast<std::int32_t>(id)});
}

std::string Joined() {
    std::string out;
    for (const std::string& entry : Log()) out += (out.empty() ? "" : " ") + entry;
    return out;
}

bool Logged(const std::string& entry) {
    for (const std::string& seen : Log()) {
        if (seen == entry) return true;
    }
    return false;
}

// Observers and hooks that log (a hook reads only its own entity).
SystemResult ObserveChildAdded(On<Add<ChildOf>> on, Query<Read<ChildOf>> of, Query<Read<Children>> children) {
    const EntityId child = *on.Target();
    const EntityId parent = of.Get<ChildOf>(child)->parent;
    const Children* list = children.Get<Children>(parent);
    Log().push_back("childof+:" + std::to_string(child) + (list != nullptr && list->Contains(child) ? ":listed" : ":unlisted"));
    return Ok();
}
SystemResult ObserveChildRemoved(On<Remove<ChildOf>> on, Query<Read<ChildOf>> of, Query<Read<Children>> children) {
    const EntityId child = *on.Target();
    const EntityId parent = of.Get<ChildOf>(child)->parent;
    const Children* list = children.Get<Children>(parent);
    Log().push_back("childof-:" + std::to_string(child) + ">" + std::to_string(parent) +
                    (list != nullptr && list->Contains(child) ? ":listed" : ":unlisted"));
    return Ok();
}
SystemResult ChildrenAdded(HookCtx ctx, Query<Read<Children>> children) {
    Log().push_back("children+:" + std::to_string(ctx.Entity()) + ":" + std::to_string(children.Get<Children>(ctx.Entity())->size()));
    return Ok();
}
SystemResult ChildrenRemoved(HookCtx ctx, Query<Read<Children>> children) {
    Log().push_back("children-:" + std::to_string(ctx.Entity()) + ":" + std::to_string(children.Get<Children>(ctx.Entity())->size()));
    return Ok();
}
SystemResult ObserveChildrenAdded(On<Add<Children>> on) {
    Log().push_back("obs-children+:" + std::to_string(*on.Target()));
    return Ok();
}
SystemResult ObserveChildrenRemoved(On<Remove<Children>> on) {
    Log().push_back("obs-children-:" + std::to_string(*on.Target()));
    return Ok();
}
SystemResult ChildOfAdded(HookCtx ctx) {
    Log().push_back("hook-childof+:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult ItemRemoved(HookCtx ctx) {
    Log().push_back("rm:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult WatchChanges(Query<Read<ChildOf>, Changed<ChildOf>> of, Query<Read<Children>, Changed<Children>> children) {
    of.ForEach([](EntityId id, const ChildOf&) { Log().push_back("childof~:" + std::to_string(id)); });
    children.ForEach([](EntityId id, const Children&) { Log().push_back("children~:" + std::to_string(id)); });
    return Ok();
}
// A hook that attaches the entity it sees (its record waits for the next sync point, or the hook rounds).
SystemResult AttachTo10(HookCtx ctx, Commands commands) {
    commands.Insert(ctx.Entity(), ChildOf{10});
    return Ok();
}
SystemResult ReadKidsOf10(Query<Read<Children>> children) {
    const Children* list = children.Get<Children>(10);
    Log().push_back("C-sees:" + std::to_string(list == nullptr ? 0U : list->size()));
    return Ok();
}

// The whole state of a World's relationships, read by an exclusive System (for the restore cases).
struct Saved {
    std::vector<EntityId> alive;
    std::vector<std::pair<EntityId, Item>> items;
    std::vector<std::pair<EntityId, ChildOf>> childOf;
    std::vector<std::pair<EntityId, Children>> children;
};
Saved& Snapshot() {
    static Saved saved;
    return saved;
}
SystemResult Save(ExclusiveWorld<>& world) {
    Saved saved;
    saved.alive = world.Entities();
    world.ReadColumn<Item>([&](EntityId id, ChangeStamp, ChangeStamp, const Item& v) { saved.items.emplace_back(id, v); });
    world.ReadColumn<ChildOf>([&](EntityId id, ChangeStamp, ChangeStamp, const ChildOf& v) { saved.childOf.emplace_back(id, v); });
    world.ReadColumn<Children>([&](EntityId id, ChangeStamp, ChangeStamp, const Children& v) { saved.children.emplace_back(id, v); });
    Snapshot() = std::move(saved);
    return Ok();
}
// targets: whether the set names Children; childOf: whether it names ChildOf.
SystemResult PutBack(ExclusiveWorld<>& world, bool childOf, bool targets) {
    RestoreSet set;
    set.Alive(Snapshot().alive);
    set.Column<Item>(Snapshot().items);
    if (childOf) set.Column<ChildOf>(Snapshot().childOf);
    if (targets) set.Column<Children>(Snapshot().children);
    return world.Restore(std::move(set));
}

std::string Summary(World& world) {
    auto summary = world.Summary();
    REQUIRE(static_cast<bool>(summary));
    return summary->Text();
}

} // namespace

// ---- The shape and the upkeep (DI-KS 29, 30)

TEST_CASE("relationship: a spawned child is in its parent's Children at the same sync point [kr-spawn]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    World& world = rig->world();
    CHECK(Kids(world, 10) == std::vector<EntityId>{20});
    CHECK(ParentOf(world, 20) == 10);
    CHECK(world.Get<Children>(20) == nullptr);
    CHECK(world.RejectedRelationships() == 0U);
}

TEST_CASE("relationship: a child with a smaller id than its parent spawned in the same phase holds [kr-same-phase-small-id]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {50});
        a.spawnChild.New(5, ChildOf{50});
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(rig->world(), 50) == std::vector<EntityId>{5});
    CHECK(rig->world().RejectedRelationships() == 0U);
}

TEST_CASE("relationship: a new ChildOf moves the child to the end of the new parent's Children [kr-reparent]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10, 11, 12});
        a.commands.Insert(12, ChildOf{11});
        a.spawnChild.New(20, ChildOf{10});
        a.spawnChild.New(21, ChildOf{10});
    };
    Early()[2] = [](Act& a) { a.commands.Insert(20, ChildOf{11}); };
    Early()[3] = [](Act& a) { a.commands.Insert(21, ChildOf{11}); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    World& world = rig->world();
    CHECK(Kids(world, 11) == std::vector<EntityId>{12});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(world, 10) == std::vector<EntityId>{21});
    CHECK(Kids(world, 11) == (std::vector<EntityId>{12, 20}));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(world.Get<Children>(10) == nullptr, "a parent left with no child has no Children cell");
    CHECK(Kids(world, 11) == (std::vector<EntityId>{12, 20, 21}));
}

TEST_CASE("relationship: removing ChildOf, or despawning the child, takes it out of Children [kr-detach] [kr-child-despawn]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
        a.spawnChild.New(21, ChildOf{10});
    };
    Early()[2] = [](Act& a) { a.commands.Remove<ChildOf>(20); };
    Early()[3] = [](Act& a) { a.despawn.Entity(21); };
    World& world = rig->world();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(world, 10) == std::vector<EntityId>{21});
    CHECK(world.Alive(20));
    CHECK(ParentOf(world, 20) == std::nullopt);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(world.Get<Children>(10) == nullptr);
}

TEST_CASE("relationship: Children follows the attach order, records in one phase by record order [kr-order]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {5, 10});
        // Recorded 31 first, then 30: the record order, not the id order.
        a.spawnChild.New(31, ChildOf{10});
        a.spawnChild.New(30, ChildOf{10});
    };
    Early()[2] = [](Act& a) { a.spawnChild.New(25, ChildOf{10}); };
    Early()[3] = [](Act& a) { a.commands.Insert(5, ChildOf{10}); };  // an older entity, attached last
    World& world = rig->world();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(world, 10) == (std::vector<EntityId>{31, 30}));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(world, 10) == (std::vector<EntityId>{31, 30, 25, 5}));
}

TEST_CASE("relationship: two Systems attaching in one phase follow the Plan's order [kr-order-two-systems]") {
    auto rig = Start();
    Early()[1] = [](Act& a) { Items(a, {10}); };
    // DriveSecond is declared after DriveEarly.
    Second()[2] = [](Act& a) { a.spawnChild.New(20, ChildOf{10}); };
    Early()[2] = [](Act& a) { a.spawnChild.New(30, ChildOf{10}); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(rig->world(), 10) == (std::vector<EntityId>{30, 20}));
}

TEST_CASE("relationship: an Add<ChildOf> observer sees the child listed; a Remove<ChildOf> observer still sees it [kr-observer-add] [kr-observer-remove]") {
    auto rig = Start([](Registrar& r) {
        r.Observer<Add<ChildOf>, &ObserveChildAdded>({.name = "obs+ChildOf"});
        r.Observer<Remove<ChildOf>, &ObserveChildRemoved>({.name = "obs-ChildOf"});
    });
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
        a.spawnChild.New(21, ChildOf{10});
    };
    Early()[2] = [](Act& a) { a.commands.Remove<ChildOf>(20); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Logged("childof+:20:listed"));
    CHECK(Logged("childof+:21:listed"));
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Joined() == "childof-:20>10:listed");
}

TEST_CASE("relationship: Children's own OnAdd and OnRemove, and its Add and Remove observers [kr-children-hooks] [kr-children-observers]") {
    auto rig = Start([](Registrar& r) {
        r.Hook<Children, &ChildrenAdded>(HookTrigger::OnAdd, {.name = "ChildrenAdded"});
        r.Hook<Children, &ChildrenRemoved>(HookTrigger::OnRemove, {.name = "ChildrenRemoved"});
        r.Observer<Add<Children>, &ObserveChildrenAdded>({.name = "obs+Children"});
        r.Observer<Remove<Children>, &ObserveChildrenRemoved>({.name = "obs-Children"});
    });
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
        a.spawnChild.New(21, ChildOf{10});
    };
    Early()[2] = [](Act& a) { a.commands.Remove<ChildOf>(20); };
    Early()[3] = [](Act& a) { a.commands.Remove<ChildOf>(21); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Joined() == "children+:10:2 obs-children+:10");
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Joined().empty());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_MESSAGE(Joined() == "obs-children-:10 children-:10:0",
                  "the OnRemove of the last child's leaving sees an empty Children (declaration 5.3, exception 4)");
}

TEST_CASE("relationship: a parent that loses one child and gains another in one phase is not notified, whatever the ids [kr-net-diff]") {
    for (const EntityId newcomer : {EntityId{30}, EntityId{15}}) {
        CAPTURE(newcomer);
        auto rig = Start([](Registrar& r) {
            r.Hook<Children, &ChildrenAdded>(HookTrigger::OnAdd, {.name = "ChildrenAdded"});
            r.Hook<Children, &ChildrenRemoved>(HookTrigger::OnRemove, {.name = "ChildrenRemoved"});
        });
        Early()[1] = [](Act& a) {
            Items(a, {10});
            a.spawnChild.New(20, ChildOf{10});
        };
        Early()[2] = [newcomer](Act& a) {
            a.commands.Remove<ChildOf>(20);
            a.spawnChild.New(newcomer, ChildOf{10});
        };
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(Joined().empty());
        CHECK(Kids(rig->world(), 10) == std::vector<EntityId>{newcomer});
    }
}

TEST_CASE("relationship: the same parent again writes ChildOf and leaves Children unchanged [kr-children-changed]") {
    auto rig = Start([](Registrar& r) { r.AddSystem<&WatchChanges>(In("WatchChanges", "C")); });
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
    };
    Early()[2] = [](Act& a) { a.commands.Insert(20, ChildOf{10}); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Joined() == "childof~:20 children~:10");
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Joined() == "childof~:20");
    CHECK(Kids(rig->world(), 10) == std::vector<EntityId>{20});
}

TEST_CASE("relationship: an initial value from ChildOf rests only on a ChildOf that holds [kr-initial-from-childof]") {
    auto rig = Start([](Registrar& r) {
        r.Component<Mark>({.name = "Mark"});
        r.InitialValue<Mark, &MarkFromChildOf>();
    });
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
        a.spawnChild.New(21, ChildOf{99});  // 99 is not alive
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    World& world = rig->world();
    REQUIRE(world.Get<Mark>(20) != nullptr);
    CHECK(world.Get<Mark>(20)->from == 10);
    CHECK(world.Get<Mark>(21) == nullptr);
}

TEST_CASE("relationship: an initial value from Children comes at the sync point of the first child [kr-initial-from-children]") {
    auto rig = Start([](Registrar& r) {
        r.Component<Crowd>({.name = "Crowd"});
        r.InitialValue<Crowd, &CrowdFromChildren>();
    });
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Get<Crowd>(10) != nullptr);
    CHECK(rig->world().Get<Crowd>(10)->first == 20);
}

// ---- Relationships that do not hold (DI-KS 31)

TEST_CASE("relationship: a parent that is not alive, or the entity itself, does not hold, with every field recorded [kr-dead-target] [kr-self]") {
    auto rig = Start();
    Early()[1] = [](Act& a) { Items(a, {20, 21}); };
    Early()[2] = [](Act& a) {
        a.commands.Insert(20, ChildOf{99});
        a.commands.Insert(21, ChildOf{21});
    };
    World& world = rig->world();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const std::uint64_t epoch = world.SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(ParentOf(world, 20) == std::nullopt);
    CHECK(ParentOf(world, 21) == std::nullopt);
    CHECK(world.RejectedRelationships() == 2U);
    const std::vector<RejectedRelationship> rejected = world.Diagnostics<RejectedRelationship>();
    REQUIRE(rejected.size() == 2U);
    const RejectedRelationship& dead = rejected[0];
    CHECK(dead.entity == 20U);
    CHECK(dead.target == 99U);
    CHECK(dead.reason == RelationshipRejectReason::TargetNotAlive);
    CHECK(dead.relationship == "ChildOf");
    CHECK(dead.emitter == "DriveEarly");
    CHECK_FALSE(dead.emitterIsHook);
    CHECK(dead.schedule == "Tick");
    CHECK(dead.step < dead.appliedAt);
    CHECK(dead.syncEpoch == epoch);
    CHECK(dead.path == RelationshipPath::SyncPoint);
    const RejectedRelationship& self = rejected[1];
    CHECK(self.entity == 21U);
    CHECK(self.target == 21U);
    CHECK(self.reason == RelationshipRejectReason::SelfReference);
}

TEST_CASE("relationship: a spawned ChildOf whose parent is not alive goes without a hook [kr-spawn-dead-target]") {
    auto rig = Start([](Registrar& r) {
        r.Hook<ChildOf, &ChildOfAdded>(HookTrigger::OnAdd, {.name = "ChildOfAdded"});
        r.Observer<Remove<ChildOf>, &ObserveChildRemoved>({.name = "obs-ChildOf"});
    });
    Early()[1] = [](Act& a) { a.spawnChild.New(20, ChildOf{99}); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    World& world = rig->world();
    CHECK(world.Alive(20));
    CHECK(ParentOf(world, 20) == std::nullopt);
    CHECK(Joined().empty());
    CHECK(world.RejectedRelationships() == 1U);
}

TEST_CASE("relationship: a child given a parent that is not alive loses its ChildOf, read through its old value [kr-reparent-dead]") {
    auto rig = Start([](Registrar& r) { r.Observer<Remove<ChildOf>, &ObserveChildRemoved>({.name = "obs-ChildOf"}); });
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
    };
    Early()[2] = [](Act& a) { a.commands.Insert(20, ChildOf{99}); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    World& world = rig->world();
    CHECK(ParentOf(world, 20) == std::nullopt);
    CHECK(world.Get<Children>(10) == nullptr);
    CHECK(Joined() == "childof-:20>10:listed");
}

// ---- Linked despawn (DI-KS 32)

TEST_CASE("relationship: despawning a parent despawns its children and grandchildren [kr-linked]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10, 30});
        a.spawnChild.New(20, ChildOf{10});
        a.commands.Insert(30, ChildOf{20});
    };
    Early()[2] = [](Act& a) { a.despawn.Entity(10); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(ParentOf(rig->world(), 30) == 20);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_FALSE(rig->world().Alive(10));
    CHECK_FALSE(rig->world().Alive(20));
    CHECK_FALSE(rig->world().Alive(30));
}

TEST_CASE("relationship: a chain deeper than the hook round limit is despawned in one sync point [kr-linked-deep]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {100});
        for (EntityId id = 101; id <= 120; ++id) a.spawnChild.New(id, ChildOf{id - 1});
    };
    Early()[2] = [](Act& a) { a.despawn.Entity(100); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(ParentOf(rig->world(), 120) == 119);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    for (EntityId id = 100; id <= 120; ++id) CHECK_FALSE(rig->world().Alive(id));
}

TEST_CASE("relationship: a parent, then each child's whole subtree, depth first; explicit despawns by id [kr-linked-order]") {
    auto rig = Start([](Registrar& r) { r.Hook<Item, &ItemRemoved>(HookTrigger::OnRemove, {.name = "ItemRemoved"}); });
    // P 50 with children B 30 then A 10 (attach order, not id order); A1 40 under A, B1 20 under B; E 25 unrelated.
    Early()[1] = [](Act& a) { Items(a, {50, 10, 30, 40, 20, 25}); };
    Early()[2] = [](Act& a) {
        a.commands.Insert(30, ChildOf{50});
        a.commands.Insert(10, ChildOf{50});
        a.commands.Insert(40, ChildOf{10});
        a.commands.Insert(20, ChildOf{30});
    };
    Early()[3] = [](Act& a) {
        a.despawn.Entity(50);
        a.despawn.Entity(25);
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(Kids(rig->world(), 50) == (std::vector<EntityId>{30, 10}));
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Joined() == "rm:25 rm:50 rm:30 rm:20 rm:10 rm:40");
}

TEST_CASE("relationship: despawning one entity of a cycle ends and despawns both [kr-linked-cycle]") {
    auto rig = Start();
    Early()[1] = [](Act& a) { Items(a, {10, 11}); };
    Early()[2] = [](Act& a) {
        a.commands.Insert(10, ChildOf{11});
        a.commands.Insert(11, ChildOf{10});
    };
    Early()[3] = [](Act& a) { a.despawn.Entity(10); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(ParentOf(rig->world(), 10) == 11);
    REQUIRE(ParentOf(rig->world(), 11) == 10);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_FALSE(rig->world().Alive(10));
    CHECK_FALSE(rig->world().Alive(11));
}

TEST_CASE("relationship: a weapon dropped in the phase its owner dies stays [kr-detach-then-despawn]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
    };
    Early()[2] = [](Act& a) {
        a.commands.Remove<ChildOf>(20);
        a.despawn.Entity(10);
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_FALSE(rig->world().Alive(10));
    CHECK(rig->world().Alive(20));
    CHECK(ParentOf(rig->world(), 20) == std::nullopt);
}

TEST_CASE("relationship: a weapon handed over in the phase its owner dies goes to the new owner [kr-reparent-then-despawn]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10, 11});
        a.spawnChild.New(20, ChildOf{10});
    };
    Early()[2] = [](Act& a) {
        a.despawn.Entity(10);
        a.commands.Insert(20, ChildOf{11});
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().Alive(20));
    CHECK(Kids(rig->world(), 11) == std::vector<EntityId>{20});
}

TEST_CASE("relationship: a child attached to a parent despawned in the same phase lives without a parent [kr-despawn-and-attach]") {
    auto rig = Start();
    Early()[1] = [](Act& a) { Items(a, {10}); };
    Early()[2] = [](Act& a) {
        a.despawn.Entity(10);
        a.spawnChild.New(20, ChildOf{10});
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().Alive(20));
    CHECK(ParentOf(rig->world(), 20) == std::nullopt);
    CHECK(rig->world().RejectedRelationships() == 1U);
}

// ---- Every application keeps them (DI-KS 30)

TEST_CASE("relationship: a hook's ChildOf is applied at step 0 of the next sync point [kr-hook-queue-phase]") {
    auto rig = Start([](Registrar& r) {
        r.Hook<Item, &AttachTo10>(HookTrigger::OnAdd, {.name = "AttachTo10"});
        r.AddSystem<&ReadKidsOf10>(In("ReadKidsOf10", "C"));
    });
    // 10 gets an Item on the Host's path (no hook); 30 at stage A's sync point (its hook records the attach).
    rig->world().Insert(10, Item{10});
    Early()[1] = [](Act& a) { Items(a, {30}); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(rig->world(), 10) == std::vector<EntityId>{30});
    CHECK(Logged("C-sees:1"));
}

TEST_CASE("relationship: a hook's ChildOf at the last sync point is applied by the hook rounds [kr-hook-round]") {
    auto rig = Start([](Registrar& r) { r.Hook<Item, &AttachTo10>(HookTrigger::OnAdd, {.name = "AttachTo10"}); });
    rig->world().Insert(10, Item{10});
    Late()[1] = [](Act& a) { Items(a, {40}); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(rig->world(), 10) == std::vector<EntityId>{40});
}

TEST_CASE("relationship: an immediate insert of ChildOf is kept before it returns; one that does not hold returns false [kr-immediate]") {
    auto rig = Start();
    Early()[1] = [](Act& a) { Items(a, {10, 20, 21}); };
    Exclusive()[1] = [](ExclusiveWorld<>& world) -> SystemResult {
        auto held = world.Insert(20, ChildOf{10});
        REQUIRE(static_cast<bool>(held));
        CHECK(*held);
        std::size_t listed = 0;
        (void)world.Read<Children>(10, [&](const Children& c) { listed = c.size(); });
        CHECK(listed == 1U);
        auto failed = world.Insert(21, ChildOf{99});
        REQUIRE(static_cast<bool>(failed));
        CHECK_FALSE(*failed);
        return Ok();
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(rig->world(), 10) == std::vector<EntityId>{20});
    const auto rejected = rig->world().Diagnostics<RejectedRelationship>();
    REQUIRE(rejected.size() == 1U);
    CHECK(rejected[0].path == RelationshipPath::ImmediateChange);
}

TEST_CASE("relationship: the Host's direct puts keep Children and despawn linked children, without a boundary report [kr-host]") {
    auto rig = Start([](Registrar& r) { r.Hook<ChildOf, &ChildOfAdded>(HookTrigger::OnAdd, {.name = "ChildOfAdded"}); });
    World& world = rig->world();
    world.Insert(10, Item{10});
    world.Insert(20, ChildOf{10});
    world.Insert(21, ChildOf{10});
    world.Insert(30, ChildOf{21});
    CHECK(Kids(world, 10) == (std::vector<EntityId>{20, 21}));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // After a Run the boundary snapshots exist: a Host insert changes Children through the World API,
    // so verification does not report it at the next Run.
    world.Insert(22, ChildOf{10});
    CHECK(Kids(world, 10) == (std::vector<EntityId>{20, 21, 22}));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(world.Remove<ChildOf>(20));
    CHECK(Kids(world, 10) == (std::vector<EntityId>{21, 22}));
    REQUIRE(world.Despawn(10));
    CHECK_FALSE(world.Alive(21));
    CHECK_FALSE(world.Alive(22));
    CHECK_FALSE(world.Alive(30));
    CHECK(world.Alive(20));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Joined().empty());
}

TEST_CASE("relationship: the Host putting a child before its parent leaves the relationship out [kr-host-load-order]") {
    auto rig = Start();
    World& world = rig->world();
    world.Insert(20, ChildOf{10});
    world.Insert(10, Item{10});
    CHECK(ParentOf(world, 20) == std::nullopt);
    const auto rejected = world.Diagnostics<RejectedRelationship>();
    REQUIRE(rejected.size() == 1U);
    CHECK(rejected[0].path == RelationshipPath::Host);
    CHECK(rejected[0].emitter == "Host");
    CHECK(rejected[0].schedule.empty());
    CHECK(rejected[0].step == 0U);
}

// ---- Restore (DI-KS 12, 32, 33)

TEST_CASE("relationship: a restore that names ChildOf and Children puts both back; the summary equals running straight [kr-restore-reparent]") {
    auto setup = [](bool reparent) {
        auto rig = Start();
        Early()[1] = [](Act& a) {
            Items(a, {10, 11});
            a.spawnChild.New(20, ChildOf{10});
        };
        Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
        if (reparent) {
            Early()[2] = [](Act& a) { a.commands.Insert(20, ChildOf{11}); };
            Exclusive()[2] = [](ExclusiveWorld<>& world) { return PutBack(world, true, true); };
        }
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        return rig;
    };
    auto straight = setup(false);
    const std::string expected = Summary(straight->world());
    auto rolled = setup(true);
    CHECK(Kids(rolled->world(), 10) == std::vector<EntityId>{20});
    CHECK(rolled->world().Get<Children>(11) == nullptr);
    CHECK(Summary(rolled->world()) == expected);
}

TEST_CASE("relationship: a restore puts back the order of Children as given [kr-restore-order]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(30, ChildOf{10});
        a.spawnChild.New(31, ChildOf{10});
    };
    Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
    Early()[2] = [](Act& a) { a.commands.Remove<ChildOf>(30); };
    Early()[3] = [](Act& a) { a.commands.Insert(30, ChildOf{10}); };
    Exclusive()[3] = [](ExclusiveWorld<>& world) { return PutBack(world, true, true); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(Kids(rig->world(), 10) == std::vector<EntityId>{31});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(rig->world(), 10) == (std::vector<EntityId>{30, 31}));
}

TEST_CASE("relationship: a restore brings back a child a linked despawn took; the summary equals running straight [kr-restore-revive]") {
    auto setup = [](bool despawn) {
        auto rig = Start();
        Early()[1] = [](Act& a) {
            Items(a, {10});
            a.spawnChild.New(20, ChildOf{10});
        };
        Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
        if (despawn) {
            Early()[2] = [](Act& a) { a.despawn.Entity(10); };
            Exclusive()[2] = [](ExclusiveWorld<>& world) { return PutBack(world, true, true); };
        }
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        return rig;
    };
    auto straight = setup(false);
    auto rolled = setup(true);
    CHECK(rolled->world().Alive(20));
    CHECK(Kids(rolled->world(), 10) == std::vector<EntityId>{20});
    CHECK(Summary(rolled->world()) == Summary(straight->world()));
}

TEST_CASE("relationship: a restore does not expand linked despawns [kr-restore-no-expand]") {
    auto rig = Start();
    Early()[1] = [](Act& a) { Items(a, {20}); };
    Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
    // 30 is born after the save and 20 is attached to it: the restore takes 30 away, 20 was alive then.
    Early()[2] = [](Act& a) {
        Items(a, {30});
        a.commands.Insert(20, ChildOf{30});
    };
    Exclusive()[2] = [](ExclusiveWorld<>& world) { return PutBack(world, true, true); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK_FALSE(rig->world().Alive(30));
    CHECK(rig->world().Alive(20));
    CHECK(ParentOf(rig->world(), 20) == std::nullopt);
}

TEST_CASE("relationship: a restore that does not name ChildOf takes the ChildOf of a child whose parent it despawns [kr-restore-unregistered-childof]") {
    auto rig = Start();
    Early()[1] = [](Act& a) { Items(a, {20}); };
    Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
    Early()[2] = [](Act& a) {
        Items(a, {30});
        a.commands.Insert(20, ChildOf{30});
    };
    Exclusive()[2] = [](ExclusiveWorld<>& world) { return PutBack(world, false, false); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().Alive(20));
    CHECK(ParentOf(rig->world(), 20) == std::nullopt);
}

TEST_CASE("relationship: a restore's ChildOf whose parent is not alive does not hold [kr-restore-rejected]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
    };
    Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
    Early()[2] = [](Act& a) { a.commands.Remove<ChildOf>(20); };
    Exclusive()[2] = [](ExclusiveWorld<>& world) -> SystemResult {
        Snapshot().childOf = {{20, ChildOf{99}}};
        Snapshot().children.clear();
        return PutBack(world, true, false);
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(ParentOf(rig->world(), 20) == std::nullopt);
    const auto rejected = rig->world().Diagnostics<RejectedRelationship>();
    REQUIRE(rejected.size() == 1U);
    CHECK(rejected[0].path == RelationshipPath::Restore);
    CHECK(rejected[0].emitter == "DriveExclusive");
}

TEST_CASE("relationship: a restore that names ChildOf but not Children keeps Children itself [kr-restore-maintain]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10, 11});
        a.spawnChild.New(20, ChildOf{10});
    };
    Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
    Early()[2] = [](Act& a) { a.commands.Insert(20, ChildOf{11}); };
    Exclusive()[2] = [](ExclusiveWorld<>& world) { return PutBack(world, true, false); };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Kids(rig->world(), 10) == std::vector<EntityId>{20});
    CHECK(rig->world().Get<Children>(11) == nullptr);
}

TEST_CASE("relationship: with verification on, ChildOf and Children that disagree are reported [kr-structure-check]") {
    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10});
        a.spawnChild.New(20, ChildOf{10});
    };
    Exclusive()[1] = [](ExclusiveWorld<>& world) { return Save(world); };
    Early()[2] = [](Act& a) { a.commands.Remove<ChildOf>(20); };
    // Put back Children {20} but no ChildOf on 20: the two disagree.
    Exclusive()[2] = [](ExclusiveWorld<>& world) -> SystemResult {
        Snapshot().childOf.clear();
        return PutBack(world, true, true);
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const auto result = rig->Run();
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK_MESSAGE(result.error().Text().find("Children lists 20") != std::string::npos, result.error().Text());
}

// ---- The summary and the hash (DI-KS 33)

TEST_CASE("relationship: the same steps give the same summary [kr-summary]") {
    auto run = [] {
        auto rig = Start();
        Early()[1] = [](Act& a) {
            Items(a, {10, 11});
            a.spawnChild.New(20, ChildOf{10});
            a.spawnChild.New(21, ChildOf{10});
        };
        Early()[2] = [](Act& a) { a.commands.Insert(20, ChildOf{11}); };
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        return Summary(rig->world());
    };
    CHECK(run() == run());
}

TEST_CASE("relationship: ChildOf hashes its parent; Children its count and each child in order [kr-hash]") {
    Hasher of;
    HashValue(of, ChildOf{5});
    Hasher expectedOf;
    expectedOf.U64(5);
    CHECK(std::vector<std::byte>(of.Data().begin(), of.Data().end()) ==
          std::vector<std::byte>(expectedOf.Data().begin(), expectedOf.Data().end()));

    auto rig = Start();
    Early()[1] = [](Act& a) {
        Items(a, {10, 11});
        a.spawnChild.New(31, ChildOf{10});
        a.spawnChild.New(30, ChildOf{10});
        a.spawnChild.New(40, ChildOf{11});
        a.spawnChild.New(41, ChildOf{11});
    };
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const Children* first = rig->world().Get<Children>(10);
    const Children* second = rig->world().Get<Children>(11);
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    Hasher h;
    HashValue(h, *first);
    Hasher expected;
    expected.U64(2);
    expected.U64(31);
    expected.U64(30);
    CHECK(std::vector<std::byte>(h.Data().begin(), h.Data().end()) ==
          std::vector<std::byte>(expected.Data().begin(), expected.Data().end()));
    Hasher other;
    HashValue(other, *second);
    Hasher ascending;
    ascending.U64(2);
    ascending.U64(40);
    ascending.U64(41);
    CHECK(std::vector<std::byte>(other.Data().begin(), other.Data().end()) ==
          std::vector<std::byte>(ascending.Data().begin(), ascending.Data().end()));
}

// ---- Loading (declaration 5.9): only ChildOf is saved; the order comes from the order of spawning

TEST_CASE("relationship: a load that spawns children in the saved order gives the same Children, ids kept or renumbered [kr-load]") {
    // Saved: parent 10 with children 31, 30 (that order).
    SUBCASE("with the saved ids") {
        auto rig = Start();
        Early()[1] = [](Act& a) {
            Items(a, {10});
            a.spawnChild.New(31, ChildOf{10});
            a.spawnChild.New(30, ChildOf{10});
        };
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(Kids(rig->world(), 10) == (std::vector<EntityId>{31, 30}));
    }
    SUBCASE("renumbered: the loader rewrites ChildOf::parent through its table") {
        auto rig = Start();
        Early()[1] = [](Act& a) {
            std::map<EntityId, EntityId> table;
            for (EntityId saved : {EntityId{10}, EntityId{31}, EntityId{30}}) {
                auto made = a.spawnFresh.New(Item{static_cast<std::int32_t>(saved)});
                REQUIRE(static_cast<bool>(made));
                table[saved] = *made;
            }
            a.commands.Insert(table[31], ChildOf{table[10]});
            a.commands.Insert(table[30], ChildOf{table[10]});
        };
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        World& world = rig->world();
        // 1000 is the parent (saved 10); 1001 is saved 31, 1002 saved 30.
        CHECK(Kids(world, 1000) == (std::vector<EntityId>{1001, 1002}));
    }
}

// ---- Ownership (DI-KS 29)

TEST_CASE("relationship: an owner cannot declare the initial value of the kernel's ChildOf [kr-kernel-owned]") {
    auto built = Finish({{"test", [](Registrar& r) {
                              r.Component<Mark>({.name = "Mark"});
                              r.InitialValue<ChildOf, &ChildOfFromMark>();
                          }}});
    REQUIRE_FALSE(static_cast<bool>(built.result));
    CHECK(built.result.error().Has(StartupCode::RE4));
}
