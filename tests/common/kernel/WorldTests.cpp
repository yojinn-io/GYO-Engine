// World guards of the world-exec batch: spec J-1, J-2, J-3, J-7, J-8, J-31,
// J-32 (with its three-platform golden), J-33, J-34, J-36.
// target/kernel DI-KS 3, 4, 9, 10, 11; target/world sections 1, 4, 5, 6;
// spec A.3, A.5 (W-1 to W-3, W-7, W-9), F.1 to F.5.

#include "WorldTestSupport.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace WorldTest;

bool Ascending(const std::vector<EntityId>& keys) {
    return std::adjacent_find(keys.begin(), keys.end(), [](EntityId a, EntityId b) { return a >= b; }) == keys.end();
}

std::vector<EntityId> AliveList(World& world) {
    const auto alive = world.Entities();
    return std::vector<EntityId>(alive.begin(), alive.end());
}

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& part : parts) out += (out.empty() ? "" : " ") + part;
    return out;
}

// ---- J-1 (W-1)

// Tick 1 spawns 50 and 30, tick 2 spawns 10 and 20: later keys are smaller,
// so a column that appended would not be ascending.
SystemResult SpawnInTwoTicks(SpawnWithId<Bundle<Tag>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) {
        spawn.New(50, Tag{50});
        spawn.New(30, Tag{30});
    } else if (tick.Tick() == 2) {
        spawn.New(10, Tag{10});
        spawn.New(20, Tag{20});
    }
    return Ok();
}

// Tick 3 removes the smallest key (a swap-and-pop erase would break the order)
// and gives entities 10 and 50 a Mode.
SystemResult RemoveSmallest(CommandsFor<Inserts<Mode>, Removes<Tag>> commands, CurrentTick tick) {
    if (tick.Tick() == 3) {
        commands.Remove<Tag>(10);
        commands.Insert(50, Mode{5});
        commands.Insert(10, Mode{1});
    }
    return Ok();
}

} // namespace

TEST_CASE("J-1: after every sync point the keys are strictly ascending, the arrays agree, the entity list is a superset") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&SpawnInTwoTicks>(B("SpawnInTwoTicks", "A"));
        r.AddSystem<&RemoveSmallest>(B("RemoveSmallest", "B"));
    });
    // The Host's initial content, in descending order.
    rig->world().Insert(70, Tag{70});
    rig->world().Insert(60, Tag{60});
    for (int tick = 0; tick < 3; ++tick) KERNEL_REQUIRE_RUN_OK(rig->Run());

    const std::vector<EntityId> tags = rig->world().Keys<Tag>();
    const std::vector<EntityId> modes = rig->world().Keys<Mode>();
    const std::vector<EntityId> alive = AliveList(rig->world());
    CHECK(tags == std::vector<EntityId>{20, 30, 50, 60, 70});
    CHECK(modes == std::vector<EntityId>{10, 50});
    CHECK(alive == std::vector<EntityId>{10, 20, 30, 50, 60, 70});
    CHECK(Ascending(tags));
    CHECK(Ascending(modes));
    CHECK(Ascending(alive));
    for (EntityId id : tags) CHECK(std::binary_search(alive.begin(), alive.end(), id));
    for (EntityId id : modes) CHECK(std::binary_search(alive.begin(), alive.end(), id));
    // Values travel with their keys (the arrays agree).
    for (EntityId id : tags) {
        REQUIRE(rig->world().Get<Tag>(id) != nullptr);
        CHECK(rig->world().Get<Tag>(id)->t == static_cast<std::int32_t>(id));
    }
}

// ---- J-2 (W-2)

