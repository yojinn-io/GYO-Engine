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
    s.domain = Domain::Fixed;
    s.clock = true;
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
    r.Component<BodyAt>({.name = "BodyAt", .writePolicy = WritePolicy::Shared});
    r.Component<Mass>({.name = "Mass"});
    r.Component<Label>({.name = "Label"});
    r.Component<Crate>({.name = "Crate", .writePolicy = WritePolicy::Shared});
    // Every System with a Spawn parameter writes the allotter.
    r.Resource<RangeAllotter>({.name = "Allotter", .writePolicy = WritePolicy::Shared});
    r.Allotter<RangeAllotter>();
    r.Schedule(Tick());
}

// Two Systems ask for a body; neither names Body.
SystemResult RequestOne(Spawn<Bundle<BodyAt>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(BodyAt{3});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}

SystemResult RequestAnother(Spawn<Bundle<BodyAt>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(BodyAt{5});
    if (!made) return Engine::Base::Err(std::move(made).error());
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

SystemResult SpawnsBodyA(Spawn<Bundle<Body>>) { return Ok(); }
SystemResult SpawnsBodyB(Spawn<Bundle<Body>>) { return Ok(); }

std::unique_ptr<WorldTest::Rig> Start(std::function<void(Registrar&)> build) {
    Seen().clear();
    auto rig = WorldTest::MakeRig(std::move(build));
    if (rig->product) rig->world().SetResource(RangeAllotter::Of(100, 199));
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
    REQUIRE(rig->product);
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

TEST_CASE("initial value: spawning the component itself from two Systems is still two writers") {
    // The same game without the tool: both Systems put Body in their bundle.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        Data(r);
        r.AddSystem<&SpawnsBodyA>(In("SpawnsBodyA", "Request"));
        r.AddSystem<&SpawnsBodyB>(In("SpawnsBodyB", "Request"));
    }}});
    CHECK_MESSAGE(KernelTest::HasError(built, StartupCode::DE2), Describe(built));
}

namespace {

SystemResult SpawnsBoth(Spawn<Bundle<BodyAt, Body>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(BodyAt{3}, Body{-1});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}

SystemResult InsertsRequests(CommandsFor<Inserts<BodyAt>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() == 1) commands.Insert(7, BodyAt{4});   // entity 7 has no request yet
    if (tick.Tick() == 2) commands.Insert(7, BodyAt{9});   // overwrites the request
    if (tick.Tick() == 3) commands.Insert(8, BodyAt{6});   // entity 8 already has a Body
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
    REQUIRE(rig->product);
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
    REQUIRE(rig->product);
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
    REQUIRE(rig->product);
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

SystemResult SpawnsTwoRequests(Spawn<Bundle<Crate, BodyAt>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Crate{1}, BodyAt{3});
    if (!made) return Engine::Base::Err(std::move(made).error());
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
        REQUIRE(rig->product);
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
    REQUIRE(rig->product);
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
    SUBCASE("a zone C component takes its value from zone C data only") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            r.Component<Body>({.name = "Body", .zone = Zone::C});
            r.Component<BodyAt>({.name = "BodyAt", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
            r.Schedule(Tick());
            r.InitialValue<Body, &BodyFromRequest>();
        }}});
        CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "Z1"), Describe(built));
    }
    SUBCASE("zone C from zone C is accepted") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            r.Component<Body>({.name = "Body", .zone = Zone::C});
            r.Component<BodyAt>({.name = "BodyAt", .zone = Zone::C, .writePolicy = WritePolicy::Shared});
            r.Schedule(Tick());
            r.InitialValue<Body, &BodyFromRequest>();
        }}});
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
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
SystemResult SpawnsCrate(Spawn<Bundle<Crate>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Crate{2});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}
SystemResult CrateAsksForBody(HookCtx ctx, CommandsFor<Inserts<BodyAt>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), BodyAt{4});
    return Ok();
}
SystemResult LabelsCrates(Query<Read<Crate>> crates, CommandsFor<Inserts<Label>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    crates.ForEach([&commands](EntityId id, const Crate&) { commands.Insert(id, Label{1}); });
    return Ok();
}

