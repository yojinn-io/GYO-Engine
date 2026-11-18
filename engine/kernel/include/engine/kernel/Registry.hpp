#pragma once

#include "engine/base/Result.hpp"
#include "engine/kernel/Declarations.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Plan.hpp"
#include "engine/kernel/SystemTraits.hpp"
#include "engine/kernel/TypeKey.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Registry (spec C; target/kernel DI-KS 3, 4, 5, 6, 8, 15, 25, 26).
//
// Registration has two steps that may repeat (declaration
// v8-arch-gyo2-kernel-subtract-structure 3.2): Register opens a Registrar for
// one owner, hands it to a build callable and closes it when the callable
// returns; BuildPlans resolves what was registered, runs every startup check,
// builds one Plan per schedule (Scheduler.cpp) and the fingerprints, and raises
// the version. Every startup error is collected and returned at once (spec
// D.2, D-5). The first round writes the tables directly (version 0); after the
// first successful BuildPlans, a round writes a
// pending copy whose contents replace the tables, in the same object, only when
// its BuildPlans succeeds; a failed or discarded round leaves the Registry and
// its Worlds on the previous version. A World attaches to a new version the
// next time it is used (World.hpp).
//
// Registration units (Module, Plugin), their lifecycle, the runner and the
// game initialization belong to the orchestration layer (engine/orchestration;
// v8-arch-gyo2-orchestration-skeleton): the kernel only keeps who registered
// (RegistrationOwner), never what kind of unit it was (DI-KS 15).
//
// The WorldExecutor type (GYO::Kernel::WorldExecutor: one World, its Registry, its
// schedules and their executor policy; kernel HANDOFF v8-arch-kernel-app-name,
// v8-arch-gyo2-kernel-world-exec-type) runs a Registry's schedules
// (WorldExecutor.hpp).

namespace GYO::Kernel {

class FrozenRegistry;
class Registrar;
class Registry;

// A run condition (spec E.4): a function or captureless lambda returning bool
// with read-only parameters (S-5). Its reads join the System's reads (S-4).
// Made only by Condition<>: its access comes from the condition's signature,
// never written by hand (DI-KS 1: no second, hand-written access declaration).
class ConditionRef {
public:
    [[nodiscard]] const std::string& Name() const noexcept { return name_; }
    [[nodiscard]] Detail::ResolveFn Resolver() const noexcept { return resolve_; }
    [[nodiscard]] Detail::ConditionFn Evaluator() const noexcept { return evaluate_; }

private:
    template <auto Fn>
    friend ConditionRef Condition(std::string name);
    template <class F>
    friend ConditionRef Condition(std::string name);
    ConditionRef(std::string name, Detail::ResolveFn resolve, Detail::ConditionFn evaluate)
        : name_(std::move(name)), resolve_(resolve), evaluate_(evaluate) {}

