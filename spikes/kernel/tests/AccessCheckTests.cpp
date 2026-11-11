// Judgment 2 of v8-arch-kernel-spike: restricted handles plus the debug-build
// World access check stop three bypasses, and a release build does not:
//   (a) keep a reference obtained through Read<T> past the System and write
//       through it later;
//   (b) write through a raw pointer inside a System;
//   (c) read (or write) the World through an id the System did not declare.
// "Limit:" cases record bypass shapes the check does not see, in both builds;
// "Control:" cases show that declared access never trips the check.
//
// gyo_kernel_spike_tests_debug builds this file with the access checks on,
// gyo_kernel_spike_tests_release with NDEBUG (checks off).

#include "SpikeTestSupport.hpp"

#include "kernel_spike/System.hpp"

#include <cstddef>
#include <vector>

using namespace KernelSpike;
using namespace KernelSpikeTest;

namespace {

void Populate(World& world) {
    world.Push(Position{1.0f, 2.0f});
    world.Push(Position{3.0f, 4.0f});
    world.Push(Velocity{0.5f, 0.0f});
    world.Push(Velocity{0.5f, 0.0f});
    world.Push(Health{10});
    world.Push(Health{20});
    world.InsertResource(Time{0});
    world.InsertResource(Gravity{9.0f});
    world.InsertResource(Stats{0});
    world.InsertResource(Inventory{{1, 2, 3}});
}

constexpr bool kChecksOn = KERNEL_SPIKE_ACCESS_CHECKS != 0;

} // namespace

TEST_CASE("Build: the debug target checks World access and the release target does not") {
#if KERNEL_SPIKE_TEST_EXPECT_CHECKS
    CHECK_MESSAGE(kChecksOn, "debug build must check World access");
#else
    CHECK_MESSAGE(!kChecksOn, "release (NDEBUG) build must not check World access");
#endif
}

// ---- Control

TEST_CASE("Control: declared reads and writes never trip the check") {
    World world(MakeRegistry());
    Populate(world);
    const Registry& types = world.Types();

    const System heal = MakeSystem(types, "Heal", [](Write<Health> health) {
        for (std::size_t row = 0; row < health.Size(); ++row) {
            health[row].points += 1;
        }
    });
    const System fall = MakeSystem(types, "Fall", [](Write<Velocity> velocity, Res<Gravity> gravity) {
        for (std::size_t row = 0; row < velocity.Size(); ++row) {
            velocity[row].dy -= gravity->g;
        }
    });
    const System announce = MakeSystem(types, "Announce", [](EventWriter<Spawned> spawned, Deferred deferred) {
        spawned.Send(Spawned{7});
        deferred.Push([](World& target) { target.Push(Health{1}); });
    });
    const System listen = MakeSystem(types, "Listen", [](EventReader<Spawned> spawned, ResMut<Stats> stats) {
        stats->count += static_cast<int>(spawned.Events().size());
    });
    float total = 0.0f;
    const System measure = MakeSystem(types, "Measure", [&total](Read<Position> position) {
        total += position[0].x + position[1].x;
    });

    SPIKE_CHECK_NO_ASSERT(
        (heal.Run(world), fall.Run(world), announce.Run(world), listen.Run(world), measure.Run(world)));
    world.ApplyDeferred();
    CHECK(world.Get<Health>(0).points == 11);
    CHECK(world.Get<Velocity>(1).dy == doctest::Approx(-9.0f));
    CHECK(world.Resource<Stats>().count == 1);
    CHECK(world.Rows<Health>() == 3);
    CHECK(total == doctest::Approx(4.0f));
}

TEST_CASE("Control: host writes through the World API between runs are not reported") {
    World world(MakeRegistry());
    Populate(world);
    const System reader = MakeSystem(world.Types(), "Reader", [](Read<Position>, Res<Time>) {});

    SPIKE_CHECK_NO_ASSERT(reader.Run(world));
    world.GetMut<Position>(0).x = 50.0f;
    world.ResourceMut<Time>().tick = 3;
    world.Push(Position{5.0f, 6.0f});
    world.ClearEvents();
    SPIKE_CHECK_NO_ASSERT(reader.Run(world));
    CHECK(world.Get<Position>(0).x == doctest::Approx(50.0f));
}

// ---- (a) A reference kept past the System, written later

TEST_CASE("Bypass (a1): reference kept from Read<T>, written in a System without Write<T>") {
    World world(MakeRegistry());
    Populate(world);
    const Position* kept = nullptr;
    const System leak = MakeSystem(world.Types(), "Leak", [&kept](Read<Position> position) { kept = &position[0]; });
    const System tamper = MakeSystem(world.Types(), "Tamper", [&kept](Read<Velocity>) {
        const_cast<Position*>(kept)->x = 99.0f;
    });

    SPIKE_CHECK_NO_ASSERT(leak.Run(world));
    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(tamper.Run(world));
    } else {
        SPIKE_CHECK_NO_ASSERT(tamper.Run(world));
        CHECK(world.Get<Position>(0).x == doctest::Approx(99.0f));
    }
}