SystemResult SpawnsMass(Spawn<Bundle<Mass>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Mass{5});
    if (!made) return Engine::Base::Err(std::move(made).error());
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
    REQUIRE(rig->product);
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
    REQUIRE(rig->product);
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
    REQUIRE(rig->product);
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
    REQUIRE(rig->product);
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
    REQUIRE(rig->product);
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
Seat SeatFromInput(const SeatInput& in) { return Seat{in.s * 2}; }
SeatInput InputFromSpark(const Spark& spark) { return SeatInput{spark.s}; }
Seat SeatFromSpark(const Spark& spark) { return Seat{spark.s}; }
Gate GateFromSpark(const Spark& spark) { return Gate{spark.s}; }
Gate GateFromSeat(const Seat& seat) { return Gate{seat.s}; }
Seat SeatFromEmber(const Ember& ember) { return Seat{ember.e}; }

SystemResult IngressAsksForSeat(CommandsFor<Inserts<SeatInput>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() == 1) commands.Insert(7, SeatInput{3});
    return Ok();
}
SystemResult SimulationSpawnsSpark(Spawn<Bundle<Spark>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Spark{4});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}

// seatRollback: Seat's rollback; worldRollback: FixedUpdate is rollback-enabled.
void SeatData(Registrar& r, Rollback seatRollback, bool worldRollback) {
    r.Component<Seat>({.name = "Seat", .zone = Zone::C, .rollback = seatRollback});
    r.Component<SeatInput>({.name = "SeatInput", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .role = Role::Input});
    r.Component<Spark>({.name = "Spark", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
    r.Component<Gate>({.name = "Gate", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
    r.Component<Ember>({.name = "Ember", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
    // Zone C, rolling back with the World: zone C Systems spawn.
    r.Resource<RangeAllotter>({.name = "Allotter", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = Rollback::Yes});
    r.Allotter<RangeAllotter>();
    r.Schedule(KernelTest::FixedSchedule(worldRollback));
}

SystemOptions Ingress(std::string name) {
    SystemOptions o = KernelTest::Sys(std::move(name), Zone::B, {At("FixedUpdate", "Ingress")});
    o.ingress = true;
    return o;
}

SystemOptions Simulation(std::string name) {
    return KernelTest::Sys(std::move(name), Zone::C, {At("FixedUpdate", "Simulate")});
}

} // namespace

TEST_CASE("initial value: zone C State data does not take its value from a role=Input request (Z2)") {
    // Only ingress (zone B) Systems write the request, so through the funnel
    // they would write zone C State data.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.InitialValue<Seat, &SeatFromInput>();
        r.AddSystem<&IngressAsksForSeat>(Ingress("IngressAsksForSeat"));
    }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "Z2"), Describe(built));
}

TEST_CASE("initial value: role=Input data does not take its value from a request that is not Input (Z3)") {
    // A zone C System may not write Input data; through the funnel its spawn would.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.InitialValue<SeatInput, &InputFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "Z3"), Describe(built));
}

TEST_CASE("initial value: in a World with rollback, zone C data takes an initial value only if it rolls back (Z6)") {
    // A zone C System in a rollback-enabled schedule may not write data that
    // does not roll back; through the funnel its spawn of Spark would.
    const Built refused = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, true);
        r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
    CHECK_MESSAGE(HasRule(refused, StartupCode::DE7, "Z6"), Describe(refused));
    // Yes and Derived pass, as for a System's own writes.
    for (const Rollback rollback : {Rollback::Yes, Rollback::Derived}) {
        CAPTURE(static_cast<int>(rollback));
        const Built accepted = KernelTest::Finish({{"test", [rollback](Registrar& r) {
            SeatData(r, rollback, true);
            r.InitialValue<Seat, &SeatFromSpark>();
            r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
        }}});
        CHECK_MESSAGE(static_cast<bool>(accepted.result), Describe(accepted));
    }
}

TEST_CASE("initial value: the combinations the zone rules allow") {
    SUBCASE("role=Input from role=Input: ingress writes both") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            r.Component<SeatInput>({.name = "SeatInput", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .role = Role::Input});
            r.Component<Seat>({.name = "Seat", .zone = Zone::C, .role = Role::Input});
            r.Schedule(KernelTest::FixedSchedule(true));
            r.InitialValue<Seat, &SeatFromInput>();
        }}});
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
    SUBCASE("zone C State that does not roll back, in a World without rollback") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::No, false);
            r.InitialValue<Seat, &SeatFromSpark>();
            r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
        }}});
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
}

