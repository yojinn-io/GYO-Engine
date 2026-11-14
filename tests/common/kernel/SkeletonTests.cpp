// The skeleton end to end, written the way a single-player product would write
// it: no zone, role or rollback is declared anywhere in this file.
//
// "skeleton:" cases check behaviour: Systems are registered, the Plan orders
// them, a Product runs them against a World, and the result matches a
// scenario worked out by hand.
// "limits:" cases only measure and print (startup analysis against the number
// of Systems; a tick against the number of entities). They assert correctness,
// not time: the numbers are for people to read.

#include "WorldTestSupport.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace GYO::Kernel;
using KernelTest::At;
using KernelTest::Built;
using KernelTest::Describe;
using KernelTest::HasError;
using KernelTest::Ok;

namespace {

// ---- A small game: things are spawned, fall, get hurt below the floor and die.

struct Position { std::int32_t x; std::int32_t y; };
struct Velocity { std::int32_t dx; std::int32_t dy; };
struct Health { std::int32_t hp; };
struct Armor { std::int32_t points; };
struct Gravity { std::int32_t g; };
struct Paused { std::int32_t on; };
struct Stats { std::int64_t hits; std::int64_t unpausedTicks; };
struct Hit { std::uint64_t target; std::int32_t amount; };

std::vector<std::string>& Deaths() {
    static std::vector<std::string> log;
    return log;
}

ScheduleOptions GameTick() {
    ScheduleOptions s;
    s.name = "Tick";
    s.domain = Domain::Fixed;
    s.clock = true;
    s.stages = {"Input", "Simulate", "Cleanup"};
    return s;
}

SystemOptions In(std::string name, std::string stage, std::vector<std::string> before = {},
                 std::vector<std::string> after = {}) {
    SystemOptions o;
    o.name = std::move(name);
    o.mounts = {At("Tick", std::move(stage), std::move(before), std::move(after))};
    return o;
}

// One new thing per tick for the first three ticks; the count is the System's own state.
SystemResult Spawner(Spawn<Bundle<Position, Velocity, Health>> spawn, Local<std::int32_t> spawned) {
    if (*spawned >= 3) return Ok();
    auto made = spawn.New(Position{0, 10}, Velocity{1, 0}, Health{2});
    if (!made) return Engine::Base::Err(std::move(made).error());
    ++*spawned;
    return Ok();
}

SystemResult ApplyGravity(Query<Write<Velocity>> bodies, Res<Gravity> gravity) {
    const std::int32_t g = (*gravity).g;
    bodies.ForEach([g](EntityId, Mut<Velocity> velocity) { velocity->dy -= g; });
    return Ok();
}

SystemResult Move(Query<Write<Position>, Read<Velocity>> bodies) {
    bodies.ForEach([](EntityId, Mut<Position> position, const Velocity& velocity) {
        position->x += velocity.dx;
        position->y += velocity.dy;
    });
    return Ok();
}

SystemResult HurtBelowFloor(Query<Read<Position>, With<Health>> bodies, EventWriter<Hit> hits) {
    bodies.ForEach([&hits](EntityId id, const Position& position) {
        if (position.y < 0) hits.Send(Hit{id, 1});
    });
    return Ok();
}

SystemResult ApplyHits(EventReader<Hit> hits, Query<Write<Health>> healths, ResMut<Stats> stats) {
    for (const Hit* hit : hits.Read()) {
        if (Mut<Health> health = healths.GetMut<Health>(hit->target)) {
            health->hp -= hit->amount;
            ++stats->hits;
        }
    }
    return Ok();
}

SystemResult Reap(Query<Read<Health>> healths, Despawn despawn) {
    Engine::Base::Result<void, KernelError> result;
    healths.ForEach([&](EntityId id, const Health& health) {
        if (health.hp > 0 || !result) return;
        if (auto r = despawn.Entity(id); !r) result = Engine::Base::Err(std::move(r).error());
    });
    return result;
}

bool NotPaused(Res<Paused> paused) { return (*paused).on == 0; }

SystemResult CountUnpaused(ResMut<Stats> stats) {
    ++stats->unpausedTicks;
    return Ok();
}

SystemResult LogDeath(HookCtx ctx) {
    Deaths().push_back("dead:" + std::to_string(ctx.Entity()));
    return Ok();
}

// Spawner and the Systems that update a component both write it, so the
// spawned components are declared Shared (see the Exclusive case below).
void GameData(Registrar& r) {
    r.Component<Position>({.name = "Position", .writePolicy = WritePolicy::Shared});
    r.Component<Velocity>({.name = "Velocity", .writePolicy = WritePolicy::Shared});
    r.Component<Health>({.name = "Health", .writePolicy = WritePolicy::Shared});
    r.Resource<Gravity>({.name = "Gravity"});
    r.Resource<Paused>({.name = "Paused"});
    r.Resource<Stats>({.name = "Stats", .writePolicy = WritePolicy::Shared});
    r.Resource<RangeAllotter>({.name = "Allotter"});
    r.Allotter<RangeAllotter>();
    r.Event<Hit>({.name = "Hit", .domain = Domain::Fixed});
    r.Schedule(GameTick());
}

void Game(Registrar& r) {
    GameData(r);
    r.AddSystem<&Spawner>(In("Spawner", "Input"));
    // Registered out of order on purpose: before / after decide, not the list.
    r.AddSystem<&ApplyHits>(In("ApplyHits", "Simulate", {}, {"HurtBelowFloor"}));
    r.AddSystem<&HurtBelowFloor>(In("HurtBelowFloor", "Simulate", {}, {"Move"}));
    r.AddSystem<&Move>(In("Move", "Simulate"));
    r.AddSystem<&ApplyGravity>(In("ApplyGravity", "Simulate", {"Move"}));
    SystemOptions counter = In("CountUnpaused", "Simulate", {}, {"ApplyHits"});
    counter.mounts[0].conditions.push_back(Condition<&NotPaused>("NotPaused"));
    r.AddSystem<&CountUnpaused>(counter);
    r.AddSystem<&Reap>(In("Reap", "Cleanup"));
    r.Hook<Health, &LogDeath>(HookTrigger::OnRemove, {.name = "LogDeath"});
}

std::unique_ptr<WorldTest::Rig> StartGame() {
    Deaths().clear();
    auto rig = WorldTest::MakeRig(Game);
    if (!rig->product) return rig;
    rig->world().SetResource(Gravity{1});
    rig->world().SetResource(Paused{0});
    rig->world().SetResource(Stats{0, 0});
    rig->world().SetResource(RangeAllotter::Of(100, 999));
    return rig;
}

std::vector<std::string> StepNames(const Plan& plan) {
    std::vector<std::string> names;
    for (const Step& step : plan.steps) {
        if (step.kind == StepKind::System) names.push_back(step.name);
    }
    return names;
}

} // namespace