TEST_CASE("Bypass (a2): reference kept from Read<T>, written by host code outside any System") {
    World world(MakeRegistry());
    Populate(world);
    const Position* kept = nullptr;
    const System leak = MakeSystem(world.Types(), "Leak", [&kept](Read<Position> position) { kept = &position[1]; });
    const System next = MakeSystem(world.Types(), "Next", [](Res<Time>) {});

    SPIKE_CHECK_NO_ASSERT(leak.Run(world));
    const_cast<Position*>(kept)->y = -1.0f;  // no System runs; the World API is not used
    if (kChecksOn) {
        // Reported when the next System starts, whatever that System declares.
        SPIKE_CHECK_ASSERTS(next.Run(world));
    } else {
        SPIKE_CHECK_NO_ASSERT(next.Run(world));
        CHECK(world.Get<Position>(1).y == doctest::Approx(-1.0f));
    }
}

// ---- (b) Raw pointer writes inside a System

TEST_CASE("Bypass (b1): const_cast write through the reference Read<T> returns") {
    World world(MakeRegistry());
    Populate(world);
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Position> position) {
        const_cast<Position&>(position[0]).x = 42.0f;
    });

    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(cheat.Run(world));
    } else {
        SPIKE_CHECK_NO_ASSERT(cheat.Run(world));
        CHECK(world.Get<Position>(0).x == doctest::Approx(42.0f));
    }
}

TEST_CASE("Bypass (b2): byte writes through a raw pointer into a read-only column") {
    World world(MakeRegistry());
    Populate(world);
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Health> health, Write<Velocity>) {
        auto* bytes = reinterpret_cast<unsigned char*>(const_cast<Health*>(&health[1]));
        bytes[0] ^= 0xFFu;
    });

    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(cheat.Run(world));
    } else {
        SPIKE_CHECK_NO_ASSERT(cheat.Run(world));
        CHECK(world.Get<Health>(1).points != 20);
    }
}

TEST_CASE("Bypass (b3): const_cast write through Res<T>") {
    World world(MakeRegistry());
    Populate(world);
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Res<Gravity> gravity) {
        const_cast<Gravity*>(&*gravity)->g = 0.0f;
    });

    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(cheat.Run(world));
    } else {
        SPIKE_CHECK_NO_ASSERT(cheat.Run(world));
        CHECK(world.Resource<Gravity>().g == doctest::Approx(0.0f));
    }
}

// ---- (c) The World through an undeclared id

TEST_CASE("Bypass (c1): undeclared component read through a captured World") {
    World world(MakeRegistry());
    Populate(world);
    float seen = 0.0f;
    bool passed = false;
    const System peek = MakeSystem(world.Types(), "Peek", [&world, &seen, &passed](Read<Position>) {
        seen = world.Get<Velocity>(0).dx;
        passed = true;
    });

    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(peek.Run(world));
        CHECK_MESSAGE(!passed, "the check must stop the System at the access, not at its end");
    } else {
        SPIKE_CHECK_NO_ASSERT(peek.Run(world));
        CHECK(passed);
        CHECK(seen == doctest::Approx(0.5f));
    }
}

TEST_CASE("Bypass (c2): undeclared resource read by id through a captured World") {
    World world(MakeRegistry());
    Populate(world);
    float seen = 0.0f;
    const ResourceId gravityId = world.Types().ResourceIdOf<Gravity>();
    bool passed = false;
    const System peek = MakeSystem(world.Types(), "Peek", [&world, &seen, &passed, gravityId](Res<Time>) {
        seen = static_cast<const Gravity*>(world.ReadResource(gravityId))->g;
        passed = true;
    });

    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(peek.Run(world));
        CHECK_MESSAGE(!passed, "the check must stop the System at the access, not at its end");
    } else {
        SPIKE_CHECK_NO_ASSERT(peek.Run(world));
        CHECK(passed);
        CHECK(seen == doctest::Approx(9.0f));
    }
}

TEST_CASE("Bypass (c3): write through a captured World to a component declared only as read") {
    World world(MakeRegistry());
    Populate(world);
    bool passed = false;
    const System escalate = MakeSystem(world.Types(), "Escalate", [&world, &passed](Read<Position>) {
        world.GetMut<Position>(0).x = 7.0f;
        passed = true;
    });

    if (kChecksOn) {
        // The boundary comparison would also see this write; the access check
        // must stop it first, before the System body continues.
        SPIKE_CHECK_ASSERTS(escalate.Run(world));
        CHECK_MESSAGE(!passed, "the check must stop the System at the access, not at its end");
    } else {
        SPIKE_CHECK_NO_ASSERT(escalate.Run(world));
        CHECK(passed);
        CHECK(world.Get<Position>(0).x == doctest::Approx(7.0f));
    }
}

