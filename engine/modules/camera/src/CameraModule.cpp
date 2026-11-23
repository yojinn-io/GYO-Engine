#include "engine/camera/CameraModule.hpp"

#include <bit>
#include <cstdint>
#include <utility>

#include "engine/camera/Camera.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"

namespace GYO::Engine::Camera {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

bool SameBits(const float a, const float b) noexcept {
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

// Bit by bit, as the hash compares (CM-3: written only when it differs; a NaN does not make it write every frame).
bool SameComputed(const ComputedCamera& a, const ComputedCamera& b) noexcept {
    if (a.valid != b.valid || a.targetPhysicalSize.x != b.targetPhysicalSize.x ||
        a.targetPhysicalSize.y != b.targetPhysicalSize.y || !SameBits(a.scaleFactor, b.scaleFactor) ||
        a.viewport.position.x != b.viewport.position.x || a.viewport.position.y != b.viewport.position.y ||
        a.viewport.size.x != b.viewport.size.x || a.viewport.size.y != b.viewport.size.y ||
        !SameBits(a.viewport.minDepth, b.viewport.minDepth) || !SameBits(a.viewport.maxDepth, b.viewport.maxDepth)) {
        return false;
    }
    for (std::size_t i = 0; i < a.clipFromView.values.size(); ++i) {
        if (!SameBits(a.clipFromView.values[i], b.clipFromView.values[i])) return false;
    }
    return true;
}

Camera CameraFrom2d(const Camera2d&) { return Camera{}; }
Camera CameraFrom3d(const Camera3d&) { return Camera{}; }
Projection ProjectionFrom2d(const Camera2d&) { return Projection{OrthographicProjection::Default2d()}; }
Projection ProjectionFrom3d(const Camera3d&) { return Projection{PerspectiveProjection{}}; }
CameraTarget TargetFromCamera(const Camera&) { return CameraTarget{}; }
ComputedCamera ComputedFromCamera(const Camera&) { return ComputedCamera{}; }

// Bevy's camera_system: every frame, every camera with a Projection; written only when the value differs, so
// Changed<ComputedCamera> means it changed (CM-3, CCR-8).
SystemResult UpdateCameras(Kernel::Query<Kernel::Read<Camera>, Kernel::Read<CameraTarget>, Kernel::Read<Projection>,
                                         Kernel::Write<ComputedCamera>>
                               cameras,
                           Kernel::Res<CameraTargetSizes> sizes) {
    cameras.ForEach([&sizes](Kernel::EntityId, const Camera& camera, const CameraTarget& target,
                             const Projection& projection, Kernel::Mut<ComputedCamera> computed) {
        ComputedCamera next = Compute(camera, projection, sizes->Find(target));
        if (!SameComputed(computed.Read(), next)) *computed = std::move(next);
    });
    return {};
}

} // namespace

void CameraModule::Build(Orchestration::WorldSetup& setup) {
    const CameraOptions* given = setup.Options<CameraOptions>();
    const CameraOptions options = given != nullptr ? *given : CameraOptions{};
    Kernel::Registrar& r = setup.Registrar();
    r.Component<Camera>({.name = "Camera"});
    r.Component<CameraTarget>({.name = "CameraTarget"});
    r.Component<Projection>({.name = "Projection"});
    r.Component<Camera2d>({.name = "Camera2d"});
    r.Component<Camera3d>({.name = "Camera3d"});
    r.Component<ComputedCamera>({.name = "ComputedCamera"});
    r.Resource<CameraTargetSizes>({.name = "CameraTargetSizes"});
    // Camera does not give a Projection (as in Bevy): with Camera and Camera2d in one spawn, the Projection would
    // come from the lower ComponentId.
    r.InitialValue<Camera, &CameraFrom2d>();
    r.InitialValue<Camera, &CameraFrom3d>();
    r.InitialValue<Projection, &ProjectionFrom2d>();
    r.InitialValue<Projection, &ProjectionFrom3d>();
    r.InitialValue<CameraTarget, &TargetFromCamera>();
    r.InitialValue<ComputedCamera, &ComputedFromCamera>();
    r.Set({.name = std::string(kUpdateSet), .schedule = options.schedule});
    Kernel::SystemOptions update;
    update.name = "UpdateCameras";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.sets.push_back(std::string(kUpdateSet));
    update.mounts.push_back(std::move(mount));
    r.AddSystem<&UpdateCameras>(std::move(update));
    // Empty unless a value is already there (carried over, O-7).
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        if (world.Resource<CameraTargetSizes>() == nullptr) world.SetResource(CameraTargetSizes{});
        return {};
    });
}

} // namespace GYO::Engine::Camera
