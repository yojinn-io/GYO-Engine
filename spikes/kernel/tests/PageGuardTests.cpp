// v8-arch-kernel-spike-pageguard: can page protection, as a third access-check
// layer, catch the writes the first two layers cannot (spike limits L1, L6)?
// Declaration: docs/architecture/plans/kernel/spike-pageguard-declaration.zh-Hant.md.
//
// gyo_kernel_spike_tests_debug runs this file with pageGuard on (and the two
// access-check layers on); gyo_kernel_spike_tests_release with pageGuard off
// (and the layers off), where no Bypass is caught.
//   Bypass:   writes the page guard must catch (judgment 1).
//   Control:  writes it must not catch, and pageGuard off costs nothing.
//   Continue: after a fault the System runs to its end, the column is recorded
//             once, the Run returns Err and the World can Run again (judgment 2).
//   Limit:    shapes it does not catch, by design (the declaration's limits).
//   Cost:     timings for judgment 4, printed, not judged here.

#include "SpikeTestSupport.hpp"

#include "kernel_spike/PageGuard.hpp"
#include "kernel_spike/System.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <utility>
#include <vector>

using namespace KernelSpike;
using namespace KernelSpikeTest;

namespace KernelSpike {

// Test access to the page guard's steps, for the cost measurement only.
struct PageGuardProbe {
    static void EnterRun(World& world) { world.Guard(World::GuardStep::EnterRun, nullptr, nullptr); }
    static void ExitRun(World& world) { world.Guard(World::GuardStep::ExitRun, nullptr, nullptr); }
    // What a Run does around one System call: BeforeSystem, then AfterSystem.
    static void Switch(World& world, const System& system, const Detail::RunContext& context) {
        world.Guard(World::GuardStep::BeforeSystem, &system.Descriptor(), &context);
        world.Guard(World::GuardStep::AfterSystem, &system.Descriptor(), &context);
    }
    static void SetColumnWritable(World& world, ComponentId id, bool writable) {
        world.SetColumnWritable(id, writable);
    }
};

} // namespace KernelSpike

namespace {

constexpr bool kPageGuard = KERNEL_SPIKE_TEST_EXPECT_CHECKS != 0;
constexpr bool kChecksOn = KERNEL_SPIKE_ACCESS_CHECKS != 0;

// A Shared column (never guarded).
struct Marker {
    int value;
};

// MakeRegistry's ids, plus Marker as component 3 (Shared).
Registry MakeGuardRegistry() {
    Registry registry = MakeRegistry();
    registry.RegisterComponent<Marker>("Marker", WritePolicy::Shared);
    return registry;
}

void Populate(World& world) {
    world.Push(Position{1.0f, 2.0f});
    world.Push(Position{3.0f, 4.0f});
    world.Push(Velocity{0.5f, 0.0f});
    world.Push(Velocity{0.5f, 0.0f});
    world.Push(Health{10});
    world.Push(Health{20});
    world.Push(Marker{1});
    world.Push(Marker{2});
    world.InsertResource(Time{0});
    world.InsertResource(Gravity{9.0f});
    world.InsertResource(Stats{0});
    world.InsertResource(Inventory{{1, 2, 3}});
}

template <class T>
ComponentId IdOf(const World& world) {
    return world.Types().ComponentIdOf<T>();
}

// Stores through a volatile glvalue so the compiler keeps every store: a
// write restored before anything opaque runs may otherwise be folded away
// when optimizing, and then there is no write for anything to see.
template <class T>
void StoreVolatile(T& target, T value) {
    static_cast<volatile T&>(target) = value;
}

std::vector<Violation> Of(const RunResult& result, Violation::Source source) {
    std::vector<Violation> found;
    for (const Violation& violation : result.violations) {
        if (violation.source == source) {
            found.push_back(violation);
        }
    }
    return found;
}

bool OnPages(const World& world, ComponentId id, std::uintptr_t address) {
    const std::span<const std::byte> pages = world.GuardedPages(id);
    const auto begin = reinterpret_cast<std::uintptr_t>(pages.data());
    return !pages.empty() && address >= begin && address < begin + pages.size();
}

// Judgment 1: exactly one page-guard violation, by system, on column id, at
// an address on that column's pages.
void CheckCaught(const World& world, const RunResult& result, ComponentId id, const char* system) {
    CHECK_FALSE(result.IsOk());
    const std::vector<Violation> faults = Of(result, Violation::Source::PageGuard);
    REQUIRE(faults.size() == 1);
    CHECK(faults[0].system == system);
    CHECK(faults[0].key == AccessKey{AccessKind::Component, id});
    CHECK_MESSAGE(OnPages(world, id, faults[0].address), "fault address must be on the written column's pages");
}

// The second layer's report on the same System (debug only), next to the
// page guard's.
void CheckSecondLayer(const RunResult& result, AccessKey key, const char* system) {
    const std::vector<Violation> after = Of(result, Violation::Source::AfterSystem);
    if (!kChecksOn) {
        CHECK(after.empty());
        return;
    }
    REQUIRE(after.size() == 1);
    CHECK(after[0].system == system);
    CHECK(after[0].key == key);
}

} // namespace