TEST_CASE("Bypass (c4): undeclared event read through a captured World") {
    World world(MakeRegistry());
    Populate(world);
    std::size_t seen = 99;
    bool passed = false;
    const System peek = MakeSystem(world.Types(), "Peek", [&world, &seen, &passed](Read<Position>) {
        seen = world.Events<Hit>().size();
        passed = true;
    });

    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(peek.Run(world));
        CHECK_MESSAGE(!passed, "the check must stop the System at the access, not at its end");
    } else {
        SPIKE_CHECK_NO_ASSERT(peek.Run(world));
        CHECK(passed);
        CHECK(seen == 0);
    }
}

TEST_CASE("Bypass (c5): another World read during a System run") {
    World world(MakeRegistry());
    World other(MakeRegistry());
    Populate(world);
    Populate(other);
    float seen = 0.0f;
    bool passed = false;
    const System peek = MakeSystem(world.Types(), "Peek", [&other, &seen, &passed](Read<Position>) {
        seen = other.Get<Position>(0).x;
        passed = true;
    });

    if (kChecksOn) {
        SPIKE_CHECK_ASSERTS(peek.Run(world));
        CHECK_MESSAGE(!passed, "the check must stop the System at the access, not at its end");
    } else {
        SPIKE_CHECK_NO_ASSERT(peek.Run(world));
        CHECK(passed);
        CHECK(seen == doctest::Approx(1.0f));
    }
}

// ---- Programmer Errors checked in every build

TEST_CASE("Always: structural changes and nested runs during a System run fail") {
    World world(MakeRegistry());
    Populate(world);
    const System grow = MakeSystem(world.Types(), "Grow", [&world](Deferred) { world.Push(Health{1}); });
    SPIKE_CHECK_ASSERTS(grow.Run(world));

    World fresh(MakeRegistry());
    Populate(fresh);
    const System inner = MakeSystem(fresh.Types(), "Inner", [](Res<Time>) {});
    const System outer = MakeSystem(fresh.Types(), "Outer", [&fresh, &inner](Res<Stats>) { inner.Run(fresh); });
    SPIKE_CHECK_ASSERTS(outer.Run(fresh));
}

// ---- Limits: shapes the check does not see (same result in both builds)

TEST_CASE("Limit: a raw write that restores the old bytes before the System returns is not reported") {
    World world(MakeRegistry());
    Populate(world);
    float during = 0.0f;
    const System sneak = MakeSystem(world.Types(), "Sneak", [&during](Read<Position> position) {
        Position& raw = const_cast<Position&>(position[0]);
        const float old = raw.x;
        raw.x = 123.0f;
        during = position[0].x;
        raw.x = old;
    });

    SPIKE_CHECK_NO_ASSERT(sneak.Run(world));
    CHECK(during == doctest::Approx(123.0f));
}

TEST_CASE("Limit: a read through a kept reference in a System that declared nothing for it is not reported") {
    World world(MakeRegistry());
    Populate(world);
    const Position* kept = nullptr;
    float seen = 0.0f;
    const System leak = MakeSystem(world.Types(), "Leak", [&kept](Read<Position> position) { kept = &position[0]; });
    const System peek = MakeSystem(world.Types(), "Peek", [&kept, &seen](Res<Time>) { seen = kept->x; });

    SPIKE_CHECK_NO_ASSERT((leak.Run(world), peek.Run(world)));
    CHECK(seen == doctest::Approx(1.0f));
}

TEST_CASE("Limit: a kept reference written in a System that declares Write<T> passes the access check") {
    World world(MakeRegistry());
    Populate(world);
    const Position* kept = nullptr;
    const System leak = MakeSystem(world.Types(), "Leak", [&kept](Read<Position> position) { kept = &position[0]; });
    // Legal for the access set, but it bypasses the handle: a change tick
    // recorded by Write<T> would miss this write.
    const System writer = MakeSystem(world.Types(), "Writer", [&kept](Write<Position>) {
        const_cast<Position*>(kept)->x = 5.0f;
    });

    SPIKE_CHECK_NO_ASSERT((leak.Run(world), writer.Run(world)));
    CHECK(world.Get<Position>(0).x == doctest::Approx(5.0f));
}

TEST_CASE("Limit: heap memory owned by a resource is not compared") {
    World world(MakeRegistry());
    Populate(world);
    int* item = &world.ResourceMut<Inventory>().items[0];
    const System sneak = MakeSystem(world.Types(), "Sneak", [item](Res<Time>) { *item = 77; });

    SPIKE_CHECK_NO_ASSERT(sneak.Run(world));
    CHECK(world.Resource<Inventory>().items[0] == 77);
}

namespace {
int globalCounter = 0;
} // namespace

TEST_CASE("Limit: state outside the World is invisible to the check") {
    World world(MakeRegistry());
    Populate(world);
    const System global = MakeSystem(world.Types(), "Global", [](Res<Time>) { ++globalCounter; });

    SPIKE_CHECK_NO_ASSERT(global.Run(world));
    CHECK(globalCounter == 1);
}
