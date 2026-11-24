// The camera Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 6.1, C-1 to
// C-5; Design Invariants CM-1 to CM-5 in docs/architecture/target/v2/camera.zh-Hant.md). Synthetic units only (no
// product). The expected projections are built with the Math functions the declaration names, from the arguments
// the Bevy formulas give, so a case checks which arguments the camera passes.
// Each CHECK carries a tag in brackets ("[cm-...]"): the mutants expect it
// (docs/architecture/target/v2/scripts/camera_mutations.json).
#include <doctest/doctest.h>

#include <bit>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/entity_ids/EntityIdsModule.hpp"
#include "engine/kernel/Kernel.hpp"
#include "engine/orchestration/Orchestration.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"

namespace C = GYO::Engine::Camera;
namespace K = GYO::Kernel;
namespace M = GYO::Engine::Math;
namespace O = GYO::Engine::Orchestration;
namespace T = GYO::Engine::Transform;

namespace {

bool SameMatrix(const M::Matrix4& a, const M::Matrix4& b) {
    for (std::size_t i = 0; i < 16; ++i) {
        if (std::bit_cast<std::uint32_t>(a.values[i]) != std::bit_cast<std::uint32_t>(b.values[i])) return false;
    }
    return true;
}

C::TargetSize Size(const int w, const int h, const float scale = 1.0F) { return {{w, h}, scale}; }

C::Projection Ortho(C::ScalingMode mode = C::ScalingWindowSize{}, M::Vec2 origin = {0.5F, 0.5F}, float scale = 1.0F) {
    C::OrthographicProjection o = C::OrthographicProjection::Default2d();
    o.scalingMode = mode;
    o.viewportOrigin = origin;
    o.scale = scale;
    return C::Projection{o};
}

M::Matrix4 ExpectedOrtho(float left, float right, float bottom, float top) {
    return M::MakeOrthographic(left, right, bottom, top, 1000.0F, -1000.0F);
}

T::GlobalTransform At(const M::Vec3 position) {
    T::Transform t;
    t.translation = position;
    return T::GlobalTransform{T::ToMatrix(t)};
}

bool Near(const float a, const float b, const float tolerance = 1e-3F) { return std::fabs(a - b) <= tolerance; }

// ---- A Product with the camera Module.
O::RegisterRow ModuleRow(std::string name, std::vector<std::string> depends = {}) {
    O::RegisterRow row;
    row.name = std::move(name);
    row.kind = O::UnitKind::Module;
    row.libraryOnly = false;
    row.defaultOn = true;
    row.depends = std::move(depends);
    return row;
}
class ProbeModule final : public O::Module {
public:
    explicit ProbeModule(std::function<void(O::WorldSetup&)> build) : build_(std::move(build)) {}
    void Build(O::WorldSetup& setup) override { build_(setup); }

private:
    std::function<void(O::WorldSetup&)> build_;
};
// With a probe: a unit registered before camera (only a declared order puts its Systems after camera's).
O::RegisteredUnits Units(std::function<void(O::WorldSetup&)> probe = {}) {
    O::RegisteredUnits units;
    if (probe) {
        units.rows.push_back(ModuleRow("probe"));
        units.modules.push_back({"probe", std::make_unique<ProbeModule>(std::move(probe))});
    }
    units.rows.push_back(ModuleRow("entity_ids"));
    units.modules.push_back({"entity_ids", std::make_unique<GYO::Engine::EntityIds::EntityIdsModule>()});
    units.rows.push_back(ModuleRow("transform"));
    units.modules.push_back({"transform", std::make_unique<T::TransformModule>()});
    units.rows.push_back(ModuleRow("camera", {"transform"}));
    units.modules.push_back({"camera", std::make_unique<C::CameraModule>()});
    return units;
}
K::SystemOptions In(std::string name, std::string schedule, std::string stage) {
    K::SystemOptions o;
    o.name = std::move(name);
    K::Mount m;
    m.schedule = std::move(schedule);
    m.stage = std::move(stage);
    o.mounts.push_back(std::move(m));
    return o;
}
std::unique_ptr<O::Product> Make(std::function<void(O::WorldSetup&)> build, std::function<void(O::WorldSetup&)> probe = {},
                                 const std::string& tag = {}) {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.build = std::move(build);
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units(std::move(probe)));
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

K::SystemResult Spawn2d(K::Spawn<K::Bundle<C::Camera2d, T::Transform>> spawn, K::Local<int> done) {
    if (*done != 0) return {};
    *done = 1;
    auto id = spawn.New(C::Camera2d{}, T::Transform{});
    if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
    return {};
}
K::SystemResult Spawn3d(K::Spawn<K::Bundle<C::Camera3d, T::Transform>> spawn, K::Local<int> done) {
    if (*done != 0) return {};
    *done = 1;
    auto id = spawn.New(C::Camera3d{}, T::Transform{});
    if (!id) return ::GYO::Engine::Base::Err(std::move(id).error());
    return {};
}
std::vector<int>& Changes() {
    static std::vector<int> changes;
    return changes;
}
K::SystemResult CountChanged(K::Query<K::Read<C::ComputedCamera>, K::Changed<C::ComputedCamera>> changed) {
    Changes().push_back(static_cast<int>(changed.Count()));
    return {};
}

} // namespace

