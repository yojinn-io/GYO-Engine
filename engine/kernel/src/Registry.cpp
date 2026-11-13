// Registry lifecycle, registration, access resolution and the startup checks
// that are not about order (spec C, D.2 steps 1 to 8j, G.1). Order, conflicts
// and Plans are in Scheduler.cpp.

#include "engine/kernel/detail/RegistryData.hpp"

#include "engine/base/Assert.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace GYO::Kernel {

// ---- Error code names

std::string_view ToString(KernelErrorCode code) noexcept {
    switch (code) {
    case KernelErrorCode::SystemFailed: return "SystemFailed";
    }
    return "Unknown";
}

std::string_view ToString(StartupReportCode code) noexcept {
    switch (code) {
    case StartupReportCode::StartupFailed: return "StartupFailed";
    }
    return "Unknown";
}

std::string_view ToString(StartupCode code) noexcept {
    switch (code) {
    case StartupCode::DE1: return "D-E1";
    case StartupCode::DE2: return "D-E2";
    case StartupCode::DE3: return "D-E3";
    case StartupCode::DE4: return "D-E4";
    case StartupCode::DE5: return "D-E5";
    case StartupCode::DE6: return "D-E6";
    case StartupCode::DE7: return "D-E7";
    case StartupCode::DE8: return "D-E8";
    case StartupCode::DE9: return "D-E9";
    case StartupCode::DE10: return "D-E10";
    case StartupCode::DE11: return "D-E11";
    case StartupCode::DE12: return "D-E12";
    case StartupCode::DE13: return "D-E13";
    case StartupCode::DE14: return "D-E14";
    case StartupCode::DE15: return "D-E15";
    case StartupCode::DE16: return "D-E16";
    case StartupCode::DE17: return "D-E17";
    case StartupCode::DE18: return "D-E18";
    case StartupCode::DE19: return "D-E19";
    case StartupCode::RE1: return "R-E1";
    case StartupCode::RE2: return "R-E2";
    case StartupCode::RE3: return "R-E3";
    case StartupCode::RE4: return "R-E4";
    case StartupCode::RE5: return "R-E5";
    case StartupCode::RE6: return "R-E6";
    case StartupCode::XE1: return "X-E1";
    case StartupCode::XE2: return "X-E2";
    case StartupCode::XE3: return "X-E3";
    case StartupCode::FE1: return "F-E1";
    }
    return "Unknown";
}

StartupReport::StartupReport(std::vector<StartupError> found)
    : code(StartupReportCode::StartupFailed),
      message("the App cannot start: " + std::to_string(found.size()) + " startup error(s)"),
      errors(std::move(found)) {
    for (const StartupError& error : errors) {
        if (!detail.empty()) detail += "\n";
        detail += std::string(ToString(error.code));
        if (error.step != error.code) {
            detail += " (step " + std::string(ToString(error.step)) + ")";
        }
        detail += " " + error.rule + ": " + error.message;
    }
}

bool StartupReport::Has(StartupCode wanted) const noexcept {
    return Count(wanted) > 0;
}

std::size_t StartupReport::Count(StartupCode wanted) const noexcept {
    return static_cast<std::size_t>(
        std::count_if(errors.begin(), errors.end(), [wanted](const StartupError& e) { return e.Is(wanted); }));
}

std::size_t Plan::NonDataflowEdges() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(declaredEdges.begin(), declaredEdges.end(), [](const DeclaredEdge& e) { return !e.dataflow; }));
}

Plugin::~Plugin() = default;

std::vector<std::string_view> Plugin::Requires() const {
    return {};
}

void Plugin::Finish(const FrozenRegistry&, FinishContext&) {}

