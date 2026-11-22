// Registrar::InitialValue (the funnel tool): a component with one writing
// System gets its first value from a request other Systems spawn or insert.
// The kernel fills it in at the sync point where the request appears.

#include "WorldTestSupport.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace GYO::Kernel;
using KernelTest::At;
using KernelTest::Built;
using KernelTest::Describe;
using KernelTest::HasRule;
using KernelTest::Ok;
using KernelTest::TestRangeAllotter;

namespace {

struct Body { std::int32_t x; };          // one writer: Advance
struct BodyAt { std::int32_t x; };        // the request of Body
struct Mass { std::int32_t m; };          // filled from Body: the second link of a chain
struct Label { std::int32_t code; };      // filled from Mass: the third link
struct Crate { std::int32_t x; };         // a second request of Body
struct Foreign { std::int32_t f; };       // registered by another plugin

Body BodyFromRequest(const BodyAt& request) { return Body{request.x * 10}; }
Body BodyFromCrate(const Crate& crate) { return Body{crate.x + 7}; }
Mass MassFromBody(const Body& body) { return Mass{body.x + 1}; }
Label LabelFromMass(const Mass& mass) { return Label{mass.m * 2}; }
Foreign ForeignFromRequest(const BodyAt& request) { return Foreign{request.x}; }
BodyAt RequestFromForeign(const Foreign& foreign) { return BodyAt{foreign.f}; }

std::vector<std::string>& Seen() {
    static std::vector<std::string> log;
    return log;
}

ScheduleOptions Tick() {
    ScheduleOptions s;
    s.name = "Tick";
    s.stages = {"Request", "Use"};
    return s;
}

SystemOptions In(std::string name, std::string stage) {
    SystemOptions o;
    o.name = std::move(name);
    o.mounts = {At("Tick", std::move(stage))};
    return o;
}

void Data(Registrar& r) {
    r.Component<Body>({.name = "Body"});  // Exclusive: the default
    r.Component<BodyAt>({.name = "BodyAt"});
    r.Component<Mass>({.name = "Mass"});
    r.Component<Label>({.name = "Label"});
    r.Component<Crate>({.name = "Crate"});
    // Every System with a Spawn parameter writes the allotter.
    r.Resource<TestRangeAllotter>({.name = "Allotter"});
    r.Allotter<TestRangeAllotter>();
    // The Runs of Tick the test counted (Rig::Run writes it; time-events declaration 6.2).
    r.Resource<WorldTest::TickCount>({.name = "TickCount"});
    r.Schedule(Tick());
}

// Two Systems ask for a body; neither names Body.
SystemResult RequestOne(Spawn<Bundle<BodyAt>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(BodyAt{3});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}

SystemResult RequestAnother(Spawn<Bundle<BodyAt>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(BodyAt{5});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}

// The one writer of Body. It runs after the sync point that follows the
// requesting Systems, in the same tick.
SystemResult Advance(Query<Write<Body>> bodies) {
    bodies.ForEach([](EntityId id, Mut<Body> body) {
        Seen().push_back("advance:" + std::to_string(id) + "=" + std::to_string(body->x));
        body->x += 1;
    });
    return Ok();
}

SystemResult CountNewBodies(Query<Read<Body>, Added<Body>> fresh) {
    fresh.ForEach([](EntityId id, const Body&) { Seen().push_back("added:" + std::to_string(id)); });
    return Ok();
}

SystemResult BodyAdded(HookCtx ctx) {
    Seen().push_back("hook:" + std::to_string(ctx.Entity()));
    return Ok();
}


std::unique_ptr<WorldTest::Rig> Start(std::function<void(Registrar&)> build) {
    Seen().clear();
    auto rig = WorldTest::MakeRig(std::move(build));
    if (rig->executor) rig->world().SetResource(TestRangeAllotter::Of(100, 199));
    return rig;
}

std::size_t SeenCount(const std::string& prefix) {
    std::size_t count = 0;
    for (const std::string& entry : Seen()) {
        if (entry.rfind(prefix, 0) == 0) ++count;
    }
    return count;
}

bool SeenEntry(const std::string& entry) {
    for (const std::string& seen : Seen()) {
        if (seen == entry) return true;
    }
    return false;
}

} // namespace

