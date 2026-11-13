#pragma once

#include "engine/kernel/Access.hpp"
#include "engine/kernel/TypeList.hpp"

// The System parameter types (spec B.4 point 4) and their ParamTraits.
//
// A System's parameter list is its only access declaration (DI-KS 1): the
// access set is the union of the parameters' Contributions. Only types with a
// ParamTraits specialization are parameters; World&, T&, raw pointers and int
// are not (spec S-2).
//
// Handles are small values that a System receives by value. Each one is
// neither copyable nor movable and has a private constructor (spec B.4 point
// 8): a System cannot keep one past its call or make one it did not declare.
// This batch defines the handles and their Contributions only; Fetch and the
// handles' accessors arrive with the batches that run Systems (world-exec,
// loading, rollback-extract).

namespace GYO::Kernel {

// ---- Query terms (spec B.4 point 4): Query<Q..., F...>

template <class T> struct Read;
template <class T> struct Write;
template <class T> struct With;
template <class T> struct Without;
template <class T> struct Changed;
template <class T> struct Added;

template <class Term>
struct QueryTerm {
    static constexpr bool valid = false;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<>;
};
template <class T>
struct QueryTerm<Read<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::ComponentRead<T>>;
};
template <class T>
struct QueryTerm<Write<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = false;
    using Contribution = TypeList<Access::ComponentWrite<T>>;
};
template <class T>
struct QueryTerm<With<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::Membership<T>>;
};
template <class T>
struct QueryTerm<Without<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::Membership<T>>;
};
template <class T>
struct QueryTerm<Changed<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::ChangeFilter<T>>;
};
template <class T>
struct QueryTerm<Added<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::ChangeFilter<T>>;
};

// ---- Bundles and typed command lists

template <class... Components> struct Bundle;
template <class... Components> struct Inserts;
template <class... Components> struct Removes;

// Not a parameter unless specialized below.
template <class P>
struct ParamTraits {
    static constexpr bool valid = false;
};