TEST_CASE("initial value: a declaration changes the Registry fingerprint and, for zone C, the C fingerprint") {
    // which: 0 none, 1 a zone C component (Seat), 2 a zone B component (Gate),
    // 3 the same zone B component from another request, 4 the same zone C
    // component from another zone C request.
    const auto build = [](int which) {
        return KernelTest::Finish({{"test", [which](Registrar& r) {
            SeatData(r, Rollback::No, false);
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
    CHECK(base.CFingerprint() != zoneC.registry->Frozen().CFingerprint());
    CHECK(base.CFingerprint() == zoneB.registry->Frozen().CFingerprint());  // zone B only: the C part is the same
    // One zone C declaration each, from different requests: the C part differs.
    CHECK(zoneC.registry->Frozen().CFingerprint() != otherZoneCRequest.registry->Frozen().CFingerprint());
    CHECK(base.RegistryFingerprint().version == 2U);
    CHECK(base.CFingerprint().version == 5U);
}

namespace {

struct Mark { std::int32_t m; };       // zone B, does not roll back
struct GateIn { std::int32_t g; };     // zone B, role Input
SeatInput InputFromSeat(const Seat& seat) { return SeatInput{seat.s}; }
Mark MarkFromGate(const Gate& gate) { return Mark{gate.g}; }
Gate GateFromInput(const SeatInput& in) { return Gate{in.s}; }
GateIn GateInFromGate(const Gate& gate) { return GateIn{gate.g}; }
Ember EmberFromSpark(const Spark& spark) { return Ember{spark.s}; }

SystemResult SparkAsksForSeat(HookCtx ctx, CommandsFor<Inserts<Ember>, Removes<>> commands) {
    commands.Insert(ctx.Entity(), Ember{1});
    return Ok();
}
SystemResult WritesGate(CommandsFor<Inserts<Gate>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() == 1) commands.Insert(7, Gate{1});
    return Ok();
}
SystemResult WritesGateIn(Query<Write<GateIn>>) { return Ok(); }

SystemOptions Presentation(std::string name) {
    return KernelTest::Sys(std::move(name), Zone::B, {At("FixedUpdate", "Present")});
}

// which: 1 Seat from Spark, 2 Perch from Spark, 3 both (Seat declared first), 4 both (Perch declared first).
struct Perch { std::int32_t p; };  // zone C, State: a second component filled from Spark
Perch PerchFromSpark(const Spark& spark) { return Perch{spark.s}; }
Built PerchVariant(int which) {
    return KernelTest::Finish({{"test", [which](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.Component<Perch>({.name = "Perch", .zone = Zone::C});
        if (which == 1 || which == 3) r.InitialValue<Seat, &SeatFromSpark>();
        if (which == 2 || which == 3 || which == 4) r.InitialValue<Perch, &PerchFromSpark>();
        if (which == 4) r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
}

} // namespace

TEST_CASE("initial value: the error names the System that writes the request and the declaration it goes through") {
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, true);
        r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
    REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z6"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("SimulationSpawnsSpark") != std::string::npos, Describe(built));
    CHECK_MESSAGE(Describe(built).find("filled from 'Spark' by Registrar::InitialValue") != std::string::npos, Describe(built));
}

TEST_CASE("initial value: a zone C hook meets the hook's Z6 through the funnel too (Yes only, not Derived)") {
    // Seat is Derived: a zone C System may write it (Z6 takes Yes or Derived),
    // a zone C hook may not (Yes only). The hook writes Ember, which rolls back;
    // Ember fills Seat, so the hook writes Seat.
    const auto build = [](bool hook) {
        return KernelTest::Finish({{"test", [hook](Registrar& r) {
            SeatData(r, Rollback::Derived, true);
            r.InitialValue<Seat, &SeatFromEmber>();
            if (hook) {
                r.Hook<Spark, &SparkAsksForSeat>(HookTrigger::OnAdd, {.name = "SparkAsksForSeat", .zone = Zone::C});
            } else {
                r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
                r.InitialValue<Ember, &EmberFromSpark>();
            }
        }}});
    };
    const Built byHook = build(true);
    CHECK_MESSAGE(HasRule(byHook, StartupCode::DE7, "Z6"), Describe(byHook));
    const Built bySystem = build(false);
    CHECK_MESSAGE(static_cast<bool>(bySystem.result), Describe(bySystem));
}

TEST_CASE("initial value: Z6 is checked per schedule, as for a direct write") {
    // FixedUpdate rolls back, Update does not: the same spawn of Spark fills a
    // Seat that does not roll back; refused in FixedUpdate only.
    const auto build = [](const char* schedule, const char* stage) {
        return KernelTest::Finish({{"test", [schedule, stage](Registrar& r) {
            SeatData(r, Rollback::No, true);
            r.Schedule(KernelTest::UpdateSchedule());
            r.InitialValue<Seat, &SeatFromSpark>();
            r.AddSystem<&SimulationSpawnsSpark>(KernelTest::Sys("SimulationSpawnsSpark", Zone::C, {At(schedule, stage)}));
        }}});
    };
    const Built fixed = build("FixedUpdate", "Simulate");
    CHECK_MESSAGE(HasRule(fixed, StartupCode::DE7, "Z6"), Describe(fixed));
    const Built update = build("Update", "First");
    CHECK_MESSAGE(static_cast<bool>(update.result), Describe(update));
}

TEST_CASE("initial value: a chain of declarations carries the write to its last link") {
    // Spark fills Seat, Seat fills SeatInput: the zone C System that spawns
    // Spark writes role=Input data (Z3).
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.InitialValue<Seat, &SeatFromSpark>();
        r.InitialValue<SeatInput, &InputFromSeat>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "Z3"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("filled from 'Seat'") != std::string::npos, Describe(built));
}

TEST_CASE("initial value: a zone C System's request does not fill zone B data (Z1)") {
    // A zone C System does not write zone B data; through the funnel it would.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.InitialValue<Gate, &GateFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "Z1"), Describe(built));
}

TEST_CASE("initial value: what a direct write may do, the funnel may do") {
    SUBCASE("an ingress System fills zone B data from its role=Input request") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::No, false);
            r.InitialValue<Gate, &GateFromInput>();
            r.AddSystem<&IngressAsksForSeat>(Ingress("IngressAsksForSeat"));
        }}});
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
    SUBCASE("a zone B System fills zone B data that does not roll back, in a World with rollback") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::Yes, true);
            r.Component<Mark>({.name = "Mark", .zone = Zone::B});
            r.InitialValue<Mark, &MarkFromGate>();
            r.AddSystem<&WritesGate>(Presentation("WritesGate"));
        }}});
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
    SUBCASE("a zone B role=Input component: a zone B System writes it directly, so through the funnel too") {
        const auto build = [](bool funnel) {
            return KernelTest::Finish({{"test", [funnel](Registrar& r) {
                SeatData(r, Rollback::No, false);
                r.Component<GateIn>({.name = "GateIn", .zone = Zone::B, .role = Role::Input});
                if (funnel) {
                    r.InitialValue<GateIn, &GateInFromGate>();
                    r.AddSystem<&WritesGate>(Presentation("WritesGate"));
                } else {
                    r.AddSystem<&WritesGateIn>(Presentation("WritesGateIn"));
                }
            }}});
        };
        const Built direct = build(false);
        const Built funnel = build(true);
        CHECK_MESSAGE(static_cast<bool>(direct.result), Describe(direct));
        CHECK_MESSAGE(static_cast<bool>(funnel.result), Describe(funnel));
    }
    SUBCASE("no System or hook writes the request: nothing is filled, nothing to check") {
        // Only the Host could write SeatInput here, and the Host's writes are not filled.
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::No, true);
            r.InitialValue<Seat, &SeatFromInput>();
        }}});
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
}

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
    // Same request, another zone C component: both fingerprints differ.
    CHECK(seat.registry->Frozen().RegistryFingerprint() != perch.registry->Frozen().RegistryFingerprint());
    CHECK(seat.registry->Frozen().CFingerprint() != perch.registry->Frozen().CFingerprint());
    // The same two declarations in either order: the same fingerprints.
    CHECK(seatFirst.registry->Frozen().RegistryFingerprint() == perchFirst.registry->Frozen().RegistryFingerprint());
    CHECK(seatFirst.registry->Frozen().CFingerprint() == perchFirst.registry->Frozen().CFingerprint());
}