TEST_CASE("initial value: two Systems request, one System writes, and the component stays Exclusive") {
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
        r.AddSystem<&RequestOne>(In("RequestOne", "Request"));
        r.AddSystem<&RequestAnother>(In("RequestAnother", "Request"));
        r.AddSystem<&Advance>(In("Advance", "Use"));
        r.AddSystem<&CountNewBodies>(In("CountNewBodies", "Use"));
        r.Hook<Body, &BodyAdded>(HookTrigger::OnAdd, {.name = "BodyAdded"});
    });
    REQUIRE(rig->executor);
    World& world = rig->world();

    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(world.Entities().size() == 2);
    const EntityId first = world.Entities()[0];
    const EntityId second = world.Entities()[1];

    // Filled at the sync point after the Request stage: Advance saw the values
    // 30 and 50 in the same tick and moved them on by one.
    CHECK(SeenEntry("advance:" + std::to_string(first) + "=30"));
    CHECK(SeenEntry("advance:" + std::to_string(second) + "=50"));
    REQUIRE(world.Get<Body>(first) != nullptr);
    REQUIRE(world.Get<Body>(second) != nullptr);
    CHECK(world.Get<Body>(first)->x == 31);
    CHECK(world.Get<Body>(second)->x == 51);

    // The filled cell is an added cell: OnAdd ran once per entity, and Added<Body> saw it.
    CHECK(SeenCount("hook:") == 2);
    CHECK(SeenCount("added:") == 2);

    // The request stays on the entity.
    REQUIRE(world.Get<BodyAt>(first) != nullptr);
    CHECK(world.Get<BodyAt>(first)->x == 3);

    // Nothing is filled twice: the next tick adds no Body and runs no hook.
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(SeenCount("hook:") == 2);
    CHECK(world.Get<Body>(first)->x == 32);
}

namespace {

SystemResult SpawnsBoth(Spawn<Bundle<BodyAt, Body>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(BodyAt{3}, Body{-1});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}

SystemResult InsertsRequests(CommandsFor<Inserts<BodyAt>, Removes<>> commands, Res<WorldTest::TickCount> tick) {
    if (tick->value == 1) commands.Insert(7, BodyAt{4});   // entity 7 has no request yet
    if (tick->value == 2) commands.Insert(7, BodyAt{9});   // overwrites the request
    if (tick->value == 3) commands.Insert(8, BodyAt{6});   // entity 8 already has a Body
    return Ok();
}

} // namespace

TEST_CASE("initial value: a component the entity already has is left as it is") {
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
        r.AddSystem<&SpawnsBoth>(In("SpawnsBoth", "Request"));
        r.Hook<Body, &BodyAdded>(HookTrigger::OnAdd, {.name = "BodyAdded"});
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    REQUIRE(rig->world().Get<Body>(id) != nullptr);
    CHECK(rig->world().Get<Body>(id)->x == -1);  // the bundle's value, not 30
    CHECK(SeenCount("hook:") == 1);                // added once, by the bundle
}

TEST_CASE("initial value: an insert of the request fills once; overwriting the request does nothing") {
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
        r.AddSystem<&InsertsRequests>(In("InsertsRequests", "Request"));
    });
    REQUIRE(rig->executor);
    World& world = rig->world();
    world.Insert(7, Label{0});   // two entities that exist before the first tick
    world.Insert(8, Body{80});

    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(world.Get<Body>(7) != nullptr);
    CHECK(world.Get<Body>(7)->x == 40);

    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(world.Get<BodyAt>(7)->x == 9);   // the request was overwritten
    CHECK(world.Get<Body>(7)->x == 40);    // the component was not

    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(world.Get<BodyAt>(8)->x == 6);
    CHECK(world.Get<Body>(8)->x == 80);    // entity 8 had a Body before its request arrived
}

