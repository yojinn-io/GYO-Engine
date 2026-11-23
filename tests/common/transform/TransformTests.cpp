// The transform Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 6.1, T-1 to
// T-9; Design Invariants TF-1 to TF-4 in docs/architecture/target/v2/transform.zh-Hant.md). Products of synthetic
// units only (no product): the transform Module, the default allotter, and for T-7 the time and rollback Modules.
// Each CHECK carries a tag in brackets ("[tf-...]"): the mutants expect it
// (docs/architecture/target/v2/scripts/transform_mutations.json).
#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/math/scalar/Angle.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/rollback/Inbox.hpp"
#include "engine/rollback/RollbackModule.hpp"
#include "engine/rollback/StateRollbackStrategy.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/world_time/WorldTimeModule.hpp"

namespace K = GYO::Kernel;
namespace M = GYO::Engine::Math;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Rollback;
namespace T = GYO::Engine::Transform;
namespace W = GYO::Engine::WorldTime;

namespace {

O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}

// A unit registered before transform: its Systems come earlier in the registration order, so only a declared order
// puts them after the transform Module's.
class ProbeModule final : public O::Module {
public:
    explicit ProbeModule(std::function<void(O::WorldSetup&)> build) : build_(std::move(build)) {}
    void Build(O::WorldSetup& setup) override { build_(setup); }

private:
    std::function<void(O::WorldSetup&)> build_;
};

// The allotter and the transform Module; with time, the time and rollback Modules first; with a probe, it before
// transform.
O::RegisteredUnits Units(bool time = false, std::function<void(O::WorldSetup&)> probe = {}) {
    O::RegisteredUnits units;
    if (probe) {
        units.rows.push_back(ModuleRow("probe"));
        units.modules.push_back({"probe", std::make_unique<ProbeModule>(std::move(probe))});
    }
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

std::unique_ptr<O::Product> Make(std::function<void(O::WorldSetup&)> build, O::OptionBag options = {}, bool time = false,
                                 std::function<void(O::WorldSetup&)> probe = {}, const std::string& tag = {}) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.options = std::move(options);
    w.build = std::move(build);
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units(time, std::move(probe)));
    REQUIRE_MESSAGE(static_cast<bool>(built), tag << (built ? std::string() : built.error().detail));
    return std::move(*built);
}

K::World& WorldOf(O::Product& p) { return p.World("Main").GetWorld(); }

void Frames(O::Product& p, int count = 1) {
    for (int i = 0; i < count; ++i) {
        auto ran = p.RunFrame();
        REQUIRE_MESSAGE(static_cast<bool>(ran), (ran ? std::string() : ran.error().message + "\n" + ran.error().detail));
    }
}

bool SameMatrix(const M::Matrix4& a, const M::Matrix4& b) {
    for (std::size_t i = 0; i < 16; ++i) {
        if (!T::Detail::SameBits(a.values[i], b.values[i])) return false;
    }
    return true;
}

// A pose whose matrix is not the identity in any block.
T::Transform Pose(const float x) {
    T::Transform t;
    t.translation = {x, 2.0F, -3.0F};
    t.rotation = M::MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, M::DegreesToRadians(30.0F));
    t.scale = {2.0F, 0.5F, 1.5F};
    return t;
}

bool GlobalMatches(K::World& world, const K::EntityId id) {
    const T::Transform* t = world.Get<T::Transform>(id);
    const T::GlobalTransform* g = world.Get<T::GlobalTransform>(id);
    return t != nullptr && g != nullptr && SameMatrix(g->matrix, T::ToMatrix(*t));
}

std::string Summary(O::Product& p) {
    auto summary = WorldOf(p).Summary();
    REQUIRE_MESSAGE(static_cast<bool>(summary), (summary ? std::string() : summary.error().Text()));
    return summary->Text();
}

// ---- Spawning (T-1).
K::SystemResult SpawnOnce(K::Spawn<K::Bundle<T::Transform>> spawn, K::Local<int> done) {
    if (*done != 0) return {};
    *done = 1;
    auto id = spawn.New(Pose(1.0F));
    if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
    return {};
}

void BuildSpawn(O::WorldSetup& s) { s.Registrar().AddSystem<&SpawnOnce>(At("SpawnOnce", "Main", "Update")); }

