#include "engine/transform/TransformModule.hpp"

#include <utility>

#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/transform/Transform.hpp"

namespace GYO::Engine::Transform {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

GlobalTransform FromTransform(const Transform& transform) {
    return GlobalTransform{ToMatrix(transform)};
}

// GlobalTransform after Transform was written (taking it for writing counts, CCR-8). Without a parent the global
// pose is the local one. Bevy's sync_simple_transforms; its Added<GlobalTransform> branch is not needed, the
// initial value and the hook put the right value already.
SystemResult SyncTransforms(
    Kernel::Query<Kernel::Read<Transform>, Kernel::Write<GlobalTransform>, Kernel::Changed<Transform>> poses) {
    poses.ForEach([](Kernel::EntityId, const Transform& transform, Kernel::Mut<GlobalTransform> global) {
        global->matrix = ToMatrix(transform);
    });
    return {};
}

// Transform appeared on an entity without GlobalTransform: the path the initial value does not take (Restore). On
// a spawn or an insert the initial value has put GlobalTransform before OnAdd, and this does nothing.
SystemResult EnsureGlobalTransform(Kernel::HookCtx hook,
                                   Kernel::Query<Kernel::Read<Transform>, Kernel::Without<GlobalTransform>> missing,
                                   Kernel::CommandsFor<Kernel::Inserts<GlobalTransform>, Kernel::Removes<>> commands) {
    if (const Transform* transform = missing.Get<Transform>(hook.Entity())) {
        commands.Insert(hook.Entity(), FromTransform(*transform));
    }
    return {};
}

} // namespace

void TransformModule::Build(Orchestration::WorldSetup& setup) {
    const TransformOptions* given = setup.Options<TransformOptions>();
    const TransformOptions options = given != nullptr ? *given : TransformOptions{};
    Kernel::Registrar& r = setup.Registrar();
    r.Component<Transform>({.name = "Transform"});
    r.Component<GlobalTransform>({.name = "GlobalTransform"});
    r.InitialValue<GlobalTransform, &FromTransform>();
    r.Hook<Transform, &EnsureGlobalTransform>(Kernel::HookTrigger::OnAdd, {.name = "EnsureGlobalTransform"});
    r.Set({.name = std::string(kPropagateSet), .schedule = options.schedule});
    Kernel::SystemOptions sync;
    sync.name = "SyncTransforms";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.sets.push_back(std::string(kPropagateSet));
    sync.mounts.push_back(std::move(mount));
    r.AddSystem<&SyncTransforms>(std::move(sync));
}

} // namespace GYO::Engine::Transform