TEST_CASE("initial value: a chain of declarations completes at one sync point") {
    auto rig = Start([](Registrar& r) {
        Data(r);
        // Declared out of order on purpose.
        r.InitialValue<Label, &LabelFromMass>();
        r.InitialValue<Body, &BodyFromRequest>();
        r.InitialValue<Mass, &MassFromBody>();
        r.AddSystem<&RequestOne>(In("RequestOne", "Request"));
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    REQUIRE(rig->world().Get<Body>(id) != nullptr);
    REQUIRE(rig->world().Get<Mass>(id) != nullptr);
    REQUIRE(rig->world().Get<Label>(id) != nullptr);
    CHECK(rig->world().Get<Body>(id)->x == 30);
    CHECK(rig->world().Get<Mass>(id)->m == 31);
    CHECK(rig->world().Get<Label>(id)->code == 62);
    // All three were added at the same stamp: one sync point.
    const auto body = rig->world().Stamps<Body>(id);
    const auto label = rig->world().Stamps<Label>(id);
    REQUIRE(body);
    REQUIRE(label);
    CHECK(body->first == label->first);
}

namespace {

SystemResult SpawnsTwoRequests(Spawn<Bundle<Crate, BodyAt>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(Crate{1}, BodyAt{3});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}

} // namespace

TEST_CASE("initial value: with two requests on one entity the lower ComponentId gives the value") {
    // BodyAt is registered before Crate (Data), so BodyAt wins whatever the
    // order of the declarations and of the bundle.
    for (const bool crateFirst : {false, true}) {
        CAPTURE(crateFirst);
        auto rig = Start([crateFirst](Registrar& r) {
            Data(r);
            if (crateFirst) r.InitialValue<Body, &BodyFromCrate>();
            r.InitialValue<Body, &BodyFromRequest>();
            if (!crateFirst) r.InitialValue<Body, &BodyFromCrate>();
            r.AddSystem<&SpawnsTwoRequests>(In("SpawnsTwoRequests", "Request"));
        });
        REQUIRE(rig->executor);
        KERNEL_REQUIRE_RUN_OK(rig->Run());
        REQUIRE(rig->world().Entities().size() == 1);
        const EntityId id = rig->world().Entities()[0];
        REQUIRE(rig->world().Get<Body>(id) != nullptr);
        CHECK(rig->world().Get<Body>(id)->x == 30);  // BodyFromRequest(3), not BodyFromCrate(1) = 8
    }
}

TEST_CASE("initial value: the Host API does not fill (like hooks, sync points only)") {
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
    });
    REQUIRE(rig->executor);
    rig->world().Insert(7, BodyAt{4});
    CHECK(rig->world().Get<Body>(7) == nullptr);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    CHECK(rig->world().Get<Body>(7) == nullptr);
}

TEST_CASE("initial value: declarations the Registry refuses") {
    SUBCASE("the request is not a registered component") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            r.Component<Body>({.name = "Body"});
            r.Schedule(Tick());
            r.InitialValue<Body, &BodyFromRequest>();
        }}});
        CHECK_MESSAGE(HasRule(built, StartupCode::RE4, "InitialValue"), Describe(built));
    }
    SUBCASE("the component is not registered") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            r.Component<BodyAt>({.name = "BodyAt"});
            r.Schedule(Tick());
            r.InitialValue<Body, &BodyFromRequest>();
        }}});
        CHECK_MESSAGE(HasRule(built, StartupCode::RE4, "InitialValue"), Describe(built));
    }
    SUBCASE("only the owner of a component declares its initial value") {
        const Built built = KernelTest::Finish({
            {"owner", [](Registrar& r) {
                 r.Component<Foreign>({.name = "Foreign"});
                 r.Schedule(Tick());
             }},
            {"other", [](Registrar& r) {
                 r.Component<BodyAt>({.name = "BodyAt"});
                 r.InitialValue<Foreign, &ForeignFromRequest>();
             }},
        });
        REQUIRE_FALSE(built.result);
        CHECK_MESSAGE(HasRule(built, StartupCode::RE4, "InitialValue"), Describe(built));
        CHECK_MESSAGE(Describe(built).find("only the owner") != std::string::npos, Describe(built));
    }
    SUBCASE("the owner may take its value from another plugin's request") {
        const Built built = KernelTest::Finish({
            {"owner", [](Registrar& r) {
                 r.Component<Foreign>({.name = "Foreign"});
                 r.Schedule(Tick());
             }},
            {"other", [](Registrar& r) {
                 r.Component<BodyAt>({.name = "BodyAt"});
                 r.InitialValue<BodyAt, &RequestFromForeign>();
             }},
        });
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
    SUBCASE("the same declaration twice") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            Data(r);
            r.InitialValue<Body, &BodyFromRequest>();
            r.InitialValue<Body, &BodyFromRequest>();
        }}});
        CHECK_MESSAGE(HasRule(built, StartupCode::RE2, "InitialValue"), Describe(built));
    }
}