TEST_CASE("initial value: golden fingerprints of a Registry with two declarations") {
    // The first golden that holds declarations; the same on every L1 platform.
    const Built both = PerchVariant(3);
    REQUIRE_MESSAGE(static_cast<bool>(both.result), Describe(both));
    MESSAGE("two declarations: registry " << both.registry->Frozen().RegistryFingerprint().Text() << " C "
                                          << both.registry->Frozen().CFingerprint().Text());
    CHECK(both.registry->Frozen().RegistryFingerprint().Text() ==
          std::string("2:6ca07796f68037287651d03a41b71f7a36b5573e96d9d1032da293a441fbd8ab"));
    CHECK(both.registry->Frozen().CFingerprint().Text() ==
          std::string("5:d5659ccfd90ade8172e3f54e2b449c3d19d490d9b3ccc143f9d222795a11ba9c"));
}

namespace {
Gate GateFromEmber(const Ember& ember) { return Gate{ember.e}; }
} // namespace

TEST_CASE("initial value: a zone C hook's request does not fill zone B data (Z1, hook path)") {
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.InitialValue<Gate, &GateFromEmber>();
        r.Hook<Spark, &SparkAsksForSeat>(HookTrigger::OnAdd, {.name = "SparkAsksForSeat", .zone = Zone::C});
    }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "Z1"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("hook 'SparkAsksForSeat'") != std::string::npos, Describe(built));
}