// ---- C-1: the initial values

TEST_CASE("camera: Camera2d brings Camera, the main target, the 2D orthographic Projection and ComputedCamera") {
    auto p = Make([](O::WorldSetup& s) { s.Registrar().AddSystem<&Spawn2d>(In("Spawn2d", "Main", "Update")); });
    Frames(*p);
    const std::vector<K::EntityId> ids = WorldOf(*p).Keys<C::Camera2d>();
    REQUIRE(ids.size() == 1);
    K::World& w = WorldOf(*p);
    CHECK_MESSAGE(w.Get<C::Camera>(ids[0]) != nullptr, "[cm-camera-chain] no Camera");
    REQUIRE_MESSAGE(w.Get<C::CameraTarget>(ids[0]) != nullptr, "[cm-camera-chain] no CameraTarget");
    CHECK(w.Get<C::CameraTarget>(ids[0])->id == 0);
    REQUIRE(w.Get<C::Projection>(ids[0]) != nullptr);
    const auto* ortho = std::get_if<C::OrthographicProjection>(w.Get<C::Projection>(ids[0]));
    REQUIRE_MESSAGE(ortho != nullptr, "[cm-camera2d-ortho] Camera2d brought a perspective Projection");
    CHECK_MESSAGE((ortho->nearClip == 1000.0F && ortho->farClip == -1000.0F), "[cm-camera2d-ortho] near and far are not swapped");
    REQUIRE_MESSAGE(w.Get<C::ComputedCamera>(ids[0]) != nullptr, "[cm-camera-chain] no ComputedCamera");
    CHECK(!w.Get<C::ComputedCamera>(ids[0])->valid);
    CHECK(w.Get<T::GlobalTransform>(ids[0]) != nullptr);
}

TEST_CASE("camera: Camera3d brings the perspective Projection") {
    auto p = Make([](O::WorldSetup& s) { s.Registrar().AddSystem<&Spawn3d>(In("Spawn3d", "Main", "Update")); });
    Frames(*p);
    const std::vector<K::EntityId> ids = WorldOf(*p).Keys<C::Camera3d>();
    REQUIRE(ids.size() == 1);
    REQUIRE(WorldOf(*p).Get<C::Projection>(ids[0]) != nullptr);
    CHECK_MESSAGE(std::holds_alternative<C::PerspectiveProjection>(*WorldOf(*p).Get<C::Projection>(ids[0])),
                  "[cm-camera3d-perspective]");
}

// ---- C-2, C-4: UpdateCameras

TEST_CASE("camera: without a target size the camera is not valid; once the size is set UpdateCameras computes it, and writes only changes") {
    Changes().clear();
    auto p = Make([](O::WorldSetup& s) {
        s.Registrar().AddSystem<&Spawn3d>(In("Spawn3d", "Main", "Update"));
        s.Registrar().AddSystem<&CountChanged>(In("CountChanged", "Main", "Last"));
    });
    Frames(*p, 3);  // spawned at the end of frame 1; frame 2 sees it added; frame 3 nothing
    const K::EntityId id = WorldOf(*p).Keys<C::Camera3d>().at(0);
    CHECK_MESSAGE(!WorldOf(*p).Get<C::ComputedCamera>(id)->valid, "[cm-valid-without-size] valid without a target size");
    WorldOf(*p).ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, Size(1920, 1080));
    Frames(*p, 2);
    const C::ComputedCamera& computed = *WorldOf(*p).Get<C::ComputedCamera>(id);
    CHECK(computed.valid);
    CHECK_MESSAGE(SameMatrix(computed.clipFromView, M::MakePerspective(M::Pi / 4.0F, 1920.0F / 1080.0F, 0.1F, 1000.0F)),
                  "[cm-perspective-aspect]");
    REQUIRE(Changes().size() == 5);
    CHECK_MESSAGE(Changes()[2] == 0, "[cm-writes-when-equal] an unchanged frame wrote ComputedCamera");
    CHECK(Changes()[3] == 1);
    CHECK_MESSAGE(Changes()[4] == 0, "[cm-writes-when-equal] an unchanged frame wrote ComputedCamera");
    // The window is resized: the next UpdateCameras writes the new values.
    WorldOf(*p).ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, Size(1000, 1000));
    Frames(*p);
    CHECK_MESSAGE((Changes().back() == 1 &&
                   SameMatrix(WorldOf(*p).Get<C::ComputedCamera>(id)->clipFromView, M::MakePerspective(M::Pi / 4.0F, 1.0F, 0.1F, 1000.0F))),
                  "[cm-recompute] a resize did not reach ComputedCamera");
}