// ---- Cases added by the adversarial check (v8-arch-gyo2-funnel-check): each
// kills a mutant of the funnel that the cases above let through.

namespace {

struct Ballast { std::int32_t b; };  // a resource, to name a resource where a component is expected

Body BodyFromBallast(const Ballast& ballast) { return Body{ballast.b}; }
Ballast BallastFromRequest(const BodyAt& request) { return Ballast{request.x}; }
Body BodyFromMass(const Mass& mass) { return Body{mass.m * 10}; }

SystemResult RequestAdded(HookCtx ctx) {
    Seen().push_back("request:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult MassAdded(HookCtx ctx) {
    Seen().push_back("mass:" + std::to_string(ctx.Entity()));
    return Ok();
}
SystemResult LabelAdded(HookCtx ctx) {
    Seen().push_back("label:" + std::to_string(ctx.Entity()));
    return Ok();
}

// A Crate appears at the sync point after the Request stage; its OnAdd hook
// records a BodyAt, which the next sync point (after the Use stage) applies at
// its step 0, together with this window's own record (a Label insert).
SystemResult SpawnsCrate(Spawn<Bundle<Crate>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(Crate{2});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}
SystemResult CrateAsksForBody(HookCtx ctx, CommandsFor<Inserts<BodyAt>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), BodyAt{4});
    return Ok();
}
SystemResult LabelsCrates(Query<Read<Crate>> crates, CommandsFor<Inserts<Label>, Removes<>> commands, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    crates.ForEach([&commands](EntityId id, const Crate&) { commands.Insert(id, Label{1}); });
    return Ok();
}

SystemResult SpawnsMass(Spawn<Bundle<Mass>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(Mass{5});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}

} // namespace

TEST_CASE("initial value: the filled cell carries the stamp of the sync point that added the request") {
    // Added<T> and Changed<T> compare stamps: a filled cell stamped other than
    // its request would be missed by, or shown again to, a System's next run.
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
        r.InitialValue<Mass, &MassFromBody>();
        r.AddSystem<&RequestOne>(In("RequestOne", "Request"));
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    const auto request = rig->world().Stamps<BodyAt>(id);
    const auto body = rig->world().Stamps<Body>(id);
    const auto mass = rig->world().Stamps<Mass>(id);
    REQUIRE(request);
    REQUIRE(body);
    REQUIRE(mass);
    CHECK(body->first == request->first);
    CHECK(body->second == request->second);
    CHECK(mass->first == request->first);
    CHECK(mass->second == request->second);
}

TEST_CASE("initial value: OnAdd runs once for the request and once for every link of a chain") {
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
        r.InitialValue<Mass, &MassFromBody>();
        r.InitialValue<Label, &LabelFromMass>();
        r.AddSystem<&RequestOne>(In("RequestOne", "Request"));
        r.Hook<BodyAt, &RequestAdded>(HookTrigger::OnAdd, {.name = "RequestAdded"});
        r.Hook<Body, &BodyAdded>(HookTrigger::OnAdd, {.name = "BodyAdded"});
        r.Hook<Mass, &MassAdded>(HookTrigger::OnAdd, {.name = "MassAdded"});
        r.Hook<Label, &LabelAdded>(HookTrigger::OnAdd, {.name = "LabelAdded"});
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    CHECK(SeenCount("request:") == 1);
    CHECK(SeenCount("hook:") == 1);
    CHECK(SeenCount("mass:") == 1);
    CHECK(SeenCount("label:") == 1);
}