    std::string name_;
    Detail::ResolveFn resolve_ = nullptr;
    Detail::ConditionFn evaluate_ = nullptr;
};

template <auto Fn>
[[nodiscard]] ConditionRef Condition(std::string name) {
    Detail::RequireAccepted<ConditionRejectionOfFunction<Fn>()>();
    if constexpr (ConditionRejectionOfFunction<Fn>() == Rejection::None) {
        using Pointer = typename Detail::Signature<decltype(Fn)>::Pointer;
        return ConditionRef{std::move(name), Detail::ResolverOf<Pointer>::value,
                            &Detail::InvokerOf<Pointer>::template CallFunction<Fn>};
    } else {
        return ConditionRef{std::move(name), nullptr, nullptr};
    }
}

template <class F>
[[nodiscard]] ConditionRef Condition(std::string name) {
    Detail::RequireAccepted<ConditionRejectionOf<F>()>();
    if constexpr (ConditionRejectionOf<F>() == Rejection::None) {
        using Pointer = typename Detail::Signature<decltype(&F::operator())>::Pointer;
        return ConditionRef{std::move(name), Detail::ResolverOf<Pointer>::value,
                            &Detail::InvokerOf<Pointer>::template CallLambda<F>};
    } else {
        return ConditionRef{std::move(name), nullptr, nullptr};
    }
}

// One place a System runs (spec B.1 mounts): a schedule and stage, with its
// own order targets, sets and run conditions. One System may have several
// mounts, at most one per schedule (D-E19).
struct Mount {
    std::string schedule;
    std::string stage;
    // Names of Systems, sets or sync points of the same schedule.
    std::vector<std::string> before;
    std::vector<std::string> after;
    std::vector<std::string> sets;
    std::vector<ConditionRef> conditions;
    // The kernel's NotInRollback condition (spec H-10, D-E15): skipped during
    // Replay. Its run-time half arrives with rollback-extract.
    bool notInRollback = false;
};

struct SystemOptions {
    std::string name;
    Zone zone = Zone::Unset;
    // Zone B only: may write role=Input (c) data, in the ingress stage (Z3).
    bool ingress = false;
    // Effect names registered with Registrar::Effect. "Diagnostics" cannot be
    // written here; it comes only from DiagnosticsWriter<T> (spec S-8).
    std::vector<std::string> effects;
    bool rollbackSafe = false;
    std::vector<Mount> mounts;
};

enum class HookTrigger : std::uint8_t {
    OnAdd,
    OnRemove,
};

struct HookOptions {
    std::string name;
    Zone zone = Zone::Unset;
};

enum class StagePlace : std::uint8_t {
    Before,
    After,
};

namespace Detail {
struct RegistryData;
}

// Who registers (the orchestration layer gives it; declaration 3.2): the kernel
// only stores it, compares two for equality and prints the name. The first
// Register of each owner fixes its place in the owner list of the Registry
// fingerprint.
struct RegistrationOwner {
    std::string name;
    std::uint32_t seq = 0;

    friend bool operator==(const RegistrationOwner&, const RegistrationOwner&) = default;
};

// Handed to the build callable of Registry::Register only.
// Not copyable or movable, and every registration asserts that the Registry is
// in a Register call, so a Registrar kept past it (a copy is impossible, a
// pointer is caught) cannot register (GYO_ASSERT: a Programmer Error).
class Registrar {
public:
    Registrar(const Registrar&) = delete;
    Registrar& operator=(const Registrar&) = delete;

    template <class T>
    ComponentId Component(ComponentOptions options) {
        static_assert(std::is_object_v<T> && !std::is_const_v<T>, "a component is a non-const object type");
        // Spec W-8, plus move assignment: a column is a std::vector of cells kept
        // in key order by engine/entity_store (target/world section 6, layout 2).
        static_assert(Detail::kStorable<T>,
                      "a component is nothrow move constructible, move assignable and destructible (spec W-8)");
        return RegisterComponent(KeyOf<T>(), Shape::Of<T>(), std::move(options), &Detail::MakeColumn<T>);
    }

    template <class T>
    ResourceId Resource(ResourceOptions options) {
        static_assert(std::is_object_v<T> && !std::is_const_v<T>, "a resource is a non-const object type");
        return RegisterResource(KeyOf<T>(), Shape::Of<T>(), std::move(options), &Detail::MakeResource<T>);
    }

    template <class E>
    EventTypeId Event(EventOptions options) {
        static_assert(std::is_object_v<E> && !std::is_const_v<E>, "an event is a non-const object type");
        return RegisterEvent(KeyOf<E>(), Shape::Of<E>(), std::move(options), &Detail::MakeEvents<E>);
    }

    // An effect id (spec C.1 effects): an opaque side-effect class such as
    // "Io:file". Two Systems declaring the same effect conflict (spec D.1).
    EffectId Effect(std::string name);

    // A named component set (spec C.1 namedSets, S13), owned by this owner.
    // Other owners add members with AddToSet; SetRead<SetTag> reads them all.
    template <class SetTag>
    SetId NamedSet(std::string name) {
        return RegisterNamedSet(KeyOf<SetTag>(), std::move(name));
    }

    // Removes the System of that name from the next BuildPlans on (declaration
    // 3.2 rule 7): it leaves a tombstone (its registrationSeq is not reused;
    // SystemInfo::removed) and its name may be registered again. False, and
    // nothing recorded, when no System of that name is in the table.
    bool RemoveSystem(std::string_view name);

    template <class SetTag, class T>
    void AddToSet() {
        AddSetMember(KeyOf<SetTag>(), KeyOf<T>());
    }