namespace {

SystemResult RecordOrders(Query<Read<Tag>> one, Query<Read<Tag>, Read<Mode>, Read<Pos>> three, Query<Without<Mode>> without) {
    std::string line = "one:";
    for (EntityId id : one.Entities()) line += " " + std::to_string(id);
    line += " three:";
    three.ForEach([&line](EntityId id, const Tag& tag, const Mode& mode, const Pos& pos) {
        line += " " + std::to_string(id) + "=" + std::to_string(tag.t + mode.m + pos.x);
    });
    line += " without:";
    for (EntityId id : without.Entities()) line += " " + std::to_string(id);
    Log().push_back(line);
    return Ok();
}

std::string OrdersOf(const std::function<void(World&)>& history) {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&RecordOrders>(B("RecordOrders", "A"));
    });
    history(rig->world());
    Log().clear();
    const auto result = rig->Run();
    REQUIRE_MESSAGE(static_cast<bool>(result), Text(result));
    REQUIRE(Log().size() == 1);
    return Log().front();
}

} // namespace

TEST_CASE("J-2: two Worlds with different histories and the same members iterate in the same order") {
    const std::string first = OrdersOf([](World& w) {
        for (EntityId id : {9, 1, 5, 3, 7}) w.Insert(id, Tag{static_cast<std::int32_t>(id)});
        for (EntityId id : {7, 3, 9}) w.Insert(id, Mode{static_cast<std::int32_t>(id) * 10});
        for (EntityId id : {3, 9, 7}) w.Insert(id, Pos{static_cast<std::int32_t>(id) * 100});
    });
    const std::string second = OrdersOf([](World& w) {
        w.Insert(11, Tag{11});  // inserted, then despawned
        for (EntityId id : {1, 3, 5, 7, 9}) w.Insert(id, Tag{static_cast<std::int32_t>(id)});
        for (EntityId id : {9, 3, 7, 1}) w.Insert(id, Mode{static_cast<std::int32_t>(id) * 10});
        CHECK(w.Remove<Mode>(1));
        for (EntityId id : {7, 9, 3}) w.Insert(id, Pos{static_cast<std::int32_t>(id) * 100});
        CHECK(w.Despawn(11));
    });
    CHECK(first == "one: 1 3 5 7 9 three: 3=333 7=777 9=999 without: 1 5");
    CHECK(second == first);
}

// ---- J-3 (W-3)

namespace {

SystemResult InsertThroughKeptWorld(Query<Read<Tag>>) {
    KeptWorld()->Insert(99, Tag{99});
    return Ok();
}
// Overwrites an existing cell: no membership change, so only the structural-call check (not the epoch check) sees it.
SystemResult OverwriteThroughKeptWorld(Query<Read<Tag>>) {
    KeptWorld()->Insert(1, Tag{5});
    return Ok();
}

SystemResult SpawnOne(Spawn<Bundle<Tag>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) {
        auto id = spawn.New(Tag{7});
        if (!id) return Engine::Base::Err(std::move(id).error());
    }
    return Ok();
}

} // namespace

TEST_CASE("J-3: a structural change while Running fails a GYO_ASSERT; a Spawn is not applied inside the call") {
    {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&InsertThroughKeptWorld>(B("InsertThroughKeptWorld", "A"));
        });
        KeptWorld() = &rig->world();
        GYO_CHECK_ASSERTS(rig->Run());
        KeptWorld() = nullptr;
        CHECK(rig->world().State() == WorldState::Interrupted);
    }
    {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&OverwriteThroughKeptWorld>(B("OverwriteThroughKeptWorld", "A"));
        }, false);
        rig->world().Insert(1, Tag{1});
        KeptWorld() = &rig->world();
        GYO_CHECK_ASSERTS(rig->Run());
        KeptWorld() = nullptr;
        CHECK(rig->world().Get<Tag>(1)->t == 1);
    }
    {
        // With Spawn applied inside the call, the always-on epoch check after the call fails.
        auto rig = MakeRig([](Registrar& r) {
            Data(r, true);
            r.AddSystem<&SpawnOne>(B("SpawnOne", "A"));
        });
        rig->world().SetResource(RangeAllotter::Of(1000, 1999));
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(rig->world().Keys<Tag>() == std::vector<EntityId>{1000});
    }
}

namespace {
// Changes a column's membership behind the World API's back: only the always-on epoch check after the call sees it.
SystemResult EraseBehindTheApi(Query<Read<Tag>>) {
    Detail::ColumnAt(*KeptWorld(), Detail::ComponentIdOf(*KeptWorld(), KeyOf<Tag>())).Erase(1);
    return Ok();
}
} // namespace