TEST_CASE("camera: the orthographic area follows Bevy's ScalingMode, origin and scale on the logical size") {
    const C::Camera camera;
    const C::TargetSize hiDpi = Size(1600, 900, 2.0F);  // logical 800 x 450
    CHECK_MESSAGE(SameMatrix(C::Compute(camera, Ortho(), &hiDpi).clipFromView, ExpectedOrtho(-400, 400, -225, 225)),
                  "[cm-windowsize-logical] WindowSize does not use the logical size");
    CHECK_MESSAGE(SameMatrix(C::Compute(camera, Ortho(C::ScalingFixed{1280, 720}), &hiDpi).clipFromView,
                             ExpectedOrtho(-640, 640, -360, 360)),
                  "[cm-fixed] Fixed follows the target");
    CHECK_MESSAGE(SameMatrix(C::Compute(camera, Ortho(C::ScalingWindowSize{}, {0.0F, 1.0F}), &hiDpi).clipFromView,
                             ExpectedOrtho(0, 800, -450, 0)),
                  "[cm-origin] viewportOrigin (0, 1)");
    CHECK_MESSAGE(SameMatrix(C::Compute(camera, Ortho(C::ScalingWindowSize{}, {0.5F, 0.5F}, 2.0F), &hiDpi).clipFromView,
                             ExpectedOrtho(-800, 800, -450, 450)),
                  "[cm-scale] scale 2");
}

TEST_CASE("camera: each camera uses the size of its own target") {
    Changes().clear();
    auto p = Make([](O::WorldSetup&) {});
    K::World& w = WorldOf(*p);
    w.Insert(1, C::Camera{});
    w.Insert(1, C::CameraTarget{0});
    w.Insert(1, C::Projection{C::PerspectiveProjection{}});
    w.Insert(1, C::ComputedCamera{});
    w.Insert(2, C::Camera{});
    w.Insert(2, C::CameraTarget{1});
    w.Insert(2, C::Projection{C::PerspectiveProjection{}});
    w.Insert(2, C::ComputedCamera{});
    w.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, Size(800, 600));
    w.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{1}, Size(400, 400));
    Frames(*p);
    CHECK_MESSAGE(SameMatrix(w.Get<C::ComputedCamera>(2)->clipFromView, M::MakePerspective(M::Pi / 4.0F, 1.0F, 0.1F, 1000.0F)),
                  "[cm-target-own] the camera of target 1 used another target's size");
    CHECK(w.Get<C::ComputedCamera>(2)->targetPhysicalSize.x == 400);
    CHECK(w.Get<C::ComputedCamera>(1)->targetPhysicalSize.x == 800);
}

// ---- C-3: the viewport