// ---- Writing Transform (T-2, T-3).
struct MoveTo {
    float x = 0.0F;
    bool on = false;
};
inline void HashValue(K::Hasher&, const MoveTo&) {}
std::vector<std::string>& Log() {
    static std::vector<std::string> log;
    return log;
}
K::SystemResult Move(K::Query<K::Write<T::Transform>> poses, K::ResMut<MoveTo> move) {
    if (!move->on) return {};
    move->on = false;
    const float x = move->x;
    poses.ForEach([x](K::EntityId, K::Mut<T::Transform> t) { t->translation.x = x; });
    return {};
}
// After Move in Update: what GlobalTransform is before the propagation.
K::SystemResult ReadBefore(K::Query<K::Read<T::GlobalTransform>> globals) {
    globals.ForEach([](K::EntityId, const T::GlobalTransform& g) { Log().push_back("before " + std::to_string(g.matrix.values[12])); });
    return {};
}
// In Last: the entities whose GlobalTransform was written since this mount last ran.
K::SystemResult CountChanged(K::Query<K::Read<T::GlobalTransform>, K::Changed<T::GlobalTransform>> changed) {
    Log().push_back("changed " + std::to_string(changed.Count()));
    return {};
}
void BuildMove(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Resource<MoveTo>({.name = "MoveTo"});
    r.AddSystem<&Move>(At("Move", "Main", "Update"));
    r.AddSystem<&ReadBefore>(At("ReadBefore", "Main", "Update", {"Move"}));
    r.AddSystem<&CountChanged>(At("CountChanged", "Main", "Last"));
    s.Prepare([](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(MoveTo{});
        return {};
    });
}

// ---- ExclusiveWorld::Restore (T-5, T-6): a script of one action per frame.
struct Script {
    int step = 0;
};
inline void HashValue(K::Hasher&, const Script&) {}
struct Recorded {
    std::vector<K::EntityId> alive;
    std::vector<std::pair<K::EntityId, T::Transform>> poses;
    bool globalRightAfterRestore = false;
    bool globalMatchedAfterRestore = false;
};
Recorded& Record() {
    static Recorded recorded;
    return recorded;
}
K::SystemResult Act(K::ExclusiveWorld<> world) {
    const int step = world.Read<Script>([](const Script& s) { return s.step; })->value_or(0);
    (void)world.Modify<Script>([](Script& s) { ++s.step; });
    Recorded& r = Record();
    switch (step) {
    case 0:  // record
        r.alive = world.Entities();
        r.poses.clear();
        world.ReadColumn<T::Transform>([&r](K::EntityId e, K::ChangeStamp, K::ChangeStamp, const T::Transform& t) {
            r.poses.emplace_back(e, t);
        });
        return {};
    case 1: {  // the T-5 script despawns entity 1, the T-6 script moves it
        if (r.alive.size() == 1) {
            auto gone = world.Despawn(1);
            if (!gone) return ::GYO::Engine::Base::Err(std::move(gone).error());
        } else {
            auto moved = world.Modify<T::Transform>(1, [](T::Transform& t) { t.translation.x = 50.0F; });
            if (!moved) return ::GYO::Engine::Base::Err(std::move(moved).error());
        }
        return {};
    }
    case 2: {  // restore what step 0 recorded; look at GlobalTransform right away
        K::RestoreSet set;
        set.Alive(r.alive);
        set.Column<T::Transform>(r.poses);
        auto restored = world.Restore(std::move(set));
        if (!restored) return ::GYO::Engine::Base::Err(std::move(restored).error());
        bool found = false;
        M::Matrix4 global;
        world.ReadColumn<T::GlobalTransform>([&](K::EntityId e, K::ChangeStamp, K::ChangeStamp, const T::GlobalTransform& g) {
            if (e == 1) {
                found = true;
                global = g.matrix;
            }
        });
        M::Matrix4 pose;
        world.ReadColumn<T::Transform>([&pose](K::EntityId e, K::ChangeStamp, K::ChangeStamp, const T::Transform& t) {
            if (e == 1) pose = T::ToMatrix(t);
        });
        r.globalRightAfterRestore = found;
        r.globalMatchedAfterRestore = found && SameMatrix(global, pose);
        return {};
    }
    default: return {};
    }
}
void BuildRestore(O::WorldSetup& s) {
    s.Registrar().Resource<Script>({.name = "Script"});
    s.Registrar().AddSystem<&Act>(At("Act", "Main", "Update"));
    s.Prepare([](K::World& world) -> ::GYO::Engine::Base::Result<void, O::OrchestrationError> {
        world.SetResource(Script{});
        return {};
    });
}

// Puts an entity with Transform and a matching GlobalTransform in directly (World::Insert derives nothing).
void PutPose(O::Product& p, const K::EntityId id, const T::Transform& pose) {
    WorldOf(p).Insert(id, pose);
    WorldOf(p).Insert(id, T::GlobalTransform{T::ToMatrix(pose)});
}

} // namespace

