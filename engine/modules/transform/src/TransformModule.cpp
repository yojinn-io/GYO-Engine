#include "engine/transform/TransformModule.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/Relationship.hpp"
#include "engine/transform/Transform.hpp"

namespace GYO::Engine::Transform {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;
using Kernel::EntityId;

GlobalTransform FromTransform(const Transform& transform) {
    return GlobalTransform{ToMatrix(transform)};
}

// One entity still to visit below a root: its parent's global matrix and whether that parent (or an ancestor) was
// rewritten this run.
struct Pending {
    EntityId entity;
    Math::Matrix4 parentGlobal;
    bool dirty;
};

// Pushes the children in reverse, so the first child is visited first (depth first, pre-order, in Children order;
// the order does not change any value, it only keeps the walk the same on every run).
void PushChildren(std::vector<Pending>& stack, const Kernel::Children& children, const Math::Matrix4& global,
                  const bool dirty) {
    for (std::size_t i = children.size(); i > 0; --i) stack.push_back({children[i - 1], global, dirty});
}

// GlobalTransform of every entity with Transform: Bevy's sync_simple_transforms and propagate_parent_transforms in
// one System (TF-3, TF-5).
// - A root (no ChildOf) is rewritten when its Transform was written, its GlobalTransform was added, or it lost its
//   ChildOf since the last run (an orphan); then global = ToMatrix(transform).
// - Below a root, the walk follows Children; an entity is rewritten when its parent was, or its Transform or
//   ChildOf was written, or its GlobalTransform was added; then global = parent's global * ToMatrix(transform).
// - The walk stops at an entity without Transform or GlobalTransform: its subtree keeps its old values (Bevy's
//   transform_query fails there the same way). An entity in a cycle of ChildOf is never reached from a root.
// "Written" counts taking for writing with the same value (CCR-8): Changed<GlobalTransform> means an input was
// written, not that the matrix moved.
//
// Orphans: the kernel keeps no record of removed components (Bevy's RemovedComponents<ChildOf>), so the System
// keeps the entities that had ChildOf at its last run (Unity's PreviousParent, kept per System instead of per
// entity). It is what the last run saw, the same view GlobalTransform holds (GlobalTransform is not restored), so a
// Restore, a Host's World::Remove and a despawn of the parent are all caught.
SystemResult PropagateTransforms(
    Kernel::Query<Kernel::Read<Transform>, Kernel::Write<GlobalTransform>> poses,
    Kernel::Query<Kernel::With<Transform>, Kernel::With<GlobalTransform>, Kernel::Without<Kernel::ChildOf>> roots,
    Kernel::Query<Kernel::Read<Kernel::Children>> children, Kernel::Query<Kernel::With<Kernel::ChildOf>> parented,
    Kernel::Query<Kernel::Changed<Transform>> moved, Kernel::Query<Kernel::Changed<Kernel::ChildOf>> reparented,
    Kernel::Query<Kernel::Added<GlobalTransform>> added, Kernel::Local<std::vector<EntityId>> hadParent) {
    std::vector<EntityId> parentedNow = parented.Entities();
    std::vector<EntityId> orphans;
    std::set_difference(hadParent->begin(), hadParent->end(), parentedNow.begin(), parentedNow.end(),
                        std::back_inserter(orphans));
    *hadParent = std::move(parentedNow);

    std::vector<Pending> stack;
    for (const EntityId root : roots.Entities()) {
        const bool dirty = moved.Contains(root) || added.Contains(root) ||
                           std::binary_search(orphans.begin(), orphans.end(), root);
        Kernel::Mut<GlobalTransform> global = poses.GetMut<GlobalTransform>(root);
        if (dirty) global->matrix = ToMatrix(*poses.Get<Transform>(root));
        if (const Kernel::Children* kids = children.Get<Kernel::Children>(root)) {
            PushChildren(stack, *kids, global.Read().matrix, dirty);
        }
        while (!stack.empty()) {
            const Pending next = stack.back();
            stack.pop_back();
            const Transform* transform = poses.Get<Transform>(next.entity);
            if (transform == nullptr) continue;
            Kernel::Mut<GlobalTransform> childGlobal = poses.GetMut<GlobalTransform>(next.entity);
            const bool childDirty = next.dirty || moved.Contains(next.entity) ||
                                    reparented.Contains(next.entity) || added.Contains(next.entity);
            if (childDirty) childGlobal->matrix = Math::Multiply(next.parentGlobal, ToMatrix(*transform));
            if (const Kernel::Children* kids = children.Get<Kernel::Children>(next.entity)) {
                PushChildren(stack, *kids, childGlobal.Read().matrix, childDirty);
            }
        }
    }
    return {};
}

// Transform appeared on an entity without GlobalTransform: the path the initial value does not take (Restore). On
// a spawn or an insert the initial value has put GlobalTransform before OnAdd, and this does nothing. With a parent
// the value is the local matrix until the next propagation (as the initial value).
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
    Kernel::SystemOptions propagate;
    propagate.name = "PropagateTransforms";
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = options.stage;
    mount.sets.push_back(std::string(kPropagateSet));
    propagate.mounts.push_back(std::move(mount));
    r.AddSystem<&PropagateTransforms>(std::move(propagate));
}

} // namespace GYO::Engine::Transform