TEST_CASE("camera: a viewport sets the aspect and is clamped into the target") {
    C::Camera camera;
    camera.viewport = C::Viewport{{100, 50}, {800, 600}};
    const C::TargetSize size = Size(1920, 1080);
    const C::ComputedCamera inside = C::Compute(camera, C::Projection{C::PerspectiveProjection{}}, &size);
    CHECK_MESSAGE(SameMatrix(inside.clipFromView, M::MakePerspective(M::Pi / 4.0F, 800.0F / 600.0F, 0.1F, 1000.0F)),
                  "[cm-viewport-aspect] the aspect is not the viewport's");
    camera.viewport = C::Viewport{{1800, 1000}, {400, 400}};
    const C::ComputedCamera clamped = C::Compute(camera, C::Projection{C::PerspectiveProjection{}}, &size);
    CHECK_MESSAGE((clamped.viewport.size.x == 120 && clamped.viewport.size.y == 80), "[cm-viewport-clamp] "
                      << clamped.viewport.size.x << " x " << clamped.viewport.size.y);
    camera.viewport = C::Viewport{{-10, 0}, {100, 100}};
    const C::ComputedCamera negative = C::Compute(camera, C::Projection{C::PerspectiveProjection{}}, &size);
    CHECK_MESSAGE((negative.viewport.position.x == 0 && negative.viewport.size.x == 100), "[cm-viewport-clamp] negative position");
    camera.viewport = C::Viewport{{2000, 0}, {100, 100}};
    const C::ComputedCamera outside = C::Compute(camera, C::Projection{C::PerspectiveProjection{}}, &size);
    CHECK_MESSAGE((outside.viewport.position.x == 1919 && outside.viewport.size.x == 1), "[cm-viewport-clamp] outside "
                      << outside.viewport.position.x << " " << outside.viewport.size.x);
    camera.viewport = C::Viewport{{0, 0}, {0, 100}};
    CHECK(!C::Compute(camera, C::Projection{C::PerspectiveProjection{}}, &size).valid);
    // A minimized window (0 x 0) and a scale factor that is not positive leave the camera not valid.
    camera.viewport = C::Viewport{{10, 10}, {100, 100}};
    const C::TargetSize minimized = Size(0, 0);
    CHECK_MESSAGE(!C::Compute(camera, C::Projection{C::PerspectiveProjection{}}, &minimized).valid, "[cm-zero-target] valid in a 0 x 0 target");
    const C::TargetSize noScale = Size(1920, 1080, 0.0F);
    CHECK_MESSAGE(!C::Compute(C::Camera{}, C::Projection{C::PerspectiveProjection{}}, &noScale).valid, "[cm-zero-scale] valid with scale factor 0");
}

// ---- C-5: the conversions

TEST_CASE("camera: ViewportToWorld2d maps the corners of a Fixed 1280 x 720 camera in a 2560 x 1440 window") {
    const C::TargetSize size = Size(2560, 1440);
    const C::ComputedCamera computed = C::Compute(C::Camera{}, Ortho(C::ScalingFixed{1280, 720}), &size);
    auto bottomRight = C::ViewportToWorld2d(computed, At({}), {2560.0F, 1440.0F});
    REQUIRE(static_cast<bool>(bottomRight));
    CHECK_MESSAGE((Near(bottomRight->x, 640.0F) && Near(bottomRight->y, -360.0F)), "[cm-to-world-2d] " << bottomRight->x << ", " << bottomRight->y);
    auto topLeft = C::ViewportToWorld2d(computed, At({}), {0.0F, 0.0F});
    REQUIRE(static_cast<bool>(topLeft));
    CHECK_MESSAGE((Near(topLeft->x, -640.0F) && Near(topLeft->y, 360.0F)), "[cm-to-world-2d-y] " << topLeft->x << ", " << topLeft->y);
    auto moved = C::ViewportToWorld2d(computed, At({100.0F, 20.0F, 0.0F}), {1280.0F, 720.0F});
    REQUIRE(static_cast<bool>(moved));
    CHECK((Near(moved->x, 100.0F) && Near(moved->y, 20.0F)));
}

TEST_CASE("camera: with a letterbox viewport the window position includes the viewport's offset (as Bevy)") {
    C::Camera camera;
    camera.viewport = C::Viewport{{0, 80}, {2560, 1440}};
    const C::TargetSize size = Size(2560, 1600);
    const C::ComputedCamera computed = C::Compute(camera, Ortho(C::ScalingFixed{1280, 720}), &size);
    auto corner = C::ViewportToWorld2d(computed, At({}), {2560.0F, 1520.0F});
    REQUIRE(static_cast<bool>(corner));
    CHECK_MESSAGE((Near(corner->x, 640.0F) && Near(corner->y, -360.0F)), "[cm-viewport-offset] " << corner->x << ", " << corner->y);
    auto back = C::WorldToViewport(computed, At({}), {-640.0F, 360.0F, 0.0F});
    REQUIRE(static_cast<bool>(back));
    CHECK_MESSAGE((Near(back->x, 0.0F) && Near(back->y, 80.0F)), "[cm-viewport-offset] back " << back->x << ", " << back->y);
}

