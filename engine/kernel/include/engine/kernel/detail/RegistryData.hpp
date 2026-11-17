#pragma once

// Internal to the kernel (namespace Detail; included only by engine/kernel/src, kept under include/ so the include
// path stays engine/kernel/..., spec J-9 K5): the Registry's tables. Shared by Registry.cpp
// (registration, resolution, startup checks), Scheduler.cpp (Plans) and
// Fingerprint.cpp (fingerprints and exports).

#include "engine/kernel/Registry.hpp"

#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <vector>

namespace GYO::Kernel::Detail {

// The kernel's own registrations (KernelSettings, effect Diagnostics) belong
// to no registration owner.
inline constexpr std::uint32_t kKernelOwner = 0xFFFFFFFFU;
inline constexpr const char* kKernelOwnerName = "kernel";
inline constexpr const char* kDiagnosticsEffect = "Diagnostics";

enum class TypeRole : std::uint8_t {
    Component,
    Resource,
    Event,
    SetTag,
};

struct TypeEntry {
    TypeKey key;
    std::size_t size;
    std::size_t align;
    TypeRole role;
    std::uint32_t id;
    std::uint32_t owner;
};

struct PluginRecord {
    Plugin* plugin;
    std::string name;
    std::vector<std::string> required;
    // False for a plugin whose name repeats an earlier one (R-E1): it is not built.
    bool built = false;
};

struct EffectRecord {
    std::string name;
    std::uint32_t owner;
};

struct NamedSetRecord {
    std::string name;
    TypeKey key;
    std::uint32_t owner;
    std::vector<TypeKey> pendingMembers;
    std::vector<std::uint32_t> memberOwners;
    std::vector<ComponentId> members;
    bool digest = false;
};

struct ScheduleRecord {
    ScheduleOptions options;
    std::uint32_t owner;
};

struct SetRecord {
    SetOptions options;
    std::vector<ConditionRef> conditions;
    std::uint32_t owner;
    // Filled at finish: reads of the set's conditions.
    std::vector<AccessKey> conditionReads;
    std::vector<ComponentId> conditionMembership;
};

struct SystemRecord {
    SystemOptions options;
    ResolveFn resolve;
    bool extractOnly;
    RunFn run;
    std::source_location source;
    std::uint32_t owner;
    RegistrationSeq seq;
    // Filled at every BuildPlans (cleared first).
    AccessSet access;
    std::vector<EffectId> effects;
    bool domainTick = false;
    // Registrar::RemoveSystem: a tombstone. seq stays its index in systems (the
    // Scheduler, the Plans and the Executor index by it); never resolved,
    // checked, planned or hashed.
    bool removed = false;
};

struct HookRecord {
    std::string name;
    Zone zone;
    TypeKey component;
    HookTrigger trigger;
    ResolveFn resolve;
    RunFn run;
    std::source_location source;
    std::uint32_t owner;
    // Filled at finish.
    std::optional<ComponentId> componentId;
    AccessSet access;
};

struct InitialValueRecord {
    TypeKey target;
    TypeKey request;
    InitialValueFn put;
    std::source_location source;
    std::uint32_t owner;
    // Filled at finish.
    ComponentId targetId = 0;
    ComponentId requestId = 0;
};

// The size of each registration input when the pending copy of a later round
// was made: what is at or past these counts was registered in this round
// (declaration 3.2 rule 6, R-E7).
struct RoundBase {
    std::size_t components = 0;
    std::size_t namedSets = 0;
    std::vector<std::size_t> setMembers;
    std::size_t schedules = 0;
    std::size_t systems = 0;
    std::size_t hooks = 0;
    std::size_t initialValues = 0;
    std::size_t allotterDeclarations = 0;
    std::uint32_t digestMarks = 0;
    std::uint32_t explicitZoneCalls = 0;
};

// One schedule's resolved order (FrozenRegistry::OrderGraph), built with its Plan.
struct ScheduleOrder {
    std::string schedule;
    std::vector<OrderEdge> edges;
};

struct RegistryData {
    std::vector<PluginRecord> plugins;
    // Registration owners in the order of their first Register (the owner
    // index of every record is a position here).
    std::vector<RegistrationOwner> owners;
    std::vector<TypeEntry> types;
    std::vector<ColumnInfo> components;
    std::vector<ResourceInfo> resources;
    std::vector<EventInfo> events;
    std::vector<EffectRecord> effects;
    std::vector<NamedSetRecord> namedSets;
    std::vector<ScheduleRecord> schedules;
    std::vector<SetRecord> sets;
    std::vector<SystemRecord> systems;
    std::vector<HookRecord> hooks;
    // After finish: (requestId, targetId) ascending, the order the World applies them in.
    std::vector<InitialValueRecord> initialValues;
    // The World's storage factories, one per component, resource and event id
    // (Detail/Storage.hpp); the World is made from them (World.cpp).
    std::vector<MakeColumnFn> columnMakers;
    std::vector<MakeResourceFn> resourceMakers;
    std::vector<MakeEventsFn> eventMakers;
    std::vector<TypeKey> allotterDeclarations;
    std::vector<AllotterOps> allotterDeclarationOps;
    AllotterOps allotterOps;
    std::vector<std::uint32_t> allotterOwners;
    std::optional<ResourceId> allotter;
    ResourceId kernelSettings = 0;
    std::vector<std::string> runners;
    std::vector<std::uint32_t> runnerOwners;
    const RegistryData* extractSource = nullptr;
    // Test seam (spec J-20): non-zero reorders the Scheduler's internal containers.
    std::uint32_t schedulerShuffle = 0;
    bool extractSourceSet = false;