namespace Detail {

// Base of every handle: no copy, no move, constructed only by its ParamTraits.
class Handle {
protected:
    Handle() = default;
    ~Handle() = default;

public:
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

template <class P, class Entries, bool ReadOnly>
struct HandleTraits {
    static constexpr bool valid = true;
    static constexpr bool readOnly = ReadOnly;
    // ExclusiveWorld must be the only parameter (spec S-10).
    static constexpr bool sole = false;
    // Extract parameters are legal only in an extract schedule (spec H.6).
    static constexpr bool extractOnly = false;
    using Contribution = Entries;
};

} // namespace Detail

#define GYO_KERNEL_HANDLE(Name)            \
    template <class>                       \
    friend struct ::GYO::Kernel::ParamTraits; \
    Name() = default

template <class... Terms>
class Query final : Detail::Handle {
    static_assert(sizeof...(Terms) > 0, "a Query names at least one term");
    static_assert((QueryTerm<Terms>::valid && ...),
                  "Query terms are Read<T>, Write<T>, With<T>, Without<T>, Changed<T> and Added<T>");
    GYO_KERNEL_HANDLE(Query);
};
template <class... Terms>
struct ParamTraits<Query<Terms...>>
    : Detail::HandleTraits<Query<Terms...>, Concat<typename QueryTerm<Terms>::Contribution...>,
                           (QueryTerm<Terms>::readOnly && ...)> {};

template <class T>
class Res final : Detail::Handle {
    GYO_KERNEL_HANDLE(Res);
};
template <class T>
struct ParamTraits<Res<T>> : Detail::HandleTraits<Res<T>, TypeList<Access::ResourceRead<T>>, true> {};

template <class T>
class ResMut final : Detail::Handle {
    GYO_KERNEL_HANDLE(ResMut);
};
template <class T>
struct ParamTraits<ResMut<T>> : Detail::HandleTraits<ResMut<T>, TypeList<Access::ResourceWrite<T>>, false> {};

template <class E>
class EventReader final : Detail::Handle {
    GYO_KERNEL_HANDLE(EventReader);
};
template <class E>
struct ParamTraits<EventReader<E>> : Detail::HandleTraits<EventReader<E>, TypeList<Access::EventRead<E>>, true> {};

template <class E>
class EventWriter final : Detail::Handle {
    GYO_KERNEL_HANDLE(EventWriter);
};
template <class E>
struct ParamTraits<EventWriter<E>> : Detail::HandleTraits<EventWriter<E>, TypeList<Access::EventWrite<E>>, false> {};

// Untyped deferred insert / remove of zone B, Shared columns only (spec F.1);
// checked when recorded, at run time (F-10). No access, no conflict edge.
class Commands final : Detail::Handle {
    GYO_KERNEL_HANDLE(Commands);
};
template <>
struct ParamTraits<Commands> : Detail::HandleTraits<Commands, TypeList<Access::UntypedCommands>, false> {};

template <class InsertList, class RemoveList>
class CommandsFor;
template <class... I, class... R>
class CommandsFor<Inserts<I...>, Removes<R...>> final : Detail::Handle {
    GYO_KERNEL_HANDLE(CommandsFor);
};
template <class... I, class... R>
struct ParamTraits<CommandsFor<Inserts<I...>, Removes<R...>>>
    : Detail::HandleTraits<CommandsFor<Inserts<I...>, Removes<R...>>,
                           TypeList<Access::DeferredWrite<I>..., Access::DeferredWrite<R>...>, false> {};

// Spawns with an id from the World's allotter: writes the allotter and defers
// a write of every bundle component (spec A.1, B3).
template <class B>
class Spawn;
template <class... C>
class Spawn<Bundle<C...>> final : Detail::Handle {
    static_assert(sizeof...(C) > 0, "a Bundle names at least one component");
    GYO_KERNEL_HANDLE(Spawn);
};
template <class... C>
struct ParamTraits<Spawn<Bundle<C...>>>
    : Detail::HandleTraits<Spawn<Bundle<C...>>, TypeList<Access::Spawn, Access::DeferredWrite<C>...>, false> {};

// Spawns with a given id: the bundle's deferred writes, no allotter (spec A.1).
template <class B>
class SpawnWithId;
template <class... C>
class SpawnWithId<Bundle<C...>> final : Detail::Handle {
    static_assert(sizeof...(C) > 0, "a Bundle names at least one component");
    GYO_KERNEL_HANDLE(SpawnWithId);
};
template <class... C>
struct ParamTraits<SpawnWithId<Bundle<C...>>>
    : Detail::HandleTraits<SpawnWithId<Bundle<C...>>, TypeList<Access::DeferredWrite<C>...>, false> {};

// Not a writer, no conflict edge (spec B.3, B.9 trade-off 1); Z5 at run time.
class Despawn final : Detail::Handle {
    GYO_KERNEL_HANDLE(Despawn);
};
template <>
struct ParamTraits<Despawn> : Detail::HandleTraits<Despawn, TypeList<Access::Despawn>, false> {};

// Per-mount state outside the World (spec B.1 runtime, B-12). Forbidden in
// zone C (S-7).
template <class T>
class Local final : Detail::Handle {
    GYO_KERNEL_HANDLE(Local);
};
template <class T>
struct ParamTraits<Local<T>> : Detail::HandleTraits<Local<T>, TypeList<Access::LocalState<T>>, false> {};

// The domain and tick of the running mount's schedule (spec B.4, B-1).
class CurrentTick final : Detail::Handle {
    GYO_KERNEL_HANDLE(CurrentTick);
};
template <>
struct ParamTraits<CurrentTick> : Detail::HandleTraits<CurrentTick, TypeList<Access::DomainTickRead>, true> {};

// Write-only diagnostics sink (spec S-8's only effect exception): effect
// Diagnostics, no access entry, no conflict edge.
template <class Record>
class DiagnosticsWriter final : Detail::Handle {
    GYO_KERNEL_HANDLE(DiagnosticsWriter);
};
template <class Record>
struct ParamTraits<DiagnosticsWriter<Record>>
    : Detail::HandleTraits<DiagnosticsWriter<Record>, TypeList<Access::Diagnostics<Record>>, true> {};

// Reads every column of a named component set (spec B.4 point 4, S13).
template <class SetTag>
class SetRead final : Detail::Handle {
    GYO_KERNEL_HANDLE(SetRead);
};
template <class SetTag>
struct ParamTraits<SetRead<SetTag>> : Detail::HandleTraits<SetRead<SetTag>, TypeList<Access::SetReadOf<SetTag>>, true> {};

// Untyped access to the whole World; zone B only (S-13); the only parameter
// (S-10). Conflicts with every System of its stage.
class ExclusiveWorld final : Detail::Handle {
    GYO_KERNEL_HANDLE(ExclusiveWorld);
};
template <>
struct ParamTraits<ExclusiveWorld> : Detail::HandleTraits<ExclusiveWorld, TypeList<Access::WholeWorld>, false> {
    static constexpr bool sole = true;
};

// Read-only access to the source World of an extract schedule (spec H.6).
// Extract<Write<T>> has no ParamTraits: it does not compile (DI-KS 13).
template <class Term>
class Extract;
template <class T>
class Extract<Read<T>> final : Detail::Handle {
    GYO_KERNEL_HANDLE(Extract);
};
template <class T>
struct ParamTraits<Extract<Read<T>>>
    : Detail::HandleTraits<Extract<Read<T>>, TypeList<Access::ExtractComponentRead<T>>, true> {
    static constexpr bool extractOnly = true;
};

template <class R>
class ExtractRes final : Detail::Handle {
    GYO_KERNEL_HANDLE(ExtractRes);
};
template <class R>
struct ParamTraits<ExtractRes<R>> : Detail::HandleTraits<ExtractRes<R>, TypeList<Access::ExtractResourceRead<R>>, true> {
    static constexpr bool extractOnly = true;
};

#undef GYO_KERNEL_HANDLE

} // namespace GYO::Kernel