TEST_CASE("initial value: a request recorded by a hook is filled at the next sync point") {
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
        r.AddSystem<&SpawnsCrate>(In("SpawnsCrate", "Request"));
        r.AddSystem<&LabelsCrates>(In("LabelsCrates", "Use"));
        r.Hook<Crate, &CrateAsksForBody>(HookTrigger::OnAdd, {.name = "CrateAsksForBody"});
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    REQUIRE(rig->world().Get<Label>(id) != nullptr);  // the window of that sync point was not empty
    REQUIRE(rig->world().Get<BodyAt>(id) != nullptr);
    REQUIRE(rig->world().Get<Body>(id) != nullptr);
    CHECK(rig->world().Get<Body>(id)->x == 40);
    // Filled where the hook's record was applied, one sync point after the Crate.
    const auto crate = rig->world().Stamps<Crate>(id);
    const auto request = rig->world().Stamps<BodyAt>(id);
    const auto body = rig->world().Stamps<Body>(id);
    REQUIRE(crate);
    REQUIRE(request);
    REQUIRE(body);
    CHECK(crate->first < request->first);
    CHECK(body->first == request->first);
}

TEST_CASE("initial value: one request gives the first value of two components") {
    // Foreign is registered by this plugin here, so it may declare it too.
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.Component<Foreign>({.name = "Foreign"});
        r.InitialValue<Body, &BodyFromRequest>();
        r.InitialValue<Foreign, &ForeignFromRequest>();
        r.AddSystem<&RequestOne>(In("RequestOne", "Request"));
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    REQUIRE(rig->world().Get<Body>(id) != nullptr);
    REQUIRE(rig->world().Get<Foreign>(id) != nullptr);
    CHECK(rig->world().Get<Body>(id)->x == 30);
    CHECK(rig->world().Get<Foreign>(id)->f == 3);
}

TEST_CASE("initial value: a resource is neither a request nor a component with an initial value") {
    // Data registers five components, so a resource id is also a valid
    // component index: a check that looks at the id alone would pass.
    SUBCASE("the request is a resource") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            Data(r);
            r.Resource<Ballast>({.name = "Ballast"});
            r.InitialValue<Body, &BodyFromBallast>();
        }}});
        CHECK_MESSAGE(HasRule(built, StartupCode::RE4, "InitialValue"), Describe(built));
    }
    SUBCASE("the component is a resource") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            Data(r);
            r.Resource<Ballast>({.name = "Ballast"});
            r.InitialValue<Ballast, &BallastFromRequest>();
        }}});
        CHECK_MESSAGE(HasRule(built, StartupCode::RE4, "InitialValue"), Describe(built));
    }
}

TEST_CASE("initial value: two declarations that name each other stop at one sync point") {
    // Body from Mass and Mass from Body: the spawned Mass fills Body, and Body
    // does not overwrite the Mass it came from.
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromMass>();
        r.InitialValue<Mass, &MassFromBody>();
        r.AddSystem<&SpawnsMass>(In("SpawnsMass", "Request"));
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    REQUIRE(rig->world().Get<Body>(id) != nullptr);
    REQUIRE(rig->world().Get<Mass>(id) != nullptr);
    CHECK(rig->world().Get<Body>(id)->x == 50);
    CHECK(rig->world().Get<Mass>(id)->m == 5);
}

// ---- The zone rules and the fingerprints (v8-arch-gyo2-funnel-check). User
// decisions 2026-11-14: B1 to B3 are refused; the fingerprints take the
// declarations (plan A); for the zone rules, a System or hook that writes a
// request writes the components it fills (FunnelWrites in Registry.cpp), so the
// rules are the ones of a direct write, not a copy of them.