    // Marks a named set as the authority digest set (spec S-12): every member
    // must be Exclusive (D-E2).
    template <class SetTag>
    void DigestSet() {
        MarkDigestSet(KeyOf<SetTag>());
    }

    // Declares resource T as the World's entity allotter (spec A.1): Spawn
    // writes it; without one, a Spawn parameter is D-E3.
    template <class T>
    void Allotter() {
        static_assert(EntityAllotter<T>, "an allotter has std::optional<EntityId> Allot() and bool Owns(EntityId) const");
        DeclareAllotter(KeyOf<T>(), Detail::AllotterOps{&Detail::AllotIn<T>, &Detail::OwnsIn<T>});
    }

    // Makes a zone left Unset a startup error (D-E7, S-6) for the whole
    // Registry instead of zone B. Called by the owner of the product's
    // (c) data, so a forgotten zone cannot pass as (b). Any owner may call it;
    // calling it twice is the same as once.
    void RequireExplicitZones();

    void Schedule(ScheduleOptions options);
    // Inserts a stage before or after an existing stage of a schedule (spec C.2 point 2).
    void InsertStage(std::string_view schedule, std::string_view anchor, StagePlace place, std::string stage);
    void Set(SetOptions options, std::vector<ConditionRef> conditions = {});

    template <auto Fn>
    RegistrationSeq AddSystem(SystemOptions options, std::source_location source = std::source_location::current()) {
        Detail::RequireAccepted<SystemRejectionOfFunction<Fn>()>();
        if constexpr (SystemRejectionOfFunction<Fn>() == Rejection::None) {
            using Pointer = typename Detail::Signature<decltype(Fn)>::Pointer;
            using Resolver = Detail::ResolverOf<Pointer>;
            return RegisterSystem(std::move(options), Resolver::value, Resolver::extractOnly,
                                  &Detail::InvokerOf<Pointer>::template CallFunction<Fn>, source);
        } else {
            return 0;
        }
    }

    template <class F>
    RegistrationSeq AddSystem(SystemOptions options, std::source_location source = std::source_location::current()) {
        Detail::RequireAccepted<SystemRejectionOf<F>()>();
        if constexpr (SystemRejectionOf<F>() == Rejection::None) {
            using Pointer = typename Detail::Signature<decltype(&F::operator())>::Pointer;
            using Resolver = Detail::ResolverOf<Pointer>;
            return RegisterSystem(std::move(options), Resolver::value, Resolver::extractOnly,
                                  &Detail::InvokerOf<Pointer>::template CallLambda<F>, source);
        } else {
            return 0;
        }
    }

    // Hooks (spec F.4): OnAdd / OnRemove of component T, run at sync points in
    // registration order. Their writes count as writers (DI-KS 6, 11).
    template <class T, auto Fn>
    void Hook(HookTrigger trigger, HookOptions options, std::source_location source = std::source_location::current()) {
        Detail::RequireAccepted<SystemRejectionOfFunction<Fn>()>();
        if constexpr (SystemRejectionOfFunction<Fn>() == Rejection::None) {
            RegisterHook(KeyOf<T>(), trigger, std::move(options), Detail::ResolverOf<typename Detail::Signature<decltype(Fn)>::Pointer>::value,
                         &Detail::InvokerOf<typename Detail::Signature<decltype(Fn)>::Pointer>::template CallFunction<Fn>, source);
        }
    }

    // The initial value of component Target, declared by the owner that registered
    // Target: when a cell of Request appears on an entity at a sync point (a
    // spawn bundle, or an insert onto an entity that had none) and the entity
    // has no Target, the kernel puts Fn(request) there at that same sync point,
    // before the OnAdd hooks. Fn is a function `Target Fn(const Request&)`.
    //
    // This is how a component with one writing System gets its first value
    // from other Systems: they spawn or insert Request (usually Shared), and
    // only the writer of Target appears in Target's writer count. A Target the
    // entity already has is left as it is; overwriting an existing Request cell
    // does nothing; the Request cell stays on the entity. A Target that is
    // itself the Request of another declaration is filled at the same sync
    // point too. Several Requests may name one Target: on an entity that gets
    // more than one of them in one round, the Request with the lower
    // ComponentId gives the value; a Request that appears through a chain is
    // looked at in the next round, after the direct ones.
    //
    // Zone rules: a System or hook that inserts or spawns Request writes Target,
    // through chains of declarations, for Z1, Z2, Z3 and Z6 (not for the writer
    // count), so the funnel allows what a direct write allows. Writing Request
    // in place or removing it fills nothing and does not count. A zone C Target takes its
    // value from a zone C Request only (Z1).
    template <class Target, auto Fn>
    void InitialValue(std::source_location source = std::source_location::current()) {
        using Signature = Detail::InitialValueRequest<Target, decltype(Fn)>;
        static_assert(Signature::valid, "InitialValue<Target, Fn>: Fn is a function `Target Fn(const Request&)`");
        if constexpr (Signature::valid) {
            using Request = typename Signature::Type;
            static_assert(!std::is_same_v<Target, Request>, "a component is not its own initial value request");
            RegisterInitialValue(KeyOf<Target>(), KeyOf<Request>(), &Detail::PutInitialValue<Target, Request, Fn>, source);
        }
    }

private:
    friend class Registry;
    friend struct TypeKeyCollisionProbe;

