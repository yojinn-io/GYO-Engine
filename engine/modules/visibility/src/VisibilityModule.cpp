#include "engine/visibility/VisibilityModule.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/camera/CameraModule.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/transform/TransformModule.hpp"
#include "engine/visibility/Visibility.hpp"

namespace GYO::Engine::Visibility {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

// Bevy's check_visibility_cpu_culling with check_visibility_ranges folded in: per active, valid 3D camera, the
// entities with an Aabb or a VisibilityRange it does not see, in ascending EntityId (the candidates are gathered once,
// for every camera); the camera's list is written only when it differs (inserted the first time), so
// Changed<CulledEntities> means it changed.
SystemResult CheckVisibility(
    Kernel::Query<Kernel::Read<Camera::Camera>, Kernel::Read<Camera::CameraView>,
                  Kernel::Read<Transform::GlobalTransform>, Kernel::With<Camera::Camera3d>>
        cameras,
    Kernel::Query<Kernel::Write<Render::CulledEntities>> lists,
    Kernel::Query<Kernel::Read<Render::Aabb>, Kernel::Read<Transform::GlobalTransform>> boxed,
    Kernel::Query<Kernel::Read<Render::VisibilityRange>, Kernel::Read<Transform::GlobalTransform>> ranged,
    Kernel::Query<Kernel::With<Render::NoFrustumCulling>> unculled,
    Kernel::CommandsFor<Kernel::Inserts<Render::CulledEntities>, Kernel::Removes<>> commands) {
    std::vector<Kernel::EntityId> ids = boxed.Entities();
    for (const Kernel::EntityId id : ranged.Entities()) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    std::vector<std::pair<Kernel::EntityId, Candidate>> candidates;
    candidates.reserve(ids.size());
    for (const Kernel::EntityId id : ids) {
        const Transform::GlobalTransform* global = boxed.Get<Transform::GlobalTransform>(id);
        if (global == nullptr) global = ranged.Get<Transform::GlobalTransform>(id);
        Candidate candidate;
        candidate.world = global->matrix;
        candidate.aabb = boxed.Get<Render::Aabb>(id);
        candidate.range = ranged.Get<Render::VisibilityRange>(id);
        candidate.noFrustumCulling = unculled.Contains(id);
        candidates.emplace_back(id, candidate);
    }

    cameras.ForEach([&](const Kernel::EntityId camera, const Camera::Camera& settings, const Camera::CameraView& cameraView,
                        const Transform::GlobalTransform& view) {
        if (!settings.active || !cameraView.valid) return;
        // The camera's one world-to-clip value (camera's CM-7), made by UpdateCameraViews from this frame's view point.
        const Math::Frustum frustum = Math::MakeFrustum(cameraView.clipFromWorld);
        const Math::Vec3 position{view.matrix.values[12], view.matrix.values[13], view.matrix.values[14]};
        Render::CulledEntities next;
        for (const auto& [id, candidate] : candidates) {
            if (!Sees(frustum, position, candidate)) next.entities.push_back(id);
        }
        if (!lists.Contains(camera)) {
            commands.Insert(camera, std::move(next));
        } else if (lists.Get<Render::CulledEntities>(camera)->entities != next.entities) {
            *lists.GetMut<Render::CulledEntities>(camera) = std::move(next);
        }
    });
    return {};
}

} // namespace

void VisibilityModule::Build(Orchestration::WorldSetup& setup) {
    const VisibilityOptions* given = setup.Options<VisibilityOptions>();
    const VisibilityOptions options = given != nullptr ? *given : VisibilityOptions{};
    Kernel::Registrar& r = setup.Registrar();
    r.Set({.name = std::string(kCheckSet), .schedule = options.schedule});
    Kernel::SystemOptions check;
    check.name = "CheckVisibility";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.sets.push_back(std::string(kCheckSet));
    mount.after.push_back(std::string(Transform::kPropagateSet));
    mount.after.push_back(std::string(Camera::kUpdateSet));
    check.mounts.push_back(std::move(mount));
    r.AddSystem<&CheckVisibility>(std::move(check));
}

} // namespace GYO::Engine::Visibility