TEST_CASE("PageGuard Build: whole pages for Exclusive columns in the debug target, operator new in the release target") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
#if KERNEL_SPIKE_TEST_EXPECT_CHECKS
    CHECK_MESSAGE(world.PageGuardEnabled(), "debug target runs the page-guard tests with pageGuard on");
#else
    CHECK_MESSAGE(!world.PageGuardEnabled(), "release target runs the page-guard tests with pageGuard off");
#endif
    const std::size_t page = PageGuard::PageSize();
    MESSAGE("page size " << page);
    CHECK(page >= 4096);
    CHECK((page & (page - 1)) == 0);
    for (const ComponentId id : {IdOf<Position>(world), IdOf<Velocity>(world), IdOf<Health>(world)}) {
        const std::span<const std::byte> pages = world.GuardedPages(id);
        if (kPageGuard) {
            REQUIRE_FALSE(pages.empty());
            CHECK(reinterpret_cast<std::uintptr_t>(pages.data()) % page == 0);
            CHECK(pages.size() % page == 0);
        } else {
            CHECK(pages.empty());
        }
    }
    CHECK_MESSAGE(world.GuardedPages(IdOf<Marker>(world)).empty(), "a Shared column is never page-guarded");
}

// ---- Bypass: caught with pageGuard on (judgment 1), not caught in release

TEST_CASE("PageGuard Bypass (L1): a write restored before the System returns") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    float during = 0.0f;
    const System sneak = MakeSystem(world.Types(), "Sneak", [&during](Read<Position> position) {
        Position& raw = const_cast<Position&>(position[0]);
        const float old = raw.x;
        StoreVolatile(raw.x, old + 1.0f);
        during = position[0].x;
        StoreVolatile(raw.x, old);
    });

    const RunResult result = Run(world, {&sneak});
    CHECK(during == doctest::Approx(2.0f));
    CHECK(world.Get<Position>(0).x == doctest::Approx(1.0f));
    if (kPageGuard) {
        CheckCaught(world, result, IdOf<Position>(world), "Sneak");
        // No net change: the second layer has nothing to report.
        CHECK(result.violations.size() == 1);
    } else {
        CHECK(result.IsOk());
    }
}