TEST_CASE("camera: Letterbox cuts the viewport to its aspect, centred, and the conversions follow it") {
    const C::Letterbox box{1280.0F, 720.0F};
    // A 2560 x 1600 window (16:10): bars of 80 pixels above and below.
    const C::TargetSize tall = Size(2560, 1600);
    const C::ComputedCamera computed = C::Compute(C::Camera{}, Ortho(C::ScalingFixed{1280, 720}), &tall, &box);
    CHECK_MESSAGE((computed.valid && computed.viewport.position.x == 0 && computed.viewport.position.y == 80 &&
                   computed.viewport.size.x == 2560 && computed.viewport.size.y == 1440),
                  "[cm-letterbox-bars] " << computed.viewport.position.x << "," << computed.viewport.position.y << " "
                                         << computed.viewport.size.x << "x" << computed.viewport.size.y);
    CHECK_MESSAGE(SameMatrix(computed.clipFromView, ExpectedOrtho(-640, 640, -360, 360)), "[cm-letterbox-projection]");
    auto corner = C::ViewportToWorld2d(computed, At({}), {2560.0F, 1520.0F});
    REQUIRE(static_cast<bool>(corner));
    CHECK_MESSAGE((Near(corner->x, 640.0F) && Near(corner->y, -360.0F)), "[cm-letterbox-convert] " << corner->x << ", " << corner->y);
    // A 2001 x 720 window (wider): bars left and right, the odd pixel on the right.
    const C::TargetSize wide = Size(2001, 720);
    const C::ComputedCamera pillar = C::Compute(C::Camera{}, Ortho(C::ScalingFixed{1280, 720}), &wide, &box);
    CHECK_MESSAGE((pillar.viewport.position.x == 360 && pillar.viewport.position.y == 0 && pillar.viewport.size.x == 1280 &&
                   pillar.viewport.size.y == 720),
                  "[cm-letterbox-pillar] " << pillar.viewport.position.x << " " << pillar.viewport.size.x);
    // Inside the author's viewport, not the whole target.
    C::Camera inViewport;
    inViewport.viewport = C::Viewport{{100, 0}, {1600, 1600}};
    const C::ComputedCamera nested = C::Compute(inViewport, Ortho(C::ScalingFixed{1280, 720}), &tall, &box);
    CHECK_MESSAGE((nested.viewport.position.x == 100 && nested.viewport.position.y == 350 && nested.viewport.size.x == 1600 &&
                   nested.viewport.size.y == 900),
                  "[cm-letterbox-in-viewport] " << nested.viewport.position.y << " " << nested.viewport.size.y);
    CHECK_MESSAGE((inViewport.viewport->position.y == 0), "the author's viewport is not changed");
    const C::Letterbox broken{0.0F, 720.0F};
    CHECK_MESSAGE(!C::Compute(C::Camera{}, Ortho(), &tall, &broken).valid, "[cm-letterbox-invalid] a zero width must leave the camera not valid");
}

TEST_CASE("camera: UpdateCameras uses a camera's Letterbox") {
    auto p = Make([](O::WorldSetup& s) { s.Registrar().AddSystem<&Spawn2d>(In("Spawn2d", "Main", "Update")); });
    Frames(*p);
    const K::EntityId id = WorldOf(*p).Keys<C::Camera2d>().at(0);
    WorldOf(*p).Insert(id, C::Letterbox{4.0F, 3.0F});
    WorldOf(*p).ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, Size(1600, 900));
    Frames(*p);
    const C::ComputedCamera& computed = *WorldOf(*p).Get<C::ComputedCamera>(id);
    CHECK_MESSAGE((computed.viewport.position.x == 200 && computed.viewport.size.x == 1200 && computed.viewport.size.y == 900),
                  "[cm-letterbox-system] " << computed.viewport.position.x << " " << computed.viewport.size.x);
}