// ---- T-1: the initial value

TEST_CASE("transform: a spawned Transform has its GlobalTransform at that sync point, before any propagation") {
    // Spawned in Update; SyncTransforms is mounted in First, which ran before the spawn: after this frame only the
    // initial value can have put the value.
    O::OptionBag options;
    options.Set(T::TransformOptions{"Main", "First"});
    auto p = Make(&BuildSpawn, std::move(options));
    Frames(*p);
    const std::vector<K::EntityId> ids = WorldOf(*p).Keys<T::Transform>();
    REQUIRE(ids.size() == 1);
    CHECK_MESSAGE(GlobalMatches(WorldOf(*p), ids[0]), "[tf-spawn-initial] GlobalTransform is not ComposeTRS(Transform)");
}

// ---- T-2, T-3: the propagation and Changed

TEST_CASE("transform: a written Transform reaches GlobalTransform in PostUpdate; before it the old value is read") {
    Log().clear();
    auto p = Make(&BuildMove);
    PutPose(*p, 1, Pose(1.0F));
    Frames(*p);
    WorldOf(*p).SetResource(MoveTo{7.0F, true});
    Log().clear();
    Frames(*p);
    CHECK_MESSAGE(GlobalMatches(WorldOf(*p), 1), "[tf-sync-writes] GlobalTransform did not follow Transform");
    CHECK_MESSAGE(WorldOf(*p).Get<T::GlobalTransform>(1)->matrix.values[12] == 7.0F, "[tf-sync-writes] translation");
    REQUIRE(!Log().empty());
    CHECK_MESSAGE(Log().front() == "before 1.000000", "[tf-sync-stage] read before the propagation: " << Log().front());
}

TEST_CASE("transform: a frame that writes no Transform writes no GlobalTransform") {
    Log().clear();
    auto p = Make(&BuildMove);
    PutPose(*p, 1, Pose(1.0F));
    Frames(*p);  // the put-in cells are newer than SyncTransforms' last run: written once
    Frames(*p, 2);
    std::vector<std::string> counts;
    for (const std::string& line : Log()) {
        if (line.rfind("changed ", 0) == 0) counts.push_back(line);
    }
    REQUIRE(counts.size() == 3);
    CHECK(counts[0] == "changed 1");
    CHECK_MESSAGE(counts[2] == "changed 0", "[tf-sync-changed-only] " << counts[0] << " | " << counts[1] << " | " << counts[2]);
}

namespace {
// In the probe (registered before transform): PostUpdate, after the public set.
K::SystemResult ReadAfterPropagate(K::Query<K::Read<T::GlobalTransform>> globals) {
    globals.ForEach([](K::EntityId, const T::GlobalTransform& g) { Log().push_back("after " + std::to_string(g.matrix.values[12])); });
    return {};
}
void ProbeAfterPropagate(O::WorldSetup& s) {
    K::SystemOptions o = At("ReadAfterPropagate", "Main", "PostUpdate", {std::string(T::kPropagateSet)});
    s.Registrar().AddSystem<&ReadAfterPropagate>(o);
}
} // namespace

TEST_CASE("transform: a System ordered after TransformPropagate in PostUpdate reads this frame's GlobalTransform") {
    Log().clear();
    // A startup error (the set has no member: D-E4) fails here with the same tag.
    auto p = Make(&BuildMove, {}, false, &ProbeAfterPropagate, "[tf-propagate-set] ");
    PutPose(*p, 1, Pose(1.0F));
    Frames(*p);
    WorldOf(*p).SetResource(MoveTo{7.0F, true});
    Log().clear();
    Frames(*p);
    std::string after;
    for (const std::string& line : Log()) {
        if (line.rfind("after ", 0) == 0) after = line;
    }
    CHECK_MESSAGE(after == "after 7.000000", "[tf-propagate-set] read before the propagation: " << after);
}

// ---- T-4: where it mounts

