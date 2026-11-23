// The World side of the physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, section
// 4.5; P3-1, P3-2, P3-4): Physics3dModule's registration, the spatial queries run from a System over the author's
// Query (nearest, ties to the smaller EntityId, the filter, Without<ChildOf>, UnsupportedScale, Unsupported), and a
// rollback that resimulates the queries leaving the World running straight would have (rollback R-2, CCR-13).
// The compile-time requirement of Without<ChildOf> is the configure-time sample (compile_fail/).
#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/physics_3d/Physics3dModule.hpp"
#include "engine/physics_3d/SpatialQuery.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace K = GYO::Kernel;
namespace M = GYO::Engine::Math;
namespace O = GYO::Engine::Orchestration;
namespace P3 = GYO::Engine::Physics3d;
namespace R = GYO::Engine::Rollback;
namespace T = GYO::Engine::Transform;
namespace W = GYO::Engine::WorldTime;

namespace {

O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}, const bool defaultOn = true) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = defaultOn;
    row.depends = std::move(depends);
    return row;
}

// The allotter, transform and physics_3d; with time, the time and rollback Modules first.
O::RegisteredUnits Units(const bool time = false) {
    O::RegisteredUnits units;
    if (time) {
        units.rows.push_back(ModuleRow("world_time"));
        units.modules.push_back({"world_time", std::make_unique<W::WorldTimeModule>()});
        units.rows.push_back(ModuleRow("rollback", {"world_time"}));
        units.modules.push_back({"rollback", std::make_unique<R::RollbackModule>()});
    }
    units.rows.push_back(ModuleRow("entity_ids"));
    units.modules.push_back({"entity_ids", std::make_unique<GYO::Engine::EntityIds::EntityIdsModule>()});
    units.rows.push_back(ModuleRow("transform"));
    units.modules.push_back({"transform", std::make_unique<T::TransformModule>()});
    units.rows.push_back(ModuleRow("physics_3d", {"transform"}));
    units.modules.push_back({"physics_3d", std::make_unique<P3::Physics3dModule>()});
    return units;
}

K::SystemOptions At(std::string name, std::string schedule, std::string stage, std::vector<std::string> after = {}) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    m.after = std::move(after);
    o.mounts.push_back(std::move(m));
    return o;
}

std::unique_ptr<O::Product> Make(std::function<void(O::WorldSetup&)> build, O::OptionBag options = {},
                                 const bool time = false) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.options = std::move(options);
    w.build = std::move(build);
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units(time));
    REQUIRE_MESSAGE(static_cast<bool>(built), (built ? std::string() : built.error().detail));
    return std::move(*built);
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

void Frames(O::Product& p, const int count = 1) {
    for (int i = 0; i < count; ++i) {
        auto ran = p.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
    }
}

T::Transform Placed(const float x, const float y, const float z) {
    T::Transform t;
    t.translation = {x, y, z};
    return t;
}

void Put(O::Product& p, const K::EntityId id, P3::Shape shape, const T::Transform& pose) {
    WorldOf(p).Insert(id, P3::Collider{std::move(shape)});
    WorldOf(p).Insert(id, pose);
}

// ---- The probe: one spatial query per frame, from a System over the author's Query.
using Colliders = K::Query<K::Read<P3::Collider>, K::Read<T::Transform>, K::Without<K::ChildOf>>;

enum class ProbeKind : std::uint8_t { Ray, Cast, Overlap };

struct Probe final {
    ProbeKind kind = ProbeKind::Ray;
    M::Ray ray{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}};
    float maxT = 1000.0F;
    P3::Shape shape = P3::Sphere{0.5F};
    P3::Pose pose{};
    M::Vec3 motion{};
    std::function<bool(K::EntityId)> filter;  // empty: every entity
    // Results.
    bool ran = false;
    std::optional<P3::Physics3dError> error;
    std::optional<P3::EntityRayHit> rayHit;
    std::optional<P3::EntityShapeHit> castHit;
    std::vector<K::EntityId> overlaps;
};

Probe& ProbeState() {
    static Probe probe;
    return probe;
}