// ---- Second adversarial check (Fable) of the zone rules through the funnel,
// and the user's decisions after it (2026-11-15): a write in place or a remove
// of a request fills nothing and does not count; the declaration's Z1 holds for
// role=Input components too.
namespace {

struct Raw { std::int32_t r; };      // zone B, Shared
struct Ledger { std::int32_t n; };   // a resource, zone C
SeatInput InputFromRaw(const Raw& raw) { return SeatInput{raw.r}; }
Spark SparkFromEmber(const Ember& ember) { return Spark{ember.e}; }

SystemResult MutatesSpark(Query<Write<Spark>> sparks) {
    sparks.ForEach([](EntityId, Mut<Spark> spark) { spark->s += 1; });
    return Ok();
}
SystemResult ReadsSpark(Query<Read<Spark>> sparks) {
    sparks.ForEach([](EntityId, const Spark&) {});
    return Ok();
}
SystemResult FiltersBySpark(Query<Read<Ember>, With<Spark>> embers) {
    embers.ForEach([](EntityId, const Ember&) {});
    return Ok();
}
SystemResult IngressInsertsRaw(CommandsFor<Inserts<Raw>, Removes<>> commands, CurrentTick tick) {
    if (tick.Tick() == 1) commands.Insert(7, Raw{3});
    return Ok();
}
SystemResult SpawnsSparkAndEmber(Spawn<Bundle<Spark, Ember>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Spark{4}, Ember{5});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}
SystemResult SpawnsSparkAndSeat(Spawn<Bundle<Spark, Seat>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Spark{4}, Seat{5});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}
SystemResult SpawnsEmberWritesLedger(Spawn<Bundle<Ember>> spawn, ResMut<Ledger> ledger, CurrentTick tick) {
    (void)ledger;
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Ember{5});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}

// FixedUpdate rolls back, Update does not. The spawner of Spark sits in Update, so the
// Seat it fills (which does not roll back) is allowed; extra adds a System to FixedUpdate.
Built SeatNoRollbackWith(std::function<void(Registrar&)> extra) {
    return KernelTest::Finish({{"test", [extra = std::move(extra)](Registrar& r) {
        SeatData(r, Rollback::No, true);
        r.Schedule(KernelTest::UpdateSchedule());
        r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(KernelTest::Sys("SimulationSpawnsSpark", Zone::C, {At("Update", "First")}));
        extra(r);
    }}});
}

} // namespace

TEST_CASE("initial value: a System that only reads or filters by a request does not write the component it fills") {
    const Built base = SeatNoRollbackWith([](Registrar&) {});
    REQUIRE_MESSAGE(static_cast<bool>(base.result), Describe(base));
    const Built reads = SeatNoRollbackWith([](Registrar& r) { r.AddSystem<&ReadsSpark>(Simulation("ReadsSpark")); });
    CHECK_MESSAGE(static_cast<bool>(reads.result), Describe(reads));
    const Built filters = SeatNoRollbackWith([](Registrar& r) { r.AddSystem<&FiltersBySpark>(Simulation("FiltersBySpark")); });
    CHECK_MESSAGE(static_cast<bool>(filters.result), Describe(filters));
}

// In place: Write<T> adds no cell, so it fills nothing (user decision 2026-11-15).
TEST_CASE("initial value: a System that writes request cells in place does not write the component") {
    const Built built = SeatNoRollbackWith([](Registrar& r) { r.AddSystem<&MutatesSpark>(Simulation("MutatesSpark")); });
    CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
}

TEST_CASE("initial value: a component the System writes itself gets no funnel note") {
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, true);
        r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SpawnsSparkAndSeat>(Simulation("SpawnsSparkAndSeat"));
    }}});
    REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z6"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("writes 'Seat', which does not roll back") != std::string::npos, Describe(built));
    CHECK_MESSAGE(Describe(built).find("'Seat' (filled from") == std::string::npos, Describe(built));
}