TEST_CASE("camera: WorldToViewport and ViewportToWorld go there and back, perspective and orthographic") {
    const C::TargetSize size = Size(1280, 720);
    {
        const C::ComputedCamera computed = C::Compute(C::Camera{}, Ortho(C::ScalingFixed{1280, 720}), &size);
        auto position = C::WorldToViewport(computed, At({}), {100.0F, 50.0F, 0.0F});
        REQUIRE(static_cast<bool>(position));
        CHECK_MESSAGE((Near(position->x, 740.0F) && Near(position->y, 310.0F)), "[cm-to-viewport] " << position->x << ", " << position->y);
    }
    {
        const C::ComputedCamera computed = C::Compute(C::Camera{}, C::Projection{C::PerspectiveProjection{}}, &size);
        const T::GlobalTransform view = At({0.0F, 0.0F, -10.0F});
        const M::Vec3 world{1.0F, 2.0F, 5.0F};
        auto position = C::WorldToViewport(computed, view, world);
        REQUIRE(static_cast<bool>(position));
        auto ray = C::ViewportToWorld(computed, view, *position);
        REQUIRE(static_cast<bool>(ray));
        // The ray starts on the near plane (view z = 0.1, world z = -9.9) and passes through the point.
        CHECK_MESSAGE(Near(ray->origin.z, -9.9F), "[cm-ray-near] the ray does not start on the near plane: " << ray->origin.z);
        CHECK_MESSAGE(ray->direction.z > 0.0F, "[cm-ray-direction] the ray points back: " << ray->direction.z);
        const float t = (world.z - ray->origin.z) / ray->direction.z;
        CHECK(Near(ray->origin.x + t * ray->direction.x, world.x));
        CHECK(Near(ray->origin.y + t * ray->direction.y, world.y));
    }
}

TEST_CASE("camera: the ray of a turned camera turns with it") {
    // Turned 90 degrees about Y: forward +Z becomes +X (math.md).
    const C::TargetSize size = Size(1280, 720);
    const C::ComputedCamera computed = C::Compute(C::Camera{}, C::Projection{C::PerspectiveProjection{}}, &size);
    T::Transform turned;
    turned.rotation = M::MakeQuaternionFromAxisAngle({0.0F, 1.0F, 0.0F}, M::Pi / 2.0F);
    auto ray = C::ViewportToWorld(computed, T::GlobalTransform{T::ToMatrix(turned)}, {640.0F, 360.0F});
    REQUIRE(static_cast<bool>(ray));
    CHECK_MESSAGE((ray->direction.x > 0.0F && Near(ray->direction.z, 0.0F) && Near(ray->origin.x, 0.1F)),
                  "[cm-ray-rotated] " << ray->direction.x << " " << ray->direction.z << " origin " << ray->origin.x);
}

TEST_CASE("camera: the conversions take logical pixels on a HiDPI target") {
    const C::TargetSize size = Size(2560, 1440, 2.0F);  // logical 1280 x 720
    const C::ComputedCamera computed = C::Compute(C::Camera{}, Ortho(C::ScalingFixed{1280, 720}), &size);
    auto corner = C::ViewportToWorld2d(computed, At({}), {1280.0F, 720.0F});
    REQUIRE(static_cast<bool>(corner));
    CHECK_MESSAGE((Near(corner->x, 640.0F) && Near(corner->y, -360.0F)), "[cm-convert-logical] " << corner->x << ", " << corner->y);
}

TEST_CASE("camera: WorldToViewport adds a pillarbox viewport's x offset") {
    C::Camera camera;
    camera.viewport = C::Viewport{{160, 0}, {2240, 1260}};
    const C::TargetSize size = Size(2560, 1440);
    const C::ComputedCamera computed = C::Compute(camera, Ortho(C::ScalingFixed{1280, 720}), &size);
    auto corner = C::WorldToViewport(computed, At({}), {-640.0F, 360.0F, 0.0F});
    REQUIRE(static_cast<bool>(corner));
    CHECK_MESSAGE((Near(corner->x, 160.0F) && Near(corner->y, 0.0F)), "[cm-world-to-viewport-offset] " << corner->x << ", " << corner->y);
}

