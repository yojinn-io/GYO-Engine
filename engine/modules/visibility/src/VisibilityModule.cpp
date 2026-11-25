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
// entities with an Aabb or a VisibilityRange it sees, in ascending EntityId; the camera's list is written only when it
// differs (inserted the first time), so Changed<VisibleEntities> means it changed.
SystemResult CheckVisibility(
    Kernel::Query<Kernel::Read<Camera::Camera>, Kernel::Read<Camera::ComputedCamera>,
                  Kernel::Read<Transform::GlobalTransform>, Kernel::With<Camera::Camera3d>>
        cameras,
    Kernel::Query<Kernel::Write<Render::VisibleEntities>> lists,
    Kernel::Query<Kernel::Read<Render::Aabb>, Kernel::Read<Transform::GlobalTransform>> boxed,
    Kernel::Query<Kernel::Read<Render::VisibilityRange>, Kernel::Read<Transform::GlobalTransform>> ranged,
    Kernel::Query<Kernel::With<Render::NoFrustumCulling>> unculled,
    Kernel::CommandsFor<Kernel::Inserts<Render::VisibleEntities>, Kernel::Removes<>> commands) {
    std::vector<Kernel::EntityId> candidates = boxed.Entities();
    for (const Kernel::EntityId id : ranged.Entities()) candidates.push_back(id);
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());

    cameras.ForEach([&](const Kernel::EntityId camera, const Camera::Camera& settings, const Camera::ComputedCamera& computed,
                        const Transform::GlobalTransform& view) {
        if (!settings.active || !computed.valid) return;
        const std::optional<Math::Matrix4> viewFromWorld = Math::Inverse(view.matrix);
        if (!viewFromWorld) return;
        const Math::Frustum frustum = Math::MakeFrustum(Math::Multiply(computed.clipFromView, *viewFromWorld));
        const Math::Vec3 position{view.matrix.values[12], view.matrix.values[13], view.matrix.values[14]};
        Render::VisibleEntities next;
        for (const Kernel::EntityId id : candidates) {
            Candidate candidate;
            const Transform::GlobalTransform* global = boxed.Get<Transform::GlobalTransform>(id);
            if (global == nullptr) global = ranged.Get<Transform::GlobalTransform>(id);
            if (global == nullptr) continue;
            candidate.world = global->matrix;
            candidate.aabb = boxed.Get<Render::Aabb>(id);
            candidate.range = ranged.Get<Render::VisibilityRange>(id);
            candidate.noFrustumCulling = unculled.Contains(id);
            if (Sees(frustum, position, candidate)) next.entities.push_back(id);
        }
        if (!lists.Contains(camera)) {
            commands.Insert(camera, std::move(next));
        } else if (lists.Get<Render::VisibleEntities>(camera)->entities != next.entities) {
            *lists.GetMut<Render::VisibleEntities>(camera) = std::move(next);
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