TEST_CASE("PageGuard Bypass (L6): a kept pointer written in a System after host code used the World API on the column") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const Position* kept = nullptr;
    const System leak = MakeSystem(world.Types(), "Leak", [&kept](Read<Position> position) { kept = &position[1]; });
    const System tamper = MakeSystem(world.Types(), "Tamper", [&kept](Read<Velocity>) {
        const_cast<Position*>(kept)->x = -5.0f;
    });

    CHECK(Run(world, {&leak}).IsOk());
    world.GetMut<Position>(0).x = 50.0f;  // marks the whole column written
    const RunResult result = Run(world, {&tamper});
    CHECK(world.Get<Position>(1).x == doctest::Approx(-5.0f));
    if (kPageGuard) {
        CheckCaught(world, result, IdOf<Position>(world), "Tamper");
        // This shape (the write inside a System) is also seen by the second
        // layer: the column is re-baselined before Tamper runs, and Tamper's
        // write is a difference after it. The original L6 shape (the write in
        // Ready) is "PageGuard Limit (L6)" below.
        CheckSecondLayer(result, AccessKey{AccessKind::Component, IdOf<Position>(world)}, "Tamper");
    } else {
        CHECK(result.IsOk());
    }
}

TEST_CASE("PageGuard Bypass (b1): const_cast write through the reference Read<T> returns") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Position> position) {
        const_cast<Position&>(position[0]).x = 42.0f;
    });

    const RunResult result = Run(world, {&cheat});
    CHECK(world.Get<Position>(0).x == doctest::Approx(42.0f));
    if (kPageGuard) {
        CheckCaught(world, result, IdOf<Position>(world), "Cheat");
        CheckSecondLayer(result, AccessKey{AccessKind::Component, IdOf<Position>(world)}, "Cheat");
    } else {
        CHECK(result.IsOk());
    }
}

TEST_CASE("PageGuard Bypass (b2): byte writes through a raw pointer into a read-only column") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Health> health, Write<Velocity>) {
        auto* bytes = reinterpret_cast<unsigned char*>(const_cast<Health*>(&health[1]));
        bytes[0] ^= 0xFFu;
    });

    const RunResult result = Run(world, {&cheat});
    CHECK(world.Get<Health>(1).points != 20);
    if (kPageGuard) {
        CheckCaught(world, result, IdOf<Health>(world), "Cheat");
        CheckSecondLayer(result, AccessKey{AccessKind::Component, IdOf<Health>(world)}, "Cheat");
    } else {
        CHECK(result.IsOk());
    }
}

// The declaration lists (b3) under judgment 1, but its own limits leave
// resources unguarded: this case records what happens, it is not a pass of
// judgment 1 (see the batch report).
TEST_CASE("PageGuard Bypass (b3), not caught: a const_cast write through Res<T> is not on a guarded page") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Res<Gravity> gravity) {
        const_cast<Gravity*>(&*gravity)->g = 0.0f;
    });

    const std::uint64_t faultsBefore = PageGuard::HandledFaults();
    const RunResult result = Run(world, {&cheat});
    CHECK(world.Resource<Gravity>().g == doctest::Approx(0.0f));
    CHECK(PageGuard::HandledFaults() == faultsBefore);
    CHECK(Of(result, Violation::Source::PageGuard).empty());
    CheckSecondLayer(result, AccessKey{AccessKind::Resource, world.Types().ResourceIdOf<Gravity>()}, "Cheat");
    CHECK(result.IsOk() == !kChecksOn);
}

// ---- Control

TEST_CASE("PageGuard Control: a (c) System writes the columns it declared as written without a fault") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const System heal = MakeSystem(world.Types(), "Heal", [](Write<Health> health) {
        for (std::size_t row = 0; row < health.Size(); ++row) {
            health[row].points += 1;
        }
    });
    const System fall = MakeSystem(world.Types(), "Fall", [](Write<Velocity> velocity, Res<Gravity> gravity) {
        for (std::size_t row = 0; row < velocity.Size(); ++row) {
            velocity[row].dy -= gravity->g;
        }
    });

    const std::uint64_t faultsBefore = PageGuard::HandledFaults();
    const RunResult result = Run(world, {&heal, &fall, &heal});
    CHECK(result.IsOk());
    CHECK(PageGuard::HandledFaults() == faultsBefore);
    CHECK(world.Get<Health>(0).points == 12);
    CHECK(world.Get<Velocity>(1).dy == doctest::Approx(-9.0f));
}

