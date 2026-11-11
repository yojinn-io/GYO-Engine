// Registry ids and the type-erased World the two judgments rely on.

#include "SpikeTestSupport.hpp"

#include "kernel_spike/System.hpp"

#include <cstdint>
#include <memory>
#include <utility>

using namespace KernelSpike;
using namespace KernelSpikeTest;

TEST_CASE("Registry: ids follow registration order") {
    Registry registry;
    CHECK(registry.RegisterComponent<Health>("Health") == 0);
    CHECK(registry.RegisterComponent<Position>("Position") == 1);
    CHECK(registry.RegisterComponent<Velocity>("Velocity") == 2);
    CHECK(registry.RegisterResource<Stats>("Stats") == 0);
    CHECK(registry.RegisterResource<Time>("Time") == 1);
    CHECK(registry.RegisterEvent<Spawned>("Spawned") == 0);
    CHECK(registry.RegisterEvent<Hit>("Hit") == 1);

    CHECK(registry.ComponentIdOf<Health>() == 0);
    CHECK(registry.ComponentIdOf<Position>() == 1);
    CHECK(registry.ComponentIdOf<Velocity>() == 2);
    CHECK(registry.ResourceIdOf<Stats>() == 0);
    CHECK(registry.ResourceIdOf<Time>() == 1);
    CHECK(registry.EventIdOf<Spawned>() == 0);
    CHECK(registry.EventIdOf<Hit>() == 1);

    // Another order gives other ids: the id is the order, not the type.
    const Registry standard = MakeRegistry();
    CHECK(standard.ComponentIdOf<Position>() == 0);
    CHECK(standard.ComponentIdOf<Health>() == 2);
    CHECK(standard.ResourceIdOf<Time>() == 0);
    CHECK(standard.EventIdOf<Hit>() == 0);
}

TEST_CASE("Registry: lookup does not create ids, and a type registers once") {
    Registry registry;
    registry.RegisterComponent<Position>("Position");
    SPIKE_CHECK_ASSERTS(registry.ComponentIdOf<Velocity>());
    SPIKE_CHECK_ASSERTS(registry.ResourceIdOf<Time>());
    SPIKE_CHECK_ASSERTS(registry.EventIdOf<Hit>());
    SPIKE_CHECK_ASSERTS(registry.RegisterComponent<Position>("Position again"));
    CHECK(registry.Components().size() == 1);
}

TEST_CASE("Registry: a System over an unregistered type fails when it is made") {
    Registry registry;
    registry.RegisterComponent<Position>("Position");
    SPIKE_CHECK_ASSERTS(MakeSystem(registry, "NeedsVelocity", [](Read<Position>, Write<Velocity>) {}));
}

namespace {

// Counts live objects to check the type-erased move and destroy.
struct Tracked {
    static inline int alive = 0;
    std::unique_ptr<int> value;
    explicit Tracked(int v) : value(std::make_unique<int>(v)) { ++alive; }
    Tracked(Tracked&& other) noexcept : value(std::move(other.value)) { ++alive; }
    Tracked& operator=(Tracked&&) = delete;
    ~Tracked() { --alive; }
};

struct alignas(64) Wide {
    float lanes[16];
};

} // namespace

TEST_CASE("World: type-erased columns move, align and destroy by the registered rules") {
    {
        Registry registry;
        registry.RegisterComponent<Tracked>("Tracked");
        registry.RegisterComponent<Wide>("Wide");
        World world(std::move(registry));
        for (int value = 0; value < 100; ++value) {
            world.Push(Tracked(value));
        }
        world.Push(Wide{});
        world.Push(Wide{});
        CHECK(Tracked::alive == 100);
        CHECK(world.Rows<Tracked>() == 100);
        CHECK(*world.Get<Tracked>(0).value == 0);
        CHECK(*world.Get<Tracked>(99).value == 99);
        CHECK(reinterpret_cast<std::uintptr_t>(&world.Get<Wide>(1)) % 64 == 0);
    }
    CHECK(Tracked::alive == 0);
}

TEST_CASE("World: events are readable in the same tick and cleared at the sync point") {
    World world(MakeRegistry());
    world.InsertResource(Stats{0});
    const System send = MakeSystem(world.Types(), "Send", [](EventWriter<Hit> hits) {
        hits.Send(Hit{0, 1});
        hits.Send(Hit{1, 2});
    });
    const System receive = MakeSystem(world.Types(), "Receive", [](EventReader<Hit> hits, ResMut<Stats> stats) {
        for (const Hit& hit : hits.Events()) {
            stats->count += hit.amount;
        }
    });

    SPIKE_CHECK_NO_ASSERT((send.Run(world), receive.Run(world)));
    CHECK(world.Resource<Stats>().count == 3);
    world.ClearEvents();
    SPIKE_CHECK_NO_ASSERT(receive.Run(world));
    CHECK(world.Resource<Stats>().count == 3);
}

TEST_CASE("World: deferred commands run in queue order at the sync point") {
    World world(MakeRegistry());
    world.InsertResource(Time{0});
    const System queue = MakeSystem(world.Types(), "Queue", [](Deferred deferred, Res<Time>) {
        deferred.Push([](World& target) { target.Push(Health{1}); });
        deferred.Push([](World& target) { target.Push(Health{2}); });
    });

    SPIKE_CHECK_NO_ASSERT(queue.Run(world));
    CHECK(world.Rows<Health>() == 0);
    world.ApplyDeferred();
    REQUIRE(world.Rows<Health>() == 2);
    CHECK(world.Get<Health>(0).points == 1);
    CHECK(world.Get<Health>(1).points == 2);
}