TEST_CASE("camera: the conversions report no viewport size, past the near plane and past the far plane") {
    auto none = C::WorldToViewport(C::ComputedCamera{}, At({}), {});
    REQUIRE(!none);
    CHECK(none.error().code == C::ViewportConversionFailureCode::NoViewportSize);
    auto noRay = C::ViewportToWorld(C::ComputedCamera{}, At({}), {});
    CHECK_MESSAGE((!noRay && noRay.error().code == C::ViewportConversionFailureCode::NoViewportSize), "[cm-errors] ViewportToWorld");
    auto no2d = C::ViewportToWorld2d(C::ComputedCamera{}, At({}), {});
    CHECK_MESSAGE((!no2d && no2d.error().code == C::ViewportConversionFailureCode::NoViewportSize), "[cm-errors] ViewportToWorld2d");
    const C::TargetSize size = Size(1280, 720);
    const C::ComputedCamera ortho = C::Compute(C::Camera{}, Ortho(), &size);  // near z 1000, far z -1000
    auto nearer = C::WorldToViewport(ortho, At({}), {0.0F, 0.0F, 1500.0F});
    REQUIRE(!nearer);
    CHECK_MESSAGE(nearer.error().code == C::ViewportConversionFailureCode::PastNearPlane, "[cm-past-planes] ortho near");
    auto farther = C::WorldToViewport(ortho, At({}), {0.0F, 0.0F, -1500.0F});
    REQUIRE(!farther);
    CHECK_MESSAGE(farther.error().code == C::ViewportConversionFailureCode::PastFarPlane, "[cm-past-planes] ortho far");
    const C::ComputedCamera perspective = C::Compute(C::Camera{}, C::Projection{C::PerspectiveProjection{}}, &size);
    auto close = C::WorldToViewport(perspective, At({}), {0.0F, 0.0F, 0.05F});
    REQUIRE(!close);
    CHECK_MESSAGE(close.error().code == C::ViewportConversionFailureCode::PastNearPlane, "[cm-past-planes] perspective near");
    auto behind = C::WorldToViewport(perspective, At({}), {0.0F, 0.0F, -5.0F});
    REQUIRE(!behind);
    CHECK_MESSAGE(behind.error().code == C::ViewportConversionFailureCode::PastFarPlane,
                  "[cm-past-planes] a point behind a perspective camera is past the far plane (as Bevy)");
    auto nan = C::WorldToViewport(perspective, At({}), {std::nanf(""), 0.0F, 5.0F});
    CHECK_MESSAGE((!nan && nan.error().code == C::ViewportConversionFailureCode::InvalidData), "[cm-errors] a NaN point");
    T::Transform flat;
    flat.scale = {0.0F, 1.0F, 1.0F};
    auto singular = C::WorldToViewport(perspective, T::GlobalTransform{T::ToMatrix(flat)}, {0.0F, 0.0F, 5.0F});
    CHECK_MESSAGE((!singular && singular.error().code == C::ViewportConversionFailureCode::InvalidData), "[cm-errors] a GlobalTransform that is not invertible");
}

TEST_CASE("camera: CameraTargetSizes keeps one size per id, finds only that id and removes it") {
    C::CameraTargetSizes sizes;
    sizes.Set(C::CameraTarget{2}, Size(20, 20));
    sizes.Set(C::CameraTarget{0}, Size(10, 10));
    CHECK_MESSAGE(sizes.Find(C::CameraTarget{1}) == nullptr, "[cm-sizes] found an id that was never set");
    REQUIRE(sizes.Find(C::CameraTarget{0}) != nullptr);
    REQUIRE(sizes.Find(C::CameraTarget{2}) != nullptr);
    CHECK_MESSAGE(sizes.Find(C::CameraTarget{2})->physicalSize.x == 20, "[cm-sizes] the sizes are out of order");
    sizes.Set(C::CameraTarget{0}, Size(30, 30));
    CHECK_MESSAGE(sizes.Find(C::CameraTarget{0})->physicalSize.x == 30, "[cm-sizes] a second Set did not replace the size");
    sizes.Remove(C::CameraTarget{0});
    CHECK_MESSAGE(sizes.Find(C::CameraTarget{0}) == nullptr, "[cm-sizes] Remove left the size");
    CHECK(sizes.Find(C::CameraTarget{2}) != nullptr);
}

namespace {
std::vector<std::string>& Reads() {
    static std::vector<std::string> reads;
    return reads;
}
// In the probe (registered before camera): PostUpdate, after the public set.
K::SystemResult ReadAfterUpdate(K::Query<K::Read<C::ComputedCamera>> cameras) {
    cameras.ForEach([](K::EntityId, const C::ComputedCamera& c) { Reads().push_back(c.valid ? "valid" : "not valid"); });
    return {};
}
void ProbeAfterUpdate(O::WorldSetup& s) {
    K::SystemOptions o = In("ReadAfterUpdate", "Main", "PostUpdate");
    o.mounts[0].after = {std::string(C::kUpdateSet)};
    s.Registrar().AddSystem<&ReadAfterUpdate>(o);
}
void PutCamera(K::World& w) {
    w.Insert(1, C::Camera{});
    w.Insert(1, C::CameraTarget{0});
    w.Insert(1, C::Projection{C::PerspectiveProjection{}});
    w.Insert(1, C::ComputedCamera{});
    w.ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, Size(800, 600));
}
} // namespace