TEST_CASE("transform: a World with the skeleton off names its stage; without one the mount is the kernel's D-E1") {
    auto build = [](O::WorldSetup& s) {
        K::ScheduleOptions frame;
        frame.name = "Frame";
        frame.stages = {"Logic", "Late"};
        s.Registrar().Schedule(frame);
    };
    {
        O::ProductDescription d;
        d.name = "game";
        O::WorldDescription w;
        w.name = "Main";
        w.skeleton = O::ScheduleSkeleton::Off;
        w.frame = {"Frame"};
        w.options.Set(T::TransformOptions{"Frame", "Late"});
        w.build = build;
        d.worlds.push_back(std::move(w));
        auto built = O::Product::Build(std::move(d), Units());
        REQUIRE_MESSAGE(static_cast<bool>(built), "[tf-mount-named] " << (built ? std::string() : built.error().detail));
        (*built)->World("Main").GetWorld().Insert(1, Pose(4.0F));
        (*built)->World("Main").GetWorld().Insert(1, T::GlobalTransform{});
        Frames(**built);
        CHECK_MESSAGE(GlobalMatches((*built)->World("Main").GetWorld(), 1), "[tf-mount-named] SyncTransforms did not run in Late");
    }
    {
        O::ProductDescription d;
        d.name = "game";
        O::WorldDescription w;
        w.name = "Main";
        w.skeleton = O::ScheduleSkeleton::Off;
        w.frame = {"Frame"};
        w.build = build;
        d.worlds.push_back(std::move(w));
        auto built = O::Product::Build(std::move(d), Units());
        REQUIRE_FALSE(static_cast<bool>(built));
        const K::StartupReport* report = built.error().KernelReportOf("Main");
        REQUIRE(report != nullptr);
        CHECK_MESSAGE(report->Has(K::StartupCode::DE1), "[tf-mount-missing] " << built.error().detail);
    }
}

// ---- T-5, T-6: ExclusiveWorld::Restore

TEST_CASE("transform: Restore brings an entity back with its Transform: GlobalTransform is there right away") {
    Record() = {};
    auto p = Make(&BuildRestore);
    PutPose(*p, 1, Pose(3.0F));
    Frames(*p, 3);  // record, despawn, restore
    REQUIRE(WorldOf(*p).Alive(1));
    CHECK_MESSAGE(Record().globalRightAfterRestore, "[tf-restore-global] no GlobalTransform right after the restore");
    CHECK_MESSAGE(Record().globalMatchedAfterRestore, "[tf-restore-global-value] the hook put another value than ComposeTRS(Transform)");
    CHECK_MESSAGE(GlobalMatches(WorldOf(*p), 1), "[tf-restore-global] GlobalTransform is not ComposeTRS(Transform)");
}

TEST_CASE("transform: Restore puts an older Transform back: the next propagation follows it") {
    Record() = {};
    auto p = Make(&BuildRestore);
    PutPose(*p, 1, Pose(3.0F));
    PutPose(*p, 2, Pose(5.0F));  // two entities: the script moves entity 1 instead of despawning it
    Frames(*p, 2);  // record, move to 50
    REQUIRE(WorldOf(*p).Get<T::GlobalTransform>(1)->matrix.values[12] == 50.0F);
    Frames(*p);  // restore 3 (a different value, so Restore writes it)
    CHECK_MESSAGE(GlobalMatches(WorldOf(*p), 1), "[tf-restore-changed] GlobalTransform still has the moved pose");
}

// ---- T-7: rollback equivalence (R-2) with the official strategy

namespace {

struct Health {
    std::int32_t hp = 0;
    friend bool operator==(const Health&, const Health&) = default;
};
inline void HashValue(K::Hasher& h, const Health& v) { HashValue(h, v.hp); }

K::SystemResult Wear(K::Query<K::Write<Health>> health) {
    health.ForEach([](K::EntityId, K::Mut<Health> hp) { hp->hp -= 1; });
    return {};
}
K::SystemResult DieAtZero(K::Despawn despawn, K::Query<K::Read<Health>> health) {
    health.ForEach([&despawn](K::EntityId entity, const Health& hp) {
        if (hp.hp <= 0) despawn.Entity(entity);
    });
    return {};
}
void BuildDeath(O::WorldSetup& s) {
    K::Registrar& r = s.Registrar();
    r.Component<Health>({.name = "Health"});
    R::RollbackEntry rollback(s);
    rollback.Component<Health>();
    rollback.Component<T::Transform>();  // transform's bit-by-bit ==, no DataOptions::equal
    rollback.StepSchedule();
    r.AddSystem<&Wear>(At("Wear", "FixedUpdate", "PreUpdate"));
    r.AddSystem<&DieAtZero>(At("DieAtZero", "FixedUpdate", "PostUpdate"));
}
O::OptionBag WithStrategy(R::StateRollbackSettings settings = {}) {
    O::OptionBag options;
    R::RollbackOptions rollback;
    rollback.strategy = std::make_shared<R::StateRollbackStrategy>(settings);
    options.Set(std::move(rollback));
    return options;
}
constexpr W::Nanoseconds kStep = 15'625'000;
std::unique_ptr<O::Product> MakeDeath(O::OptionBag options, std::int32_t hp) {
    auto p = Make(&BuildDeath, std::move(options), true);
    WorldOf(*p).SetResource(W::TimeSampling{kStep, W::SamplingMode::ManualDelta});
    WorldOf(*p).Insert(1, Health{hp});
    PutPose(*p, 1, Pose(6.0F));
    return p;
}

} // namespace