namespace {

struct Seat { std::int32_t s; };       // zone C
struct SeatInput { std::int32_t s; };  // zone C, role Input (no rollback: Z8)
struct Spark { std::int32_t s; };      // zone C, State, rolls back
struct Gate { std::int32_t g; };       // zone B
struct Ember { std::int32_t e; };      // zone C, State, rolls back: a second request of Seat
Seat SeatFromSpark(const Spark& spark) { return Seat{spark.s}; }
Gate GateFromSpark(const Spark& spark) { return Gate{spark.s}; }
Gate GateFromSeat(const Seat& seat) { return Gate{seat.s}; }
Seat SeatFromEmber(const Ember& ember) { return Seat{ember.e}; }

SystemResult SimulationSpawnsSpark(Spawn<Bundle<Spark>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(Spark{4});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}

// count: TickCount (the Runs the test counted, read by the gated Systems) is registered here.
void SeatData(Registrar& r, bool count = true) {
    r.Component<Seat>({.name = "Seat"});
    r.Component<SeatInput>({.name = "SeatInput"});
    r.Component<Spark>({.name = "Spark"});
    r.Component<Gate>({.name = "Gate"});
    r.Component<Ember>({.name = "Ember"});
    r.Resource<TestRangeAllotter>({.name = "Allotter"});
    r.Allotter<TestRangeAllotter>();
    if (count) r.Resource<WorldTest::TickCount>({.name = "TickCount"});
    r.Schedule(KernelTest::FixedSchedule());
}

SystemOptions Simulation(std::string name) {
    return KernelTest::Sys(std::move(name), {At("FixedUpdate", "Simulate")});
}

} // namespace

TEST_CASE("initial value: a declaration changes the Registry fingerprint") {
    // which: 0 none, 1 Seat from Spark, 2 Gate from Spark, 3 Gate from another request, 4 Seat from another request.
    const auto build = [](int which) {
        return KernelTest::Finish({{"test", [which](Registrar& r) {
            SeatData(r);
            if (which == 1) r.InitialValue<Seat, &SeatFromSpark>();
            if (which == 2) r.InitialValue<Gate, &GateFromSpark>();
            if (which == 3) r.InitialValue<Gate, &GateFromSeat>();
            if (which == 4) r.InitialValue<Seat, &SeatFromEmber>();
        }}});
    };
    const Built none = build(0);
    const Built zoneC = build(1);
    const Built zoneB = build(2);
    const Built otherRequest = build(3);
    const Built otherZoneCRequest = build(4);
    REQUIRE_MESSAGE(static_cast<bool>(none.result), Describe(none));
    REQUIRE_MESSAGE(static_cast<bool>(zoneC.result), Describe(zoneC));
    REQUIRE_MESSAGE(static_cast<bool>(zoneB.result), Describe(zoneB));
    REQUIRE_MESSAGE(static_cast<bool>(otherRequest.result), Describe(otherRequest));
    REQUIRE_MESSAGE(static_cast<bool>(otherZoneCRequest.result), Describe(otherZoneCRequest));
    const FrozenRegistry& base = none.registry->Frozen();
    CHECK(base.RegistryFingerprint() != zoneC.registry->Frozen().RegistryFingerprint());
    CHECK(base.RegistryFingerprint() != zoneB.registry->Frozen().RegistryFingerprint());
    CHECK(zoneC.registry->Frozen().RegistryFingerprint() != zoneB.registry->Frozen().RegistryFingerprint());
    CHECK(zoneB.registry->Frozen().RegistryFingerprint() != otherRequest.registry->Frozen().RegistryFingerprint());
    CHECK(zoneC.registry->Frozen().RegistryFingerprint() != otherZoneCRequest.registry->Frozen().RegistryFingerprint());
    CHECK(base.RegistryFingerprint().version == 9U);
}