TEST_CASE("camera: a System ordered after CameraUpdate in PostUpdate reads this frame's ComputedCamera") {
    Reads().clear();
    // A startup error (the set has no member: D-E4) fails here with the same tag.
    auto p = Make([](O::WorldSetup&) {}, &ProbeAfterUpdate, "[cm-update-set] ");
    PutCamera(WorldOf(*p));
    Frames(*p);
    CHECK_MESSAGE((Reads().size() == 1 && Reads()[0] == "valid"), "[cm-update-set] read before UpdateCameras");
}

TEST_CASE("camera: a World with the skeleton off names where UpdateCameras runs") {
    O::ProductDescription d;
    d.name = "game";
    O::WorldDescription w;
    w.name = "Main";
    w.skeleton = O::ScheduleSkeleton::Off;
    w.frame = {"Frame"};
    w.options.Set(C::CameraOptions{"Frame", "Late"});
    w.options.Set(T::TransformOptions{"Frame", "Late"});
    w.build = [](O::WorldSetup& s) {
        K::ScheduleOptions frame;
        frame.name = "Frame";
        frame.stages = {"Logic", "Late"};
        s.Registrar().Schedule(frame);
    };
    d.worlds.push_back(std::move(w));
    auto built = O::Product::Build(std::move(d), Units());
    REQUIRE_MESSAGE(static_cast<bool>(built), "[cm-options] " << (built ? std::string() : built.error().detail));
    PutCamera((*built)->World("Main").GetWorld());
    Frames(**built);
    CHECK_MESSAGE((*built)->World("Main").GetWorld().Get<C::ComputedCamera>(1)->valid, "[cm-options] UpdateCameras did not run in Late");
}

// ---- The summary (CM-3, CM-5)

TEST_CASE("camera: the summary hashes the author's settings and nothing of ComputedCamera or the target sizes") {
    auto a = Make([](O::WorldSetup&) {});
    auto b = Make([](O::WorldSetup&) {});
    for (O::Product* p : {a.get(), b.get()}) {
        WorldOf(*p).Insert(1, C::Camera{});
        WorldOf(*p).Insert(1, C::Projection{C::PerspectiveProjection{}});
    }
    auto text = [](O::Product& p) {
        auto s = WorldOf(p).Summary();
        REQUIRE(static_cast<bool>(s));
        return s->Text();
    };
    WorldOf(*a).Insert(1, C::ComputedCamera{});
    WorldOf(*b).Insert(1, C::ComputedCamera{true});
    CHECK_MESSAGE(text(*a) == text(*b), "[cm-hash-computed-empty]");
    WorldOf(*b).Insert(1, C::ComputedCamera{});
    WorldOf(*b).ResourceMut<C::CameraTargetSizes>()->Set(C::CameraTarget{0}, Size(10, 10));
    CHECK_MESSAGE(text(*a) == text(*b), "[cm-hash-target-sizes-empty]");
    WorldOf(*b).Insert(1, C::Projection{C::PerspectiveProjection{1.0F}});
    CHECK_MESSAGE(text(*a) != text(*b), "[cm-hash-settings] Projection");
    WorldOf(*b).Insert(1, C::Projection{C::PerspectiveProjection{}});
    C::Camera ordered;
    ordered.order = 3;
    WorldOf(*b).Insert(1, ordered);
    CHECK_MESSAGE(text(*a) != text(*b), "[cm-hash-settings] Camera");
    WorldOf(*b).Insert(1, C::Camera{});
    WorldOf(*b).Insert(1, C::CameraTarget{4});
    CHECK_MESSAGE(text(*a) != text(*b), "[cm-hash-settings] CameraTarget");
    WorldOf(*a).Insert(1, Ortho());
    WorldOf(*b).Insert(1, C::CameraTarget{});
    WorldOf(*b).Insert(1, Ortho(C::ScalingFixed{2.0F, 3.0F}));
    CHECK_MESSAGE(text(*a) != text(*b), "[cm-hash-settings] OrthographicProjection");
    WorldOf(*b).Insert(1, Ortho());
    WorldOf(*a).Insert(1, C::CameraTarget{});
    REQUIRE(text(*a) == text(*b));
    WorldOf(*a).Insert(1, C::Letterbox{16.0F, 9.0F});
    WorldOf(*b).Insert(1, C::Letterbox{4.0F, 3.0F});
    CHECK_MESSAGE(text(*a) != text(*b), "[cm-hash-settings] Letterbox");
}