namespace Detail {

std::string RegistryData::OwnerName(std::uint32_t owner) const {
    if (owner == kKernelOwner) return kKernelOwnerName;
    return owner < plugins.size() ? plugins[owner].name : std::string("?");
}

const TypeEntry* RegistryData::FindType(TypeKey key) const noexcept {
    for (const TypeEntry& entry : types) {
        if (entry.key == key) return &entry;
    }
    return nullptr;
}

const ScheduleRecord* RegistryData::FindSchedule(const std::string& name) const noexcept {
    for (const ScheduleRecord& schedule : schedules) {
        if (schedule.options.name == name) return &schedule;
    }
    return nullptr;
}

void RegistryData::Error(StartupCode code, std::string rule, std::string message, std::vector<std::string> subjects,
                         std::vector<std::string> sources) {
    Error(code, code, std::move(rule), std::move(message), std::move(subjects), std::move(sources));
}

void RegistryData::Error(StartupCode code, StartupCode step, std::string rule, std::string message,
                         std::vector<std::string> subjects, std::vector<std::string> sources) {
    errors.push_back(StartupError{code, step, std::move(rule), std::move(message), std::move(subjects),
                                  std::move(sources)});
}

std::string SourceText(const std::source_location& source) {
    return std::string(source.file_name()) + ":" + std::to_string(source.line());
}

const char* DomainName(Domain domain) noexcept {
    switch (domain) {
    case Domain::None: return "None";
    case Domain::Fixed: return "Fixed";
    case Domain::Frame: return "Frame";
    case Domain::Loading: return "Loading";
    }
    return "?";
}

const char* ZoneName(Zone zone) noexcept {
    switch (zone) {
    case Zone::Unset: return "unset";
    case Zone::C: return "C";
    case Zone::B: return "B";
    }
    return "?";
}

namespace {

const char* RoleName(TypeRole role) noexcept {
    switch (role) {
    case TypeRole::Component: return "component";
    case TypeRole::Resource: return "resource";
    case TypeRole::Event: return "event";
    case TypeRole::SetTag: return "named set";
    }
    return "?";
}

template <class T>
void SortUnique(std::vector<T>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

// Registers one type key; R-E2 when the key is taken (R-E5 when the shapes
// differ, so the key cannot be the same type: spec C.4).
std::optional<std::uint32_t> ClaimType(RegistryData& data, TypeKey key, std::size_t size, std::size_t align,
                                       TypeRole role, std::uint32_t id, std::uint32_t owner, const std::string& name) {
    if (const TypeEntry* existing = data.FindType(key)) {
        if (existing->size != size || existing->align != align || existing->role != role) {
            data.Error(StartupCode::RE5, "C.4",
                       "TypeKey collision: two different types share one key (" + std::string(RoleName(existing->role)) +
                           " registered by " + data.OwnerName(existing->owner) + ", " + RoleName(role) + " '" + name +
                           "' by " + data.OwnerName(owner) + ")",
                       {data.OwnerName(existing->owner), data.OwnerName(owner), name});
        } else {
            data.Error(StartupCode::RE2, "R-6",
                       "type registered twice: " + std::string(RoleName(role)) + " '" + name + "' (first by " +
                           data.OwnerName(existing->owner) + ", again by " + data.OwnerName(owner) + ")",
                       {data.OwnerName(existing->owner), data.OwnerName(owner), name});
        }
        return std::nullopt;
    }
    data.types.push_back(TypeEntry{key, size, align, role, id, owner});
    return id;
}

class SystemResolver final : public AccessResolver {
public:
    SystemResolver(RegistryData& data, std::string subject, std::string source)
        : data_(data), subject_(std::move(subject)), source_(std::move(source)) {}

    AccessSet access;
    bool domainTick = false;
    bool kernelSettings = false;

    void Parameter(std::size_t position) override { position_ = position; }

    void Component(TypeKey type, AccessMode mode) override {
        if (const auto id = Lookup(type, TypeRole::Component, "component")) {
            (mode == AccessMode::Read ? access.reads : access.writes).push_back({AccessKind::Component, *id});
        }
    }
    void Resource(TypeKey type, AccessMode mode) override {
        if (const auto id = Lookup(type, TypeRole::Resource, "resource")) {
            if (*id == data_.kernelSettings) {
                kernelSettings = true;
            }
            (mode == AccessMode::Read ? access.reads : access.writes).push_back({AccessKind::Resource, *id});
        }
    }
    void Event(TypeKey type, AccessMode mode) override {
        if (const auto id = Lookup(type, TypeRole::Event, "event")) {
            (mode == AccessMode::Read ? access.reads : access.writes).push_back({AccessKind::Event, *id});
        }
    }
    void DomainTick() override { domainTick = true; }
    void Membership(TypeKey component) override {
        if (const auto id = Lookup(component, TypeRole::Component, "component")) access.membershipReads.push_back(*id);
    }
    void ChangeFilter(TypeKey component) override {
        access.changeFilters = true;
        Membership(component);
    }
    void DeferredWrite(TypeKey component) override {
        if (const auto id = Lookup(component, TypeRole::Component, "component")) access.deferredWrites.push_back(*id);
    }
    void Despawn() override { access.despawn = true; }
    void Spawn() override { access.spawn = true; }
    void UntypedCommands() override { access.untypedCommands = true; }
    void SetRead(TypeKey setTag) override {
        if (const auto id = Lookup(setTag, TypeRole::SetTag, "named set")) access.setReads.push_back(*id);
    }
    void Local() override { access.local = true; }
    void WholeWorld() override { access.exclusive = true; }
    void Diagnostics() override { access.diagnostics = true; }
    void ExtractComponent(TypeKey component) override { Extract(component, TypeRole::Component, AccessKind::Component); }
    void ExtractResource(TypeKey resource) override { Extract(resource, TypeRole::Resource, AccessKind::Resource); }

    bool usesExtract = false;

private:
    std::optional<std::uint32_t> Lookup(TypeKey key, TypeRole role, const char* what) {
        const TypeEntry* entry = data_.FindType(key);
        if (entry != nullptr && entry->role == role) {
            return entry->id;
        }
        data_.Error(StartupCode::RE4, "R-6",
                    subject_ + " references a " + what + " type that is not registered" +
                        (entry != nullptr ? std::string(" (the type is registered as a ") + RoleName(entry->role) + ")"
                                          : std::string()) +
                        " (parameter " + std::to_string(position_ + 1) + ")",
                    {subject_}, {source_});
        return std::nullopt;
    }

    void Extract(TypeKey key, TypeRole role, AccessKind kind) {
        usesExtract = true;
        const RegistryData* source = data_.extractSource;
        if (source == nullptr || !source->frozen) {
            return;  // D-E14 is reported once per System by the caller
        }
        const TypeEntry* entry = source->FindType(key);
        if (entry == nullptr || entry->role != role) {
            data_.Error(StartupCode::RE4, "R-6",
                        subject_ + " extracts a type that the source Registry does not register (parameter " +
                            std::to_string(position_ + 1) + ")",
                        {subject_}, {source_});
            return;
        }
        access.extractReads.push_back({kind, entry->id});
    }

    RegistryData& data_;
    std::string subject_;
    std::string source_;
    std::size_t position_ = 0;
};

void Normalize(AccessSet& access) {
    SortUnique(access.reads);
    SortUnique(access.writes);
    SortUnique(access.membershipReads);
    SortUnique(access.deferredWrites);
    SortUnique(access.setReads);
    SortUnique(access.extractReads);
}

} // namespace

} // namespace Detail

// ---- FinishContext, Registrar

void FinishContext::InstallRunner(std::string_view runner) {
    registry_->data_->runners.emplace_back(runner);
    registry_->data_->runnerOwners.push_back(plugin_);
}

ComponentId Registrar::RegisterComponent(TypeKey key, Shape shape, ComponentOptions options) {
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<ComponentId>(data.components.size());
    if (!Detail::ClaimType(data, key, shape.size, shape.align, Detail::TypeRole::Component, id, plugin_, options.name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.components.push_back(ColumnInfo{std::move(options.name), shape.size, shape.align, shape.trivialRelocate,
                                         options.writePolicy, options.zone, options.role, options.rollback,
                                         data.OwnerName(plugin_)});
    return id;
}

ComponentId TypeKeyCollisionProbe::RegisterComponent(Registrar& registrar, TypeKey key, std::size_t size,
                                                     std::size_t align, ComponentOptions options) {
    return registrar.RegisterComponent(key, Registrar::Shape{size, align, true}, std::move(options));
}

ResourceId Registrar::RegisterResource(TypeKey key, Shape shape, ResourceOptions options) {
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<ResourceId>(data.resources.size());
    if (!Detail::ClaimType(data, key, shape.size, shape.align, Detail::TypeRole::Resource, id, plugin_, options.name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.resources.push_back(ResourceInfo{std::move(options.name), shape.size, shape.align, options.writePolicy,
                                          options.zone, options.role, options.rollback, options.external,
                                          std::move(options.affinity), data.OwnerName(plugin_)});
    return id;
}

EventTypeId Registrar::RegisterEvent(TypeKey key, Shape shape, EventOptions options) {
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<EventTypeId>(data.events.size());
    if (!Detail::ClaimType(data, key, shape.size, shape.align, Detail::TypeRole::Event, id, plugin_, options.name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.events.push_back(EventInfo{std::move(options.name), options.domain, options.zone, data.OwnerName(plugin_)});
    return id;
}

EffectId Registrar::Effect(std::string name) {
    Detail::RegistryData& data = *data_;
    for (std::size_t i = 0; i < data.effects.size(); ++i) {
        if (data.effects[i].name == name) {
            data.Error(StartupCode::RE2, "R-6",
                       "effect '" + name + "' registered twice (first by " + data.OwnerName(data.effects[i].owner) +
                           ", again by " + data.OwnerName(plugin_) + ")",
                       {data.OwnerName(data.effects[i].owner), data.OwnerName(plugin_), name});
            return static_cast<EffectId>(i);
        }
    }
    data.effects.push_back(Detail::EffectRecord{std::move(name), plugin_});
    return static_cast<EffectId>(data.effects.size() - 1);
}

SetId Registrar::RegisterNamedSet(TypeKey key, std::string name) {
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<SetId>(data.namedSets.size());
    if (!Detail::ClaimType(data, key, 0, 0, Detail::TypeRole::SetTag, id, plugin_, name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.namedSets.push_back(Detail::NamedSetRecord{std::move(name), key, plugin_, {}, {}, {}, false});
    return id;
}

void Registrar::AddSetMember(TypeKey set, TypeKey component) {
    Detail::RegistryData& data = *data_;
    for (Detail::NamedSetRecord& record : data.namedSets) {
        if (record.key == set) {
            record.pendingMembers.push_back(component);
            record.memberOwners.push_back(plugin_);
            return;
        }
    }
    data.Error(StartupCode::RE4, "R-6", data.OwnerName(plugin_) + " adds a member to a named set that is not registered",
               {data.OwnerName(plugin_)});
}

void Registrar::MarkDigestSet(TypeKey set) {
    Detail::RegistryData& data = *data_;
    for (Detail::NamedSetRecord& record : data.namedSets) {
        if (record.key == set) {
            record.digest = true;
            return;
        }
    }
    data.Error(StartupCode::RE4, "R-6", data.OwnerName(plugin_) + " marks a named set that is not registered as the digest set",
               {data.OwnerName(plugin_)});
}

void Registrar::DeclareAllotter(TypeKey resource) {
    data_->allotterDeclarations.push_back(resource);
    data_->allotterOwners.push_back(plugin_);
}

void Registrar::Schedule(ScheduleOptions options) {
    Detail::RegistryData& data = *data_;
    if (const Detail::ScheduleRecord* existing = data.FindSchedule(options.name)) {
        data.Error(StartupCode::RE2, "C.2", "schedule '" + options.name + "' registered twice (first by " +
                                               data.OwnerName(existing->owner) + ", again by " + data.OwnerName(plugin_) + ")",
                   {data.OwnerName(existing->owner), data.OwnerName(plugin_), options.name});
        return;
    }
    data.schedules.push_back(Detail::ScheduleRecord{std::move(options), plugin_});
}

void Registrar::InsertStage(std::string_view schedule, std::string_view anchor, StagePlace place, std::string stage) {
    Detail::RegistryData& data = *data_;
    for (Detail::ScheduleRecord& record : data.schedules) {
        if (record.options.name != schedule) continue;
        std::vector<std::string>& stages = record.options.stages;
        const auto at = std::find(stages.begin(), stages.end(), anchor);
        if (at == stages.end()) {
            data.Error(StartupCode::DE1, "C.2",
                       data.OwnerName(plugin_) + " inserts stage '" + stage + "' next to stage '" + std::string(anchor) +
                           "', which schedule '" + std::string(schedule) + "' does not have",
                       {data.OwnerName(plugin_), std::string(schedule)});
            return;
        }
        stages.insert(place == StagePlace::Before ? at : at + 1, std::move(stage));
        return;
    }
    data.Error(StartupCode::DE1, "C.2",
               data.OwnerName(plugin_) + " inserts stage '" + stage + "' into schedule '" + std::string(schedule) +
                   "', which no earlier plugin registered",
               {data.OwnerName(plugin_), std::string(schedule)});
}

void Registrar::Set(SetOptions options, std::vector<ConditionRef> conditions) {
    Detail::RegistryData& data = *data_;
    for (const Detail::SetRecord& existing : data.sets) {
        if (existing.options.schedule == options.schedule && existing.options.name == options.name) {
            data.Error(StartupCode::RE2, "C.1",
                       "set '" + options.name + "' of schedule '" + options.schedule + "' registered twice",
                       {data.OwnerName(existing.owner), data.OwnerName(plugin_), options.name});
            return;
        }
    }
    data.sets.push_back(Detail::SetRecord{std::move(options), std::move(conditions), plugin_, {}, {}});
}

RegistrationSeq Registrar::RegisterSystem(SystemOptions options, Detail::ResolveFn resolve, bool extractOnly,
                                          std::source_location source) {
    Detail::RegistryData& data = *data_;
    const auto seq = static_cast<RegistrationSeq>(data.systems.size());
    for (const Detail::SystemRecord& existing : data.systems) {
        if (existing.options.name == options.name) {
            data.Error(StartupCode::RE2, "B.1",
                       "System name '" + options.name + "' registered twice (before / after and reports refer to "
                                                        "Systems by name)",
                       {data.OwnerName(existing.owner), data.OwnerName(plugin_), options.name},
                       {Detail::SourceText(existing.source), Detail::SourceText(source)});
            break;
        }
    }
    data.systems.push_back(Detail::SystemRecord{std::move(options), resolve, extractOnly, source, plugin_, seq, {}, {}, false});
    return seq;
}

void Registrar::RegisterHook(TypeKey component, HookTrigger trigger, HookOptions options, Detail::ResolveFn resolve,
                             std::source_location source) {
    data_->hooks.push_back(Detail::HookRecord{std::move(options.name), options.zone, component, trigger, resolve, source,
                                              plugin_, std::nullopt, {}});
}

// ---- FrozenRegistry

std::span<const ColumnInfo> FrozenRegistry::Components() const noexcept { return data_->components; }
std::span<const ResourceInfo> FrozenRegistry::Resources() const noexcept { return data_->resources; }
std::span<const EventInfo> FrozenRegistry::Events() const noexcept { return data_->events; }
std::span<const SystemInfo> FrozenRegistry::Systems() const noexcept { return data_->systemInfos; }
std::span<const Plan> FrozenRegistry::Plans() const noexcept { return data_->plans; }
std::span<const TieBreak> FrozenRegistry::TieBreaks() const noexcept { return data_->ties; }

const SystemInfo* FrozenRegistry::SystemNamed(std::string_view name) const noexcept {
    for (const SystemInfo& info : data_->systemInfos) {
        if (info.name == name) return &info;
    }
    return nullptr;
}

const Plan* FrozenRegistry::PlanOf(std::string_view schedule) const noexcept {
    for (const Plan& plan : data_->plans) {
        if (plan.schedule == schedule) return &plan;
    }
    return nullptr;
}

std::vector<ComponentId> FrozenRegistry::SetMembers(SetId set) const {
    return set < data_->namedSets.size() ? data_->namedSets[set].members : std::vector<ComponentId>{};
}

std::optional<ComponentId> FrozenRegistry::FindComponent(TypeKey key) const noexcept {
    const Detail::TypeEntry* entry = data_->FindType(key);
    return entry != nullptr && entry->role == Detail::TypeRole::Component ? std::optional<ComponentId>(entry->id)
                                                                           : std::nullopt;
}

std::optional<ResourceId> FrozenRegistry::FindResource(TypeKey key) const noexcept {
    const Detail::TypeEntry* entry = data_->FindType(key);
    return entry != nullptr && entry->role == Detail::TypeRole::Resource ? std::optional<ResourceId>(entry->id)
                                                                          : std::nullopt;
}

std::optional<EventTypeId> FrozenRegistry::FindEvent(TypeKey key) const noexcept {
    const Detail::TypeEntry* entry = data_->FindType(key);
    return entry != nullptr && entry->role == Detail::TypeRole::Event ? std::optional<EventTypeId>(entry->id)
                                                                       : std::nullopt;
}

Fingerprint FrozenRegistry::RegistryFingerprint() const {
    return Detail::RegistryFingerprintOf(*data_);
}

Fingerprint FrozenRegistry::CFingerprint() const {
    return Detail::CFingerprintOf(*data_);
}

// ---- Registry

Registry::Registry(std::vector<Plugin*> plugins)
    : data_(std::make_unique<Detail::RegistryData>()), frozen_(*data_) {
    for (Plugin* plugin : plugins) {
        GYO_ASSERT(plugin != nullptr);
        Detail::PluginRecord record{plugin, std::string(plugin->Name()), {}, false};
        for (std::string_view required : plugin->Requires()) {
            record.required.emplace_back(required);
        }
        data_->plugins.push_back(std::move(record));
    }
    // The kernel's own registrations come first (ResourceId 0, EffectId 0), so
    // they never shift with the plugin list.
    Detail::RegistryData& data = *data_;
    data.kernelSettings = static_cast<ResourceId>(data.resources.size());
    data.types.push_back(Detail::TypeEntry{KeyOf<KernelSettings>(), sizeof(KernelSettings), alignof(KernelSettings),
                                           Detail::TypeRole::Resource, data.kernelSettings, Detail::kKernelOwner});
    data.resources.push_back(ResourceInfo{"KernelSettings", sizeof(KernelSettings), alignof(KernelSettings),
                                          WritePolicy::Exclusive, Zone::B, Role::State, Rollback::No, true,
                                          Affinity::Any(), Detail::kKernelOwnerName});
    data.effects.push_back(Detail::EffectRecord{Detail::kDiagnosticsEffect, Detail::kKernelOwner});
}

Registry::~Registry() = default;

void Registry::SetExtractSource(const Registry& source) {
    GYO_ASSERT(!data_->finished);
    data_->extractSource = source.data_.get();
    data_->extractSourceSet = true;
}

void SchedulerShuffleProbe::Set(Registry& registry, std::uint32_t seed) {
    GYO_ASSERT(!registry.data_->finished);
    registry.data_->schedulerShuffle = seed;
}

bool Registry::IsFrozen() const noexcept {
    return data_->frozen;
}

const FrozenRegistry& Registry::Frozen() const {
    GYO_ASSERT(data_->frozen);
    return frozen_;
}

namespace Detail {
namespace {

void CheckPluginList(RegistryData& data) {
    for (std::size_t i = 0; i < data.plugins.size(); ++i) {
        PluginRecord& record = data.plugins[i];
        bool duplicate = false;
        for (std::size_t j = 0; j < i; ++j) {
            if (data.plugins[j].name == record.name) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            data.Error(StartupCode::RE1, "R-4", "plugin name '" + record.name + "' appears twice in the Host's list",
                       {record.name});
            continue;
        }
        record.built = true;
        for (const std::string& required : record.required) {
            bool earlier = false;
            bool later = false;
            for (std::size_t j = 0; j < data.plugins.size(); ++j) {
                if (data.plugins[j].name != required) continue;
                (j < i ? earlier : later) = true;
            }
            if (!earlier) {
                data.Error(StartupCode::RE3, "C.2",
                           "plugin '" + record.name + "' requires '" + required + "', which " +
                               (later ? "comes later in the Host's list (the kernel does not reorder)"
                                      : "is not in the Host's list"),
                           {record.name, required});
            }
        }
    }
}

void CheckData(RegistryData& data) {
    for (const ColumnInfo& info : data.components) {
        if (info.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", "component '" + info.name + "' declares no zone (there is no default)",
                       {info.name, info.ownerPlugin});
        }
        if (info.role == Role::Input && info.rollback != Rollback::No) {
            data.Error(StartupCode::DE7, "Z8", "component '" + info.name + "' has role Input and must not roll back",
                       {info.name, info.ownerPlugin});
        }
    }
    for (const ResourceInfo& info : data.resources) {
        if (info.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", "resource '" + info.name + "' declares no zone (there is no default)",
                       {info.name, info.ownerPlugin});
        }
        if (info.external && info.zone == Zone::C) {
            data.Error(StartupCode::DE7, "Z1", "external resource '" + info.name + "' is zone C; external is always zone B",
                       {info.name, info.ownerPlugin});
        }
        if (info.role == Role::Input && info.rollback != Rollback::No) {
            data.Error(StartupCode::DE7, "Z8", "resource '" + info.name + "' has role Input and must not roll back",
                       {info.name, info.ownerPlugin});
        }
    }
    for (const EventInfo& info : data.events) {
        if (info.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", "event '" + info.name + "' declares no zone (there is no default)",
                       {info.name, info.ownerPlugin});
        }
        if (info.domain == Domain::None) {
            data.Error(StartupCode::FE1, "F-9", "event '" + info.name + "' belongs to no clock domain",
                       {info.name, info.ownerPlugin});
        }
    }
    // Allotter.
    for (std::size_t i = 0; i < data.allotterDeclarations.size(); ++i) {
        const TypeEntry* entry = data.FindType(data.allotterDeclarations[i]);
        if (entry == nullptr || entry->role != TypeRole::Resource) {
            data.Error(StartupCode::RE4, "A.1",
                       data.OwnerName(data.allotterOwners[i]) + " declares an allotter that is not a registered resource",
                       {data.OwnerName(data.allotterOwners[i])});
        } else if (data.allotter && *data.allotter != entry->id) {
            data.Error(StartupCode::RE2, "A.1",
                       "two allotters declared: '" + data.resources[*data.allotter].name + "' and '" +
                           data.resources[entry->id].name + "'",
                       {data.resources[*data.allotter].name, data.resources[entry->id].name});
        } else {
            data.allotter = entry->id;
        }
    }
    // Named sets.
    for (NamedSetRecord& set : data.namedSets) {
        for (std::size_t i = 0; i < set.pendingMembers.size(); ++i) {
            const TypeEntry* entry = data.FindType(set.pendingMembers[i]);
            if (entry == nullptr || entry->role != TypeRole::Component) {
                data.Error(StartupCode::RE4, "S13",
                           data.OwnerName(set.memberOwners[i]) + " adds a type that is not a registered component to set '" +
                               set.name + "'",
                           {data.OwnerName(set.memberOwners[i]), set.name});
                continue;
            }
            set.members.push_back(entry->id);
        }
        SortUnique(set.members);
        if (set.digest) {
            for (ComponentId member : set.members) {
                if (data.components[member].writePolicy == WritePolicy::Shared) {
                    data.Error(StartupCode::DE2, "S-12",
                               "component '" + data.components[member].name + "' is in the digest set '" + set.name +
                                   "' but is Shared (a digest column must be Exclusive)",
                               {data.components[member].name, set.name});
                }
            }
        }
    }
}

void CheckSchedules(RegistryData& data) {
    std::map<Domain, std::vector<std::string>> clocks;
    std::set<Domain> usedDomains;
    for (const ScheduleRecord& record : data.schedules) {
        const ScheduleOptions& s = record.options;
        std::set<std::string> stages;
        for (const std::string& stage : s.stages) {
            if (!stages.insert(stage).second) {
                data.Error(StartupCode::RE2, "D.1", "schedule '" + s.name + "' has stage '" + stage + "' twice", {s.name});
            }
        }
        if (s.stages.empty()) {
            data.Error(StartupCode::DE1, "D.1", "schedule '" + s.name + "' has no stage", {s.name});
        }
        if (!s.ingressStage.empty() && stages.count(s.ingressStage) == 0) {
            data.Error(StartupCode::DE1, "Z3", "schedule '" + s.name + "' names ingress stage '" + s.ingressStage +
                                                   "', which it does not have", {s.name});
        }
        std::set<std::string> syncNames;
        for (const SyncPointOptions& sync : s.syncPoints) {
            if (stages.count(sync.stage) == 0) {
                data.Error(StartupCode::DE1, "D.1", "sync point '" + sync.name + "' of schedule '" + s.name +
                                                       "' is in stage '" + sync.stage + "', which the schedule does not have",
                           {s.name, sync.name});
            }
            if (stages.count(sync.name) != 0 || !syncNames.insert(sync.name).second) {
                data.Error(StartupCode::RE2, "D.1", "sync point name '" + sync.name + "' of schedule '" + s.name +
                                                       "' is not unique", {s.name, sync.name});
            }
        }
        if (!s.eventTrim.empty() && syncNames.count(s.eventTrim) == 0 && stages.count(s.eventTrim) == 0) {
            data.Error(StartupCode::DE1, "D.1", "schedule '" + s.name + "' trims events at '" + s.eventTrim +
                                                   "', which is neither a stage end nor a sync point", {s.name});
        }
        if (s.startup) {
            if (s.clock || s.domain != Domain::None) {
                data.Error(StartupCode::DE12, "H-1", "startup schedule '" + s.name + "' has a clock or a domain", {s.name});
            }
        } else {
            if (s.domain == Domain::None) {
                data.Error(StartupCode::DE12, "H-1", "schedule '" + s.name + "' belongs to no clock domain", {s.name});
            } else {
                usedDomains.insert(s.domain);
                if (s.clock) clocks[s.domain].push_back(s.name);
            }
        }
        if (s.executor == ExecutorKind::Parallel) {
            data.Error(StartupCode::DE17, "X-2", "schedule '" + s.name + "' declares Parallel; v1 has only Serial (LIM-2)",
                       {s.name});
        }
        if (s.runner.empty()) {
            data.Error(StartupCode::DE13, "H-2", "schedule '" + s.name + "' names no runner", {s.name});
        }
    }
    for (Domain domain : usedDomains) {
        const std::vector<std::string>& names = clocks[domain];
        if (names.size() != 1) {
            std::vector<std::string> subjects = names;
            data.Error(StartupCode::DE12, "H-1",
                       std::string("domain ") + DomainName(domain) + " has " + std::to_string(names.size()) +
                           " clock schedules; a World has exactly one per domain",
                       subjects);
        }
    }
    for (std::size_t i = 1; i < data.schedules.size(); ++i) {
        const ScheduleOptions& first = data.schedules[0].options;
        const ScheduleOptions& other = data.schedules[i].options;
        if (other.runner != first.runner && !other.runner.empty() && !first.runner.empty()) {
            data.Error(StartupCode::DE13, "H-2",
                       "schedules '" + first.name + "' (runner " + first.runner + ") and '" + other.name + "' (runner " +
                           other.runner + ") of one World run on different threads; two threads need two Worlds",
                       {first.name, other.name});
        }
    }
}

void ResolveSystems(RegistryData& data) {
    for (SystemRecord& system : data.systems) {
        const std::string subject = "System '" + system.options.name + "'";
        SystemResolver resolver(data, subject, SourceText(system.source));
        system.resolve(resolver);
        for (const Mount& mount : system.options.mounts) {
            for (const ConditionRef& condition : mount.conditions) {
                if (condition.resolve == nullptr) continue;
                SystemResolver conditionResolver(data, subject + " run condition '" + condition.name + "'",
                                                 SourceText(system.source));
                condition.resolve(conditionResolver);
                for (const AccessKey& key : conditionResolver.access.reads) resolver.access.reads.push_back(key);
                for (ComponentId id : conditionResolver.access.membershipReads) resolver.access.membershipReads.push_back(id);
                resolver.access.changeFilters = resolver.access.changeFilters || conditionResolver.access.changeFilters;
                resolver.access.setReads.insert(resolver.access.setReads.end(), conditionResolver.access.setReads.begin(),
                                                conditionResolver.access.setReads.end());
                resolver.domainTick = resolver.domainTick || conditionResolver.domainTick;
                resolver.kernelSettings = resolver.kernelSettings || conditionResolver.kernelSettings;
            }
            // Set conditions join every member's reads (spec E.4).
            for (const std::string& setName : mount.sets) {
                for (SetRecord& set : data.sets) {
                    if (set.options.schedule != mount.schedule || set.options.name != setName) continue;
                    for (const ConditionRef& condition : set.conditions) {
                        if (condition.resolve == nullptr) continue;
                        SystemResolver conditionResolver(data, "set '" + setName + "' run condition '" + condition.name + "'",
                                                         SourceText(system.source));
                        condition.resolve(conditionResolver);
                        for (const AccessKey& key : conditionResolver.access.reads) resolver.access.reads.push_back(key);
                        for (ComponentId id : conditionResolver.access.membershipReads)
                            resolver.access.membershipReads.push_back(id);
                    }
                }
            }
        }
        AccessSet& access = resolver.access;
        // CurrentTick: one domain-tick read per mount's domain (spec S-4).
        if (resolver.domainTick) {
            for (const Mount& mount : system.options.mounts) {
                if (const ScheduleRecord* schedule = data.FindSchedule(mount.schedule)) {
                    if (schedule->options.domain != Domain::None) {
                        access.reads.push_back({AccessKind::DomainTick, static_cast<std::uint32_t>(schedule->options.domain)});
                    }
                }
            }
        }
        // SetRead: the named set's members as component reads (spec S13).
        for (SetId set : access.setReads) {
            for (ComponentId member : data.namedSets[set].members) {
                access.reads.push_back({AccessKind::Component, member});
            }
        }
        // Spawn writes the allotter (spec A.1).
        if (access.spawn) {
            if (data.allotter) {
                access.writes.push_back({AccessKind::Resource, *data.allotter});
            } else {
                data.Error(StartupCode::DE3, "A.1",
                           subject + " spawns entities, but this World declares no allotter (use SpawnWithId, or "
                                     "declare an allotter resource)",
                           {system.options.name}, {SourceText(system.source)});
            }
        }
        if (resolver.kernelSettings) {
            data.Error(StartupCode::DE18, "A.6",
                       subject + " declares Res or ResMut of KernelSettings; only the Host writes it and only the "
                                 "kernel reads it",
                       {system.options.name, "KernelSettings"}, {SourceText(system.source)});
        }
        if (resolver.usesExtract && (data.extractSource == nullptr || !data.extractSource->frozen)) {
            data.Error(StartupCode::DE14, "H-6",
                       subject + " extracts from a source Registry that " +
                           (data.extractSource == nullptr ? std::string("is not set") : std::string("is not frozen")),
                       {system.options.name}, {SourceText(system.source)});
        }
        // A System that reads and writes one key through two signatures (a
        // condition reading what the System writes) keeps only the write: the
        // write already covers the read for conflicts (spec B.4 point 5).
        Normalize(access);
        std::vector<AccessKey> reads;
        for (const AccessKey& key : access.reads) {
            if (!std::binary_search(access.writes.begin(), access.writes.end(), key)) reads.push_back(key);
        }
        access.reads = std::move(reads);
        system.access = std::move(access);
        system.domainTick = resolver.domainTick;
        // Effects.
        for (const std::string& effect : system.options.effects) {
            if (effect == kDiagnosticsEffect) {
                data.Error(StartupCode::DE7, "S-8",
                           subject + " writes effect Diagnostics by hand; it comes only from DiagnosticsWriter<T>",
                           {system.options.name}, {SourceText(system.source)});
                continue;
            }
            bool found = false;
            for (std::size_t i = 0; i < data.effects.size(); ++i) {
                if (data.effects[i].name == effect) {
                    system.effects.push_back(static_cast<EffectId>(i));
                    found = true;
                }
            }
            if (!found) {
                data.Error(StartupCode::RE4, "C.1", subject + " declares effect '" + effect + "', which is not registered",
                           {system.options.name, effect}, {SourceText(system.source)});
            }
        }
        SortUnique(system.effects);
    }
    for (HookRecord& hook : data.hooks) {
        const std::string subject = "hook '" + hook.name + "'";
        const TypeEntry* entry = data.FindType(hook.component);
        if (entry == nullptr || entry->role != TypeRole::Component) {
            data.Error(StartupCode::RE4, "R-6", subject + " is attached to a type that is not a registered component",
                       {hook.name}, {SourceText(hook.source)});
        } else {
            hook.componentId = entry->id;
        }
        SystemResolver resolver(data, subject, SourceText(hook.source));
        hook.resolve(resolver);
        for (SetId set : resolver.access.setReads) {
            for (ComponentId member : data.namedSets[set].members) {
                resolver.access.reads.push_back({AccessKind::Component, member});
            }
        }
        if (resolver.access.spawn && data.allotter) {
            resolver.access.writes.push_back({AccessKind::Resource, *data.allotter});
        }
        Normalize(resolver.access);
        hook.access = std::move(resolver.access);
        if (hook.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", subject + " declares no zone (there is no default)", {hook.name},
                       {SourceText(hook.source)});
        }
    }
}

// The zone of the data behind one access key; DomainTick is the kernel's (c) time source.
Zone ZoneOf(const RegistryData& data, const AccessKey& key) {
    switch (key.kind) {
    case AccessKind::Component: return data.components[key.id].zone;
    case AccessKind::Resource: return data.resources[key.id].zone;
    case AccessKind::Event: return data.events[key.id].zone;
    case AccessKind::DomainTick: return Zone::C;
    }
    return Zone::Unset;
}

std::string NameOf(const RegistryData& data, const AccessKey& key) {
    switch (key.kind) {
    case AccessKind::Component: return data.components[key.id].name;
    case AccessKind::Resource: return data.resources[key.id].name;
    case AccessKind::Event: return data.events[key.id].name;
    case AccessKind::DomainTick: return std::string("domain tick ") + DomainName(static_cast<Domain>(key.id));
    }
    return "?";
}

bool IsInput(const RegistryData& data, const AccessKey& key) {
    return (key.kind == AccessKind::Component && data.components[key.id].role == Role::Input) ||
           (key.kind == AccessKind::Resource && data.resources[key.id].role == Role::Input);
}

bool IsExternal(const RegistryData& data, const AccessKey& key) {
    return key.kind == AccessKind::Resource && data.resources[key.id].external;
}

Rollback RollbackOf(const RegistryData& data, const AccessKey& key) {
    if (key.kind == AccessKind::Component) return data.components[key.id].rollback;
    if (key.kind == AccessKind::Resource) return data.resources[key.id].rollback;
    return Rollback::Yes;
}

// Every key an access set writes: immediate writes and deferred writes.
std::vector<AccessKey> WrittenKeys(const AccessSet& access) {
    std::vector<AccessKey> keys = access.writes;
    for (ComponentId id : access.deferredWrites) keys.push_back({AccessKind::Component, id});
    SortUnique(keys);
    return keys;
}

// Every key an access set touches (spec Z1 "全部存取").
std::vector<AccessKey> TouchedKeys(const AccessSet& access) {
    std::vector<AccessKey> keys = WrittenKeys(access);
    keys.insert(keys.end(), access.reads.begin(), access.reads.end());
    for (ComponentId id : access.membershipReads) keys.push_back({AccessKind::Component, id});
    SortUnique(keys);
    return keys;
}

// Z1, Z2 (first half), Z7 and the boundary rule for one System or hook.
void CheckZoneAccess(RegistryData& data, const std::string& subject, const std::string& name, Zone zone, bool ingress,
                     const AccessSet& access, const std::string& source) {
    if (zone == Zone::Unset) {
        return;  // already S-6; zone rules need a zone
    }
    if (zone == Zone::C) {
        const std::vector<AccessKey> written = WrittenKeys(access);
        for (const AccessKey& key : TouchedKeys(access)) {
            if (ZoneOf(data, key) == Zone::C) continue;
            const bool writes = std::binary_search(written.begin(), written.end(), key);
            const bool external = IsExternal(data, key);
            data.Error(StartupCode::DE7, writes && external ? "Z7" : "Z1",
                       subject + " is zone C but " + (writes ? "writes" : "reads") + " '" + NameOf(data, key) + "', which is " +
                           (external ? "external (always zone B)" : "zone " + std::string(ZoneName(ZoneOf(data, key)))),
                       {name, NameOf(data, key)}, {source});
        }
        for (const AccessKey& key : written) {
            if (IsInput(data, key)) {
                data.Error(StartupCode::DE7, "Z3",
                           subject + " is zone C and writes role=Input data '" + NameOf(data, key) +
                               "'; only the Host and ingress Systems write boundary input",
                           {name, NameOf(data, key)}, {source});
            }
        }
        return;
    }
    for (const AccessKey& key : WrittenKeys(access)) {
        if (ZoneOf(data, key) != Zone::C) continue;
        if (ingress && IsInput(data, key)) continue;  // Z3
        data.Error(StartupCode::DE7, "Z2",
                   subject + " is zone " + ZoneName(zone) + (ingress ? " (ingress)" : "") + " but writes zone C data '" +
                       NameOf(data, key) + "'" + (ingress ? " whose role is not Input" : ""),
                   {name, NameOf(data, key)}, {source});
    }
}

void CheckSystems(RegistryData& data) {
    bool anyRollback = false;
    for (const ScheduleRecord& schedule : data.schedules) anyRollback = anyRollback || schedule.options.rollbackEnabled;

    for (const SystemRecord& system : data.systems) {
        const std::string& name = system.options.name;
        const std::string subject = "System '" + name + "'";
        const std::string source = SourceText(system.source);
        const Zone zone = system.options.zone;
        const AccessSet& access = system.access;
        if (zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", subject + " declares no zone (there is no default)", {name}, {source});
        }
        if (system.options.mounts.empty()) {
            data.Error(StartupCode::DE1, "B.1", subject + " has no mount, so it never runs", {name}, {source});
        }
        if (zone == Zone::C) {
            for (EffectId effect : system.effects) {
                data.Error(StartupCode::DE7, "S-8",
                           subject + " is zone C and declares effect '" + data.effects[effect].name +
                               "'; only Diagnostics (from DiagnosticsWriter<T>) is allowed",
                           {name, data.effects[effect].name}, {source});
            }
            if (access.local) {
                data.Error(StartupCode::DE7, "S-7", subject + " is zone C and has Local<T> (state outside the World)",
                           {name}, {source});
            }
            if (access.changeFilters) {
                data.Error(StartupCode::DE7, "S-9", subject + " is zone C and filters by Changed or Added", {name}, {source});
            }
            if (access.exclusive) {
                data.Error(StartupCode::DE7, "S-13", subject + " is zone C and takes ExclusiveWorld", {name}, {source});
            }
            if (system.options.ingress) {
                data.Error(StartupCode::DE7, "Z3", subject + " is zone C and ingress; ingress Systems are zone B",
                           {name}, {source});
            }
        }
        CheckZoneAccess(data, subject, name, zone, system.options.ingress, access, source);

        std::set<std::string> mountedSchedules;
        for (const Mount& mount : system.options.mounts) {
            const ScheduleRecord* record = data.FindSchedule(mount.schedule);
            if (record == nullptr) {
                data.Error(StartupCode::DE1, "D.2", subject + " is mounted in schedule '" + mount.schedule +
                                                        "', which this App does not have",
                           {name, mount.schedule}, {source});
                continue;
            }
            const ScheduleOptions& s = record->options;
            if (std::find(s.stages.begin(), s.stages.end(), mount.stage) == s.stages.end()) {
                data.Error(StartupCode::DE1, "D.2", subject + " is mounted in stage '" + mount.stage + "' of schedule '" +
                                                        s.name + "', which the schedule does not have",
                           {name, s.name}, {source});
            }
            if (!mountedSchedules.insert(s.name).second) {
                data.Error(StartupCode::DE19, "B.1", subject + " is mounted twice in schedule '" + s.name + "'",
                           {name, s.name}, {source});
            }
            if (system.options.ingress && (s.ingressStage.empty() || mount.stage != s.ingressStage)) {
                data.Error(StartupCode::DE7, "Z3",
                           subject + " is ingress but its mount in schedule '" + s.name + "' is in stage '" + mount.stage +
                               "', not the ingress stage" + (s.ingressStage.empty() ? " (the schedule has none)" : " '" + s.ingressStage + "'"),
                           {name, s.name}, {source});
            }
            if (system.options.ingress && s.rollbackEnabled && !mount.notInRollback) {
                data.Error(StartupCode::DE15, "H-10",
                           subject + " is ingress and its mount in rollback-enabled schedule '" + s.name +
                               "' lacks NotInRollback",
                           {name, s.name}, {source});
            }
            if (system.extractOnly && !s.extract) {
                data.Error(StartupCode::DE7, "H-6",
                           subject + " has Extract parameters but schedule '" + s.name + "' is not an extract schedule",
                           {name, s.name}, {source});
            }
            if (zone == Zone::C && s.rollbackEnabled) {
                for (const AccessKey& key : WrittenKeys(access)) {
                    if (key.kind == AccessKind::Event) continue;
                    const Rollback rollback = RollbackOf(data, key);
                    if (rollback != Rollback::Yes && rollback != Rollback::Derived) {
                        data.Error(StartupCode::DE7, "Z6",
                                   subject + " is zone C in rollback-enabled schedule '" + s.name + "' and writes '" +
                                       NameOf(data, key) + "', which does not roll back",
                                   {name, NameOf(data, key), s.name}, {source});
                    }
                }
            }
            // Events: only in the event's domain's clock schedule (F-9).
            std::vector<AccessKey> eventKeys;
            for (const AccessKey& key : access.reads) if (key.kind == AccessKind::Event) eventKeys.push_back(key);
            for (const AccessKey& key : access.writes) if (key.kind == AccessKind::Event) eventKeys.push_back(key);
            for (const AccessKey& key : eventKeys) {
                const EventInfo& event = data.events[key.id];
                if (!s.clock || s.domain != event.domain) {
                    data.Error(StartupCode::FE1, "F-9",
                               subject + " uses event '" + event.name + "' (domain " + DomainName(event.domain) +
                                   ") in schedule '" + s.name + "', which is not that domain's clock schedule",
                               {name, event.name, s.name}, {source});
                }
            }
            // Affinity (spec E.3, X-3): of the System, then of every resource it touches.
            const auto checkAffinity = [&](const Affinity& affinity, const std::string& holder) {
                if (affinity.IsAny() || affinity.role == s.runner) return;
                const bool main = affinity.role == kMainRole;
                data.Error(main ? StartupCode::XE2 : StartupCode::XE3, StartupCode::DE10, "X-3",
                           holder + " needs thread role '" + affinity.role + "' but schedule '" + s.name +
                               "' runs on '" + s.runner + "'",
                           {name, s.name}, {source});
            };
            checkAffinity(system.options.affinity, subject);
            for (const AccessKey& key : TouchedKeys(access)) {
                if (key.kind == AccessKind::Resource) {
                    checkAffinity(data.resources[key.id].affinity,
                                  subject + " (through resource '" + data.resources[key.id].name + "')");
                }
            }
            if (s.executor == ExecutorKind::Parallel && zone == Zone::C && s.parallelGuardTest.empty()) {
                data.Error(StartupCode::XE1, StartupCode::DE11, "X-2",
                           "schedule '" + s.name + "' declares Parallel and holds zone C System '" + name +
                               "', but names no Serial-versus-Parallel guard test",
                           {s.name, name}, {source});
            }
        }
    }
    for (const HookRecord& hook : data.hooks) {
        const std::string subject = "hook '" + hook.name + "'";
        CheckZoneAccess(data, subject, hook.name, hook.zone, false, hook.access, SourceText(hook.source));
        if (hook.zone == Zone::C && anyRollback) {
            for (const AccessKey& key : WrittenKeys(hook.access)) {
                if (key.kind != AccessKind::Event && RollbackOf(data, key) != Rollback::Yes) {
                    data.Error(StartupCode::DE7, "Z6",
                               subject + " is zone C and writes '" + NameOf(data, key) +
                                   "', which is not rollback=Yes, in a World with a rollback-enabled schedule",
                               {hook.name, NameOf(data, key)}, {SourceText(hook.source)});
                }
            }
        }
    }
}

// D-E2: at most one writer of an Exclusive column or resource over every
// non-Startup schedule; writers are counted per System, hooks included (DI-KS 6).
void CheckExclusiveWriters(RegistryData& data) {
    std::map<AccessKey, std::vector<std::pair<std::string, std::string>>> writers;
    for (const SystemRecord& system : data.systems) {
        bool counted = false;
        for (const Mount& mount : system.options.mounts) {
            const ScheduleRecord* schedule = data.FindSchedule(mount.schedule);
            if (schedule != nullptr && !schedule->options.startup) counted = true;
        }
        if (!counted) continue;
        for (const AccessKey& key : WrittenKeys(system.access)) {
            if (key.kind == AccessKind::Component || key.kind == AccessKind::Resource) {
                writers[key].emplace_back(system.options.name, SourceText(system.source));
            }
        }
    }
    for (const HookRecord& hook : data.hooks) {
        for (const AccessKey& key : WrittenKeys(hook.access)) {
            if (key.kind == AccessKind::Component || key.kind == AccessKind::Resource) {
                writers[key].emplace_back("hook " + hook.name, SourceText(hook.source));
            }
        }
    }
    for (const auto& [key, list] : writers) {
        const WritePolicy policy =
            key.kind == AccessKind::Component ? data.components[key.id].writePolicy : data.resources[key.id].writePolicy;
        if (policy != WritePolicy::Exclusive || list.size() < 2) continue;
        std::vector<std::string> subjects{NameOf(data, key)};
        std::vector<std::string> sources;
        std::string names;
        for (const auto& [writer, source] : list) {
            subjects.push_back(writer);
            sources.push_back(source);
            names += (names.empty() ? "" : ", ") + writer;
        }
        data.Error(StartupCode::DE2, "S-11",
                   "'" + NameOf(data, key) + "' is Exclusive but has " + std::to_string(list.size()) + " writers: " + names,
                   std::move(subjects), std::move(sources));
    }
}

// D-E9: two hooks on one trigger that conflict, one of them zone C (spec D.2 step 11).
void CheckHookConflicts(RegistryData& data) {
    for (std::size_t i = 0; i < data.hooks.size(); ++i) {
        for (std::size_t j = i + 1; j < data.hooks.size(); ++j) {
            const HookRecord& a = data.hooks[i];
            const HookRecord& b = data.hooks[j];
            if (!a.componentId || a.componentId != b.componentId || a.trigger != b.trigger) continue;
            if (a.zone != Zone::C && b.zone != Zone::C) continue;
            const std::vector<AccessKey> writesA = WrittenKeys(a.access);
            const std::vector<AccessKey> writesB = WrittenKeys(b.access);
            const std::vector<AccessKey> touchedA = TouchedKeys(a.access);
            const std::vector<AccessKey> touchedB = TouchedKeys(b.access);
            std::string shared;
            for (const AccessKey& key : writesA) {
                if (std::binary_search(touchedB.begin(), touchedB.end(), key)) shared = NameOf(data, key);
            }
            for (const AccessKey& key : writesB) {
                if (std::binary_search(touchedA.begin(), touchedA.end(), key)) shared = NameOf(data, key);
            }
            if (shared.empty()) continue;
            data.Error(StartupCode::DE9, "F-11",
                       "hooks '" + a.name + "' and '" + b.name + "' on the same trigger of '" +
                           data.components[*a.componentId].name + "' conflict on '" + shared +
                           "' and one is zone C; hooks have no before / after to order them",
                       {a.name, b.name, shared}, {SourceText(a.source), SourceText(b.source)});
        }
    }
}

void CheckRunners(RegistryData& data) {
    if (data.runners.size() > 1) {
        std::vector<std::string> subjects;
        for (std::uint32_t owner : data.runnerOwners) subjects.push_back(data.OwnerName(owner));
        data.Error(StartupCode::RE6, "C.2", "two plugins install a runner; an App has one", std::move(subjects));
    }
}

void FillSystemInfos(RegistryData& data) {
    data.systemInfos.clear();
    for (const SystemRecord& system : data.systems) {
        data.systemInfos.push_back(SystemInfo{system.options.name, system.seq, system.options.zone, system.options.ingress,
                                              system.options.mounts, system.access, SourceText(system.source),
                                              data.OwnerName(system.owner)});
    }
}

} // namespace
} // namespace Detail

Engine::Base::Result<void, StartupReport> Registry::Finish() {
    Detail::RegistryData& data = *data_;
    GYO_ASSERT(!data.finished);
    data.finished = true;

    Detail::CheckPluginList(data);
    for (std::size_t i = 0; i < data.plugins.size(); ++i) {
        if (!data.plugins[i].built) continue;
        Registrar registrar(data, static_cast<std::uint32_t>(i));
        data.plugins[i].plugin->Build(registrar);
    }
    // Plugins' Finish reads final ids (spec C.2 point 4); with build errors
    // the ids are not final, so it is not called.
    if (data.errors.empty()) {
        for (std::size_t i = 0; i < data.plugins.size(); ++i) {
            if (!data.plugins[i].built) continue;
            FinishContext context(*this, static_cast<std::uint32_t>(i));
            data.plugins[i].plugin->Finish(frozen_, context);
        }
        Detail::CheckRunners(data);
    }
    Detail::CheckData(data);
    Detail::CheckSchedules(data);
    Detail::ResolveSystems(data);
    Detail::CheckSystems(data);
    Detail::CheckExclusiveWriters(data);
    Detail::CheckHookConflicts(data);
    Detail::BuildPlans(data);
    if (!data.errors.empty()) {
        data.plans.clear();
        data.ties.clear();
        return Engine::Base::Err(StartupReport(std::move(data.errors)));
    }
    Detail::FillSystemInfos(data);
    for (Plan& plan : data.plans) {
        plan.fingerprint = Detail::PlanFingerprintOf(plan);
    }
    data.frozen = true;
    return {};
}

} // namespace GYO::Kernel
