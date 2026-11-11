// Judgment 1 of v8-arch-kernel-spike: the access set {reads, writes} of a
// System is derived from its parameter types at compile time, with no second
// hand-written declaration. Each System below is checked twice against a
// hand-written expectation: as types (static_assert) and as ids (run time).

#include "SpikeTestSupport.hpp"

#include "kernel_spike/System.hpp"

#include <algorithm>
#include <type_traits>
#include <vector>

using namespace KernelSpike;
using namespace KernelSpikeTest;

namespace {

template <class T>
using C = Access<AccessKind::Component, T>;
template <class T>
using R = Access<AccessKind::Resource, T>;
template <class T>
using E = Access<AccessKind::Event, T>;
using D = Access<AccessKind::Deferred, void>;

template <class F, class Reads, class Writes>
inline constexpr bool DerivesExactly = Detail::IsSystemFunction<F> &&
                                       SameSet<typename AccessOf<F>::Reads, Reads> &&
                                       SameSet<typename AccessOf<F>::Writes, Writes>;

// Hand-written ids, from the registration order in MakeRegistry().
constexpr AccessKey kPosition{AccessKind::Component, 0};
constexpr AccessKey kVelocity{AccessKind::Component, 1};
constexpr AccessKey kHealth{AccessKind::Component, 2};
constexpr AccessKey kTime{AccessKind::Resource, 0};
constexpr AccessKey kGravity{AccessKind::Resource, 1};
constexpr AccessKey kStats{AccessKind::Resource, 2};
constexpr AccessKey kHit{AccessKind::Event, 0};
constexpr AccessKey kSpawned{AccessKind::Event, 1};
constexpr AccessKey kDeferred{AccessKind::Deferred, 0};

void CheckDescriptor(const SystemDescriptor& descriptor, std::vector<AccessKey> reads, std::vector<AccessKey> writes) {
    std::sort(reads.begin(), reads.end());
    std::sort(writes.begin(), writes.end());
    CHECK_MESSAGE(descriptor.reads == reads, descriptor.name << ": reads differ from the hand-written expectation");
    CHECK_MESSAGE(descriptor.writes == writes, descriptor.name << ": writes differ from the hand-written expectation");
}

// ---- Systems: free functions

void Integrate(Write<Position> position, Read<Velocity> velocity) {
    for (std::size_t row = 0; row < position.Size(); ++row) {
        position[row].x += velocity[row].dx;
        position[row].y += velocity[row].dy;
    }
}
void ApplyGravity(Write<Velocity> velocity, Res<Gravity> gravity) {
    for (std::size_t row = 0; row < velocity.Size(); ++row) {
        velocity[row].dy -= gravity->g;
    }
}
void CountPositions(Read<Position> position, ResMut<Stats> stats) {
    stats->count = static_cast<int>(position.Size());
}
void TakeDamage(EventReader<Hit> hits, Write<Health> health) {
    for (const Hit& hit : hits.Events()) {
        health[static_cast<std::size_t>(hit.target)].points -= hit.amount;
    }
}
void Fire(Read<Position> position, EventWriter<Hit> hits) {
    if (position.Size() > 0) {
        hits.Send(Hit{0, 1});
    }
}
void SpawnLater(Deferred deferred, Res<Time> time) {
    const int tick = time->tick;
    deferred.Push([tick](World& world) { world.Push(Health{tick}); });
}
void Idle() {}
void Everything(Read<Position>, Write<Velocity>, Res<Time>, ResMut<Stats>, EventReader<Hit>, EventWriter<Spawned>,
                Deferred) {}
void AdvanceTime(ResMut<Time> time) noexcept {
    ++time->tick;
}
void ReadTwice(Read<Position>, Read<Position>, Res<Time>) {}

// ---- Systems: member functions and a function object

struct Steering {
    int thoughts = 0;
    void Think(Read<Position>, Write<Velocity>) { ++thoughts; }
    void Report(Res<Stats>) const {}
    void Audit(EventReader<Hit>, EventReader<Spawned>) const noexcept {}
    void Prepare(Write<Health>, Res<Gravity>) noexcept { ++thoughts; }
};

struct Regenerate {
    void operator()(Write<Health>, Res<Time>) const {}
};

// ---- Compile-time: derived type sets equal the hand-written ones.

static_assert(DerivesExactly<decltype(&Integrate), TypeList<C<Velocity>>, TypeList<C<Position>>>);
static_assert(DerivesExactly<decltype(&ApplyGravity), TypeList<R<Gravity>>, TypeList<C<Velocity>>>);
static_assert(DerivesExactly<decltype(&CountPositions), TypeList<C<Position>>, TypeList<R<Stats>>>);
static_assert(DerivesExactly<decltype(&TakeDamage), TypeList<E<Hit>>, TypeList<C<Health>>>);
static_assert(DerivesExactly<decltype(&Fire), TypeList<C<Position>>, TypeList<E<Hit>>>);
static_assert(DerivesExactly<decltype(&SpawnLater), TypeList<R<Time>>, TypeList<D>>);
static_assert(DerivesExactly<decltype(&Idle), TypeList<>, TypeList<>>);
static_assert(DerivesExactly<decltype(&Everything), TypeList<C<Position>, R<Time>, E<Hit>>,
                             TypeList<C<Velocity>, R<Stats>, E<Spawned>, D>>);
static_assert(DerivesExactly<decltype(&AdvanceTime), TypeList<>, TypeList<R<Time>>>);
static_assert(DerivesExactly<decltype(&ReadTwice), TypeList<C<Position>, R<Time>>, TypeList<>>);
static_assert(DerivesExactly<decltype(&Steering::Think), TypeList<C<Position>>, TypeList<C<Velocity>>>);
static_assert(DerivesExactly<decltype(&Steering::Report), TypeList<R<Stats>>, TypeList<>>);
static_assert(DerivesExactly<decltype(&Steering::Audit), TypeList<E<Hit>, E<Spawned>>, TypeList<>>);
static_assert(DerivesExactly<decltype(&Steering::Prepare), TypeList<R<Gravity>>, TypeList<C<Health>>>);
static_assert(DerivesExactly<Regenerate, TypeList<R<Time>>, TypeList<C<Health>>>);

// ---- Compile-time: what is not a System.

static_assert(!Detail::IsSystemFunction<void (*)(World&)>, "the World is not a parameter");
static_assert(!Detail::IsSystemFunction<void (*)(Position&)>, "a raw component reference is not a parameter");
static_assert(!Detail::IsSystemFunction<void (*)(const Read<Position>&)>, "handles are taken by value");
static_assert(!Detail::IsSystemFunction<void (*)(Read<Position>, int)>, "every parameter is a handle");
static_assert(!Detail::IsSystemFunction<int (*)(Read<Position>)>, "a System returns void");
static_assert(!Detail::IsSystemFunction<int>, "not callable");

// Aliasing within one System is rejected; repeated reads are not aliasing.
static_assert(HasAliasing<void (*)(Read<Position>, Write<Position>)>);
static_assert(HasAliasing<void (*)(Write<Position>, Write<Position>)>);
static_assert(HasAliasing<void (*)(Res<Time>, ResMut<Time>)>);
static_assert(HasAliasing<void (*)(EventReader<Hit>, EventWriter<Hit>)>);
static_assert(HasAliasing<void (*)(Deferred, Deferred)>);
static_assert(!HasAliasing<void (*)(Read<Position>, Read<Position>)>);
static_assert(!HasAliasing<void (*)(Read<Position>, Write<Velocity>)>);
static_assert(!Detail::IsSystemFunction<void (*)(Read<Position>, Write<Position>)>);

// Handles cannot be kept past a run or made for an undeclared access.
template <class P>
inline constexpr bool IsSealedHandle = !std::is_copy_constructible_v<P> && !std::is_move_constructible_v<P> &&
                                       !std::is_copy_assignable_v<P> && !std::is_constructible_v<P, World&>;
static_assert(IsSealedHandle<Read<Position>>);
static_assert(IsSealedHandle<Write<Position>>);
static_assert(IsSealedHandle<Res<Time>>);
static_assert(IsSealedHandle<ResMut<Time>>);
static_assert(IsSealedHandle<EventReader<Hit>>);
static_assert(IsSealedHandle<EventWriter<Hit>>);
static_assert(IsSealedHandle<Deferred>);

} // namespace