K::SystemResult RunProbe(Colliders colliders) {
    Probe& p = ProbeState();
    p.ran = true;
    const auto filter = [&p](const K::EntityId e) { return !p.filter || p.filter(e); };
    switch (p.kind) {
    case ProbeKind::Ray: {
        auto hit = P3::CastRay(colliders, p.ray, p.maxT, filter);
        if (hit) p.rayHit = *hit; else p.error = hit.error();
        break;
    }
    case ProbeKind::Cast: {
        auto hit = P3::CastShape(colliders, p.shape, p.pose, p.motion, P3::ShapeCastOptions{}, filter);
        if (hit) p.castHit = *hit; else p.error = hit.error();
        break;
    }
    case ProbeKind::Overlap: {
        auto found = P3::Intersections(colliders, p.shape, p.pose, filter);
        if (found) p.overlaps = *found; else p.error = found.error();
        break;
    }
    }
    return {};
}

void BuildProbe(O::WorldSetup& s) { s.Registrar().AddSystem<&RunProbe>(At("RunProbe", "Main", "Update")); }

// Runs one frame with a fresh probe of the given setup; returns it.
Probe Ask(O::Product& p, Probe setup) {
    ProbeState() = std::move(setup);
    Frames(p);
    REQUIRE(ProbeState().ran);
    return ProbeState();
}

std::unique_ptr<O::Product> RayScene() {
    auto p = Make(&BuildProbe);
    // Inserted out of id order: the tie goes to the smaller EntityId, not to the order of insertion.
    Put(*p, 5, P3::Sphere{1.0F}, Placed(5.0F, 0.0F, 0.0F));
    Put(*p, 3, P3::Sphere{1.0F}, Placed(5.0F, 0.0F, 0.0F));
    Put(*p, 7, P3::Cuboid{{1.0F, 1.0F, 1.0F}}, Placed(2.5F, 5.0F, 0.0F));
    Put(*p, 9, P3::Sphere{1.0F}, Placed(10.0F, 0.0F, 0.0F));
    // A child would be the nearest: Without<ChildOf> leaves it out (its Transform is not its global pose).
    Put(*p, 11, P3::Sphere{1.0F}, Placed(2.0F, 0.0F, 0.0F));
    WorldOf(*p).Insert(11, K::ChildOf{9});
    return p;
}

} // namespace

TEST_CASE("world: Physics3dModule registers Collider as Physics3d.Collider and nothing else") {
    auto p = Make([](O::WorldSetup&) {});
    const K::FrozenRegistry& types = WorldOf(*p).Types();
    std::vector<std::string> components;
    for (const K::ColumnInfo& c : types.Components()) {
        if (c.owner == "physics_3d") components.push_back(c.name);
    }
    REQUIRE_MESSAGE(components.size() == 1, "[p3-registrations] " << components.size());
    CHECK(components[0] == "Physics3d.Collider");
    int resources = 0;
    for (const K::ResourceInfo& info : types.Resources()) {
        if (info.owner == "physics_3d") ++resources;
    }
    CHECK_MESSAGE(resources == 0, "[p3-registrations] a Resource");
    int systems = 0;
    for (const K::SystemInfo& info : types.Systems()) {
        if (info.owner == "physics_3d") ++systems;
    }
    CHECK_MESSAGE(systems == 0, "[p3-registrations] a System (this batch has no simulation step)");
}

TEST_CASE("world: a ray query returns the nearest collider; a tie goes to the smaller EntityId; a child is left out") {
    auto p = RayScene();
    const Probe hit = Ask(*p, Probe{});
    REQUIRE_MESSAGE(!hit.error.has_value(), (hit.error ? hit.error->message : std::string()));
    REQUIRE_MESSAGE(hit.rayHit.has_value(), "[p3-world-ray] no hit");
    CHECK_MESSAGE(hit.rayHit->entity == 3, "[p3-world-tie] entity " << hit.rayHit->entity);
    CHECK(hit.rayHit->hit.t == 4.0F);
    CHECK(hit.rayHit->hit.normal.x == -1.0F);
}

TEST_CASE("world: the filter leaves entities out before they are looked at") {
    auto p = RayScene();
    Probe noThree;
    noThree.filter = [](const K::EntityId e) { return e != 3; };
    const Probe five = Ask(*p, noThree);
    REQUIRE(five.rayHit.has_value());
    CHECK_MESSAGE(five.rayHit->entity == 5, "[p3-world-filter] entity " << five.rayHit->entity);
    Probe neither;
    neither.filter = [](const K::EntityId e) { return e != 3 && e != 5; };
    const Probe nine = Ask(*p, neither);
    REQUIRE(nine.rayHit.has_value());
    CHECK(nine.rayHit->entity == 9);
    CHECK(nine.rayHit->hit.t == 9.0F);
    Probe none;
    none.maxT = 3.0F;
    const Probe empty = Ask(*p, none);
    CHECK(!empty.error.has_value());
    CHECK_MESSAGE(!empty.rayHit.has_value(), "[p3-world-ray] maxT");
}