TEST_CASE("PageGuard Control: a Shared column is not guarded") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const System declared = MakeSystem(world.Types(), "Declared", [](Write<Marker> marker) { marker[1].value = 8; });
    // Only reads Marker, writes it anyway: no fault, because a Shared column
    // is never guarded; the second layer still reports it (debug).
    const System stray = MakeSystem(world.Types(), "Stray", [](Read<Marker> marker) {
        const_cast<Marker&>(marker[0]).value = 9;
    });

    const std::uint64_t faultsBefore = PageGuard::HandledFaults();
    CHECK(Run(world, {&declared}).IsOk());
    const RunResult result = Run(world, {&stray});
    CHECK(PageGuard::HandledFaults() == faultsBefore);
    CHECK(Of(result, Violation::Source::PageGuard).empty());
    CheckSecondLayer(result, AccessKey{AccessKind::Component, IdOf<Marker>(world)}, "Stray");
    CHECK(world.Get<Marker>(0).value == 9);
    CHECK(world.Get<Marker>(1).value == 8);
    CHECK(world.GuardedPages(IdOf<Marker>(world)).empty());
}

TEST_CASE("PageGuard Control: a (b) System writes any column without a fault") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    // (b): every guarded column is writable during its call; its undeclared
    // write is the second layer's to report (debug).
    const System frame = MakeSystem(world.Types(), "Frame", Zone::B, [](Read<Position> position, Write<Health> health) {
        health[0].points += 1;
        const_cast<Position&>(position[0]).x = 7.0f;
    });
    const System tidy = MakeSystem(world.Types(), "Tidy", Zone::B, [](Write<Health> health) { health[1].points += 1; });
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Position> position) {
        const_cast<Position&>(position[1]).y = -3.0f;
    });

    const std::uint64_t faultsBefore = PageGuard::HandledFaults();
    const RunResult result = Run(world, {&frame});
    CHECK(PageGuard::HandledFaults() == faultsBefore);
    CHECK(Of(result, Violation::Source::PageGuard).empty());
    CheckSecondLayer(result, AccessKey{AccessKind::Component, IdOf<Position>(world)}, "Frame");
    CHECK(world.Get<Position>(0).x == doctest::Approx(7.0f));
    CHECK(world.Get<Health>(0).points == 11);

    // After a (b) System the columns are guarded again for the next (c) one.
    const RunResult next = Run(world, {&tidy, &cheat});
    CHECK(world.Get<Health>(1).points == 21);
    if (kPageGuard) {
        CheckCaught(world, next, IdOf<Position>(world), "Cheat");
    } else {
        CHECK(next.IsOk());
    }
}

TEST_CASE("PageGuard Control: pageGuard off makes no protection call and catches nothing") {
    World world(MakeGuardRegistry(), WorldOptions{false});  // off in both targets
    Populate(world);
    const System heal = MakeSystem(world.Types(), "Heal", [](Write<Health> health) { health[0].points += 1; });
    const System sneak = MakeSystem(world.Types(), "Sneak", [](Read<Position> position) {
        Position& raw = const_cast<Position&>(position[0]);
        const float old = raw.x;
        StoreVolatile(raw.x, old + 1.0f);
        StoreVolatile(raw.x, old);
    });
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Position> position) {
        const_cast<Position&>(position[1]).x = 42.0f;
    });

    const std::uint64_t faultsBefore = PageGuard::HandledFaults();
    CHECK(Run(world, {&heal, &sneak}).IsOk());
    const RunResult result = Run(world, {&cheat});
    CHECK(PageGuard::HandledFaults() == faultsBefore);
    CHECK(Of(result, Violation::Source::PageGuard).empty());
    CheckSecondLayer(result, AccessKey{AccessKind::Component, IdOf<Position>(world)}, "Cheat");
    CHECK_MESSAGE(world.GuardStats().protectCalls == 0, "pageGuard off must not ask for any protection change");
    for (ComponentId id = 0; id < world.Types().Components().size(); ++id) {
        CHECK_MESSAGE(world.GuardedPages(id).empty(), "pageGuard off must not allocate whole pages");
    }
}