TEST_CASE("Derivation: free functions, member functions and a function object match the hand-written sets") {
    const Registry registry = MakeRegistry();

    CheckDescriptor(Describe<decltype(&Integrate)>(registry, "Integrate"), {kVelocity}, {kPosition});
    CheckDescriptor(Describe<decltype(&ApplyGravity)>(registry, "ApplyGravity"), {kGravity}, {kVelocity});
    CheckDescriptor(Describe<decltype(&CountPositions)>(registry, "CountPositions"), {kPosition}, {kStats});
    CheckDescriptor(Describe<decltype(&TakeDamage)>(registry, "TakeDamage"), {kHit}, {kHealth});
    CheckDescriptor(Describe<decltype(&Fire)>(registry, "Fire"), {kPosition}, {kHit});
    CheckDescriptor(Describe<decltype(&SpawnLater)>(registry, "SpawnLater"), {kTime}, {kDeferred});
    CheckDescriptor(Describe<decltype(&Idle)>(registry, "Idle"), {}, {});
    CheckDescriptor(Describe<decltype(&Everything)>(registry, "Everything"), {kPosition, kTime, kHit},
                    {kVelocity, kStats, kSpawned, kDeferred});
    CheckDescriptor(Describe<decltype(&AdvanceTime)>(registry, "AdvanceTime"), {}, {kTime});
    CheckDescriptor(Describe<decltype(&ReadTwice)>(registry, "ReadTwice"), {kPosition, kTime}, {});
    CheckDescriptor(Describe<decltype(&Steering::Think)>(registry, "Steering::Think"), {kPosition}, {kVelocity});
    CheckDescriptor(Describe<decltype(&Steering::Report)>(registry, "Steering::Report"), {kStats}, {});
    CheckDescriptor(Describe<decltype(&Steering::Audit)>(registry, "Steering::Audit"), {kHit, kSpawned}, {});
    CheckDescriptor(Describe<decltype(&Steering::Prepare)>(registry, "Steering::Prepare"), {kGravity}, {kHealth});
    CheckDescriptor(Describe<Regenerate>(registry, "Regenerate"), {kTime}, {kHealth});
}