TEST_CASE("J-3: a membership change inside a System call fails the epoch check") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&EraseBehindTheApi>(B("EraseBehindTheApi", "A"));
    }, false);
    rig->world().Insert(1, Tag{1});
    KeptWorld() = &rig->world();
    GYO_CHECK_ASSERTS(rig->Run());
    KeptWorld() = nullptr;
}

// ---- J-7 (W-7)

namespace {

std::vector<std::string>& DtorLog() {
    static std::vector<std::string> log;
    return log;
}

// Logs "<column>:<entity>" when a live value is destroyed (by its destructor or
// by being overwritten through move assignment, which is how a vector erases).
template <int Column>
struct Tracked {
    std::uint64_t id = 0;
    bool live = false;
    explicit Tracked(std::uint64_t value) : id(value), live(true) {}
    Tracked(Tracked&& other) noexcept : id(other.id), live(other.live) { other.live = false; }
    Tracked& operator=(Tracked&& other) noexcept {
        if (live) Record();
        id = other.id;
        live = other.live;
        other.live = false;
        return *this;
    }
    ~Tracked() {
        if (live) Record();
    }
    void Record() const noexcept { DtorLog().push_back(std::to_string(Column) + ":" + std::to_string(id)); }
};

SystemResult DespawnOne(Despawn despawn, CurrentTick tick) {
    if (tick.Tick() == 1) {
        if (auto r = despawn.Entity(1); !r) return r;
    }
    return Ok();
}

} // namespace

