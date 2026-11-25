// The visibility Module (big-map declaration section 13, K1 to K4): CheckVisibility writes each 3D camera's
// CulledEntities from the entities' Aabb, NoFrustumCulling and VisibilityRange (after bevy_camera's
// check_visibility_cpu_culling and check_visibility_ranges), and Queue3d leaves out the items of the entities in a camera's list. Each CHECK carries a tag in brackets ("[vs-...]"): the mutants expect it (tests/common/render/visibility_mutations.json).
#include <doctest/doctest.h>

#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline3d.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/visibility/Visibility.hpp"
#include "engine/visibility/VisibilityModule.hpp"
#include "engine/world_render/WorldRenderModule.hpp"

namespace C = GYO::Engine::Camera;
namespace K = GYO::Kernel;
namespace M = GYO::Engine::Math;
namespace O = GYO::Engine::Orchestration;
namespace R = GYO::Engine::Render;
namespace T = GYO::Engine::Transform;
namespace V = GYO::Engine::Visibility;

namespace {

O::RegisterRow Row(std::string name, const bool library, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = library;
    row.defaultOn = false;
    row.depends = std::move(depends);
    return row;
}

template <class Unit>
void Add(O::RegisteredUnits& units, std::string name, std::vector<std::string> depends = {}) {
    units.rows.push_back(Row(name, false, std::move(depends)));
    units.modules.push_back({std::move(name), std::make_unique<Unit>()});
}

std::vector<int>& Changes() {
    static std::vector<int> changes;
    return changes;
}

K::SystemResult CountChanged(K::Query<K::Read<R::CulledEntities>, K::Changed<R::CulledEntities>> changed) {
    Changes().push_back(static_cast<int>(changed.Count()));
    return {};
}

std::unique_ptr<O::Product> Make(const bool withVisibility = true) {
    Changes().clear();
    O::RegisteredUnits units;
    units.rows.push_back(Row("render", true));
    Add<GYO::Engine::EntityIds::EntityIdsModule>(units, "entity_ids");
    Add<T::TransformModule>(units, "transform");
    Add<C::CameraModule>(units, "camera", {"transform"});
    Add<GYO::Engine::WorldRender::WorldRenderModule>(units, "world_render", {"render"});
    Add<V::VisibilityModule>(units, "visibility", {"render", "transform", "camera"});
    O::ProductDescription d;
    d.name = "seen";
    O::WorldDescription w;
    w.name = "Main";
    w.units = {"entity_ids", "transform", "camera", "world_render"};
    if (withVisibility) w.units.push_back("visibility");
    w.build = [withVisibility](O::WorldSetup& setup) {
        if (!withVisibility) return;
        K::SystemOptions o;
        o.name = "TestCountChanged";
        K::Mount m;
        m.schedule = std::string(O::DefaultSchedules::frame);
        m.stage = std::string(O::DefaultSchedules::postUpdate);
        m.after.push_back(std::string(V::kCheckSet));
        o.mounts.push_back(std::move(m));
        setup.Registrar().AddSystem<&CountChanged>(std::move(o));
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), std::move(units));
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

void Put(K::World& world, const K::EntityId id, const T::Transform& t) {
    world.Insert(id, t);
    world.Insert(id, T::GlobalTransform{T::ToMatrix(t)});
}

void Place(K::World& world, const K::EntityId id, const M::Vec3 position) {
    T::Transform t;
    t.translation = position;
    Put(world, id, t);
}

// A 3D camera at position looking along +Z (the default rotation), 90 degrees vertically, on a 100 x 100 target.
void Camera3dAt(K::World& world, const K::EntityId id, const M::Vec3 position, const bool active = true) {
    world.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, C::TargetSize{{100, 100}, 1.0F});
    C::Camera camera;
    camera.active = active;
    world.Insert(id, camera);
    world.Insert(id, C::Camera3d{});
    world.Insert(id, C::CameraTarget{});
    C::PerspectiveProjection projection;
    projection.verticalFov = M::Pi / 2.0F;
    projection.farClip = 100.0F;
    world.Insert(id, C::Projection{projection});
    world.Insert(id, C::ComputedCamera{});
    Place(world, id, position);
}

const R::Aabb kUnitBox{{{-1.0F, -1.0F, -1.0F}, {1.0F, 1.0F, 1.0F}}};

