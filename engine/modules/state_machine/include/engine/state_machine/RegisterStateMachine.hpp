#pragma once

// Registering a state tree and the apply System (state machine declaration
// docs/architecture/target/v2/state-machine-declaration.zh-Hant.md, sections 5.1 and 5.2 rows 5 and 6; Design
// Invariants 2 to 5 in docs/architecture/target/v2/state-machine.zh-Hant.md).
//
// The product calls RegisterStateMachine<Root>(setup) in its own build. It registers the two Resources of the root
// and of each child, an enter and an exit schedule per value, a stage "<root>.Apply" of its own (by default right
// after the default skeleton's Main/PreUpdate: Bevy's place, insert_after(PreUpdate, StateTransition)) holding the
// apply System, and a prepare function that puts the first values. The kernel and the orchestration layer know
// nothing of it (DI 9).

#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/state_machine/Names.hpp"
#include "engine/state_machine/StateMachine.hpp"

#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace GYO::Engine::StateMachine {

struct StateMachineOptions {
    // Where the apply System's own stage goes: after (or before) the anchor stage of this schedule. The default
    // skeleton's names; an author who turns the skeleton off names its own (a missing one is the kernel's D-E1).
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string anchor = std::string(Orchestration::DefaultSchedules::preUpdate);
    Kernel::StagePlace place = Kernel::StagePlace::After;
    // Run conditions and sets of the apply System. They hold back every apply, the first one included (example:
    // AfterGenerating() to enter the initial value only once the World has generated; declaration section 6).
    std::vector<Kernel::ConditionRef> conditions;
    std::vector<std::string> sets;
};

namespace Detail {

template <class F, class... Children>
void ForEachChild(Kernel::TypeList<Children...>, F&& f) {
    (f.template operator()<Children>(), ...);
}

template <class S, class List>
struct ApplyWritesOf;
template <class S, class... Children>
struct ApplyWritesOf<S, Kernel::TypeList<Children...>> {
    using type = Kernel::Writes<StateMachine<S>, StateMachineRequest<S>, StateMachine<Children>...,
                                StateMachineRequest<Children>...>;
};
template <RootState S>
using ApplyWorld = Kernel::ExclusiveWorld<typename ApplyWritesOf<S, SubStatesOf<S>>::type>;

template <class S, class W>
StateMachine<S> ReadMachine(W& world) {
    auto read = world.template Read<StateMachine<S>>([](const StateMachine<S>& m) { return m; });
    GYO_ASSERT(read && read->has_value() && "a state machine's value is missing: RegisterStateMachine puts it");
    return **read;
}

template <class S, class W, class F>
void ModifyMachine(W& world, F&& f) {
    auto done = world.template Modify<StateMachine<S>>(std::forward<F>(f));
    GYO_ASSERT(done && *done);
}

// Read first, write only when there is a request (CCR-8): an apply runs every frame, and an empty take would mark
// the request Resource written each frame.
template <class S, class W>
std::optional<S> TakeRequest(W& world) {
    auto pending = world.template Read<StateMachineRequest<S>>([](const StateMachineRequest<S>& r) { return r.Pending(); });
    GYO_ASSERT(pending && pending->has_value());
    if (!pending->value()) return std::nullopt;
    auto taken = world.template Modify<StateMachineRequest<S>>(
        [](StateMachineRequest<S>& r) { return Access<S>::Take(r); });
    GYO_ASSERT(taken && taken->has_value());
    return **taken;
}

template <class S>
Base::Result<void, Kernel::KernelError> Valid(std::optional<S> value) {
    if (value && !NameOf(*value)) return Base::Err(ValueNotInTable(StateName<S>(), static_cast<long long>(*value)));
    return {};
}

// A child appears under its parent's new value: its initial value (given, for the first apply, by SetInitial),
// then its enter schedule.
template <class Child, class W>
Kernel::SystemResult Appear(W& world, std::optional<Child> given) {
    const Child value = given.value_or(StateMachineTraits<Child>::initial);
    if (auto valid = Valid<Child>(value); !valid) return valid;
    ModifyMachine<Child>(world, [value](StateMachine<Child>& m) { Access<Child>::Enter(m, value); });
    return world.RunSchedule(EnterScheduleName(value));
}

// The first apply (DI 4): the root enters its initial value, then each child that exists under it enters its
// own, in registration order. No request is taken.
template <RootState S>
Kernel::SystemResult FirstApply(ApplyWorld<S>& world, const StateMachine<S>& root) {
    const S value = Access<S>::Initial(root).value_or(StateMachineTraits<S>::initial);
    if (auto valid = Valid<S>(value); !valid) return valid;
    ModifyMachine<S>(world, [value](StateMachine<S>& m) {
        Access<S>::Enter(m, value);
        Access<S>::MarkApplied(m);
    });
    if (auto ran = world.RunSchedule(EnterScheduleName(value)); !ran) return ran;
    Kernel::SystemResult result;
    ForEachChild(SubStatesOf<S>{}, [&]<class Child>() {
        if (!result) return;
        const StateMachine<Child> child = ReadMachine<Child>(world);
        const std::optional<Child> given = Access<Child>::Initial(child);
        ModifyMachine<Child>(world, [](StateMachine<Child>& m) { Access<Child>::MarkApplied(m); });
        if (StateMachineTraits<Child>::parentValue == value) result = Appear<Child>(world, given);
    });
    return result;
}

// One apply after the first (section 5.1): take every request of the tree and check them; the root's change
// (children leave in registration order, the root leaves, changes, enters, children appear); then each child's
// own request. A request made during this apply waits for the next one (DI 3).
template <RootState S>
Kernel::SystemResult Apply(ApplyWorld<S> world) {
    const StateMachine<S> root = ReadMachine<S>(world);
    if (!Access<S>::Applied(root)) return FirstApply<S>(world, root);

    const std::optional<S> rootRequest = TakeRequest<S>(world);
    if (auto valid = Valid<S>(rootRequest); !valid) return valid;
    Kernel::SystemResult result;
    // The children's requests, taken and checked together with the root's, absent children's too.
    auto childRequests = [&]<class... Children>(Kernel::TypeList<Children...>) {
        return std::tuple<std::optional<Children>...>{TakeRequest<Children>(world)...};
    }(SubStatesOf<S>{});
    ForEachChild(SubStatesOf<S>{}, [&]<class Child>() {
        if (result) result = Valid<Child>(std::get<std::optional<Child>>(childRequests));
    });
    if (!result) return result;

    if (rootRequest) {
        const S from = *root.Current();
        const S to = *rootRequest;
        // Children leave first, in registration order, each marked absent as soon as it has left.
        ForEachChild(SubStatesOf<S>{}, [&]<class Child>() {
            if (!result) return;
            const StateMachine<Child> child = ReadMachine<Child>(world);
            if (child.Status() != StateStatus::Active) return;
            if (from == to && KeepsOnParentReenter<Child>()) return;
            result = world.RunSchedule(ExitScheduleName(*child.Current()));
            if (result) ModifyMachine<Child>(world, [](StateMachine<Child>& m) { Access<Child>::Leave(m); });
        });
        if (!result) return result;
        if (auto ran = world.RunSchedule(ExitScheduleName(from)); !ran) return ran;
        ModifyMachine<S>(world, [to](StateMachine<S>& m) { Access<S>::Enter(m, to); });
        if (auto ran = world.RunSchedule(EnterScheduleName(to)); !ran) return ran;
        ForEachChild(SubStatesOf<S>{}, [&]<class Child>() {
            if (!result) return;
            if (StateMachineTraits<Child>::parentValue != to) return;
            if (from == to && KeepsOnParentReenter<Child>() &&
                ReadMachine<Child>(world).Status() == StateStatus::Active) {
                return;
            }
            result = Appear<Child>(world, std::nullopt);
        });
        if (!result) return result;
    }

    // Each child's own request, in registration order: applied when the child exists, dropped otherwise.
    ForEachChild(SubStatesOf<S>{}, [&]<class Child>() {
        if (!result) return;
        const std::optional<Child> request = std::get<std::optional<Child>>(childRequests);
        if (!request) return;
        const StateMachine<Child> child = ReadMachine<Child>(world);
        if (child.Status() != StateStatus::Active) return;
        result = world.RunSchedule(ExitScheduleName(*child.Current()));
        if (!result) return;
        const Child to = *request;
        ModifyMachine<Child>(world, [to](StateMachine<Child>& m) { Access<Child>::Enter(m, to); });
        result = world.RunSchedule(EnterScheduleName(to));
    });
    return result;
}

} // namespace Detail