TEST_CASE("J-7: destroy order: a despawn in ascending ComponentId, the World in (ComponentId, EntityId)") {
    DtorLog().clear();
    {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            // Registered in this order: ComponentId of Tracked<1> is smaller than Tracked<0>'s.
            r.Component<Tracked<1>>({.name = "Tracked1", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
            r.Component<Tracked<0>>({.name = "Tracked0", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
            r.AddSystem<&DespawnOne>(B("DespawnOne", "A"));
        });
        World& world = rig->world();
        for (EntityId id : {5, 2, 1, 3}) {
            world.Insert(id, Tracked<0>(id));
            world.Insert(id, Tracked<1>(id));
        }
        DtorLog().clear();
        CHECK(world.Despawn(2));  // the Host's despawn
        CHECK(DtorLog() == std::vector<std::string>{"1:2", "0:2"});
        DtorLog().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());  // the sync point's despawn of 1
        CHECK(DtorLog() == std::vector<std::string>{"1:1", "0:1"});
        DtorLog().clear();
    }
    // The World's destructor: column Tracked1 (smaller id) first, each column in ascending EntityId.
    CHECK(DtorLog() == std::vector<std::string>{"1:3", "1:5", "0:3", "0:5"});
}

// ---- J-8 (W-9)

namespace {

template <class A>
concept HasReset = requires(A allotter) { allotter.Reset(); };

SystemResult SpawnIntoAllotterRange(SpawnWithId<Bundle<Tag>> spawn) {
    spawn.New(150, Tag{150});
    return Ok();
}

SystemResult SpawnEveryTick(Spawn<Bundle<Tag>> spawn) {
    auto id = spawn.New(Tag{1});
    if (!id) return Engine::Base::Err(std::move(id).error().Wrap("SpawnEveryTick"));
    return Ok();
}

} // namespace

TEST_CASE("J-8: the allotter is monotonic, reports exhaustion and never wraps; no allotter, no Spawn") {
    RangeAllotter allotter = RangeAllotter::Of(100, 102);
    CHECK(allotter.Allot() == std::optional<EntityId>(100));
    CHECK(allotter.Allot() == std::optional<EntityId>(101));
    CHECK(allotter.Allot() == std::optional<EntityId>(102));
    CHECK(allotter.Allot() == std::nullopt);
    CHECK(allotter.Allot() == std::nullopt);
    CHECK(allotter.Owns(100));
    CHECK(allotter.Owns(102));
    CHECK_FALSE(allotter.Owns(103));
    CHECK_FALSE(allotter.Owns(99));
    CHECK(RangeAllotter::Of(5, 4).Allot() == std::nullopt);
    RangeAllotter top = RangeAllotter::Of(~EntityId{0} - 1, ~EntityId{0});
    CHECK(top.Allot() == std::optional<EntityId>(~EntityId{0} - 1));
    CHECK(top.Allot() == std::optional<EntityId>(~EntityId{0}));
    CHECK(top.Allot() == std::nullopt);
    // The World has no way to reset the allotter (W-9: a new generation is a new World).
    CHECK_MESSAGE(!HasReset<RangeAllotter>, "RangeAllotter must not have a Reset");

    auto rig = MakeRig([](Registrar& r) {
        Data(r, true);
        r.AddSystem<&SpawnEveryTick>(B("SpawnEveryTick", "A"));
    });
    rig->world().SetResource(RangeAllotter::Of(100, 101));
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    const auto third = rig->Run();
    CHECK(HasCode(third, KernelErrorCode::AllotterExhausted));
    CHECK(rig->world().State() == WorldState::Interrupted);
    CHECK(rig->recorded.size() == 1);
    CHECK(rig->world().Keys<Tag>() == std::vector<EntityId>{100, 101});

    // SpawnWithId with an id of this World's allotter range: a GYO_ASSERT at the sync point (spec A.1).
    {
        auto ranged = MakeRig([](Registrar& r) {
            Data(r, true);
            r.AddSystem<&SpawnIntoAllotterRange>(B("SpawnIntoAllotterRange", "A"));
        });
        ranged->world().SetResource(RangeAllotter::Of(100, 199));
        GYO_CHECK_ASSERTS(ranged->Run());
    }

    // A Spawn without an allotter: D-E3 at Finish.
    const Built none = Finish({{"test", [](Registrar& r) {
                                    Data(r);
                                    r.AddSystem<&SpawnEveryTick>(B("SpawnEveryTick", "A"));
                                }}});
    CHECK(HasError(none, StartupCode::DE3));
}

// ---- J-31 (F-1)

namespace {

SystemResult SpawnTag(SpawnWithId<Bundle<Tag>> spawn, CurrentTick tick) {
    if (tick.Tick() == 1) spawn.New(4, Tag{4});
    return Ok();
}
SystemResult SeeSameWindow(Query<Read<Tag>> tags) {
    Log().push_back("same:" + std::to_string(tags.Count()));
    return Ok();
}
SystemResult SeeNextWindow(Query<Read<Tag>> tags) {
    Log().push_back("next:" + std::to_string(tags.Count()));
    return Ok();
}

} // namespace

TEST_CASE("J-31: a spawn is not visible in its own window, and is after the sync point") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&SpawnTag>(B("SpawnTag", "A"));
        r.AddSystem<&SeeSameWindow>(B("SeeSameWindow", "A", {}, {"SpawnTag"}));
        r.AddSystem<&SeeNextWindow>(B("SeeNextWindow", "B"));
    });
    Log().clear();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Log() == std::vector<std::string>{"same:0", "next:1"});
}

// ---- J-32 (F-2)

namespace {

SystemResult InsertTagFive(CommandsFor<Inserts<Tag>, Removes<>> commands, Despawn despawn) {
    commands.Insert(5, Tag{50});
    if (auto r = despawn.Entity(2); !r) return r;
    return Ok();
}
SystemResult SpawnModeSeven(SpawnWithId<Bundle<Mode>> spawn) {
    spawn.New(7, Mode{70});
    return Ok();
}

Fingerprint ReorderSummary(bool tagFirst) {
    auto rig = MakeRig([tagFirst](Registrar& r) {
        Data(r);
        if (tagFirst) {
            r.AddSystem<&InsertTagFive>(B("InsertTagFive", "A"));
            r.AddSystem<&SpawnModeSeven>(B("SpawnModeSeven", "A"));
        } else {
            r.AddSystem<&SpawnModeSeven>(B("SpawnModeSeven", "A"));
            r.AddSystem<&InsertTagFive>(B("InsertTagFive", "A"));
        }
    });
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    CHECK((PositionOf(*plan, "InsertTagFive") < PositionOf(*plan, "SpawnModeSeven")) == tagFirst);
    rig->world().Insert(1, Tag{1});
    rig->world().Insert(2, Tag{2});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    return rig->world().Summary();
}

// Two records of one key by one System (recSeq decides), two hooks with one key each (hook order decides).
SystemResult TwoRecordsOneKey(CommandsFor<Inserts<Tag>, Removes<>> commands, SpawnWithId<Bundle<Mode>> spawn,
                              CurrentTick tick) {
    commands.Insert(3, Tag{1});
    commands.Insert(3, Tag{2});
    if (tick.Tick() == 1) spawn.New(9, Mode{9});
    return Ok();
}
SystemResult HookTag100(HookCtx ctx, CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Tag{100});
    return Ok();
}
SystemResult HookTag200(HookCtx ctx, CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Tag{200});
    return Ok();
}