    struct Shape {
        std::size_t size;
        std::size_t align;
        bool trivialRelocate;

        template <class T>
        static Shape Of() noexcept {
            return Shape{sizeof(T), alignof(T), std::is_trivially_copyable_v<T>};
        }
    };

    Registrar(Detail::RegistryData& data, std::uint32_t owner) : data_(&data), owner_(owner) {}

    ComponentId RegisterComponent(TypeKey key, Shape shape, ComponentOptions options, Detail::MakeColumnFn make);
    ResourceId RegisterResource(TypeKey key, Shape shape, ResourceOptions options, Detail::MakeResourceFn make);
    EventTypeId RegisterEvent(TypeKey key, Shape shape, EventOptions options, Detail::MakeEventsFn make);
    SetId RegisterNamedSet(TypeKey key, std::string name);
    void AddSetMember(TypeKey set, TypeKey component);
    void MarkDigestSet(TypeKey set);
    void DeclareAllotter(TypeKey resource, Detail::AllotterOps ops);
    RegistrationSeq RegisterSystem(SystemOptions options, Detail::ResolveFn resolve, bool extractOnly, Detail::RunFn run,
                                   std::source_location source);
    void RegisterHook(TypeKey component, HookTrigger trigger, HookOptions options, Detail::ResolveFn resolve,
                      Detail::RunFn run, std::source_location source);
    void RegisterInitialValue(TypeKey target, TypeKey request, Detail::InitialValueFn put, std::source_location source);

    Detail::RegistryData* data_;
    // Index in the Registry's owner list.
    std::uint32_t owner_;
};

// Test seam for R-E5 (spec C.4: TypeKey address collision). A linker that
// folds two types' anchors gives them one key; a test cannot make a linker do
// that, so it registers a second shape under an existing key through this.
struct TypeKeyCollisionProbe {
    static ComponentId RegisterComponent(Registrar& registrar, TypeKey key, std::size_t size, std::size_t align,
                                         ComponentOptions options);
};

// Test seam for spec J-20 ("in a test build, shuffling the internal containers
// gives the same Plan"): a non-zero seed makes the Scheduler scan Systems in
// reverse and rotate every successor list before it orders anything. The Plan
// must not change; a Plan that changes depends on container order.
struct SchedulerShuffleProbe {
    static void Set(Registry& registry, std::uint32_t seed);
};

// The kernel's own resource (spec section "reading" / A.6; DI-KS 24): the
// access verification switch, breakOnKernelError and the product's default hook
// round limit (spec F-12). Development and tuning settings only: the Host, a
// System (Res / ResMut), a hook, a run condition and ExclusiveWorld may read
// and write it, at any time. The kernel reads it once, when each
// WorldExecutor::Run call starts, so a change counts from the next Run (a nested
// RunSchedule does not read it again). Without it the defaults below are used.
// One per World. Registered by the Registry before any owner (ResourceId 0).
// hookRoundLimit is in no fingerprint; 0 is a GYO_ASSERT when a Run starts.
struct KernelSettings {
    bool accessVerification = false;
    bool breakOnKernelError = false;
    std::uint32_t hookRoundLimit = 8;
};

enum class AccessKind : std::uint8_t {
    Component,
    Resource,
    Event,
    // id = the Domain of a mount's schedule (CurrentTick).
    DomainTick,
};

struct AccessKey {
    AccessKind kind;
    std::uint32_t id;