namespace {

struct Mark { std::int32_t m; };       // zone B, does not roll back
struct GateIn { std::int32_t g; };     // zone B, role Input

// which: 1 Seat from Spark, 2 Perch from Spark, 3 both (Seat declared first), 4 both (Perch declared first).
struct Perch { std::int32_t p; };  // zone C, State: a second component filled from Spark
Perch PerchFromSpark(const Spark& spark) { return Perch{spark.s}; }
Built PerchVariant(int which) {
    return KernelTest::Finish({{"test", [which](Registrar& r) {
        SeatData(r);
        r.Component<Perch>({.name = "Perch"});
        if (which == 1 || which == 3) r.InitialValue<Seat, &SeatFromSpark>();
        if (which == 2 || which == 3 || which == 4) r.InitialValue<Perch, &PerchFromSpark>();
        if (which == 4) r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
}

} // namespace

// Added by the second adversarial check (Fable).
TEST_CASE("initial value: the fingerprints tell which component a declaration fills, not the order of the declarations") {
    const Built seat = PerchVariant(1);
    const Built perch = PerchVariant(2);
    const Built seatFirst = PerchVariant(3);
    const Built perchFirst = PerchVariant(4);
    REQUIRE_MESSAGE(static_cast<bool>(seat.result), Describe(seat));
    REQUIRE_MESSAGE(static_cast<bool>(perch.result), Describe(perch));
    REQUIRE_MESSAGE(static_cast<bool>(seatFirst.result), Describe(seatFirst));
    REQUIRE_MESSAGE(static_cast<bool>(perchFirst.result), Describe(perchFirst));
    // Same request, another component: the fingerprint differs.
    CHECK(seat.registry->Frozen().RegistryFingerprint() != perch.registry->Frozen().RegistryFingerprint());
    // The same two declarations in either order: the same fingerprint.
    CHECK(seatFirst.registry->Frozen().RegistryFingerprint() == perchFirst.registry->Frozen().RegistryFingerprint());
}

TEST_CASE("initial value: golden fingerprints of a Registry with two declarations") {
    // The first golden that holds declarations; the same on every L1 platform.
    const Built both = PerchVariant(3);
    REQUIRE_MESSAGE(static_cast<bool>(both.result), Describe(both));
    MESSAGE("two declarations: registry " << both.registry->Frozen().RegistryFingerprint().Text());
    CHECK(both.registry->Frozen().RegistryFingerprint().Text() ==
          std::string("9:d38e323a85074d24806f1b55f1e8806380edd9365f60a63002ad702f5a1fdddd"));
}


// ---- Second adversarial check (Fable) of the zone rules through the funnel,
// and the user's decisions after it (2026-11-15): a write in place or a remove
// of a request fills nothing and does not count; the declaration's Z1 holds for
// role=Input components too.
namespace {

struct Raw { std::int32_t r; };      // zone B, Shared
struct Ledger { std::int32_t n; };   // a resource, zone C

} // namespace

namespace {

} // namespace

namespace {

} // namespace

// ---- Follow-ups of the adversarial check (v8-arch-gyo2-funnel-check, S1 and S2).

namespace {

Mass MassFromRequest(const BodyAt& request) { return Mass{request.x}; }

SystemResult SpawnsRequestAndCrate(Spawn<Bundle<BodyAt, Crate>> spawn, Res<WorldTest::TickCount> tick) {
    if (tick->value != 1) return Ok();
    auto made = spawn.New(BodyAt{3}, Crate{1});
    if (!made) return GYO::Engine::Base::Err(std::move(made).error());
    return Ok();
}

} // namespace

TEST_CASE("initial value: a request that appears through a chain is looked at after the direct requests") {
    // Mass (from BodyAt) and Crate both give Body a value. Mass has the lower
    // ComponentId, but it only appears in the second round, after Crate was
    // looked at: Crate gives the value. "The lower ComponentId" holds among
    // the requests of one round.
    auto rig = Start([](Registrar& r) {
        Data(r);
        r.InitialValue<Mass, &MassFromRequest>();
        r.InitialValue<Body, &BodyFromMass>();
        r.InitialValue<Body, &BodyFromCrate>();
        r.AddSystem<&SpawnsRequestAndCrate>(In("SpawnsRequestAndCrate", "Request"));
    });
    REQUIRE(rig->executor);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    REQUIRE(rig->world().Get<Mass>(id) != nullptr);
    REQUIRE(rig->world().Get<Body>(id) != nullptr);
    CHECK(rig->world().Get<Body>(id)->x == 8);  // BodyFromCrate(1), not BodyFromMass(3) = 30
}