// Same window: despawn 4, spawn 4 again with an id (a rejoin), despawn 9 then 3
// (recorded out of order), insert into 6 that is despawned.
SystemResult RejoinAndDrop(Despawn despawn, SpawnWithId<Bundle<Mode>> spawn, CommandsFor<Inserts<Tag>, Removes<>> commands,
                           CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    for (EntityId id : {4, 9, 3, 6}) {
        if (auto r = despawn.Entity(id); !r) return r;
    }
    spawn.New(4, Mode{2});
    commands.Insert(1, Tag{1});  // a new cell of entity 1: its OnAdd comes before the spawn's (entity 4)
    commands.Insert(6, Tag{6});
    commands.Insert(8, Tag{8});  // a new cell of entity 8 (OnAdd after the spawn's cell of 4)
    return Ok();
}
SystemResult LogRemoveMode(HookCtx ctx) {
    Log().push_back("remove-mode:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult LogAddMode(HookCtx ctx) {
    Log().push_back("add-mode:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult LogAddTag(HookCtx ctx) {
    Log().push_back("add-tag:" + std::to_string(ctx.Entity()));
    return Ok();
}

} // namespace

TEST_CASE("J-32: the sync point's result depends only on the records, in spec F.2's order (golden)") {
    SUBCASE("reordering two Systems that do not conflict keeps the World") {
        CHECK(ReorderSummary(true) == ReorderSummary(false));
    }
    SUBCASE("same key: the largest (planPos, recSeq) wins; the golden is the same on every L1 platform") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&TwoRecordsOneKey>(B("TwoRecordsOneKey", "A"));
            r.Hook<Mode, &HookTag100>(HookTrigger::OnAdd, {.name = "HookTag100", .zone = Zone::B});
            r.Hook<Mode, &HookTag200>(HookTrigger::OnAdd, {.name = "HookTag200", .zone = Zone::B});
        });
        rig->world().Insert(3, Pos{3});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        REQUIRE(rig->world().Get<Tag>(3) != nullptr);
        CHECK(rig->world().Get<Tag>(3)->t == 2);
        REQUIRE(rig->world().Get<Tag>(9) != nullptr);
        CHECK(rig->world().Get<Tag>(9)->t == 200);
        const std::string summary = rig->world().Summary().Text();
        MESSAGE("J-32 golden summary: " << summary);
        CHECK(summary == "1:9b47697704716f9a716d98380ce62743da6a3b6d2a51368e5cc2bacf4cd2e587");
    }
    SUBCASE("despawn then spawn with the same id: the new entity lives, hooks see remove then add; inserts into a gone entity are dropped") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&RejoinAndDrop>(B("RejoinAndDrop", "A"));
            r.Hook<Mode, &LogRemoveMode>(HookTrigger::OnRemove, {.name = "LogRemoveMode", .zone = Zone::B});
            r.Hook<Mode, &LogAddMode>(HookTrigger::OnAdd, {.name = "LogAddMode", .zone = Zone::B});
            r.Hook<Tag, &LogAddTag>(HookTrigger::OnAdd, {.name = "LogAddTag", .zone = Zone::B});
        });
        World& world = rig->world();
        for (EntityId id : {1, 3, 4, 9}) world.Insert(id, Mode{1});
        world.Insert(6, Tag{0});
        world.Insert(8, Mode{8});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(world.Alive(4));
        REQUIRE(world.Get<Mode>(4) != nullptr);
        CHECK(world.Get<Mode>(4)->m == 2);
        CHECK_FALSE(world.Alive(6));
        CHECK_FALSE(world.Alive(3));
        CHECK_FALSE(world.Alive(9));
        CHECK(world.DroppedMissingEntity() == 1);
        // Despawns in ascending EntityId (3, 4, 9 although recorded 4, 9, 3), then the
        // spawn's OnAdd and the new Tag cell's, in ascending (EntityId, ComponentId).
        CHECK(Join(Log()) == "remove-mode:3 remove-mode:4 remove-mode:9 add-tag:1 add-mode:4 add-tag:8");
    }
}

