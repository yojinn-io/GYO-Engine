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