std::vector<std::uint64_t> CulledBy(K::World& world, const K::EntityId camera) {
    const R::CulledEntities* list = world.Get<R::CulledEntities>(camera);
    REQUIRE(list != nullptr);
    return list->entities;
}

} // namespace

TEST_CASE("visibility: a camera sees the boxes in its frustum, a box across a plane, a NoFrustumCulling box; not one behind it") {
    auto p = Make();
    K::World& world = WorldOf(*p);
    Camera3dAt(world, 1, {0.0F, 0.0F, 0.0F});
    Place(world, 2, {0.0F, 0.0F, 10.0F});   // in front
    world.Insert(2, kUnitBox);
    Place(world, 3, {0.0F, 0.0F, -10.0F});  // behind
    world.Insert(3, kUnitBox);
    Place(world, 4, {0.0F, 0.0F, -10.0F});  // behind, not tested against the frustum
    world.Insert(4, kUnitBox);
    world.Insert(4, R::NoFrustumCulling{});
    Place(world, 5, {10.5F, 0.0F, 10.0F});  // its box crosses the right plane (x = z at 90 degrees)
    world.Insert(5, kUnitBox);
    Place(world, 6, {0.0F, 0.0F, -10.0F});  // no Aabb, no range: never in the list (always drawn)
    Frames(*p);
    CHECK_MESSAGE((CulledBy(world, 1) == std::vector<std::uint64_t>{3}), "[vs-frustum] only the box behind; in front, unculled and across a plane are seen");
}

TEST_CASE("visibility: the box is placed by the entity's GlobalTransform, scale and rotation included") {
    auto p = Make();
    K::World& world = WorldOf(*p);
    Camera3dAt(world, 1, {0.0F, 0.0F, 0.0F});
    // Behind the camera, but scaled 30 times along z: its box reaches z = +20.
    T::Transform stretched;
    stretched.translation = {0.0F, 0.0F, -10.0F};
    stretched.scale = {1.0F, 1.0F, 30.0F};
    Put(world, 2, stretched);
    world.Insert(2, kUnitBox);
    // Behind the camera in its own space (z -30 to -28), in front once placed at z 40.
    Place(world, 3, {0.0F, 0.0F, 40.0F});
    world.Insert(3, R::Aabb{{{-1.0F, -1.0F, -30.0F}, {1.0F, 1.0F, -28.0F}}});
    Frames(*p);
    CHECK_MESSAGE(CulledBy(world, 1).empty(), "[vs-world-box] the local box goes through the GlobalTransform");
}

TEST_CASE("visibility: VisibilityRange keeps distances in [start of startMargin, end of endMargin), from the position or the box centre") {
    auto p = Make();
    K::World& world = WorldOf(*p);
    Camera3dAt(world, 1, {0.0F, 0.0F, 0.0F});
    R::VisibilityRange range;
    range.startMargin = {5.0F, 6.0F};
    range.endMargin = {10.0F, 20.0F};
    const auto at = [&](const K::EntityId id, const float z) {
        Place(world, id, {0.0F, 0.0F, z});
        world.Insert(id, range);
    };
    at(2, 4.0F);   // nearer than 5
    at(3, 5.0F);   // 5 itself: seen
    at(4, 19.0F);  // inside
    at(5, 20.0F);  // 20 itself: not seen
    // With useAabb the box's centre counts: the entity at 30, its box centred 25 nearer.
    R::VisibilityRange centred = range;
    centred.useAabb = true;
    Place(world, 6, {0.0F, 0.0F, 30.0F});
    world.Insert(6, centred);
    world.Insert(6, R::Aabb{{{-1.0F, -1.0F, -26.0F}, {1.0F, 1.0F, -24.0F}}});
    Frames(*p);
    CHECK_MESSAGE((CulledBy(world, 1) == std::vector<std::uint64_t>{2, 5}), "[vs-range] the half-open range, and the box centre");
}