namespace {

SystemResult EarlyTwoRecords(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(5, Tag{1});
    commands.Insert(5, Tag{2});
    return Ok();
}
SystemResult LateOneRecord(CommandsFor<Inserts<Tag>, Removes<>> commands) {
    commands.Insert(5, Tag{3});
    return Ok();
}
SystemResult OverwriteAndRemove(CommandsFor<Inserts<Tag>, Removes<Tag>> commands) {
    commands.Insert(1, Tag{10});  // an overwrite: no OnAdd
    commands.Remove<Tag>(2);      // OnRemove before the cell goes
    return Ok();
}
SystemResult LogRemoveTag(HookCtx ctx, Query<Read<Tag>> tags) {
    const Tag* tag = tags.Get<Tag>(ctx.Entity());
    Log().push_back("remove-tag:" + std::to_string(ctx.Entity()) + "=" + std::to_string(tag != nullptr ? tag->t : -1));
    return Ok();
}
SystemResult DespawnOneAtTickOne(Despawn despawn, CurrentTick tick) {
    if (tick.Tick() == 1) return despawn.Entity(1);
    return Ok();
}
// OnRemove of Mode reads Tag (a smaller ComponentId) of the same entity during its despawn.
SystemResult ModeRemovedSeesTag(HookCtx ctx, Query<Read<Tag>> tags) {
    const Tag* tag = tags.Get<Tag>(ctx.Entity());
    Log().push_back("mode-removed sees tag:" + std::to_string(tag != nullptr ? tag->t : -1));
    return Ok();
}
SystemResult SpawnAlive(SpawnWithId<Bundle<Mode>> spawn) {
    spawn.New(1, Mode{1});
    return Ok();
}

} // namespace

TEST_CASE("J-32: two Systems, one key: the later Plan position wins; an overwrite is not an add; a Remove runs OnRemove") {
    SUBCASE("two Systems, one key: the later Plan position wins") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&EarlyTwoRecords>(B("EarlyTwoRecords", "A"));
            r.AddSystem<&LateOneRecord>(B("LateOneRecord", "A", {}, {"EarlyTwoRecords"}));
        });
        rig->world().Insert(5, Mode{0});
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(rig->world().Get<Tag>(5)->t == 3);
    }
    SUBCASE("an overwrite is not an add; a Remove runs OnRemove while the cell is readable") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&OverwriteAndRemove>(B("OverwriteAndRemove", "A"));
            r.Hook<Tag, &LogAddTag>(HookTrigger::OnAdd, {.name = "LogAddTag", .zone = Zone::B});
            r.Hook<Tag, &LogRemoveTag>(HookTrigger::OnRemove, {.name = "LogRemoveTag", .zone = Zone::B});
        });
        rig->world().Insert(1, Tag{1});
        rig->world().Insert(2, Tag{2});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(Join(Log()) == "remove-tag:2=2");
        CHECK(rig->world().Get<Tag>(1)->t == 10);
    }
    SUBCASE("a despawn runs every OnRemove before destroying anything (the whole entity is readable)") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&DespawnOneAtTickOne>(B("DespawnOneAtTickOne", "A"));
            r.Hook<Mode, &ModeRemovedSeesTag>(HookTrigger::OnRemove, {.name = "ModeRemovedSeesTag", .zone = Zone::B});
        });
        rig->world().Insert(1, Tag{7});
        rig->world().Insert(1, Mode{1});
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        CHECK(Join(Log()) == "mode-removed sees tag:7");
        CHECK_FALSE(rig->world().Alive(1));
    }
    SUBCASE("SpawnWithId of an alive id that this window does not despawn fails a GYO_ASSERT") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnAlive>(B("SpawnAlive", "A"));
        });
        rig->world().Insert(1, Tag{1});
        GYO_CHECK_ASSERTS(rig->Run());
    }
}