TEST_CASE("transform: R-2: a predicted death undone by a rollback leaves the World running straight would have") {
    // Straight: 105 at the start, 100 after step 5, alive throughout. Rolled: 7 at the start, the prediction kills
    // entity 1 at step 7; the authority says 100 at step 5 and the rollback revives it.
    auto straight = MakeDeath(WithStrategy(), 105);
    Frames(*straight, 10);
    auto rolled = MakeDeath(WithStrategy({.depth = 20, .mode = R::RollbackMode::Always}), 7);
    Frames(*rolled, 9);
    REQUIRE(!WorldOf(*rolled).Alive(1));
    WorldOf(*rolled).ResourceMut<R::RollbackInbox>()->Value<Health>(1, 5, Health{100});
    WorldOf(*rolled).ResourceMut<R::RollbackInbox>()->ConfirmedThrough(5);
    Frames(*rolled, 1);
    REQUIRE_MESSAGE(WorldOf(*rolled).Alive(1), "entity 1 did not come back");
    CHECK_MESSAGE(GlobalMatches(WorldOf(*rolled), 1), "[tf-r2-revived-global] the revived entity has no right GlobalTransform");
    CHECK_MESSAGE(Summary(*rolled) == Summary(*straight), "[tf-r2-summary] the World summaries differ");
}

TEST_CASE("transform: == compares bits: -0 and +0 differ, the same NaN bits are equal") {
    T::Transform a;
    T::Transform b;
    a.translation.x = 0.0F;
    b.translation.x = -0.0F;
    CHECK_MESSAGE(!(a == b), "[tf-eq-bits] -0 and +0 compare equal");
    const float nan = std::bit_cast<float>(0x7FC00001U);
    a.translation.x = nan;
    b.translation.x = nan;
    CHECK(a == b);
}

// ---- T-8: the summary

TEST_CASE("transform: the summary hashes every field of Transform and nothing of GlobalTransform") {
    auto a = Make([](O::WorldSetup&) {});
    auto b = Make([](O::WorldSetup&) {});
    WorldOf(*a).Insert(1, Pose(1.0F));
    WorldOf(*a).Insert(1, T::GlobalTransform{});
    WorldOf(*b).Insert(1, Pose(1.0F));
    WorldOf(*b).Insert(1, T::GlobalTransform{T::ToMatrix(Pose(1.0F))});
    CHECK_MESSAGE(Summary(*a) == Summary(*b), "[tf-hash-global-empty] GlobalTransform changed the summary");
    T::Transform scaled = Pose(1.0F);
    scaled.scale.z = 3.0F;
    WorldOf(*b).Insert(1, scaled);
    CHECK_MESSAGE(Summary(*a) != Summary(*b), "[tf-hash-transform-fields] a different scale left the summary as it was");
    T::Transform moved = Pose(1.0F);
    moved.translation.y = 9.0F;
    WorldOf(*b).Insert(1, moved);
    CHECK_MESSAGE(Summary(*a) != Summary(*b), "[tf-hash-transform-fields] a different translation left the summary as it was");
    T::Transform turned = Pose(1.0F);
    turned.rotation.w = -turned.rotation.w;
    WorldOf(*b).Insert(1, turned);
    CHECK_MESSAGE(Summary(*a) != Summary(*b), "[tf-hash-transform-fields] a different rotation left the summary as it was");
}

// ---- T-9: no rollback or authority registration

TEST_CASE("transform: the Module registers its two components, one initial value, one hook and nothing for rollback") {
    auto p = Make([](O::WorldSetup&) {});
    const K::FrozenRegistry& types = WorldOf(*p).Types();
    int components = 0;
    for (const K::ColumnInfo& c : types.Components()) {
        if (c.owner == "transform") ++components;
    }
    CHECK_MESSAGE(components == 2, "[tf-registrations] " << components);
    int initialValues = 0;
    for (const K::InitialValueInfo& info : types.InitialValues()) {
        if (info.owner == "transform") ++initialValues;
    }
    CHECK_MESSAGE(initialValues == 1, "[tf-registrations] initial values " << initialValues);
    int resources = 0;
    for (const K::ResourceInfo& info : types.Resources()) {
        if (info.owner == "transform") ++resources;
    }
    CHECK_MESSAGE(resources == 0, "[tf-registrations] the Module registers a Resource (a rollback or authority table)");
}