TEST_CASE("initial value: a resource with the id of a filled component gets no funnel note") {
    // ResourceId: KernelSettings 0, Allotter 1, Ledger 2. ComponentId: Seat 0, SeatInput 1, Spark 2.
    const auto build = [](Rollback ledger) {
        return KernelTest::Finish({{"test", [ledger](Registrar& r) {
            SeatData(r, Rollback::No, true);
            r.Resource<Ledger>({.name = "Ledger", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .rollback = ledger});
            r.InitialValue<Spark, &SparkFromEmber>();
            r.AddSystem<&SpawnsEmberWritesLedger>(Simulation("SpawnsEmberWritesLedger"));
        }}});
    };
    const Built ids = build(Rollback::Yes);
    REQUIRE_MESSAGE(static_cast<bool>(ids.result), Describe(ids));
    REQUIRE(ids.registry->Frozen().ResourceIdOf<Ledger>() == ids.registry->Frozen().ComponentIdOf<Spark>());
    const Built built = build(Rollback::No);
    REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z6"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("'Ledger', which does not roll back") != std::string::npos, Describe(built));
    CHECK_MESSAGE(Describe(built).find("'Ledger' (filled from") == std::string::npos, Describe(built));
}

TEST_CASE("initial value: every zone error of a filled component says which request fills it") {
    SUBCASE("Z1: zone C System, zone B component") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::No, false);
            r.InitialValue<Gate, &GateFromSpark>();
            r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
        }}});
        REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z1"), Describe(built));
        CHECK_MESSAGE(Describe(built).find("writes 'Gate' (filled from 'Spark' by Registrar::InitialValue)") != std::string::npos,
                      Describe(built));
    }
    SUBCASE("Z2: ingress System, zone C State component") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::No, false);
            r.InitialValue<Seat, &SeatFromInput>();
            r.AddSystem<&IngressAsksForSeat>(Ingress("IngressAsksForSeat"));
        }}});
        REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z2"), Describe(built));
        CHECK_MESSAGE(Describe(built).find("'Seat' (filled from 'SeatInput' by Registrar::InitialValue)") != std::string::npos,
                      Describe(built));
    }
    SUBCASE("Z6: zone C hook, Derived component") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::Derived, true);
            r.InitialValue<Seat, &SeatFromEmber>();
            r.Hook<Spark, &SparkAsksForSeat>(HookTrigger::OnAdd, {.name = "SparkAsksForSeat", .zone = Zone::C});
        }}});
        REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z6"), Describe(built));
        CHECK_MESSAGE(Describe(built).find("hook 'SparkAsksForSeat' is zone C and writes 'Seat' (filled from 'Ember' by "
                                           "Registrar::InitialValue)") != std::string::npos,
                      Describe(built));
    }
}

TEST_CASE("initial value: a hook that records the request is not a writer of the Exclusive component") {
    // Body is Exclusive and Advance writes it; the hook asks through BodyAt.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
        r.AddSystem<&SpawnsCrate>(In("SpawnsCrate", "Request"));
        r.AddSystem<&Advance>(In("Advance", "Use"));
        r.Hook<Crate, &CrateAsksForBody>(HookTrigger::OnAdd, {.name = "CrateAsksForBody"});
    }}});
    CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
}

TEST_CASE("initial value: with two written requests of one component the error names one of them, the same every time") {
    // Current rule: the request visited first, which is the one with the higher ComponentId
    // (Ember), although on an entity with both the lower one (Spark) gives the value.
    for (const bool emberFirst : {false, true}) {
        CAPTURE(emberFirst);
        const Built built = KernelTest::Finish({{"test", [emberFirst](Registrar& r) {
            SeatData(r, Rollback::No, true);
            if (emberFirst) r.InitialValue<Seat, &SeatFromEmber>();
            r.InitialValue<Seat, &SeatFromSpark>();
            if (!emberFirst) r.InitialValue<Seat, &SeatFromEmber>();
            r.AddSystem<&SpawnsSparkAndEmber>(Simulation("SpawnsSparkAndEmber"));
        }}});
        REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z6"), Describe(built));
        CHECK_MESSAGE(Describe(built).find("writes 'Seat' (filled from 'Ember' by Registrar::InitialValue)") != std::string::npos,
                      Describe(built));
    }
}