// ---- J-33 (F-3)

namespace {

ScheduleOptions MidSchedule() {
    ScheduleOptions s = TickSchedule();
    s.syncPoints = {SyncPointOptions{"Mid", "A"}};
    return s;
}

SystemResult SeeBeforeMid(Query<Read<Tag>> tags) {
    Log().push_back("before-mid:" + std::to_string(tags.Count()));
    return Ok();
}
SystemResult SeeAfterMid(Query<Read<Tag>> tags) {
    Log().push_back("after-mid:" + std::to_string(tags.Count()));
    return Ok();
}

} // namespace

TEST_CASE("J-33: stage ends and declared sync points apply; nowhere else") {
    auto rig = MakeRig([](Registrar& r) {
        r.Component<Tag>({.name = "Tag", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
        r.Schedule(MidSchedule());
        r.AddSystem<&SpawnTag>(B("SpawnTag", "A", {"Mid"}));
        r.AddSystem<&SeeBeforeMid>(B("SeeBeforeMid", "A", {"Mid"}, {"SpawnTag"}));
        r.AddSystem<&SeeAfterMid>(B("SeeAfterMid", "A", {}, {"Mid"}));
    });
    Log().clear();
    const std::uint64_t epochBefore = rig->world().SyncEpoch();
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Log() == std::vector<std::string>{"before-mid:0", "after-mid:1"});
    // F-6: every sync point counts, empty ones too: Mid and the three stage ends.
    CHECK(rig->world().SyncEpoch() == epochBefore + 4);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().SyncEpoch() == epochBefore + 8);
}

// ---- J-34 (F-4, F-5, F-11)

namespace {

SystemResult HookFirst(HookCtx ctx, CommandsFor<Inserts<Mode>, Removes<>> commands) {
    Log().push_back("first:" + std::to_string(ctx.Entity()));
    commands.Insert(ctx.Entity(), Mode{1});
    return Ok();
}
SystemResult HookSecond(HookCtx ctx) {
    Log().push_back("second:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult CountModes(Query<Read<Mode>> modes, CurrentTick tick) {
    Log().push_back("modes@" + std::to_string(tick.Tick()) + ":" + std::to_string(modes.Count()));
    return Ok();
}
// A record made on the System path while the World is Applying (spec F-5).
SystemResult RecordOnSystemPath(HookCtx ctx) {
    Detail::SystemContext fake;
    fake.world = KeptWorld();
    fake.hook = false;
    std::vector<Detail::Record> queue;
    std::uint32_t seq = 0;
    fake.queue = &queue;
    fake.recSeq = &seq;
    Detail::Push(fake, Detail::Record::Of(Detail::RecordKind::Despawn, ctx.Entity()));
    return Ok();
}
SystemResult HookWritesPos(Query<Write<Pos>>) { return Ok(); }
SystemResult SystemTakesHookCtx(HookCtx) { return Ok(); }
SystemResult HookReadsPos(Query<Read<Pos>>) { return Ok(); }

} // namespace

TEST_CASE("J-34: hooks run in registration order, their changes belong to the next window; F-5; (c) hook conflicts") {
    SUBCASE("order and the next window") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnTag>(B("SpawnTag", "A"));
            r.AddSystem<&CountModes>(B("CountModes", "B"));
            r.Hook<Tag, &HookFirst>(HookTrigger::OnAdd, {.name = "HookFirst", .zone = Zone::B});
            r.Hook<Tag, &HookSecond>(HookTrigger::OnAdd, {.name = "HookSecond", .zone = Zone::B});
        });
        Log().clear();
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        // The Mode HookFirst recorded at the end of stage A is applied at the end of stage B, after CountModes.
        CHECK(Join(Log()) == "first:4 second:4 modes@1:0");
        CHECK(rig->world().Get<Mode>(4) != nullptr);
    }
    SUBCASE("a record on the System path while Applying fails a GYO_ASSERT") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SpawnTag>(B("SpawnTag", "A"));
            r.Hook<Tag, &RecordOnSystemPath>(HookTrigger::OnAdd, {.name = "RecordOnSystemPath", .zone = Zone::B});
        });
        KeptWorld() = &rig->world();
        GYO_CHECK_ASSERTS(rig->Run());
        KeptWorld() = nullptr;
    }
    SUBCASE("HookCtx in a System fails a GYO_ASSERT when fetched") {
        auto rig = MakeRig([](Registrar& r) {
            Data(r);
            r.AddSystem<&SystemTakesHookCtx>(B("SystemTakesHookCtx", "A"));
        });
        GYO_CHECK_ASSERTS(rig->Run());
    }
    SUBCASE("two conflicting hooks on one trigger, one of them zone C: D-E9") {
        const Built built = Finish({{"test", [](Registrar& r) {
                                         Data(r);
                                         r.Hook<Vel, &HookWritesPos>(HookTrigger::OnAdd, {.name = "HookWritesPos", .zone = Zone::C});
                                         r.Hook<Vel, &HookReadsPos>(HookTrigger::OnAdd, {.name = "HookReadsPos", .zone = Zone::C});
                                     }}});
        CHECK(HasError(built, StartupCode::DE9));
    }
}