    friend constexpr bool operator==(const AccessKey&, const AccessKey&) = default;
    friend constexpr bool operator<(const AccessKey& a, const AccessKey& b) {
        return a.kind != b.kind ? a.kind < b.kind : a.id < b.id;
    }
};

// The derived half of a SystemDescriptor (spec B.1), in ids, after Finish.
// Vectors are sorted and unique.
struct AccessSet {
    // Immediate reads and writes: components, resources (the allotter included
    // for Spawn), events, domain ticks. Run-condition reads and SetRead
    // expansions are in reads.
    std::vector<AccessKey> reads;
    std::vector<AccessKey> writes;
    // With / Without / Changed / Added: membership reads, no conflict (spec D.1).
    std::vector<ComponentId> membershipReads;
    // CommandsFor inserts and removes, Spawn / SpawnWithId bundles.
    std::vector<ComponentId> deferredWrites;
    // The part of deferredWrites that can add a cell: CommandsFor inserts and
    // bundle components, not removes. Only these make a sync point fill a
    // component (Registrar::InitialValue); not hashed in the fingerprints.
    std::vector<ComponentId> deferredInserts;
    std::vector<SetId> setReads;
    // Component and resource ids of the extract source Registry.
    std::vector<AccessKey> extractReads;
    // The resources an ExclusiveWorld<Writes<R...>> lists (DI-KS 1): an
    // annotation for the authority policy to read; not a write, not a writer,
    // no conflict. In the Registry fingerprint.
    std::vector<ResourceId> annotatedWrites;
    bool changeFilters = false;
    bool local = false;
    bool exclusive = false;
    bool diagnostics = false;
    bool despawn = false;
    bool spawn = false;
    bool untypedCommands = false;
};

struct ColumnInfo {
    std::string name;
    std::size_t size;
    std::size_t align;
    bool trivialRelocate;
    WritePolicy writePolicy;
    Zone zone;
    Role role;
    Rollback rollback;
    std::string owner;
};

struct ResourceInfo {
    std::string name;
    std::size_t size;
    std::size_t align;
    WritePolicy writePolicy;
    Zone zone;
    Role role;
    Rollback rollback;
    bool external;
    bool threadBound;
    std::string owner;
};

struct EventInfo {
    std::string name;
    Domain domain;
    Zone zone;
    std::string owner;
};

struct SystemInfo {
    std::string name;
    RegistrationSeq registrationSeq;
    Zone zone;
    bool ingress;
    std::vector<Mount> mounts;
    AccessSet access;
    // "file:line" of the registration (spec B.1 source).
    std::string source;
    std::string owner;
    // The effects it declares, as ids (Registrar::Effect), ascending.
    std::vector<EffectId> effects;
    // A tombstone (Registrar::RemoveSystem): it does not run; its
    // registrationSeq is not given to another System.
    bool removed = false;
};

// One hook (Registrar::Hook; declaration 3.3).
struct HookInfo {
    std::string name;
    HookTrigger trigger;
    ComponentId component;
    Zone zone;
    AccessSet access;
    std::string source;
    std::string owner;
};

// One initial value declaration (Registrar::InitialValue), in the order the
// sync point applies them: (request, target) ascending.
struct InitialValueInfo {
    ComponentId target;
    ComponentId request;
    std::string owner;
    std::string source;
};

// One schedule: its options, stages in their final order (InsertStage applied).
struct ScheduleInfo {
    ScheduleOptions options;
    std::string owner;
};

// One named System set (Registrar::Set): its options (before / after of the
// set included) and the names of its run conditions.
struct SetInfo {
    SetOptions options;
    std::vector<std::string> conditions;
    std::string owner;
};

// Where an edge of a schedule's resolved order comes from (FrozenRegistry::OrderGraph).
enum class OrderSource : std::uint8_t {
    // Stage order alone (through stage ends).
    Stage,
    // A before / after naming the System or one of its sets.
    Declared,
    // A path through sync points with a declared before / after on it (to or
    // from a named sync point).
    SyncPoint,
    // A conflicting pair with no order, ordered by registration (TieBreaks).
    TieBreak,
};

// One edge of a schedule's resolved order, System to System: sets and sync
// points are expanded, so first runs before second in the schedule's Plan.
struct OrderEdge {
    std::string first;
    std::string second;
    OrderSource source;