// ---- Continue (judgment 2)

TEST_CASE("PageGuard Continue: after a fault the System runs to its end") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    bool reachedEnd = false;
    const System cheat = MakeSystem(world.Types(), "Cheat", [&reachedEnd](Read<Health> health, Write<Velocity> velocity) {
        const_cast<Health&>(health[0]).points = 99;
        velocity[0].dx = 3.0f;
        reachedEnd = true;
    });

    const RunResult result = Run(world, {&cheat});
    CHECK(reachedEnd);
    CHECK(world.Get<Health>(0).points == 99);
    CHECK(world.Get<Velocity>(0).dx == doctest::Approx(3.0f));
    if (kPageGuard) {
        CheckCaught(world, result, IdOf<Health>(world), "Cheat");
    } else {
        CHECK(result.IsOk());
    }
}

TEST_CASE("PageGuard Continue: a second write to the same column in the same call does not fault and is recorded once") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Position> position) {
        StoreVolatile(const_cast<Position&>(position[0]).x, 10.0f);
        StoreVolatile(const_cast<Position&>(position[1]).y, 20.0f);
        StoreVolatile(const_cast<Position&>(position[0]).x, 30.0f);
    });
    const System both = MakeSystem(world.Types(), "Both", [](Read<Position> position, Read<Health> health) {
        StoreVolatile(const_cast<Health&>(health[0]).points, 5);
        StoreVolatile(const_cast<Position&>(position[1]).x, 6.0f);
        StoreVolatile(const_cast<Health&>(health[1]).points, 7);
    });

    const std::uint64_t faultsBefore = PageGuard::HandledFaults();
    const RunResult result = Run(world, {&cheat});
    CHECK(world.Get<Position>(0).x == doctest::Approx(30.0f));
    CHECK(world.Get<Position>(1).y == doctest::Approx(20.0f));
    if (kPageGuard) {
        CHECK_MESSAGE(PageGuard::HandledFaults() - faultsBefore == 1, "only the first write to the column faults");
        CheckCaught(world, result, IdOf<Position>(world), "Cheat");
    } else {
        CHECK(PageGuard::HandledFaults() == faultsBefore);
        CHECK(result.IsOk());
    }

    // Two columns in one call: one fault and one record each.
    const std::uint64_t bothBefore = PageGuard::HandledFaults();
    const RunResult twoColumns = Run(world, {&both});
    CHECK(world.Get<Health>(1).points == 7);
    CHECK(world.Get<Position>(1).x == doctest::Approx(6.0f));
    if (kPageGuard) {
        CHECK(PageGuard::HandledFaults() - bothBefore == 2);
        const std::vector<Violation> faults = Of(twoColumns, Violation::Source::PageGuard);
        REQUIRE(faults.size() == 2);
        CHECK(faults[0].key == AccessKey{AccessKind::Component, IdOf<Health>(world)});
        CHECK(faults[1].key == AccessKey{AccessKind::Component, IdOf<Position>(world)});
        CHECK(OnPages(world, IdOf<Health>(world), faults[0].address));
        CHECK(OnPages(world, IdOf<Position>(world), faults[1].address));
    } else {
        CHECK(twoColumns.IsOk());
    }
}