TEST_CASE("Derivation: lambdas match the hand-written sets") {
    const Registry registry = MakeRegistry();
    int seen = 0;

    auto plain = [](Read<Velocity>, Write<Health>) {};
    auto capturing = [&seen](Res<Gravity>, EventReader<Spawned> spawned) {
        seen += static_cast<int>(spawned.Events().size());
    };
    auto stateful = [calls = 0](Write<Position> position) mutable { position[0].x = static_cast<float>(++calls); };

    static_assert(DerivesExactly<decltype(plain), TypeList<C<Velocity>>, TypeList<C<Health>>>);
    static_assert(DerivesExactly<decltype(capturing), TypeList<R<Gravity>, E<Spawned>>, TypeList<>>);
    static_assert(DerivesExactly<decltype(stateful), TypeList<>, TypeList<C<Position>>>);

    CheckDescriptor(Describe<decltype(plain)>(registry, "plain"), {kVelocity}, {kHealth});
    CheckDescriptor(Describe<decltype(capturing)>(registry, "capturing"), {kGravity, kSpawned}, {});
    CheckDescriptor(Describe<decltype(stateful)>(registry, "stateful"), {}, {kPosition});

    // MakeSystem stores the same derived descriptor.
    CheckDescriptor(MakeSystem(registry, "plain", plain).Descriptor(), {kVelocity}, {kHealth});
    CheckDescriptor(MakeSystem(registry, "capturing", capturing).Descriptor(), {kGravity, kSpawned}, {});
    CheckDescriptor(MakeSystem(registry, "stateful", stateful).Descriptor(), {}, {kPosition});
}