    friend bool operator==(const OrderEdge&, const OrderEdge&) = default;
};

// One conflicting pair with no order path that the Scheduler ordered by
// registration (spec D.2 step 10), zone C or not: listed at diagnostic level.
// There is no ambiguity error (v8-arch-gyo2-no-ambiguity-detection): a Serial
// schedule runs the two in some order anyway, and the kernel does not promise
// a result that is the same in every registration order (DI-KS 5).
struct TieBreak {
    std::string schedule;
    std::string first;
    std::string second;
};

// Spec G-5 export (docs/architecture/zone-c-sources.zh-Hant.md, gyo.zone-c-sources v1).
struct ZoneCSourcesOptions {
    // Absolute path of the repository root; registration sites are made
    // relative to it.
    std::string repositoryRoot;
    std::string ownerRoot;
    std::vector<std::string> includeRoots;
    std::string repoCommit;
};

// The read-only, versioned view of a Registry (DI-KS 25). The object itself
// stays valid as long as the Registry; the spans and pointers it hands out
// (Systems(), PlanOf, SystemNamed, ...) are valid for the current version only:
// a successful BuildPlans makes them stale. Changes go through Registrar only.
class FrozenRegistry {
public:
    // 0 before the first successful BuildPlans; 1 after it; +1 at every later
    // successful BuildPlans; a failed one keeps it.
    [[nodiscard]] std::uint64_t Version() const noexcept;

    template <class T>
    [[nodiscard]] std::optional<ComponentId> ComponentIdOf() const {
        return FindComponent(KeyOf<T>());
    }
    template <class T>
    [[nodiscard]] std::optional<ResourceId> ResourceIdOf() const {
        return FindResource(KeyOf<T>());
    }
    template <class E>
    [[nodiscard]] std::optional<EventTypeId> EventIdOf() const {
        return FindEvent(KeyOf<E>());
    }

    [[nodiscard]] std::span<const ColumnInfo> Components() const noexcept;
    [[nodiscard]] std::span<const ResourceInfo> Resources() const noexcept;
    [[nodiscard]] std::span<const EventInfo> Events() const noexcept;
    [[nodiscard]] std::span<const SystemInfo> Systems() const noexcept;
    [[nodiscard]] const SystemInfo* SystemNamed(std::string_view name) const noexcept;
    [[nodiscard]] std::span<const Plan> Plans() const noexcept;
    [[nodiscard]] const Plan* PlanOf(std::string_view schedule) const noexcept;
    [[nodiscard]] std::span<const TieBreak> TieBreaks() const noexcept;
    [[nodiscard]] std::span<const HookInfo> Hooks() const noexcept;
    [[nodiscard]] std::span<const InitialValueInfo> InitialValues() const noexcept;
    [[nodiscard]] std::span<const ScheduleInfo> Schedules() const noexcept;
    [[nodiscard]] std::span<const SetInfo> Sets() const noexcept;
    // The resolved order of one schedule, System to System, each edge with its
    // source, in (first, second) registrationSeq order; empty for an unknown
    // schedule. The order D-E16 reads; a rollback Module reads it instead of
    // resolving the order rules again.
    [[nodiscard]] std::span<const OrderEdge> OrderGraph(std::string_view schedule) const noexcept;
    // Members of a named set (spec S13), in registration order of the members.
    [[nodiscard]] std::vector<ComponentId> SetMembers(SetId set) const;

    // Fingerprints (spec C.2 point 5, H-4, J-18, J-20). Each carries its
    // format version; see Fingerprint.cpp for the exact bytes hashed.
    [[nodiscard]] Fingerprint RegistryFingerprint() const;
    // Only the zone C part (spec H.3: two Products share one FixedUpdate when their
    // cFingerprints match).
    [[nodiscard]] Fingerprint CFingerprint() const;

    // Text report of every Plan (Plan.hpp ExportPlans).
    [[nodiscard]] std::string ExportPlans() const;