TEST_CASE("PageGuard Continue: the Run returns Err, and the World can be written in Ready and Run again") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    bool laterRan = false;
    const System cheat = MakeSystem(world.Types(), "Cheat", [](Read<Position> position) {
        const_cast<Position&>(position[0]).x = 42.0f;
    });
    const System later = MakeSystem(world.Types(), "Later", [&laterRan](Write<Velocity> velocity) {
        velocity[0].dx = 1.5f;
        laterRan = true;
    });
    const System heal = MakeSystem(world.Types(), "Heal", [](Write<Health> health) { health[0].points += 1; });

    const RunResult first = Run(world, {&cheat, &later});
    if (kPageGuard) {
        CheckCaught(world, first, IdOf<Position>(world), "Cheat");
        CHECK_MESSAGE(!laterRan, "Err ends the Run");
    } else {
        CHECK(first.IsOk());
        CHECK(laterRan);
    }

    // Ready: not guarded; host writes through the World API.
    world.GetMut<Position>(0).x = 11.0f;
    world.GetMut<Health>(1).points = 40;
    world.Push(Position{5.0f, 6.0f});
    CHECK(world.Get<Position>(0).x == doctest::Approx(11.0f));

    CHECK(Run(world, {&heal}).IsOk());
    CHECK(world.Get<Health>(0).points == 11);

    // Guarded again in the next Run.
    const RunResult again = Run(world, {&cheat});
    if (kPageGuard) {
        CheckCaught(world, again, IdOf<Position>(world), "Cheat");
    } else {
        CHECK(again.IsOk());
    }
}

// ---- Limits (same result in both targets)

TEST_CASE("PageGuard Limit (L6): a kept pointer written by host code in Ready after it used the World API on the column") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const Position* kept = nullptr;
    const System leak = MakeSystem(world.Types(), "Leak", [&kept](Read<Position> position) { kept = &position[1]; });
    const System next = MakeSystem(world.Types(), "Next", [](Res<Time>) {});

    CHECK(Run(world, {&leak}).IsOk());
    // The original L6 shape: Ready is not guarded, and GetMut re-baselines
    // the column for the second layer.
    world.GetMut<Position>(0).x = 50.0f;
    const_cast<Position*>(kept)->x = -5.0f;
    CHECK(Run(world, {&next}).IsOk());
    CHECK(world.Get<Position>(1).x == doctest::Approx(-5.0f));
}

TEST_CASE("PageGuard Limit (L3): a kept pointer written in a System that declares Write<T>") {
    World world(MakeGuardRegistry(), WorldOptions{kPageGuard});
    Populate(world);
    const Position* kept = nullptr;
    const System leak = MakeSystem(world.Types(), "Leak", [&kept](Read<Position> position) { kept = &position[0]; });
    // The column is lifted for a System that declares Write<Position>.
    const System writer = MakeSystem(world.Types(), "Writer", [&kept](Write<Position>) {
        const_cast<Position*>(kept)->x = 5.0f;
    });

    CHECK(Run(world, {&leak, &writer}).IsOk());
    CHECK(world.Get<Position>(0).x == doctest::Approx(5.0f));
}

// ---- Cost (judgment 4): printed, not judged; the thresholds are judged in
// the batch report. Debug target only (pageGuard on).

namespace {

template <int N>
struct Lane {
    std::uint64_t value;
};

template <int... N>
void RegisterLanes(Registry& registry, std::integer_sequence<int, N...>) {
    (static_cast<void>(registry.RegisterComponent<Lane<N>>("Lane")), ...);
}

template <int... N>
void PushLanes(World& world, std::integer_sequence<int, N...>) {
    (world.Push(Lane<N>{static_cast<std::uint64_t>(N)}), ...);
}

using Clock = std::chrono::steady_clock;

double Micros(Clock::duration duration) {
    return std::chrono::duration<double, std::micro>(duration).count();
}

double Median(std::vector<double> samples) {
    const auto middle = samples.begin() + static_cast<std::ptrdiff_t>(samples.size() / 2);
    std::nth_element(samples.begin(), middle, samples.end());
    return *middle;
}

} // namespace