TEST_CASE("skeleton: the Plan follows before and after, not the registration order") {
    auto rig = StartGame();
    REQUIRE(rig->product);
    const Plan* plan = rig->built.registry->Frozen().PlanOf("Tick");
    REQUIRE(plan != nullptr);
    CHECK(StepNames(*plan) == std::vector<std::string>{"Spawner", "ApplyGravity", "Move", "HurtBelowFloor", "ApplyHits",
                                                       "CountUnpaused", "Reap"});
}

TEST_CASE("skeleton: eight ticks of the game give the hand-worked result") {
    auto rig = StartGame();
    REQUIRE(rig->product);
    World& world = rig->world();

    // A thing spawned in tick t has been simulated k = now - t + 1 times:
    // x = k, y = 10 - k (k + 1) / 2. It is hit once y < 0 (k = 5 and k = 6) and
    // dies with 2 hp at the end of the tick where k = 6.
    for (int tick = 1; tick <= 4; ++tick) KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(world.Entities().size() == 3);
    CHECK(world.Entities()[0] == 100);
    CHECK(world.Entities()[1] == 101);
    CHECK(world.Entities()[2] == 102);
    REQUIRE(world.Get<Position>(100) != nullptr);
    CHECK(world.Get<Position>(100)->x == 4);
    CHECK(world.Get<Position>(100)->y == 0);
    CHECK(world.Get<Position>(101)->x == 3);
    CHECK(world.Get<Position>(101)->y == 4);
    CHECK(world.Get<Position>(102)->x == 2);
    CHECK(world.Get<Position>(102)->y == 7);
    CHECK(world.Resource<Stats>()->hits == 0);

    // Tick 5: the first thing is below the floor. The hit sent by
    // HurtBelowFloor reaches ApplyHits in the same tick.
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(world.Get<Position>(100)->y == -5);
    CHECK(world.Get<Health>(100)->hp == 1);
    CHECK(world.Resource<Stats>()->hits == 1);

    // Tick 6, paused for CountUnpaused only: the run condition skips one System.
    world.SetResource(Paused{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    world.SetResource(Paused{0});
    CHECK_FALSE(world.Alive(100));
    CHECK(world.Get<Health>(101)->hp == 1);
    CHECK(Deaths() == std::vector<std::string>{"dead:100"});

    KERNEL_REQUIRE_RUN_OK(rig->Run());
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(world.Entities().empty());
    CHECK(Deaths() == std::vector<std::string>{"dead:100", "dead:101", "dead:102"});
    CHECK(world.Resource<Stats>()->hits == 6);
    CHECK(world.Resource<Stats>()->unpausedTicks == 7);
    CHECK(world.Tick(Domain::Fixed) == 8);
    CHECK(rig->recorded.empty());
}

namespace {

SystemResult Fails() { return Engine::Base::Err(KernelError(KernelErrorCode::SystemFailed, "boom")); }

SystemResult WritePositionA(Query<Write<Position>> q) {
    q.ForEach([](EntityId, Mut<Position> p) { p->x += 1; });
    return Ok();
}
SystemResult WritePositionB(Query<Write<Position>> q) {
    q.ForEach([](EntityId, Mut<Position> p) { p->x *= 2; });
    return Ok();
}

} // namespace

TEST_CASE("skeleton: a System that returns Err stops the Run and interrupts the World") {
    auto rig = WorldTest::MakeRig([](Registrar& r) {
        GameData(r);
        r.AddSystem<&Fails>(In("Fails", "Simulate"));
    });
    REQUIRE(rig->product);
    const auto result = rig->Run();
    CHECK_FALSE(static_cast<bool>(result));
    CHECK(rig->world().State() == WorldState::Interrupted);
    CHECK(rig->recorded.size() == 1);
}

namespace {

// An inner operation fails; the System adds its own layer of context and
// returns the error. It does not log: the Executor's handler does, once.
SystemResult SpawnsWave(Spawn<Bundle<Position, Velocity, Health>> spawn) {
    for (int member = 0; member < 3; ++member) {
        auto made = spawn.New(Position{0, 0}, Velocity{0, 0}, Health{1});
        if (!made) {
            return Engine::Base::Err(std::move(made).error().Wrap("SpawnsWave: wave member " + std::to_string(member)));
        }
    }
    return Ok();
}

} // namespace

TEST_CASE("skeleton: an Err reaches the handler once, with the chain of where it happened") {
    auto rig = WorldTest::MakeRig([](Registrar& r) {
        GameData(r);
        r.AddSystem<&SpawnsWave>(In("SpawnsWave", "Input"));
    });
    REQUIRE(rig->product);
    rig->world().SetResource(RangeAllotter::Of(1, 2));  // room for two of the three
    const auto result = rig->Run();
    REQUIRE_FALSE(static_cast<bool>(result));
    CHECK(result.error().Has(KernelErrorCode::AllotterExhausted));
    const std::string text = result.error().Text();
    CHECK_MESSAGE(text.find("SpawnsWave: wave member 2") != std::string::npos, text);
    CHECK_MESSAGE(text.find("System 'SpawnsWave'") != std::string::npos, text);
    CHECK_MESSAGE(text.find("schedule 'Tick'") != std::string::npos, text);
    CHECK(rig->recorded.size() == 1);
    CHECK(rig->world().State() == WorldState::Interrupted);
    // What the failed call had queued is dropped, not half applied.
    CHECK(rig->world().Entities().empty());
    std::printf("\n[skeleton] an Err as the Executor's handler receives it:\n%s\n", text.c_str());
    std::fflush(stdout);
}

namespace {

struct Shield { std::int32_t points; };

// Both orders an author might write inside one System. At the sync point the
// changes of one kind are applied together, in a fixed order of kinds (remove
// before spawn and insert), not in the order they were recorded.
SystemResult RecordsBothOrders(Spawn<Bundle<Armor>> spawn, Despawn despawn,
                               CommandsFor<Inserts<Shield>, Removes<Shield>> shields) {
    auto made = spawn.New(Armor{1});
    if (!made) return Engine::Base::Err(std::move(made).error());
    if (auto r = despawn.Entity(made.value()); !r) return Engine::Base::Err(std::move(r).error());
    shields.Insert(1, Shield{7});
    shields.Remove<Shield>(1);
    return Ok();
}

} // namespace

TEST_CASE("skeleton: one sync point applies despawn and remove before spawn and insert, whatever the recorded order") {
    auto rig = WorldTest::MakeRig([](Registrar& r) {
        GameData(r);
        r.Component<Armor>({.name = "Armor"});
        r.Component<Shield>({.name = "Shield"});
        r.AddSystem<&RecordsBothOrders>(In("RecordsBothOrders", "Input"));
    });
    REQUIRE(rig->product);
    World& world = rig->world();
    world.SetResource(RangeAllotter::Of(500, 599));
    world.Insert(1, Position{0, 0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    // Spawned, then despawned in the same call: the despawn finds nothing yet, the spawn stays.
    CHECK(world.Alive(500));
    CHECK(world.Get<Armor>(500) != nullptr);
    // Inserted, then removed in the same call: the remove runs first, the insert stays.
    REQUIRE(world.Get<Shield>(1) != nullptr);
    CHECK(world.Get<Shield>(1)->points == 7);
}

namespace {

SystemResult CountsBothWays(Query<Read<Position>> all, Query<Read<Position>, With<Health>> alive, ResMut<Stats> stats) {
    // One component: the number its storage already keeps. Two terms: a walk.
    stats->hits = static_cast<std::int64_t>(all.Count());
    stats->unpausedTicks = static_cast<std::int64_t>(alive.CountMatching());
    return Ok();
}

} // namespace

TEST_CASE("skeleton: Count reads the size of one component; a Query of several terms is counted by walking") {
    auto rig = WorldTest::MakeRig([](Registrar& r) {
        GameData(r);
        r.AddSystem<&CountsBothWays>(In("CountsBothWays", "Simulate"));
    });
    REQUIRE(rig->product);
    World& world = rig->world();
    world.SetResource(Stats{0, 0});
    for (EntityId id = 1; id <= 5; ++id) world.Insert(id, Position{0, 0});
    world.Insert(2, Health{1});
    world.Insert(4, Health{1});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(world.Resource<Stats>()->hits == 5);
    CHECK(world.Resource<Stats>()->unpausedTicks == 2);
}

TEST_CASE("skeleton: two writers with no order are tied by registration and listed") {
    auto rig = WorldTest::MakeRig([](Registrar& r) {
        GameData(r);
        r.AddSystem<&WritePositionA>(In("AddOne", "Simulate"));
        r.AddSystem<&WritePositionB>(In("Double", "Simulate"));
    });
    REQUIRE(rig->product);
    const FrozenRegistry& frozen = rig->built.registry->Frozen();
    REQUIRE(frozen.TieBreaks().size() == 1);
    CHECK(frozen.TieBreaks()[0].first == "AddOne");
    CHECK(frozen.TieBreaks()[0].second == "Double");
    rig->world().Insert(1, Position{3, 0});
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().Get<Position>(1)->x == 8);  // (3 + 1) * 2, not 3 * 2 + 1
}

TEST_CASE("skeleton: a component left Exclusive rejects a second writer at startup") {
    Built built = KernelTest::Finish({{"test", [](Registrar& r) {
                                           r.Component<Position>({.name = "Position"});
                                           r.Schedule(GameTick());
                                           r.AddSystem<&WritePositionA>(In("AddOne", "Simulate"));
                                           r.AddSystem<&WritePositionB>(In("Double", "Simulate"));
                                       }}});
    REQUIRE_FALSE(static_cast<bool>(built.result));
    CHECK_MESSAGE(HasError(built, StartupCode::DE2), Describe(built));
}

// ---- Limits

namespace {

using Clock = std::chrono::steady_clock;

double Milliseconds(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

SystemResult ReadsPosition(Query<Read<Position>> q) {
    static_cast<void>(q);
    return Ok();
}

SystemResult MoveFour(Query<Write<Position>, Read<Velocity>, Read<Health>, Read<Armor>> bodies) {
    bodies.ForEach([](EntityId, Mut<Position> position, const Velocity& velocity, const Health&, const Armor&) {
        position->x += velocity.dx;
    });
    return Ok();
}

SystemResult CountOnly(Query<Read<Position>> q, ResMut<Stats> stats) {
    stats->hits = static_cast<std::int64_t>(q.Count());
    return Ok();
}

// Despawns the hundred lowest ids each tick.
SystemResult ReapHundred(Query<Read<Health>> healths, Despawn despawn) {
    int left = 100;
    Engine::Base::Result<void, KernelError> result;
    healths.ForEach([&](EntityId id, const Health&) {
        if (left <= 0 || !result) return;
        --left;
        if (auto r = despawn.Entity(id); !r) result = Engine::Base::Err(std::move(r).error());
    });
    return result;
}

SystemResult SpawnHundred(Spawn<Bundle<Position, Velocity, Health, Armor>> spawn) {
    for (int i = 0; i < 100; ++i) {
        auto made = spawn.New(Position{0, 0}, Velocity{1, 0}, Health{1}, Armor{0});
        if (!made) return Engine::Base::Err(std::move(made).error());
    }
    return Ok();
}

void LimitData(Registrar& r) {
    r.Component<Position>({.name = "Position", .writePolicy = WritePolicy::Shared});
    r.Component<Velocity>({.name = "Velocity", .writePolicy = WritePolicy::Shared});
    r.Component<Health>({.name = "Health", .writePolicy = WritePolicy::Shared});
    r.Component<Armor>({.name = "Armor", .writePolicy = WritePolicy::Shared});
    r.Resource<Stats>({.name = "Stats"});
    r.Resource<RangeAllotter>({.name = "Allotter"});
    r.Allotter<RangeAllotter>();
    r.Schedule(GameTick());
}

struct TickCost {
    double fill = 0;
    double moveTwo = 0;
    double moveFour = 0;
    double count = 0;
    double spawn = 0;
    double despawn = 0;
};

// One Product per measured System, so a tick's time is that System's.
template <auto Fn>
double TimeTicks(std::size_t entities, const char* name, int ticks, double* fillMs = nullptr) {
    auto rig = WorldTest::MakeRig(
        [name](Registrar& r) {
            LimitData(r);
            r.AddSystem<Fn>(In(name, "Simulate"));
        },
        false);
    REQUIRE(rig->product);
    World& world = rig->world();
    world.SetResource(Stats{0, 0});
    world.SetResource(RangeAllotter::Of(10'000'000, 20'000'000));
    const auto fillStart = Clock::now();
    for (std::size_t i = 0; i < entities; ++i) {
        const EntityId id = static_cast<EntityId>(i + 1);
        world.Insert(id, Position{0, 0});
        world.Insert(id, Velocity{1, 0});
        world.Insert(id, Health{1});
        world.Insert(id, Armor{0});
    }
    if (fillMs != nullptr) *fillMs = Milliseconds(Clock::now() - fillStart);
    KERNEL_REQUIRE_RUN_OK(rig->Run());  // warm up
    const auto start = Clock::now();
    for (int i = 0; i < ticks; ++i) KERNEL_REQUIRE_RUN_OK(rig->Run());
    return Milliseconds(Clock::now() - start) / ticks;
}

} // namespace

TEST_CASE("limits: startup analysis against the number of Systems") {
    std::printf("\n[limits] startup analysis (Registry::Finish), one schedule, one stage\n");
    std::printf("  %8s  %22s  %22s  %10s\n", "systems", "read-only (no conflict)", "all write one component", "tie-breaks");
    for (const int count : {10, 100, 300, 1000}) {
        double readOnlyMs = 0;
        double writersMs = 0;
        std::size_t ties = 0;
        for (const bool writers : {false, true}) {
            std::vector<std::pair<std::string, std::function<void(Registrar&)>>> builds;
            builds.emplace_back("test", [count, writers](Registrar& r) {
                LimitData(r);
                for (int i = 0; i < count; ++i) {
                    const std::string name = "S" + std::to_string(i);
                    if (writers) {
                        r.AddSystem<&WritePositionA>(In(name, "Simulate"));
                    } else {
                        r.AddSystem<&ReadsPosition>(In(name, "Simulate"));
                    }
                }
            });
            const auto start = Clock::now();
            Built built = KernelTest::Finish(std::move(builds));
            const double ms = Milliseconds(Clock::now() - start);
            REQUIRE_MESSAGE(static_cast<bool>(built.result), Describe(built));
            const Plan* plan = built.registry->Frozen().PlanOf("Tick");
            REQUIRE(plan != nullptr);
            CHECK(StepNames(*plan).size() == static_cast<std::size_t>(count));
            if (writers) {
                writersMs = ms;
                ties = built.registry->Frozen().TieBreaks().size();
            } else {
                readOnlyMs = ms;
            }
        }
        std::printf("  %8d  %19.2f ms  %19.2f ms  %10zu\n", count, readOnlyMs, writersMs, ties);
    }
    std::printf("  hard limit: %zu parameters per System (kMaxParameters)\n", kMaxParameters);
    std::fflush(stdout);
}

TEST_CASE("limits: one tick against the number of entities") {
    std::printf("\n[limits] time per tick of one System, four components per entity, verification off\n");
    std::printf("  %8s  %10s  %12s  %12s  %10s  %14s  %14s\n", "entities", "fill", "2-comp move", "4-comp move", "count",
                "spawn 100", "despawn 100");
    for (const std::size_t entities : {std::size_t{1000}, std::size_t{10'000}, std::size_t{100'000}}) {
        TickCost cost;
        const int ticks = entities >= 100'000 ? 3 : 5;  // 6 runs of ReapHundred leave entities behind
        cost.moveTwo = TimeTicks<&Move>(entities, "Move", ticks, &cost.fill);
        cost.moveFour = TimeTicks<&MoveFour>(entities, "MoveFour", ticks);
        cost.count = TimeTicks<&CountOnly>(entities, "CountOnly", ticks);
        cost.spawn = TimeTicks<&SpawnHundred>(entities, "SpawnHundred", ticks);
        cost.despawn = TimeTicks<&ReapHundred>(entities, "ReapHundred", ticks);
        std::printf("  %8zu  %7.2f ms  %9.3f ms  %9.3f ms  %7.3f ms  %11.3f ms  %11.3f ms\n", entities, cost.fill,
                    cost.moveTwo, cost.moveFour, cost.count, cost.spawn, cost.despawn);
        std::fflush(stdout);
    }
}