    // Registrar::RequireExplicitZones: an Unset zone is an error instead of zone B.
    bool explicitZones = false;
    // Calls of Registrar::DigestSet and RequireExplicitZones (R-E7 in a later round).
    std::uint32_t digestMarks = 0;
    std::uint32_t explicitZoneCalls = 0;
    std::vector<StartupError> errors;
    // The first BuildPlans (or Finish) has started: SetExtractSource is closed.
    bool finished = false;
    // True only while a Register call's build callable runs (Registrar asserts it).
    bool building = false;
    // A BuildPlans has succeeded (version >= 1).
    bool frozen = false;
    // The first round failed: the Registry is not usable.
    bool failed = false;
    // The first round has a Register since the Registry was made or discarded.
    bool registered = false;
    // FrozenRegistry::Version (DI-KS 25).
    std::uint64_t version = 0;
    // Set in a later round's pending copy.
    RoundBase base;
    // Derived at every BuildPlans (cleared first; declaration 3.2 rule 2).
    std::vector<Plan> plans;
    std::vector<TieBreak> ties;
    std::vector<SystemInfo> systemInfos;
    std::vector<HookInfo> hookInfos;
    std::vector<InitialValueInfo> initialValueInfos;
    std::vector<ScheduleInfo> scheduleInfos;
    std::vector<SetInfo> setInfos;
    std::vector<ScheduleOrder> orders;
    // The Worlds made from this Registry that are alive, and those of them that
    // declare no dynamic insert (DI-KS 26). Changed by World's constructor and
    // destructor through a const pointer, never copied into or out of a
    // pending round. Not the Worlds themselves: the Registry does not know them.
    mutable std::uint32_t liveWorlds = 0;
    mutable std::uint32_t refusingWorlds = 0;

    [[nodiscard]] std::string OwnerName(std::uint32_t owner) const;
    [[nodiscard]] const TypeEntry* FindType(TypeKey key) const noexcept;
    [[nodiscard]] const ScheduleRecord* FindSchedule(const std::string& name) const noexcept;

    void Error(StartupCode code, std::string rule, std::string message, std::vector<std::string> subjects = {},
               std::vector<std::string> sources = {});
    void Error(StartupCode code, StartupCode step, std::string rule, std::string message,
               std::vector<std::string> subjects = {}, std::vector<std::string> sources = {});
};

[[nodiscard]] std::string SourceText(const std::source_location& source);
[[nodiscard]] const char* DomainName(Domain domain) noexcept;
[[nodiscard]] const char* ZoneName(Zone zone) noexcept;

// Scheduler.cpp: builds every schedule's Plan into data.plans and its resolved
// order into data.orders, reporting D-E4, D-E5, D-E6, D-E16 into data.errors.
// Access sets must be resolved.
void BuildPlans(RegistryData& data);

// Fingerprint.cpp.
[[nodiscard]] Fingerprint RegistryFingerprintOf(const RegistryData& data);
[[nodiscard]] Fingerprint CFingerprintOf(const RegistryData& data);
[[nodiscard]] Fingerprint PlanFingerprintOf(const Plan& plan);

} // namespace GYO::Kernel::Detail