TEST_CASE("PageGuard Cost: protection calls, a System switch and a Run's full protection (printed, not judged)" *
          doctest::skip(!kPageGuard)) {
    constexpr int kColumns = 64;
    constexpr int kSamples = 1000;
    Registry registry;
    RegisterLanes(registry, std::make_integer_sequence<int, kColumns>{});
    World world(std::move(registry), WorldOptions{true});
    PushLanes(world, std::make_integer_sequence<int, kColumns>{});
    const std::size_t page = PageGuard::PageSize();
    for (ComponentId id = 0; id < static_cast<ComponentId>(kColumns); ++id) {
        REQUIRE_MESSAGE(world.GuardedPages(id).size() == page, "64 Exclusive columns, one page each");
    }

    const System two = MakeSystem(world.Types(), "TwoWrites", [](Write<Lane<0>> a, Write<Lane<1>> b) {
        a[0].value += 1;
        b[0].value += 1;
    });
    const System stray = MakeSystem(world.Types(), "Stray", [](Read<Lane<2>> c) {
        const_cast<Lane<2>&>(c[0]).value = 7;
    });
    // The setup does what it should before anything is timed.
    REQUIRE(Run(world, {&two}).IsOk());
    REQUIRE(Of(Run(world, {&stray}), Violation::Source::PageGuard).size() == 1);

    // 1. One protection call (one column, one page), alternating read-only and writable.
    std::vector<double> toReadOnly;
    std::vector<double> toWritable;
    std::vector<double> oneCall;
    for (int sample = 0; sample < kSamples; ++sample) {
        const bool writable = sample % 2 == 1;
        const Clock::time_point start = Clock::now();
        PageGuardProbe::SetColumnWritable(world, 5, writable);
        const double micros = Micros(Clock::now() - start);
        (writable ? toWritable : toReadOnly).push_back(micros);
        oneCall.push_back(micros);
    }

    // 2. A (c) System's switch: lift its 2 declared columns, protect them again.
    PageGuardProbe::EnterRun(world);
    const Detail::RunContext context;
    const std::uint64_t switchCallsBefore = world.GuardStats().protectCalls;
    std::vector<double> switches;
    for (int sample = 0; sample < kSamples; ++sample) {
        const Clock::time_point start = Clock::now();
        PageGuardProbe::Switch(world, two, context);
        switches.push_back(Micros(Clock::now() - start));
    }
    CHECK_MESSAGE(world.GuardStats().protectCalls - switchCallsBefore == 4u * kSamples,
                  "a (c) System costs 2 x |writes| protection calls");
    PageGuardProbe::ExitRun(world);

    // 3. A Run's entry (all 64 read-only) and exit (all 64 writable).
    const std::uint64_t fullCallsBefore = world.GuardStats().protectCalls;
    std::vector<double> enters;
    std::vector<double> exits;
    std::vector<double> fulls;
    for (int sample = 0; sample < kSamples; ++sample) {
        const Clock::time_point start = Clock::now();
        PageGuardProbe::EnterRun(world);
        const Clock::time_point entered = Clock::now();
        PageGuardProbe::ExitRun(world);
        const Clock::time_point exited = Clock::now();
        enters.push_back(Micros(entered - start));
        exits.push_back(Micros(exited - entered));
        fulls.push_back(Micros(exited - start));
    }
    CHECK(world.GuardStats().protectCalls - fullCallsBefore == 2u * kColumns * kSamples);

    const double protectUs = Median(oneCall);
    const double switchUs = Median(switches);
    const double fullUs = Median(fulls);
    std::printf("PAGEGUARD_COST protect_us=%.3f switch_us=%.3f full_us=%.3f page_bytes=%zu\n", protectUs, switchUs,
                fullUs, page);
    std::printf("PAGEGUARD_COST_DETAIL to_readonly_us=%.3f to_writable_us=%.3f enter_us=%.3f exit_us=%.3f "
                "columns=%d samples=%d\n",
                Median(toReadOnly), Median(toWritable), Median(enters), Median(exits), kColumns, kSamples);
    std::fflush(stdout);
    MESSAGE("PAGEGUARD_COST protect_us=" << protectUs << " switch_us=" << switchUs << " full_us=" << fullUs
                                         << " page_bytes=" << page);
}