TEST_CASE("world: a collider whose Transform's scale is not exactly (1, 1, 1) makes the query return UnsupportedScale") {
    auto p = RayScene();
    T::Transform scaled = Placed(50.0F, 0.0F, 0.0F);
    scaled.scale = {1.0F, 1.0F, 1.0000001F};
    Put(*p, 6, P3::Sphere{1.0F}, scaled);
    const Probe failed = Ask(*p, Probe{});
    REQUIRE_MESSAGE(failed.error.has_value(), "[p3-world-scale] the scale was ignored");
    CHECK(failed.error->code == P3::Physics3dErrorCode::UnsupportedScale);
    CHECK(failed.error->detail == "entity 6");
    Probe skip;
    skip.filter = [](const K::EntityId e) { return e != 6; };
    const Probe fine = Ask(*p, skip);
    CHECK_MESSAGE(!fine.error.has_value(), "[p3-world-scale] a filtered-out entity is not looked at");
    REQUIRE(fine.rayHit.has_value());
    CHECK(fine.rayHit->entity == 3);
}

TEST_CASE("world: a shape cast returns the earliest collider; an unsupported pair is an error naming the entity") {
    auto p = Make(&BuildProbe);
    Put(*p, 1, P3::Cuboid{{10.0F, 1.0F, 10.0F}}, Placed(0.0F, -1.0F, 0.0F));
    auto platform = P3::BoxSet::Build(std::vector<M::Aabb>{{{-1.0F, 1.0F, -1.0F}, {1.0F, 2.0F, 1.0F}}});
    REQUIRE(static_cast<bool>(platform));
    Put(*p, 2, *platform, Placed(0.0F, 0.0F, 0.0F));
    Probe cast;
    cast.kind = ProbeKind::Cast;
    cast.pose = P3::Pose{{0.0F, 5.0F, 0.0F}, {}};
    cast.motion = {0.0F, -10.0F, 0.0F};
    const Probe top = Ask(*p, cast);
    REQUIRE_MESSAGE(!top.error.has_value(), (top.error ? top.error->message : std::string()));
    REQUIRE_MESSAGE(top.castHit.has_value(), "[p3-world-cast]");
    CHECK(top.castHit->entity == 2);
    CHECK(top.castHit->hit.time == 0.25F);
    Probe floorOnly = cast;
    floorOnly.filter = [](const K::EntityId e) { return e != 2; };
    const Probe floor = Ask(*p, floorOnly);
    REQUIRE(floor.castHit.has_value());
    CHECK(floor.castHit->entity == 1);
    CHECK(floor.castHit->hit.time == 0.45F);
    Put(*p, 4, P3::Sphere{0.5F}, Placed(8.0F, 0.0F, 0.0F));
    const Probe unsupported = Ask(*p, cast);
    REQUIRE_MESSAGE(unsupported.error.has_value(), "[p3-world-unsupported] a sphere collider was swept against");
    CHECK(unsupported.error->code == P3::Physics3dErrorCode::Unsupported);
    CHECK(unsupported.error->detail.find("entity 4") != std::string::npos);
}

TEST_CASE("world: Intersections lists the overlapped colliders by ascending EntityId") {
    auto p = Make(&BuildProbe);
    Put(*p, 4, P3::Sphere{0.5F}, Placed(0.5F, 0.25F, 0.0F));
    Put(*p, 1, P3::Cuboid{{10.0F, 1.0F, 10.0F}}, Placed(0.0F, -1.0F, 0.0F));
    Put(*p, 2, P3::Capsule{1.0F, 0.5F}, Placed(5.0F, 1.5F, 0.0F));
    Probe overlap;
    overlap.kind = ProbeKind::Overlap;
    overlap.pose = P3::Pose{{0.0F, 0.25F, 0.0F}, {}};
    const Probe found = Ask(*p, overlap);
    REQUIRE(!found.error.has_value());
    REQUIRE_MESSAGE(found.overlaps.size() == 2, "[p3-world-intersections] " << found.overlaps.size());
    CHECK(found.overlaps[0] == 1);
    CHECK(found.overlaps[1] == 4);
}

// ---- Rollback: the queries are pure functions of Transform and Collider, so a resimulation repeats them.