TEST_CASE("initial value: a zone C role=Input component does not take its value from zone B data either") {
    // Current rule (the kept declaration rule). An ingress System may write SeatInput itself
    // from zone B data; through the funnel the same is refused where it is declared.
    const auto build = [](bool funnel) {
        return KernelTest::Finish({{"test", [funnel](Registrar& r) {
            SeatData(r, Rollback::No, false);
            r.Component<Raw>({.name = "Raw", .zone = Zone::B, .writePolicy = WritePolicy::Shared});
            if (funnel) {
                r.InitialValue<SeatInput, &InputFromRaw>();
                r.AddSystem<&IngressInsertsRaw>(Ingress("IngressInsertsRaw"));
            } else {
                r.AddSystem<&IngressAsksForSeat>(Ingress("IngressWritesSeatInput"));
            }
        }}});
    };
    const Built direct = build(false);
    CHECK_MESSAGE(static_cast<bool>(direct.result), Describe(direct));
    const Built funnel = build(true);
    CHECK_MESSAGE(HasRule(funnel, StartupCode::DE7, "Z1"), Describe(funnel));
}

TEST_CASE("initial value: every component a request fills is checked, not only the first") {
    // Spark fills Seat (ComponentId 0, allowed) and Gate (ComponentId 3, zone B: Z1).
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.InitialValue<Gate, &GateFromSpark>();
        r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
    }}});
    REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z1"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("'Gate'") != std::string::npos, Describe(built));
}

TEST_CASE("initial value: an ingress System fills role=Input data from its role=Input request") {
    // The case "role=Input from role=Input: ingress writes both" above has no System, so since
    // c94c303f it checks nothing; here an ingress System writes the request.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        r.Component<SeatInput>({.name = "SeatInput", .zone = Zone::C, .writePolicy = WritePolicy::Shared, .role = Role::Input});
        r.Component<Seat>({.name = "Seat", .zone = Zone::C, .role = Role::Input});
        r.Schedule(KernelTest::FixedSchedule(false));
        r.InitialValue<Seat, &SeatFromInput>();
        r.AddSystem<&IngressAsksForSeat>(Ingress("IngressAsksForSeat"));
    }}});
    CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
}

namespace {

SystemResult RemovesSpark(CommandsFor<Inserts<>, Removes<Spark>> commands, CurrentTick tick) {
    if (tick.Tick() == 2) commands.Remove<Spark>(7);
    return Ok();
}
// "Use the request, then take it away": a zone C hook on the filled component removes the request.
SystemResult SeatAddedRemovesSpark(HookCtx ctx, CommandsFor<Inserts<>, Removes<Spark>> commands) {
    commands.Remove<Spark>(ctx.Entity());
    return Ok();
}
SystemResult SpawnsSparkWritesSeat(Spawn<Bundle<Spark>> spawn, Query<Write<Seat>> seats, CurrentTick tick) {
    seats.ForEach([](EntityId, Mut<Seat> seat) { seat->s += 1; });
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Spark{4});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}

} // namespace

TEST_CASE("initial value: a System or hook that only removes a request does not write the component") {
    SUBCASE("a zone C System in a rollback-enabled schedule removes Spark") {
        const Built built = SeatNoRollbackWith([](Registrar& r) { r.AddSystem<&RemovesSpark>(Simulation("RemovesSpark")); });
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
    SUBCASE("a zone C hook removes the request once the Derived component is filled") {
        const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
            SeatData(r, Rollback::Derived, true);
            r.InitialValue<Seat, &SeatFromSpark>();
            r.AddSystem<&SimulationSpawnsSpark>(Simulation("SimulationSpawnsSpark"));
            r.Hook<Seat, &SeatAddedRemovesSpark>(HookTrigger::OnAdd, {.name = "SeatAddedRemovesSpark", .zone = Zone::C});
        }}});
        CHECK_MESSAGE(static_cast<bool>(built.result), Describe(built));
    }
}

TEST_CASE("initial value: a chain goes on through a component the System also writes in place") {
    // The System spawns Spark and writes Seat in place; Spark fills Seat, Seat
    // fills SeatInput: the spawn still writes SeatInput (Z3), though the System
    // names Seat itself.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, false);
        r.InitialValue<Seat, &SeatFromSpark>();
        r.InitialValue<SeatInput, &InputFromSeat>();
        r.AddSystem<&SpawnsSparkWritesSeat>(Simulation("SpawnsSparkWritesSeat"));
    }}});
    CHECK_MESSAGE(HasRule(built, StartupCode::DE7, "Z3"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("'SeatInput' (filled from 'Seat'") != std::string::npos, Describe(built));
}

namespace {

SystemResult SpawnsEmberAndSpark(Spawn<Bundle<Ember, Spark>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(Ember{5}, Spark{4});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}

} // namespace