TEST_CASE("Derivation: a generic lambda has no single parameter list") {
    auto generic = [](auto) {};
    static_assert(!FunctionArgs<decltype(generic)>::deducible);
    static_assert(!Detail::IsSystemFunction<decltype(generic)>);
    CHECK(true);
}

TEST_CASE("Derivation: MakeSystem keeps the derived descriptor for every callable form") {
    const Registry registry = MakeRegistry();
    Steering steering;

    CheckDescriptor(MakeSystem(registry, "Integrate", &Integrate).Descriptor(), {kVelocity}, {kPosition});
    CheckDescriptor(MakeSystem(registry, "AdvanceTime", &AdvanceTime).Descriptor(), {}, {kTime});
    CheckDescriptor(MakeSystem(registry, "Idle", &Idle).Descriptor(), {}, {});
    CheckDescriptor(MakeSystem(registry, "Everything", &Everything).Descriptor(), {kPosition, kTime, kHit},
                    {kVelocity, kStats, kSpawned, kDeferred});
    CheckDescriptor(MakeSystem(registry, "ReadTwice", &ReadTwice).Descriptor(), {kPosition, kTime}, {});
    CheckDescriptor(MakeSystem(registry, "Steering::Think", steering, &Steering::Think).Descriptor(), {kPosition},
                    {kVelocity});
    CheckDescriptor(MakeSystem(registry, "Steering::Audit", steering, &Steering::Audit).Descriptor(),
                    {kHit, kSpawned}, {});
    CheckDescriptor(MakeSystem(registry, "Steering::Prepare", steering, &Steering::Prepare).Descriptor(), {kGravity},
                    {kHealth});
    CheckDescriptor(MakeSystem(registry, "Regenerate", Regenerate{}).Descriptor(), {kTime}, {kHealth});
}

TEST_CASE("Derivation: systems run with handles built from the derived access") {
    World world(MakeRegistry());
    world.Push(Position{0.0f, 10.0f});
    world.Push(Position{1.0f, 20.0f});
    world.Push(Velocity{1.0f, 0.0f});
    world.Push(Velocity{2.0f, 0.0f});
    world.Push(Health{10});
    world.InsertResource(Time{5});
    world.InsertResource(Gravity{0.5f});
    world.InsertResource(Stats{0});
    world.InsertResource(Inventory{});
    Steering steering;

    const Registry& types = world.Types();
    const System gravity = MakeSystem(types, "ApplyGravity", &ApplyGravity);
    const System integrate = MakeSystem(types, "Integrate", &Integrate);
    const System count = MakeSystem(types, "CountPositions", &CountPositions);
    const System fire = MakeSystem(types, "Fire", &Fire);
    const System damage = MakeSystem(types, "TakeDamage", &TakeDamage);
    const System spawn = MakeSystem(types, "SpawnLater", &SpawnLater);
    const System advance = MakeSystem(types, "AdvanceTime", &AdvanceTime);
    const System think = MakeSystem(types, "Steering::Think", steering, &Steering::Think);

    SPIKE_CHECK_NO_ASSERT((gravity.Run(world), integrate.Run(world), count.Run(world), fire.Run(world),
                           damage.Run(world), spawn.Run(world), advance.Run(world), think.Run(world)));
    world.ApplyDeferred();

    CHECK(world.Get<Position>(0).x == doctest::Approx(1.0f));
    CHECK(world.Get<Position>(0).y == doctest::Approx(9.5f));
    CHECK(world.Get<Position>(1).x == doctest::Approx(3.0f));
    CHECK(world.Resource<Stats>().count == 2);
    CHECK(world.Get<Health>(0).points == 9);
    CHECK(world.Rows<Health>() == 2);  // the deferred spawn
    CHECK(world.Get<Health>(1).points == 5);
    CHECK(world.Resource<Time>().tick == 6);
    CHECK(steering.thoughts == 1);
}