namespace {

struct Seen final {
    float t = 0.0F;
    K::EntityId entity = 0;
    friend bool operator==(const Seen& a, const Seen& b) {
        return T::Detail::SameBits(a.t, b.t) && a.entity == b.entity;
    }
};
inline void HashValue(K::Hasher& h, const Seen& v) {
    HashValue(h, v.t);
    HashValue(h, v.entity);
}

int& LookRuns() {
    static int runs = 0;
    return runs;
}

// The mover (the entity with Seen) steps along +X.
K::SystemResult Advance(K::Query<K::Write<T::Transform>, K::With<Seen>> movers) {
    movers.ForEach([](K::EntityId, K::Mut<T::Transform> t) { t->translation.x += 0.25F; });
    return {};
}

// The mover looks along +X past itself and keeps what it sees.
K::SystemResult Look(Colliders colliders, K::Query<K::Write<Seen>> seen) {
    ++LookRuns();
    for (const K::EntityId mover : seen.Entities()) {
        const T::Transform* t = colliders.Get<T::Transform>(mover);
        if (t == nullptr) continue;
        auto hit = P3::CastRay(colliders, M::Ray{t->translation, {1.0F, 0.0F, 0.0F}}, 1000.0F,
                               [mover](const K::EntityId e) { return e != mover; });
        if (!hit) return ::GYO::Engine::Base::Err(K::KernelError(K::KernelErrorCode::SystemFailed, hit.error().message));
        if (*hit) {
            K::Mut<Seen> cell = seen.GetMut<Seen>(mover);
            cell->t = (*hit)->hit.t;
            cell->entity = (*hit)->entity;
        }
    }
    return {};
}

void BuildRoll(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Component<Seen>({.name = "Seen"});
    R::RollbackEntry rollback(s);
    rollback.Component<Seen>();
    rollback.Component<T::Transform>();
    rollback.Component<P3::Collider>();  // Collider's bit-by-bit ==, no DataOptions::equal (CCR-13)
    rollback.StepSchedule();
    r.AddSystem<&Advance>(At("Advance", "FixedUpdate", "PreUpdate"));
    r.AddSystem<&Look>(At("Look", "FixedUpdate", "Update"));
}

O::OptionBag WithStrategy(R::StateRollbackSettings settings = {}) {
    O::OptionBag options;
    R::RollbackOptions rollback;
    rollback.strategy = std::make_shared<R::StateRollbackStrategy>(settings);
    options.Set(std::move(rollback));
    return options;
}

constexpr W::Nanoseconds kStep = 15'625'000;

std::unique_ptr<O::Product> MakeRoll(O::OptionBag options) {
    auto p = Make(&BuildRoll, std::move(options), true);
    WorldOf(*p).SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    Put(*p, 1, P3::Sphere{0.5F}, Placed(0.0F, 0.0F, 0.0F));
    WorldOf(*p).Insert(1, Seen{});
    Put(*p, 2, P3::Cuboid{{1.0F, 1.0F, 1.0F}}, Placed(20.0F, 0.0F, 0.0F));
    auto boxes = P3::BoxSet::Build(std::vector<M::Aabb>{{{30.0F, -1.0F, -1.0F}, {31.0F, 1.0F, 1.0F}}});
    REQUIRE(static_cast<bool>(boxes));
    Put(*p, 3, *boxes, Placed(0.0F, 0.0F, 0.0F));
    return p;
}

std::string Summary(O::Product& p) {
    auto summary = WorldOf(p).Summary();
    REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().Text()));
    return summary->Text();
}

} // namespace

TEST_CASE("world: R-2: a rollback that resimulates the queries leaves the World running straight would have") {
    LookRuns() = 0;
    auto straight = MakeRoll(WithStrategy());
    Frames(*straight, 10);
    const int straightRuns = LookRuns();
    REQUIRE(WorldOf(*straight).Get<Seen>(1)->entity == 2);
    LookRuns() = 0;
    auto rolled = MakeRoll(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}));
    Frames(*rolled, 9);
    WorldOf(*rolled).ResourceMut<R::RollbackInbox>()->ConfirmedThrough(5);
    Frames(*rolled, 1);
    CHECK_MESSAGE(LookRuns() > straightRuns, "[p3-world-r2] no resimulation ran (" << LookRuns() << " runs)");
    CHECK_MESSAGE(Summary(*rolled) == Summary(*straight), "[p3-world-r2] the World summaries differ");
    CHECK(WorldOf(*rolled).Get<Seen>(1)->t == WorldOf(*straight).Get<Seen>(1)->t);
}