TEST_CASE("initial value: a component the System writes in place is its own write, not a funnel write") {
    // The System writes Seat in place and spawns Spark, which fills Seat: the
    // error is about its own write of Seat, with no funnel note.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, true);
        r.InitialValue<Seat, &SeatFromSpark>();
        r.AddSystem<&SpawnsSparkWritesSeat>(Simulation("SpawnsSparkWritesSeat"));
    }}});
    REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z6"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("writes 'Seat', which does not roll back") != std::string::npos, Describe(built));
    CHECK_MESSAGE(Describe(built).find("'Seat' (filled from") == std::string::npos, Describe(built));
}

TEST_CASE("initial value: the request an error names does not depend on the order of the bundle") {
    // Same as the case with two written requests, with the bundle in the other order.
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        SeatData(r, Rollback::No, true);
        r.InitialValue<Seat, &SeatFromSpark>();
        r.InitialValue<Seat, &SeatFromEmber>();
        r.AddSystem<&SpawnsEmberAndSpark>(Simulation("SpawnsEmberAndSpark"));
    }}});
    REQUIRE_MESSAGE(HasRule(built, StartupCode::DE7, "Z6"), Describe(built));
    CHECK_MESSAGE(Describe(built).find("writes 'Seat' (filled from 'Ember' by Registrar::InitialValue)") != std::string::npos,
                  Describe(built));
}

// ---- Follow-ups of the adversarial check (v8-arch-gyo2-funnel-check, S1 and S2).

namespace {

Mass MassFromRequest(const BodyAt& request) { return Mass{request.x}; }

SystemResult SpawnsRequestAndCrate(Spawn<Bundle<BodyAt, Crate>> spawn, CurrentTick tick) {
    if (tick.Tick() != 1) return Ok();
    auto made = spawn.New(BodyAt{3}, Crate{1});
    if (!made) return Engine::Base::Err(std::move(made).error());
    return Ok();
}

} // namespace

TEST_CASE("initial value: the source file of a zone C declaration is in the zone C sources export") {
    // The function that computes zone C data is zone C code: the determinism
    // scan (spec G-5) must see the file that declares it, System or not.
    const auto build = [](bool declare) {
        return KernelTest::Finish({{"test", [declare](Registrar& r) {
            SeatData(r, Rollback::Yes, false);
            if (declare) r.InitialValue<Seat, &SeatFromSpark>();
        }}});
    };
    ZoneCSourcesOptions options{GYO_KERNEL_TEST_REPOSITORY_ROOT, "tests/common/kernel", {}, "0000000"};
    const Built with = build(true);
    KERNEL_REQUIRE_OK(with);
    const auto exported = with.registry->Frozen().ExportZoneCSources(options);
    REQUIRE(static_cast<bool>(exported));
    CHECK_MESSAGE(exported.value().find("tests/common/kernel/InitialValueTests.cpp") != std::string::npos, exported.value());
    // Without the declaration this Registry has no zone C code in this file.
    const Built without = build(false);
    KERNEL_REQUIRE_OK(without);
    const auto bare = without.registry->Frozen().ExportZoneCSources(options);
    REQUIRE(static_cast<bool>(bare));
    CHECK_MESSAGE(bare.value().find("InitialValueTests.cpp") == std::string::npos, bare.value());
}

TEST_CASE("initial value: a zone B declaration adds no file to the zone C sources export") {
    const Built built = KernelTest::Finish({{"test", [](Registrar& r) {
        Data(r);
        r.InitialValue<Body, &BodyFromRequest>();
    }}});
    KERNEL_REQUIRE_OK(built);
    ZoneCSourcesOptions options{GYO_KERNEL_TEST_REPOSITORY_ROOT, "tests/common/kernel", {}, "0000000"};
    const auto exported = built.registry->Frozen().ExportZoneCSources(options);
    REQUIRE(static_cast<bool>(exported));
    CHECK_MESSAGE(exported.value().find("InitialValueTests.cpp") == std::string::npos, exported.value());
}

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
    REQUIRE(rig->product);
    KERNEL_REQUIRE_RUN_OK(rig->Run());
    REQUIRE(rig->world().Entities().size() == 1);
    const EntityId id = rig->world().Entities()[0];
    REQUIRE(rig->world().Get<Mass>(id) != nullptr);
    REQUIRE(rig->world().Get<Body>(id) != nullptr);
    CHECK(rig->world().Get<Body>(id)->x == 8);  // BodyFromCrate(1), not BodyFromMass(3) = 30
}
