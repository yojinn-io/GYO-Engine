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

// Registry (spec C; target/kernel DI-KS 3, 4, 5, 6, 8, 15).
//
// Lifecycle (spec C.2): the Host lists plugins in order; Finish calls every
// plugin's Build(Registrar&) in that order, checks requires, calls every
// plugin's Finish(const FrozenRegistry&, FinishContext&) in the same order,
// then freezes the Registry, builds one Plan per schedule (Scheduler.cpp) and
// computes the fingerprints. Every startup error is collected and returned at
// once (spec D.2, D-5). After Finish nothing can register: Registrar is only
// handed to Build (spec R-2).
//
// The App type (GYO::Kernel::App: one World, its Registry, its schedules and
// their executor policy; kernel HANDOFF v8-arch-kernel-app-name) owns a
// Registry; it arrives with world-exec.

namespace GYO::Kernel {

class FrozenRegistry;
class Registrar;
class Registry;

// The context of Plugin::Finish (spec C.2 point 4): a plugin may install the
// runner; there is no Registrar here, so registering is impossible by type.
class FinishContext {
public:
    // One runner per App; a second is R-E6. The runner callable arrives with
    // the batches that run schedules; this batch records who installs it.
    void InstallRunner(std::string_view runner);

private:
    friend class Registry;
    FinishContext(Registry& registry, std::uint32_t plugin) : registry_(&registry), plugin_(plugin) {}
    Registry* registry_;
    std::uint32_t plugin_;
};

// A plugin (spec C.5): called only at startup.
class Plugin {
public:
    virtual ~Plugin();
    // A string constant of the plugin, not a compiler type name (bevy_core:10).
    [[nodiscard]] virtual std::string_view Name() const = 0;
    // Plugins that must come earlier in the Host's list (R-E3). The kernel does
    // not reorder: list order is id order (spec C.2 point 3).
    [[nodiscard]] virtual std::vector<std::string_view> Requires() const;
    virtual void Build(Registrar& registrar) = 0;
    virtual void Finish(const FrozenRegistry& registry, FinishContext& context);

protected:
    Plugin() = default;
    Plugin(const Plugin&) = default;
    Plugin& operator=(const Plugin&) = default;
};

// A run condition (spec E.4): a function or captureless lambda returning bool
// with read-only parameters (S-5). Its reads join the System's reads (S-4).
struct ConditionRef {
    std::string name;
    Detail::ResolveFn resolve = nullptr;
};

template <auto Fn>
[[nodiscard]] ConditionRef Condition(std::string name) {
    Detail::RequireAccepted<ConditionRejectionOfFunction<Fn>()>();
    if constexpr (ConditionRejectionOfFunction<Fn>() == Rejection::None) {
        return ConditionRef{std::move(name), Detail::ResolverOf<typename Detail::Signature<decltype(Fn)>::Pointer>::value};
    } else {
        return ConditionRef{std::move(name), nullptr};
    }
}

template <class F>
[[nodiscard]] ConditionRef Condition(std::string name) {
    Detail::RequireAccepted<ConditionRejectionOf<F>()>();
    if constexpr (ConditionRejectionOf<F>() == Rejection::None) {
        return ConditionRef{std::move(name),
                            Detail::ResolverOf<typename Detail::Signature<decltype(&F::operator())>::Pointer>::value};
    } else {
        return ConditionRef{std::move(name), nullptr};
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
    Affinity affinity;
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

// Handed to Plugin::Build only (spec C.5).
class Registrar {
public:
    template <class T>
    ComponentId Component(ComponentOptions options) {
        static_assert(std::is_object_v<T> && !std::is_const_v<T>, "a component is a non-const object type");
        return RegisterComponent(KeyOf<T>(), Shape::Of<T>(), std::move(options));
    }

    template <class T>
    ResourceId Resource(ResourceOptions options) {
        static_assert(std::is_object_v<T> && !std::is_const_v<T>, "a resource is a non-const object type");
        return RegisterResource(KeyOf<T>(), Shape::Of<T>(), std::move(options));
    }

    template <class E>
    EventTypeId Event(EventOptions options) {
        static_assert(std::is_object_v<E> && !std::is_const_v<E>, "an event is a non-const object type");
        return RegisterEvent(KeyOf<E>(), Shape::Of<E>(), std::move(options));
    }

    // An effect id (spec C.1 effects): an opaque side-effect class such as
    // "Io:file". Two Systems declaring the same effect conflict (spec D.1).
    EffectId Effect(std::string name);

    // A named component set (spec C.1 namedSets, S13), owned by this plugin.
    // Other plugins add members with AddToSet; SetRead<SetTag> reads them all.
    template <class SetTag>
    SetId NamedSet(std::string name) {
        return RegisterNamedSet(KeyOf<SetTag>(), std::move(name));
    }

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
        DeclareAllotter(KeyOf<T>());
    }

    void Schedule(ScheduleOptions options);
    // Inserts a stage before or after an existing stage of a schedule (spec C.2 point 2).
    void InsertStage(std::string_view schedule, std::string_view anchor, StagePlace place, std::string stage);
    void Set(SetOptions options, std::vector<ConditionRef> conditions = {});

    template <auto Fn>
    RegistrationSeq AddSystem(SystemOptions options, std::source_location source = std::source_location::current()) {
        Detail::RequireAccepted<SystemRejectionOfFunction<Fn>()>();
        if constexpr (SystemRejectionOfFunction<Fn>() == Rejection::None) {
            using Resolver = Detail::ResolverOf<typename Detail::Signature<decltype(Fn)>::Pointer>;
            return RegisterSystem(std::move(options), Resolver::value, Resolver::extractOnly, source);
        } else {
            return 0;
        }
    }

    template <class F>
    RegistrationSeq AddSystem(SystemOptions options, std::source_location source = std::source_location::current()) {
        Detail::RequireAccepted<SystemRejectionOf<F>()>();
        if constexpr (SystemRejectionOf<F>() == Rejection::None) {
            using Resolver = Detail::ResolverOf<typename Detail::Signature<decltype(&F::operator())>::Pointer>;
            return RegisterSystem(std::move(options), Resolver::value, Resolver::extractOnly, source);
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
            RegisterHook(KeyOf<T>(), trigger, std::move(options), Detail::ResolverOf<typename Detail::Signature<decltype(Fn)>::Pointer>::value, source);
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

    Registrar(Detail::RegistryData& data, std::uint32_t plugin) : data_(&data), plugin_(plugin) {}

    ComponentId RegisterComponent(TypeKey key, Shape shape, ComponentOptions options);
    ResourceId RegisterResource(TypeKey key, Shape shape, ResourceOptions options);
    EventTypeId RegisterEvent(TypeKey key, Shape shape, EventOptions options);
    SetId RegisterNamedSet(TypeKey key, std::string name);
    void AddSetMember(TypeKey set, TypeKey component);
    void MarkDigestSet(TypeKey set);
    void DeclareAllotter(TypeKey resource);
    RegistrationSeq RegisterSystem(SystemOptions options, Detail::ResolveFn resolve, bool extractOnly,
                                   std::source_location source);
    void RegisterHook(TypeKey component, HookTrigger trigger, HookOptions options, Detail::ResolveFn resolve,
                      std::source_location source);

    Detail::RegistryData* data_;
    std::uint32_t plugin_;
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

// The kernel's own resource (spec section "reading" / A.6): the access
// verification switch and breakOnKernelError, written by the Host before the
// first Run, read only by the kernel. Registered by the Registry before any
// plugin (ResourceId 0); a System that declares Res or ResMut of it is D-E18.
struct KernelSettings {
    bool accessVerification = false;
    bool breakOnKernelError = false;
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
    std::vector<SetId> setReads;
    // Component and resource ids of the extract source Registry.
    std::vector<AccessKey> extractReads;
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
    std::string ownerPlugin;
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
    Affinity affinity;
    std::string ownerPlugin;
};

struct EventInfo {
    std::string name;
    Domain domain;
    Zone zone;
    std::string ownerPlugin;
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
    std::string ownerPlugin;
};

// One (b) pair the Scheduler ordered by registration (spec D.2 step 10): listed
// at diagnostic level.
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

class FrozenRegistry {
public:
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
    // Members of a named set (spec S13), in registration order of the members.
    [[nodiscard]] std::vector<ComponentId> SetMembers(SetId set) const;

    // Fingerprints (spec C.2 point 5, H-4, J-18, J-20). Each carries its
    // format version; see Fingerprint.cpp for the exact bytes hashed.
    [[nodiscard]] Fingerprint RegistryFingerprint() const;
    // Only the zone C part (spec H.3: two Apps share one FixedUpdate when their
    // cFingerprints match).
    [[nodiscard]] Fingerprint CFingerprint() const;

    // Text report of every Plan (Plan.hpp ExportPlans).
    [[nodiscard]] std::string ExportPlans() const;

    // Spec G-5: the source files of zone C Systems as gyo.zone-c-sources v1
    // JSON. Err when a registration site is outside repositoryRoot or
    // ownerRoot, so the list never names a file the scanner cannot read.
    [[nodiscard]] Engine::Base::Result<std::string, KernelError> ExportZoneCSources(
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
    // plugins: the Host's ordered list; the Registry does not own them.
    explicit Registry(std::vector<Plugin*> plugins);
    ~Registry();
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    // The Registry an extract schedule reads from (spec H.6). It must be
    // frozen when this Registry finishes (D-E14).
    void SetExtractSource(const Registry& source);

    // Runs the whole lifecycle once (spec C.2). Ok: the Registry is frozen and
    // every Plan is built. Err: every startup error at once; the Registry is
    // not usable.
    Engine::Base::Result<void, StartupReport> Finish();

    [[nodiscard]] bool IsFrozen() const noexcept;
    // GYO_ASSERT: only after a successful Finish.
    [[nodiscard]] const FrozenRegistry& Frozen() const;

private:
    friend class FinishContext;
    friend struct SchedulerShuffleProbe;
    std::unique_ptr<Detail::RegistryData> data_;
    FrozenRegistry frozen_;
};

} // namespace GYO::Kernel
