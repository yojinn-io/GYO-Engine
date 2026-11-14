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
// to no plugin of the Host list.
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
    // Filled at finish.
    AccessSet access;
    std::vector<EffectId> effects;
    bool domainTick = false;
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

struct RegistryData {
    std::vector<PluginRecord> plugins;
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

    std::vector<StartupError> errors;
    bool finished = false;
    // True only while plugins' Build functions run (Registrar asserts it).
    bool building = false;
    bool frozen = false;
    std::vector<Plan> plans;
    std::vector<TieBreak> ties;
    std::vector<SystemInfo> systemInfos;

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

// Scheduler.cpp: builds every schedule's Plan into data.plans, reporting
// D-E4, D-E5, D-E6, D-E8, D-E16 into data.errors. Access sets must be resolved.
void BuildPlans(RegistryData& data);

// Fingerprint.cpp.
[[nodiscard]] Fingerprint RegistryFingerprintOf(const RegistryData& data);
[[nodiscard]] Fingerprint CFingerprintOf(const RegistryData& data);
[[nodiscard]] Fingerprint PlanFingerprintOf(const Plan& plan);

} // namespace GYO::Kernel::Detail