// ---- J-36 (F-7, F-8)

namespace {

SystemResult NoOp() { return Ok(); }
SystemResult EarlyReader(EventReader<Ping> pings, CurrentTick tick) {
    std::string line = "early@" + std::to_string(tick.Tick()) + ":";
    for (const Ping* ping : pings.Read()) line += " " + std::to_string(ping->n);
    Log().push_back(line);
    return Ok();
}
SystemResult Writer(EventWriter<Ping> pings, CurrentTick tick) {
    if (tick.Tick() == 1) {
        pings.Send(Ping{11});
        pings.Send(Ping{12});
    }
    return Ok();
}
SystemResult SecondWriter(EventWriter<Ping> pings, CurrentTick tick) {
    if (tick.Tick() == 1) pings.Send(Ping{21});
    return Ok();
}
SystemResult LateReader(EventReader<Ping> pings, CurrentTick tick) {
    std::string line = "late@" + std::to_string(tick.Tick()) + ":";
    for (const Ping* ping : pings.Read()) line += " " + std::to_string(ping->n);
    Log().push_back(line);
    return Ok();
}
SystemResult HookPing(HookCtx, EventWriter<Ping> pings) {
    pings.Send(Ping{99});
    return Ok();
}

} // namespace

TEST_CASE("J-36: an event is read once by every reader: this tick after its writer, next tick before it; then gone") {
    auto rig = MakeRig([](Registrar& r) {
        Data(r);
        r.AddSystem<&NoOp>(B("NoOp", "A"));
        r.AddSystem<&SpawnTag>(B("SpawnTag", "A"));
        r.AddSystem<&EarlyReader>(B("EarlyReader", "A", {}, {"NoOp", "SpawnTag"}));
        r.AddSystem<&Writer>(B("Writer", "B"));
        r.AddSystem<&SecondWriter>(B("SecondWriter", "B", {}, {"Writer"}));
        r.AddSystem<&LateReader>(B("LateReader", "C"));
        // Writes at the end of stage A (its sync point's Plan position).
        r.Hook<Tag, &HookPing>(HookTrigger::OnAdd, {.name = "HookPing", .zone = Zone::B});
    });
    Log().clear();
    for (int tick = 0; tick < 3; ++tick) KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(Join(Log()) == std::string("early@1: late@1: 99 11 12 21 ") +
                             "early@2: 99 11 12 21 late@2: early@3: late@3:");
    // Kept to the end of the next tick, then trimmed: nothing is left after tick 3.
    CHECK(rig->world().EventPayloads<Ping>().empty());
}