// Registers the state tree of S in the World being built (declaration 5.2 row 5). Call it once per World in the
// product's own build; twice is the kernel's R-E2 (DI 7).
template <RootState S>
void RegisterStateMachine(Orchestration::WorldSetup& setup, StateMachineOptions options = {}) {
    Kernel::Registrar& r = setup.Registrar();
    auto registerState = [&]<class T>() {
        r.Resource<StateMachine<T>>({.name = ResourceName<T>()});
        r.Resource<StateMachineRequest<T>>({.name = RequestName<T>()});
        for (const auto& entry : StateMachineTraits<T>::values) {
            r.Schedule({.name = EnterScheduleName(entry.value), .stages = {std::string(kEnterExitStage)}});
            r.Schedule({.name = ExitScheduleName(entry.value), .stages = {std::string(kEnterExitStage)}});
        }
    };
    registerState.template operator()<S>();
    Detail::ForEachChild(SubStatesOf<S>{}, registerState);

    r.InsertStage(options.schedule, options.anchor, options.place, ApplyStageName<S>());
    Kernel::SystemOptions apply;
    apply.name = ApplySystemName<S>();
    Kernel::Mount mount;
    mount.schedule = options.schedule;
    mount.stage = ApplyStageName<S>();
    mount.sets = std::move(options.sets);
    mount.conditions = std::move(options.conditions);
    apply.mounts.push_back(std::move(mount));
    r.AddSystem<&Detail::Apply<S>>(std::move(apply));

    // The first values, before the first Run, without overwriting a value already there (O-7).
    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        if (world.Resource<StateMachine<S>>() == nullptr) world.SetResource(Detail::Access<S>::Root());
        if (world.Resource<StateMachineRequest<S>>() == nullptr) world.SetResource(StateMachineRequest<S>{});
        Detail::ForEachChild(SubStatesOf<S>{}, [&]<class Child>() {
            if (world.Resource<StateMachine<Child>>() == nullptr) world.SetResource(Detail::Access<Child>::Child());
            if (world.Resource<StateMachineRequest<Child>>() == nullptr) world.SetResource(StateMachineRequest<Child>{});
        });
        return {};
    });
}

} // namespace GYO::Engine::StateMachine