TEST_CASE("visibility: each 3D camera has its own list; an inactive camera is not checked") {
    auto p = Make();
    K::World& world = WorldOf(*p);
    Camera3dAt(world, 1, {0.0F, 0.0F, 0.0F});
    Camera3dAt(world, 2, {0.0F, 0.0F, 20.0F});  // looks along +Z from 20: the box at 10 is behind it
    Camera3dAt(world, 3, {0.0F, 0.0F, 0.0F}, false);
    Place(world, 4, {0.0F, 0.0F, 10.0F});
    world.Insert(4, kUnitBox);
    Place(world, 5, {0.0F, 0.0F, 30.0F});
    world.Insert(5, kUnitBox);
    Frames(*p);
    CHECK_MESSAGE(CulledBy(world, 1).empty(), "[vs-per-camera] the first camera sees both");
    CHECK_MESSAGE((CulledBy(world, 2) == std::vector<std::uint64_t>{4}), "[vs-per-camera] the second only the far one");
    CHECK_MESSAGE(world.Get<R::CulledEntities>(3) == nullptr, "[vs-inactive] an inactive camera gets no list");
}

TEST_CASE("visibility: the list is written only when it changes") {
    auto p = Make();
    K::World& world = WorldOf(*p);
    Camera3dAt(world, 1, {0.0F, 0.0F, 0.0F});
    Place(world, 2, {0.0F, 0.0F, 10.0F});
    world.Insert(2, kUnitBox);
    Frames(*p, 3);
    // The first list is inserted by a deferred command (seen as Changed by a System after it in the next frame); then
    // nothing changes, so nothing is written.
    CHECK_MESSAGE(std::accumulate(Changes().begin(), Changes().end(), 0) == 1, "[vs-write-changes] written once");
    Place(world, 2, {0.0F, 0.0F, -10.0F});
    Frames(*p);
    CHECK_MESSAGE(Changes().back() == 1, "[vs-write-changes] written again when it changes");
    CHECK((CulledBy(world, 1) == std::vector<std::uint64_t>{2}));
}

TEST_CASE("visibility: without the visibility Module no camera has a list") {
    auto p = Make(false);
    K::World& world = WorldOf(*p);
    Camera3dAt(world, 1, {0.0F, 0.0F, 0.0F});
    Place(world, 2, {0.0F, 0.0F, -10.0F});
    world.Insert(2, kUnitBox);
    Frames(*p);
    CHECK_MESSAGE(world.Get<R::CulledEntities>(1) == nullptr, "[vs-opt-in] nothing writes a list");
}

TEST_CASE("visibility: Queue3d leaves out the items of the entities in the camera's culled list, and nothing else") {
    R::ExtractedCamera camera;
    camera.is3d = true;
    camera.computed.valid = true;
    camera.computed.viewport.size = {8, 8};
    camera.computed.clipFromView = M::MakePerspective(M::Pi / 2.0F, 1.0F, 0.1F, 100.0F);
    camera.view = M::Matrix4{};
    std::vector<R::Extracted3dItem> items(3);
    for (std::size_t i = 0; i < items.size(); ++i) {
        items[i].entity = 10 + i;
        items[i].mesh = R::MeshId{1};
        items[i].indexCount = 3;
        items[i].world = M::Matrix4{};
    }
    camera.culled = {11};
    auto pass = R::Queue3d(camera, items);
    REQUIRE(pass.has_value());
    CHECK_MESSAGE(pass->draws.size() == 2, "[vs-queue] the culled entity's item is left out");
    camera.culled.clear();
    pass = R::Queue3d(camera, items);
    REQUIRE(pass.has_value());
    CHECK_MESSAGE(pass->draws.size() == 3, "[vs-queue-no-list] a camera without a list leaves nothing out");
}

TEST_CASE("visibility: WorldBox and InRange") {
    T::Transform t;
    t.translation = {1.0F, 2.0F, 3.0F};
    t.rotation = M::MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, M::Pi / 2.0F);
    const M::Aabb box = V::WorldBox({{0.0F, 0.0F, 0.0F}, {2.0F, 1.0F, 1.0F}}, T::ToMatrix(t));
    CHECK(box.minimum.x == doctest::Approx(1.0F));
    CHECK(box.maximum.x == doctest::Approx(2.0F));
    CHECK(box.minimum.z == doctest::Approx(1.0F));
    CHECK(box.maximum.z == doctest::Approx(3.0F));
    R::VisibilityRange range;
    range.startMargin = {1.0F, 2.0F};
    range.endMargin = {3.0F, 4.0F};
    CHECK(!V::InRange(range, 0.99F));
    CHECK(V::InRange(range, 1.0F));
    CHECK(V::InRange(range, 3.99F));
    CHECK(!V::InRange(range, 4.0F));
}