    // Spec G-5: the source files of zone C Systems as gyo.zone-c-sources v1
    // JSON. Err when a registration site is outside repositoryRoot or
    // ownerRoot, so the list never names a file the scanner cannot read.
    [[nodiscard]] ::Engine::Base::Result<std::string, KernelError> ExportZoneCSources(
        const ZoneCSourcesOptions& options) const;

private:
    friend class Registry;
    explicit FrozenRegistry(const Detail::RegistryData& data) : data_(&data) {}

    [[nodiscard]] std::optional<ComponentId> FindComponent(TypeKey key) const noexcept;
    [[nodiscard]] std::optional<ResourceId> FindResource(TypeKey key) const noexcept;
    [[nodiscard]] std::optional<EventTypeId> FindEvent(TypeKey key) const noexcept;

    const Detail::RegistryData* data_;
};

class Registry {
public:
    // Registration goes through Register, then BuildPlans.
    Registry();
    ~Registry();
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    // The Registry an extract schedule reads from (spec H.6); before the first
    // BuildPlans only. The source must have built its Plans at least once when
    // this Registry builds (D-E14).
    void SetExtractSource(const Registry& source);

    // Step 1 (declaration 3.2): opens a Registrar for owner, calls
    // build(Registrar&) and closes the Registrar when build returns. Ok: what
    // build registered takes effect at the next successful BuildPlans. Err
    // DynamicInsertRefused (a Runtime Error; the Registry and every World are
    // unchanged): a live World made from this Registry declares no dynamic
    // insert. GYO_ASSERT: called inside build, or while a World made from
    // this Registry is inside a Run (from its Systems, hooks, initial value
    // functions or other Worlds they run), or after a failed first round.
    template <class Build>
    ::Engine::Base::Result<void, KernelError> Register(RegistrationOwner owner, Build&& build) {
        ::Engine::Base::Result<std::uint32_t, KernelError> opened = OpenRegistrar(std::move(owner));
        if (!opened) return ::Engine::Base::Err(std::move(opened).error());
        RegistrarScope scope(*this);
        Registrar registrar(Target(), *opened);
        std::forward<Build>(build)(registrar);
        return {};
    }

    // Step 2. Ok: the version +1, the Plans and fingerprints are built again;
    // the Worlds made from this Registry attach the next time they are used.
    // With nothing registered since the last BuildPlans: nothing happens, the
    // version stays. Err: every startup error at once; the round is dropped and
    // the Registry and its Worlds stay on the previous version (a failed first
    // round leaves the Registry unusable).
    ::Engine::Base::Result<void, StartupReport> BuildPlans();

    // Drops the round registered since the last BuildPlans (its ids are void).
    void DiscardRegistrations();

    // Whether a BuildPlans has succeeded.
    [[nodiscard]] bool IsFrozen() const noexcept;
    // The read-only view (DI-KS 25). GYO_ASSERT: only after a successful
    // BuildPlans.
    [[nodiscard]] const FrozenRegistry& Frozen() const;

private:
    friend struct SchedulerShuffleProbe;
    // The World is made from the tables (storage factories, hooks, the
    // allotter), keeps the two World counters and attaches to new versions.
    friend class World;

    // Closes the Registrar on every way out of build (an exception included:
    // what was registered stays in the round, for the caller to drop or keep).
    class RegistrarScope {
    public:
        explicit RegistrarScope(Registry& registry) noexcept : registry_(registry) {}
        ~RegistrarScope() { registry_.CloseRegistrar(); }
        RegistrarScope(const RegistrarScope&) = delete;
        RegistrarScope& operator=(const RegistrarScope&) = delete;

    private:
        Registry& registry_;
    };

    // Ok: the owner's index in the table this round writes (Target()).
    ::Engine::Base::Result<std::uint32_t, KernelError> OpenRegistrar(RegistrationOwner owner);
    void CloseRegistrar() noexcept;
    // The table the open round writes: the pending copy after the first
    // successful BuildPlans, the tables themselves before it.
    [[nodiscard]] Detail::RegistryData& Target() noexcept;
    // GYO_ASSERT: no World made from this Registry is inside a Run
    // (declaration 3.2 rule 4).
    void RequireNoWorldRunning() const;

    std::unique_ptr<Detail::RegistryData> data_;
    // The round registered after the first successful BuildPlans; null otherwise.
    std::unique_ptr<Detail::RegistryData> pending_;
    FrozenRegistry frozen_;
};

} // namespace GYO::Kernel
