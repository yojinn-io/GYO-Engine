// Registry lifecycle, registration, access resolution and the startup checks
// that are not about order (spec C, D.2 steps 1 to 8j, G.1). Order, conflicts
// and Plans are in Scheduler.cpp.

#include "engine/kernel/detail/RegistryData.hpp"


#include "engine/base/Assert.hpp"
#include "engine/kernel/Relationship.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <utility>

namespace GYO::Kernel {

// ---- Error code names

std::string_view ToString(KernelErrorCode code) noexcept {
    switch (code) {
    case KernelErrorCode::SystemFailed: return "SystemFailed";
    case KernelErrorCode::Siblings: return "Siblings";
    case KernelErrorCode::AccessViolation: return "AccessViolation";
    case KernelErrorCode::BoundaryChange: return "BoundaryChange";
    case KernelErrorCode::PreCallChange: return "PreCallChange";
    case KernelErrorCode::StaleHandle: return "StaleHandle";
    case KernelErrorCode::AllotterExhausted: return "AllotterExhausted";
    case KernelErrorCode::StructureBroken: return "StructureBroken";
    case KernelErrorCode::HookRoundLimit: return "HookRoundLimit";
    case KernelErrorCode::ScheduleRunning: return "ScheduleRunning";
    case KernelErrorCode::UnknownSchedule: return "UnknownSchedule";
    case KernelErrorCode::ExtractNotNestable: return "ExtractNotNestable";
    case KernelErrorCode::HookRunsSchedule: return "HookRunsSchedule";
    case KernelErrorCode::ClosureRunsSchedule: return "ClosureRunsSchedule";
    case KernelErrorCode::NestedRunAborted: return "NestedRunAborted";
    case KernelErrorCode::DynamicInsertRefused: return "DynamicInsertRefused";
    case KernelErrorCode::AllottedIdAlive: return "AllottedIdAlive";
    case KernelErrorCode::NoAllotter: return "NoAllotter";
    case KernelErrorCode::ClosureChangesMembership: return "ClosureChangesMembership";
    case KernelErrorCode::HookChangesMembership: return "HookChangesMembership";
    case KernelErrorCode::ImmediateChangeAborted: return "ImmediateChangeAborted";
    case KernelErrorCode::HashMissing: return "HashMissing";
    }
    return "Unknown";
}

std::vector<const KernelError*> KernelError::Leaves() const {
    std::vector<const KernelError*> out;
    if (code != KernelErrorCode::Siblings) {
        out.push_back(this);
        return out;
    }
    for (const KernelError& sibling : siblings) {
        for (const KernelError* leaf : sibling.Leaves()) out.push_back(leaf);
    }
    return out;
}

bool KernelError::Has(KernelErrorCode wanted) const {
    for (const KernelError* leaf : Leaves()) {
        if (leaf->code == wanted) return true;
    }
    return false;
}

namespace {
void AppendText(const KernelError& error, std::string& out, const std::string& indent) {
    out += indent;
    out += std::string(ToString(error.code));
    if (!error.rule.empty()) out += " [" + error.rule + "]";
    out += ": " + error.message;
    if (!error.detail.empty()) out += " (" + error.detail + ")";
    if (!error.world.empty()) out += " world=" + error.world;
    out += "\n";
    for (const std::string& layer : error.context) out += indent + "  at " + layer + "\n";
    for (const KernelError& sibling : error.siblings) AppendText(sibling, out, indent + "  ");
}
} // namespace

std::string KernelError::Text() const {
    std::string out;
    AppendText(*this, out, "");
    return out;
}

std::string_view ToString(StartupReportCode code) noexcept {
    switch (code) {
    case StartupReportCode::StartupFailed: return "StartupFailed";
    }
    return "Unknown";
}

// DI-KS 15: every StartupCode has its text, so the guard that lists the codes by
// their text ("DI-KS 15: the StartupCode values ...", RegistryTests.cpp) sees
// every code. A value without a case is a build error, not a warning.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(error : 4062)
#endif
std::string_view ToString(StartupCode code) noexcept {
    switch (code) {
    case StartupCode::DE1: return "D-E1";
    case StartupCode::DE3: return "D-E3";
    case StartupCode::DE4: return "D-E4";
    case StartupCode::DE5: return "D-E5";
    case StartupCode::DE6: return "D-E6";
    case StartupCode::DE7: return "D-E7";
    case StartupCode::DE14: return "D-E14";
    case StartupCode::DE17: return "D-E17";
    case StartupCode::DE19: return "D-E19";
    case StartupCode::DE20: return "D-E20";
    case StartupCode::RE2: return "R-E2";
    case StartupCode::RE4: return "R-E4";
    case StartupCode::RE5: return "R-E5";
    case StartupCode::RE7: return "R-E7";
    }
    return "Unknown";
}
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

StartupReport::StartupReport(std::vector<StartupError> found)
    : code(StartupReportCode::StartupFailed),
      message("the Product cannot start: " + std::to_string(found.size()) + " startup error(s)"),
      errors(std::move(found)) {
    for (const StartupError& error : errors) {
        if (!detail.empty()) detail += "\n";
        detail += std::string(ToString(error.code));
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

std::size_t Plan::NonDataflowEdgesExtended() const noexcept {
    return static_cast<std::size_t>(std::count_if(declaredEdges.begin(), declaredEdges.end(),
                                                  [](const DeclaredEdge& e) { return !e.dataflowExtended; }));
}

namespace Detail {

std::string RegistryData::OwnerName(std::uint32_t owner) const {
    if (owner == kKernelOwner) return kKernelOwnerName;
    return owner < owners.size() ? owners[owner].name : std::string("?");
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
    errors.push_back(StartupError{code, std::move(rule), std::move(message), std::move(subjects), std::move(sources)});
}

std::string SourceText(const std::source_location& source) {
    return std::string(source.file_name()) + ":" + std::to_string(source.line());
}

namespace {

const char* RoleName(TypeRole role) noexcept {
    switch (role) {
    case TypeRole::Component: return "component";
    case TypeRole::Resource: return "resource";
    case TypeRole::Message: return "message";
    case TypeRole::SetTag: return "named set";
    case TypeRole::Event: return "event";
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
        // Same role, different shape: the key cannot be one type (R-E5). Anything else is a second registration.
        if (existing->role == role && (existing->size != size || existing->align != align)) {
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

    void Parameter(std::size_t position) override { position_ = position; }

    void Component(TypeKey type, AccessMode mode) override {
        if (const auto id = Lookup(type, TypeRole::Component, "component")) {
            (mode == AccessMode::Read ? access.reads : access.writes).push_back({AccessKind::Component, *id});
        }
    }
    void Resource(TypeKey type, AccessMode mode) override {
        if (const auto id = Lookup(type, TypeRole::Resource, "resource")) {
            (mode == AccessMode::Read ? access.reads : access.writes).push_back({AccessKind::Resource, *id});
        }
    }
    void Message(TypeKey type, AccessMode mode) override {
        if (const auto id = Lookup(type, TypeRole::Message, "message")) {
            (mode == AccessMode::Read ? access.reads : access.writes).push_back({AccessKind::Message, *id});
        }
    }
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
    void DeferredInsert(TypeKey component) override {
        if (const auto id = Lookup(component, TypeRole::Component, "component")) {
            access.deferredWrites.push_back(*id);
            access.deferredInserts.push_back(*id);
        }
    }
    void Despawn() override { access.despawn = true; }
    void Spawn() override { access.spawn = true; }
    void UntypedCommands() override { access.untypedCommands = true; }
    void SetRead(TypeKey setTag) override {
        if (const auto id = Lookup(setTag, TypeRole::SetTag, "named set")) access.setReads.push_back(*id);
    }
    void Local() override { access.local = true; }
    void WholeWorld() override { access.exclusive = true; }
    void AnnotatedWrite(TypeKey type) override {
        const TypeEntry* entry = data_.FindType(type);
        if (entry != nullptr && entry->role == TypeRole::Component) {
            access.annotatedWrites.push_back({AccessKind::Component, entry->id});
        } else if (const auto id = Lookup(type, TypeRole::Resource, "component or resource")) {
            access.annotatedWrites.push_back({AccessKind::Resource, *id});
        }
    }
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
    SortUnique(access.deferredInserts);
    SortUnique(access.setReads);
    SortUnique(access.extractReads);
    SortUnique(access.annotatedWrites);
}

} // namespace

} // namespace Detail

// ---- Registrar

ComponentId Registrar::RegisterComponent(TypeKey key, Shape shape, ComponentOptions options, Detail::MakeColumnFn make) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<ComponentId>(data.components.size());
    if (!Detail::ClaimType(data, key, shape.size, shape.align, Detail::TypeRole::Component, id, owner_, options.name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.components.push_back(ColumnInfo{std::move(options.name), shape.size, shape.align, shape.trivialRelocate,
                                         data.OwnerName(owner_)});
    data.columnMakers.push_back(make);
    return id;
}

ComponentId TypeKeyCollisionProbe::RegisterComponent(Registrar& registrar, TypeKey key, std::size_t size,
                                                     std::size_t align, ComponentOptions options) {
    return registrar.RegisterComponent(key, Registrar::Shape{size, align, true}, std::move(options), nullptr);
}

ResourceId Registrar::RegisterResource(TypeKey key, Shape shape, ResourceOptions options, Detail::MakeResourceFn make) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<ResourceId>(data.resources.size());
    if (!Detail::ClaimType(data, key, shape.size, shape.align, Detail::TypeRole::Resource, id, owner_, options.name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.resources.push_back(ResourceInfo{std::move(options.name), shape.size, shape.align, options.threadBound,
                                          data.OwnerName(owner_)});
    data.resourceMakers.push_back(make);
    return id;
}

MessageTypeId Registrar::RegisterMessage(TypeKey key, Shape shape, MessageOptions options, Detail::MakeMessagesFn make) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<MessageTypeId>(data.messages.size());
    if (!Detail::ClaimType(data, key, shape.size, shape.align, Detail::TypeRole::Message, id, owner_, options.name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    // A buffer that keeps nothing would drop every record it is given: a Programmer Error.
    GYO_ASSERT(options.capacity >= 1);
    data.messages.push_back(
        MessageInfo{std::move(options.name), data.OwnerName(owner_), options.capacity, {}, {}});
    data.messageMakers.push_back(make);
    return id;
}

EventTypeId Registrar::RegisterEvent(TypeKey key, Shape shape, EventOptions options) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<EventTypeId>(data.events.size());
    if (!Detail::ClaimType(data, key, shape.size, shape.align, Detail::TypeRole::Event, id, owner_, options.name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.events.push_back(EventInfo{std::move(options.name), data.OwnerName(owner_)});
    return id;
}

void Registrar::RegisterObserver(TypeKey event, ObserverTrigger trigger, TypeKey observed, ObserverOptions options,
                                 Detail::ResolveFn resolve, Detail::RunFn run, std::source_location source) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    for (const Detail::ObserverRecord& existing : data.observers) {
        // The observers of one trigger run in name order: a name names one observer.
        if (existing.name == options.name) {
            data.Error(StartupCode::RE2, "B.1",
                       "observer name '" + options.name + "' registered twice (observers run in name order)",
                       {data.OwnerName(existing.owner), data.OwnerName(owner_), options.name},
                       {Detail::SourceText(existing.source), Detail::SourceText(source)});
            break;
        }
    }
    data.observers.push_back(Detail::ObserverRecord{std::move(options.name), trigger, event, observed, resolve,
                                                    run, source, owner_, std::nullopt, {}});
}

EffectId Registrar::Effect(std::string name) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    for (std::size_t i = 0; i < data.effects.size(); ++i) {
        if (data.effects[i].name == name) {
            data.Error(StartupCode::RE2, "R-6",
                       "effect '" + name + "' registered twice (first by " + data.OwnerName(data.effects[i].owner) +
                           ", again by " + data.OwnerName(owner_) + ")",
                       {data.OwnerName(data.effects[i].owner), data.OwnerName(owner_), name});
            return static_cast<EffectId>(i);
        }
    }
    data.effects.push_back(Detail::EffectRecord{std::move(name), owner_});
    return static_cast<EffectId>(data.effects.size() - 1);
}

SetId Registrar::RegisterNamedSet(TypeKey key, std::string name) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    const auto id = static_cast<SetId>(data.namedSets.size());
    if (!Detail::ClaimType(data, key, 0, 0, Detail::TypeRole::SetTag, id, owner_, name)) {
        const Detail::TypeEntry* existing = data.FindType(key);
        return existing != nullptr ? existing->id : 0;
    }
    data.namedSets.push_back(Detail::NamedSetRecord{std::move(name), key, owner_, {}, {}, {}});
    return id;
}

void Registrar::AddSetMember(TypeKey set, TypeKey component) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    for (Detail::NamedSetRecord& record : data.namedSets) {
        if (record.key == set) {
            record.pendingMembers.push_back(component);
            record.memberOwners.push_back(owner_);
            return;
        }
    }
    data.Error(StartupCode::RE4, "R-6", data.OwnerName(owner_) + " adds a member to a named set that is not registered",
               {data.OwnerName(owner_)});
}

void Registrar::DeclareAllotter(TypeKey resource, Detail::AllotterOps ops) {
    GYO_ASSERT(data_->building);
    data_->allotterDeclarations.push_back(resource);
    data_->allotterDeclarationOps.push_back(ops);
    data_->allotterOwners.push_back(owner_);
}

void Registrar::Schedule(ScheduleOptions options) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    if (const Detail::ScheduleRecord* existing = data.FindSchedule(options.name)) {
        data.Error(StartupCode::RE2, "C.2", "schedule '" + options.name + "' registered twice (first by " +
                                               data.OwnerName(existing->owner) + ", again by " + data.OwnerName(owner_) + ")",
                   {data.OwnerName(existing->owner), data.OwnerName(owner_), options.name});
        return;
    }
    data.schedules.push_back(Detail::ScheduleRecord{std::move(options), owner_});
}

void Registrar::InsertStage(std::string_view schedule, std::string_view anchor, StagePlace place, std::string stage) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    for (Detail::ScheduleRecord& record : data.schedules) {
        if (record.options.name != schedule) continue;
        std::vector<std::string>& stages = record.options.stages;
        const auto at = std::find(stages.begin(), stages.end(), anchor);
        if (at == stages.end()) {
            data.Error(StartupCode::DE1, "C.2",
                       data.OwnerName(owner_) + " inserts stage '" + stage + "' next to stage '" + std::string(anchor) +
                           "', which schedule '" + std::string(schedule) + "' does not have",
                       {data.OwnerName(owner_), std::string(schedule)});
            return;
        }
        stages.insert(place == StagePlace::Before ? at : at + 1, std::move(stage));
        return;
    }
    data.Error(StartupCode::DE1, "C.2",
               data.OwnerName(owner_) + " inserts stage '" + stage + "' into schedule '" + std::string(schedule) +
                   "', which no earlier owner registered",
               {data.OwnerName(owner_), std::string(schedule)});
}

void Registrar::Set(SetOptions options, std::vector<ConditionRef> conditions) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    for (const Detail::SetRecord& existing : data.sets) {
        if (existing.options.schedule == options.schedule && existing.options.name == options.name) {
            data.Error(StartupCode::RE2, "C.1",
                       "set '" + options.name + "' of schedule '" + options.schedule + "' registered twice",
                       {data.OwnerName(existing.owner), data.OwnerName(owner_), options.name});
            return;
        }
    }
    data.sets.push_back(Detail::SetRecord{std::move(options), std::move(conditions), owner_, {}, {}});
}

RegistrationSeq Registrar::RegisterSystem(SystemOptions options, Detail::ResolveFn resolve, bool extractOnly,
                                          Detail::RunFn run, std::source_location source) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    const auto seq = static_cast<RegistrationSeq>(data.systems.size());
    for (const Detail::SystemRecord& existing : data.systems) {
        // A tombstone's name is free again (declaration 3.2 rule 7).
        if (!existing.removed && existing.options.name == options.name) {
            data.Error(StartupCode::RE2, "B.1",
                       "System name '" + options.name + "' registered twice (before / after and reports refer to "
                                                        "Systems by name)",
                       {data.OwnerName(existing.owner), data.OwnerName(owner_), options.name},
                       {Detail::SourceText(existing.source), Detail::SourceText(source)});
            break;
        }
    }
    data.systems.push_back(
        Detail::SystemRecord{std::move(options), resolve, extractOnly, run, source, owner_, seq, {}, {}, false});
    return seq;
}

bool Registrar::RemoveSystem(std::string_view name) {
    GYO_ASSERT(data_->building);
    for (Detail::SystemRecord& system : data_->systems) {
        if (!system.removed && system.options.name == name) {
            system.removed = true;
            return true;
        }
    }
    return false;
}

void Registrar::RegisterHook(TypeKey component, HookTrigger trigger, HookOptions options, Detail::ResolveFn resolve,
                             Detail::RunFn run, std::source_location source) {
    GYO_ASSERT(data_->building);
    data_->hooks.push_back(Detail::HookRecord{std::move(options.name), component, trigger, resolve, run,
                                              source, owner_, std::nullopt, {}});
}

void Registrar::RegisterInitialValue(TypeKey target, TypeKey request, Detail::InitialValueFn put,
                                     std::source_location source) {
    GYO_ASSERT(data_->building);
    data_->initialValues.push_back(Detail::InitialValueRecord{target, request, put, source, owner_});
}

// ---- FrozenRegistry

std::uint64_t FrozenRegistry::Version() const noexcept { return data_->version; }
std::span<const HookInfo> FrozenRegistry::Hooks() const noexcept { return data_->hookInfos; }
std::span<const EventInfo> FrozenRegistry::Events() const noexcept { return data_->events; }
std::span<const ObserverInfo> FrozenRegistry::Observers() const noexcept { return data_->observerInfos; }
std::span<const InitialValueInfo> FrozenRegistry::InitialValues() const noexcept { return data_->initialValueInfos; }
std::span<const ScheduleInfo> FrozenRegistry::Schedules() const noexcept { return data_->scheduleInfos; }
std::span<const SetInfo> FrozenRegistry::Sets() const noexcept { return data_->setInfos; }

std::span<const OrderEdge> FrozenRegistry::OrderGraph(std::string_view schedule) const noexcept {
    for (const Detail::ScheduleOrder& order : data_->orders) {
        if (order.schedule == schedule) return order.edges;
    }
    return {};
}

std::span<const ColumnInfo> FrozenRegistry::Components() const noexcept { return data_->components; }
std::span<const ResourceInfo> FrozenRegistry::Resources() const noexcept { return data_->resources; }
std::span<const MessageInfo> FrozenRegistry::Messages() const noexcept { return data_->messages; }
std::span<const SystemInfo> FrozenRegistry::Systems() const noexcept { return data_->systemInfos; }
std::span<const Plan> FrozenRegistry::Plans() const noexcept { return data_->plans; }
std::span<const TieBreak> FrozenRegistry::TieBreaks() const noexcept { return data_->ties; }

// The live System of that name; else the latest tombstone of it.
const SystemInfo* FrozenRegistry::SystemNamed(std::string_view name) const noexcept {
    const SystemInfo* tombstone = nullptr;
    for (const SystemInfo& info : data_->systemInfos) {
        if (info.name != name) continue;
        if (!info.removed) return &info;
        tombstone = &info;
    }
    return tombstone;
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

std::optional<MessageTypeId> FrozenRegistry::FindMessage(TypeKey key) const noexcept {
    const Detail::TypeEntry* entry = data_->FindType(key);
    return entry != nullptr && entry->role == Detail::TypeRole::Message ? std::optional<MessageTypeId>(entry->id)
                                                                       : std::nullopt;
}

std::optional<EventTypeId> FrozenRegistry::FindEvent(TypeKey key) const noexcept {
    const Detail::TypeEntry* entry = data_->FindType(key);
    return entry != nullptr && entry->role == Detail::TypeRole::Event ? std::optional<EventTypeId>(entry->id) : std::nullopt;
}

Fingerprint FrozenRegistry::RegistryFingerprint() const {
    return Detail::RegistryFingerprintOf(*data_);
}


// ---- Registry

namespace Detail {
namespace {

// The kernel's own registrations come first (ResourceId 0, EffectId 0), so they
// never shift with the owners.
void AddKernelRegistrations(RegistryData& data) {
    data.kernelSettings = static_cast<ResourceId>(data.resources.size());
    data.types.push_back(TypeEntry{KeyOf<KernelSettings>(), sizeof(KernelSettings), alignof(KernelSettings),
                                   TypeRole::Resource, data.kernelSettings, kKernelOwner});
    data.resources.push_back(ResourceInfo{"KernelSettings", sizeof(KernelSettings), alignof(KernelSettings),
                                          false, kKernelOwnerName});
    data.resourceMakers.push_back(&MakeResource<KernelSettings>);
    data.effects.push_back(EffectRecord{kDiagnosticsEffect, kKernelOwner});
    // The relationship pair (DI-KS 29; declaration v8-arch-gyo2-kernel-relationship 5.1):
    // every World has ChildOf and Children, as every Bevy World has bevy_ecs's.
    auto component = [&](TypeKey key, std::size_t size, std::size_t align, bool trivial, const char* name,
                         MakeColumnFn make) {
        const auto id = static_cast<ComponentId>(data.components.size());
        data.types.push_back(TypeEntry{key, size, align, TypeRole::Component, id, kKernelOwner});
        data.components.push_back(ColumnInfo{name, size, align, trivial, kKernelOwnerName});
        data.columnMakers.push_back(make);
        return id;
    };
    data.childOf = component(KeyOf<ChildOf>(), sizeof(ChildOf), alignof(ChildOf), std::is_trivially_copyable_v<ChildOf>,
                             "ChildOf", &MakeColumn<ChildOf>);
    data.children = component(KeyOf<Children>(), sizeof(Children), alignof(Children),
                              std::is_trivially_copyable_v<Children>, "Children", &MakeColumn<Children>);
}

} // namespace
} // namespace Detail

Registry::Registry() : data_(std::make_unique<Detail::RegistryData>()), frozen_(*data_) {
    Detail::AddKernelRegistrations(*data_);
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

void CheckData(RegistryData& data) {
    // Allotter.
    data.allotter.reset();
    data.allotterOps = AllotterOps{};
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
            data.allotterOps = data.allotterDeclarationOps[i];
        }
    }
    // Named sets.
    for (NamedSetRecord& set : data.namedSets) {
        set.members.clear();
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
    }
}

// Registrar::InitialValue: resolves the two components of every declaration and
// leaves the valid ones in (requestId, targetId) order. The kernel has no writer
// rule: a Module that asks who writes what reads the declarations from the
// read-only registry (InitialValues).
void CheckInitialValues(RegistryData& data) {
    std::vector<InitialValueRecord> valid;
    for (InitialValueRecord& record : data.initialValues) {
        const std::string declarer = data.OwnerName(record.owner);
        const std::string source = SourceText(record.source);
        const TypeEntry* target = data.FindType(record.target);
        const TypeEntry* request = data.FindType(record.request);
        if (target == nullptr || target->role != TypeRole::Component) {
            data.Error(StartupCode::RE4, "InitialValue",
                       declarer + " declares the initial value of a type that is not a registered component", {declarer},
                       {source});
            continue;
        }
        const ColumnInfo& targetInfo = data.components[target->id];
        if (request == nullptr || request->role != TypeRole::Component) {
            data.Error(StartupCode::RE4, "InitialValue",
                       "the initial value of component '" + targetInfo.name +
                           "' comes from a type that is not a registered component",
                       {targetInfo.name, declarer}, {source});
            continue;
        }
        const ColumnInfo& requestInfo = data.components[request->id];
        if (target->owner != record.owner) {
            data.Error(StartupCode::RE4, "InitialValue",
                       declarer + " declares the initial value of component '" + targetInfo.name + "', which " +
                           targetInfo.owner + " owns; only the owner of a component declares its initial value",
                       {targetInfo.name, declarer, targetInfo.owner}, {source});
            continue;
        }
        const bool repeated = std::any_of(valid.begin(), valid.end(), [&](const InitialValueRecord& other) {
            return other.targetId == target->id && other.requestId == request->id;
        });
        if (repeated) {
            data.Error(StartupCode::RE2, "InitialValue",
                       "the initial value of component '" + targetInfo.name + "' from '" + requestInfo.name +
                           "' is declared twice",
                       {targetInfo.name, requestInfo.name}, {source});
            continue;
        }
        record.targetId = target->id;
        record.requestId = request->id;
        valid.push_back(record);
    }
    std::sort(valid.begin(), valid.end(), [](const InitialValueRecord& a, const InitialValueRecord& b) {
        return a.requestId != b.requestId ? a.requestId < b.requestId : a.targetId < b.targetId;
    });
    data.initialValues = std::move(valid);
}

void CheckSchedules(RegistryData& data) {
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
        if (s.hookRoundLimit && *s.hookRoundLimit == 0) {
            data.Error(StartupCode::DE20, "F-12", "schedule '" + s.name + "' declares a hook round limit of 0 (at least 1)",
                       {s.name});
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
        if (s.executor == ExecutorKind::Parallel) {
            data.Error(StartupCode::DE17, "LIM-2", "schedule '" + s.name + "' declares Parallel; v1 has only Serial (LIM-2)",
                       {s.name});
        }
    }
}

// A run condition's access joins its System's (spec S-4, E.4): every read,
// filter, set read and the extract flag, so the System's checks
// (D-E14) and the conflict graph see it. Conditions are read-only
// by type (S-5), so nothing is written.
void MergeCondition(SystemResolver& into, const SystemResolver& condition) {
    into.access.reads.insert(into.access.reads.end(), condition.access.reads.begin(), condition.access.reads.end());
    into.access.membershipReads.insert(into.access.membershipReads.end(), condition.access.membershipReads.begin(),
                                       condition.access.membershipReads.end());
    into.access.setReads.insert(into.access.setReads.end(), condition.access.setReads.begin(),
                                condition.access.setReads.end());
    into.access.extractReads.insert(into.access.extractReads.end(), condition.access.extractReads.begin(),
                                    condition.access.extractReads.end());
    into.access.changeFilters = into.access.changeFilters || condition.access.changeFilters;
    into.access.local = into.access.local || condition.access.local;
    into.usesExtract = into.usesExtract || condition.usesExtract;
}

// Event cursor declaration 3.2 (DI-KS 25): reads of a message by the System
// itself (condition empty) or by one of the mount's run conditions, each a
// reader of its own. Two MessageReader<M> of one signature are one reader.
void AddMessageReaders(RegistryData& data, const std::vector<AccessKey>& reads, const SystemRecord& system,
                       const Mount& mount, const std::string& condition) {
    for (const AccessKey& key : reads) {
        if (key.kind != AccessKind::Message) continue;
        std::vector<MessageReaderInfo>& readers = data.messages[key.id].readers;
        MessageReaderInfo reader{system.options.name, mount.schedule, condition};
        if (std::find(readers.begin(), readers.end(), reader) == readers.end()) readers.push_back(std::move(reader));
    }
}

void ResolveSystems(RegistryData& data) {
    // Derived: cleared, then filled in (registrationSeq, mount, condition) order.
    for (MessageInfo& info : data.messages) {
        info.readers.clear();
        info.writers.clear();
    }
    for (SystemRecord& system : data.systems) {
        // Derived: cleared, so a later BuildPlans does not add to the last one's (declaration 3.2 rule 2).
        system.access = AccessSet{};
        system.effects.clear();
        if (system.removed) continue;
        const std::string subject = "System '" + system.options.name + "'";
        SystemResolver resolver(data, subject, SourceText(system.source));
        system.resolve(resolver);
        const std::vector<AccessKey> ownReads = resolver.access.reads;
        for (const Mount& mount : system.options.mounts) {
            AddMessageReaders(data, ownReads, system, mount, {});
            // H-6 (DI-KS 35): a run condition with Extract parameters is legal only on a
            // mount in an extract schedule, as for the System's own parameters (CheckSystems).
            // An unknown schedule is CheckSystems' D-E1.
            auto conditionExtractOutside = [&](const SystemResolver& condition, const std::string& what) {
                if (!condition.usesExtract) return;
                const ScheduleRecord* record = data.FindSchedule(mount.schedule);
                if (record == nullptr || record->options.extract) return;
                data.Error(StartupCode::DE7, "H-6",
                           what + " has Extract parameters but schedule '" + mount.schedule + "' is not an extract schedule",
                           {system.options.name, mount.schedule}, {SourceText(system.source)});
            };
            for (const ConditionRef& condition : mount.conditions) {
                if (condition.Resolver() == nullptr) continue;
                SystemResolver conditionResolver(data, subject + " run condition '" + condition.Name() + "'",
                                                 SourceText(system.source));
                condition.Resolver()(conditionResolver);
                AddMessageReaders(data, conditionResolver.access.reads, system, mount, condition.Name());
                conditionExtractOutside(conditionResolver, subject + " run condition '" + condition.Name() + "'");
                MergeCondition(resolver, conditionResolver);
            }
            // Set conditions join every member's reads (spec E.4).
            for (const std::string& setName : mount.sets) {
                for (SetRecord& set : data.sets) {
                    if (set.options.schedule != mount.schedule || set.options.name != setName) continue;
                    for (const ConditionRef& condition : set.conditions) {
                        if (condition.Resolver() == nullptr) continue;
                        SystemResolver conditionResolver(data, "set '" + setName + "' run condition '" + condition.Name() + "'",
                                                         SourceText(system.source));
                        condition.Resolver()(conditionResolver);
                        AddMessageReaders(data, conditionResolver.access.reads, system, mount, condition.Name());
                        conditionExtractOutside(conditionResolver,
                                                "set '" + setName + "' run condition '" + condition.Name() + "' (on " + subject + ")");
                        MergeCondition(resolver, conditionResolver);
                    }
                }
            }
        }
        AccessSet& access = resolver.access;
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
        if (resolver.usesExtract && (data.extractSource == nullptr || !data.extractSource->frozen)) {
            data.Error(StartupCode::DE14, "H-6",
                       subject + " extracts from a source Registry that " +
                           (data.extractSource == nullptr ? std::string("is not set")
                                                      : std::string("has not built its Plans yet")),
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
        for (const Mount& mount : system.options.mounts) {
            for (const AccessKey& key : system.access.writes) {
                if (key.kind != AccessKind::Message) continue;
                data.messages[key.id].writers.push_back(MessageWriterInfo{system.options.name, mount.schedule});
            }
        }
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
        hook.componentId.reset();
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
        if (resolver.access.spawn) {
            if (data.allotter) {
                resolver.access.writes.push_back({AccessKind::Resource, *data.allotter});
            } else {
                data.Error(StartupCode::DE3, "A.1", subject + " spawns entities, but this World declares no allotter",
                           {hook.name}, {SourceText(hook.source)});
            }
        }
        if (resolver.usesExtract) {
            data.Error(StartupCode::DE7, "H-6", subject + " has Extract parameters; only extract-schedule Systems may",
                       {hook.name}, {SourceText(hook.source)});
        }
        Normalize(resolver.access);
        hook.access = std::move(resolver.access);
        for (const AccessKey& key : hook.access.writes) {
            if (key.kind != AccessKind::Message) continue;
            data.messages[key.id].writers.push_back(MessageWriterInfo{hook.name, {}, true, false});
        }
    }
    // Observers (event cursor declaration 3.3): Systems that run where their
    // event is triggered; each is a message reader of its own (3.2).
    for (std::size_t o = 0; o < data.observers.size(); ++o) {
        ObserverRecord& observer = data.observers[o];
        const std::string subject = "observer '" + observer.name + "'";
        observer.observedId.reset();
        const TypeRole wanted = observer.trigger == ObserverTrigger::Event ? TypeRole::Event : TypeRole::Component;
        const TypeEntry* entry = data.FindType(observer.observed);
        if (entry == nullptr || entry->role != wanted) {
            data.Error(StartupCode::RE4, "R-6",
                       subject + " observes a type that is not a registered " +
                           (wanted == TypeRole::Event ? "event" : "component"),
                       {observer.name}, {SourceText(observer.source)});
        } else {
            observer.observedId = entry->id;
        }
        SystemResolver found(data, subject, SourceText(observer.source));
        observer.resolve(found);
        for (SetId set : found.access.setReads) {
            for (ComponentId member : data.namedSets[set].members) {
                found.access.reads.push_back({AccessKind::Component, member});
            }
        }
        if (found.access.spawn) {
            if (data.allotter) {
                found.access.writes.push_back({AccessKind::Resource, *data.allotter});
            } else {
                data.Error(StartupCode::DE3, "A.1", subject + " spawns entities, and the World declares no allotter",
                           {observer.name}, {SourceText(observer.source)});
            }
        }
        if (found.usesExtract) {
            data.Error(StartupCode::DE7, "H-6", subject + " has Extract parameters; only extract-schedule Systems may",
                       {observer.name}, {SourceText(observer.source)});
        }
        Normalize(found.access);
        for (const AccessKey& key : found.access.reads) {
            if (key.kind != AccessKind::Message) continue;
            data.messages[key.id].readers.push_back(MessageReaderInfo{observer.name, {}, {}, true});
        }
        observer.access = std::move(found.access);
        for (const AccessKey& key : observer.access.writes) {
            if (key.kind != AccessKind::Message) continue;
            data.messages[key.id].writers.push_back(MessageWriterInfo{observer.name, {}, false, true});
        }
    }
}

void CheckSystems(RegistryData& data) {
    for (const SystemRecord& system : data.systems) {
        if (system.removed) continue;
        const std::string& name = system.options.name;
        const std::string subject = "System '" + name + "'";
        const std::string source = SourceText(system.source);
        if (system.options.mounts.empty()) {
            data.Error(StartupCode::DE1, "B.1", subject + " has no mount, so it never runs", {name}, {source});
        }
        std::set<std::string> mountedSchedules;
        for (const Mount& mount : system.options.mounts) {
            const ScheduleRecord* record = data.FindSchedule(mount.schedule);
            if (record == nullptr) {
                data.Error(StartupCode::DE1, "D.2", subject + " is mounted in schedule '" + mount.schedule +
                                                        "', which this Product does not have",
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
            if (system.extractOnly && !s.extract) {
                data.Error(StartupCode::DE7, "H-6",
                           subject + " has Extract parameters but schedule '" + s.name + "' is not an extract schedule",
                           {name, s.name}, {source});
            }
        }
    }
}

// The read-only view's lists (FrozenRegistry; DI-KS 25), made again at every BuildPlans.
void FillSystemInfos(RegistryData& data) {
    data.systemInfos.clear();
    for (const SystemRecord& system : data.systems) {
        data.systemInfos.push_back(SystemInfo{system.options.name, system.seq,
                                              system.options.mounts, system.access, SourceText(system.source),
                                              data.OwnerName(system.owner), system.effects, system.removed});
    }
    data.hookInfos.clear();
    for (const HookRecord& hook : data.hooks) {
        data.hookInfos.push_back(HookInfo{hook.name, hook.trigger, hook.componentId.value_or(0), hook.access,
                                          SourceText(hook.source), data.OwnerName(hook.owner)});
    }
    data.observerInfos.clear();
    data.observerOrder.clear();
    for (std::size_t o = 0; o < data.observers.size(); ++o) {
        const ObserverRecord& observer = data.observers[o];
        data.observerInfos.push_back(ObserverInfo{observer.name, observer.trigger, observer.observedId.value_or(0),
                                                  observer.access, SourceText(observer.source),
                                                  data.OwnerName(observer.owner)});
        data.observerOrder.push_back(static_cast<std::uint32_t>(o));
    }
    std::sort(data.observerOrder.begin(), data.observerOrder.end(), [&data](std::uint32_t a, std::uint32_t b) {
        return data.observers[a].name < data.observers[b].name;
    });
    data.initialValueInfos.clear();
    for (const InitialValueRecord& record : data.initialValues) {
        data.initialValueInfos.push_back(InitialValueInfo{record.targetId, record.requestId, data.OwnerName(record.owner),
                                                          SourceText(record.source)});
    }
    data.scheduleInfos.clear();
    for (const ScheduleRecord& schedule : data.schedules) {
        data.scheduleInfos.push_back(ScheduleInfo{schedule.options, data.OwnerName(schedule.owner)});
    }
    data.setInfos.clear();
    for (const SetRecord& set : data.sets) {
        std::vector<std::string> conditions;
        for (const ConditionRef& condition : set.conditions) conditions.push_back(condition.Name());
        data.setInfos.push_back(SetInfo{set.options, std::move(conditions), data.OwnerName(set.owner)});
    }
}

// Declaration 3.2 rule 6: a later round may add components, resources, events,
// named sets, schedules (not Startup), stages, System sets, effects and
// Systems, remove Systems and mount new Systems on existing schedules. What
// would change something the previous version already built is R-E7: the round
// is dropped and the Worlds go on (a mistyped command does not stop the game).
// Each message starts with the kind refused. Runs before CheckInitialValues,
// which sorts the declarations.
void CheckLaterRound(RegistryData& data) {
    const RoundBase& base = data.base;
    const auto refuse = [&](const char* kind, std::string message, std::vector<std::string> subjects,
                            std::vector<std::string> sources = {}) {
        data.Error(StartupCode::RE7, "L5",
                   std::string(kind) + ": " + std::move(message) + " (only the first round may; the round is dropped)",
                   std::move(subjects), std::move(sources));
    };
    for (std::size_t h = base.hooks; h < data.hooks.size(); ++h) {
        const HookRecord& hook = data.hooks[h];
        if (hook.componentId && *hook.componentId < base.components) {
            // Its cells that exist ran no OnAdd but would run OnRemove (Bevy refuses too).
            refuse("hook", data.OwnerName(hook.owner) + " adds hook '" + hook.name + "' to component '" +
                               data.components[*hook.componentId].name + "', which the previous version has",
                   {hook.name, data.components[*hook.componentId].name}, {SourceText(hook.source)});
        }
    }
    for (std::size_t o = base.observers; o < data.observers.size(); ++o) {
        const ObserverRecord& observer = data.observers[o];
        if (observer.trigger != ObserverTrigger::Event && observer.observedId && *observer.observedId < base.components) {
            // Like a hook: the cells that exist ran no Add observer but would run Remove ones.
            refuse("observer", data.OwnerName(observer.owner) + " adds observer '" + observer.name + "' of component '" +
                                   data.components[*observer.observedId].name + "', which the previous version has",
                   {observer.name, data.components[*observer.observedId].name}, {SourceText(observer.source)});
        }
    }
    const auto oldComponent = [&](TypeKey key) -> const TypeEntry* {
        const TypeEntry* entry = data.FindType(key);
        return entry != nullptr && entry->role == TypeRole::Component && entry->id < base.components ? entry : nullptr;
    };
    for (std::size_t i = base.initialValues; i < data.initialValues.size(); ++i) {
        const InitialValueRecord& record = data.initialValues[i];
        // An old Target: its cells that exist were never filled. An old
        // Request: the entities that have it already never get the Target.
        if (const TypeEntry* target = oldComponent(record.target)) {
            refuse("InitialValue Target", data.OwnerName(record.owner) + " declares the initial value of component '" +
                                              data.components[target->id].name + "', which the previous version has",
                   {data.components[target->id].name, data.OwnerName(record.owner)}, {SourceText(record.source)});
        }
        if (const TypeEntry* request = oldComponent(record.request)) {
            refuse("InitialValue Request", data.OwnerName(record.owner) + " declares an initial value requested by component '" +
                                               data.components[request->id].name + "', which the previous version has",
                   {data.components[request->id].name, data.OwnerName(record.owner)}, {SourceText(record.source)});
        }
    }
    for (std::size_t n = 0; n < base.namedSets && n < data.namedSets.size(); ++n) {
        const NamedSetRecord& set = data.namedSets[n];
        for (std::size_t m = base.setMembers[n]; m < set.pendingMembers.size(); ++m) {
            // Would change the access of Systems and hooks that read the set.
            refuse("set member", data.OwnerName(set.memberOwners[m]) + " adds a member to named set '" + set.name +
                                     "', which the previous version has",
                   {set.name, data.OwnerName(set.memberOwners[m])});
        }
    }
    for (std::size_t i = base.allotterDeclarations; i < data.allotterDeclarations.size(); ++i) {
        refuse("allotter", data.OwnerName(data.allotterOwners[i]) + " declares an allotter",
               {data.OwnerName(data.allotterOwners[i])});
    }
}

// What every round resolves first (named set members, the allotter, Systems'
// access sets). Derived
// fields are cleared and made again (declaration 3.2 rule 2).
void ResolveStage(RegistryData& data, bool laterRound) {
    CheckData(data);
    if (laterRound) {
        // Needs hooks' component ids: resolved here first, again by ResolveSystems.
        for (HookRecord& hook : data.hooks) {
            const TypeEntry* entry = data.FindType(hook.component);
            hook.componentId = entry != nullptr && entry->role == TypeRole::Component ? std::optional<ComponentId>(entry->id)
                                                                                       : std::nullopt;
        }
        for (ObserverRecord& observer : data.observers) {
            const TypeEntry* entry = data.FindType(observer.observed);
            observer.observedId = entry != nullptr && entry->role == TypeRole::Component &&
                                          observer.trigger != ObserverTrigger::Event
                                      ? std::optional<std::uint32_t>(entry->id)
                                      : std::nullopt;
        }
        CheckLaterRound(data);
    }
    CheckInitialValues(data);
    CheckSchedules(data);
    ResolveSystems(data);
    FillSystemInfos(data);
}

// The checks that come after resolving, the Plans (spec C.2 points 5 and 6)
// and their fingerprints. Ok when no error was found in the whole round.
bool PlanStage(RegistryData& data) {
    data.plans.clear();
    data.ties.clear();
    data.orders.clear();
    CheckSystems(data);
    BuildPlans(data);
    if (!data.errors.empty()) {
        data.plans.clear();
        data.ties.clear();
        data.orders.clear();
        return false;
    }
    for (Plan& plan : data.plans) {
        plan.fingerprint = PlanFingerprintOf(plan);
    }
    return true;
}

// A successful later round: its tables replace the Registry's, in the same
// object, so FrozenRegistry, the Worlds' table pointer and the extract-source
// pointers of other Registries stay valid (declaration 3.2 rule 1). The World
// counters belong to the object, not to a round.
// CommitPlans relies on this throwing nothing (DI-KS 38: several staged rounds are taken in together).
static_assert(std::is_nothrow_move_assignable_v<RegistryData>, "Adopt only moves");
void Adopt(RegistryData& into, RegistryData&& round) noexcept {
    const std::uint32_t liveWorlds = into.liveWorlds;
    const std::uint32_t refusingWorlds = into.refusingWorlds;
    const std::uint32_t runningWorlds = into.runningWorlds;
    into = std::move(round);
    into.liveWorlds = liveWorlds;
    into.refusingWorlds = refusingWorlds;
    into.runningWorlds = runningWorlds;
}

} // namespace
} // namespace Detail

void Registry::RequireNoWorldRunning() const {
    // While a World made from this Registry is inside a Run, its Executor holds
    // the Plan, the Systems and the initial values of this version: anything
    // the Run calls (a System, a hook, an initial value function, another
    // World's Run) would rebuild them under it (declaration 3.2 rule 4). The
    // count covers the whole call chain, not only the innermost call. Another
    // Registry's Worlds are not concerned.
    GYO_ASSERT(data_->runningWorlds == 0);
}

Detail::RegistryData& Registry::Target() noexcept {
    return pending_ ? *pending_ : *data_;
}

Engine::Base::Result<void, KernelError> Registry::OpenRound(const std::string& who) {
    GYO_ASSERT(!data_->building && !(pending_ && pending_->building));  // not inside build (rule 3)
    GYO_ASSERT(!data_->failed);  // a failed first round leaves the Registry unusable (rule 9)
    GYO_ASSERT(!staged_ && "a staged round waits for CommitPlans or DiscardRegistrations");
    RequireNoWorldRunning();
    if (data_->refusingWorlds > 0) {
        // DI-KS 26: one World that declares no dynamic insert refuses for the whole Registry.
        KernelError error(KernelErrorCode::DynamicInsertRefused,
                          who + " refused: " + std::to_string(data_->refusingWorlds) +
                              " live World(s) made from this Registry declare no dynamic insert "
                              "(WorldOptions::dynamicInsert = false); nothing was registered");
        error.rule = "DI-KS 26";
        return Engine::Base::Err(std::move(error));
    }
    if (data_->frozen && !pending_) {
        // Rule 1: a later round writes a copy; its contents replace the tables only when it builds.
        pending_ = std::make_unique<Detail::RegistryData>(*data_);
        Detail::RegistryData& round = *pending_;
        Detail::RoundBase& base = round.base;
        base.components = round.components.size();
        base.namedSets = round.namedSets.size();
        base.setMembers.clear();
        for (const Detail::NamedSetRecord& set : round.namedSets) base.setMembers.push_back(set.pendingMembers.size());
        base.schedules = round.schedules.size();
        base.systems = round.systems.size();
        base.hooks = round.hooks.size();
        base.observers = round.observers.size();
        base.initialValues = round.initialValues.size();
        base.allotterDeclarations = round.allotterDeclarations.size();
    }
    return {};
}

Engine::Base::Result<std::uint32_t, KernelError> Registry::OpenRegistrar(RegistrationOwner owner) {
    if (auto opened = OpenRound("Register by '" + owner.name + "'"); !opened) {
        return Engine::Base::Err(std::move(opened).error());
    }
    Detail::RegistryData& target = Target();
    target.registered = true;
    std::uint32_t index = 0;
    while (index < target.owners.size() && !(target.owners[index] == owner)) ++index;
    if (index == target.owners.size()) target.owners.push_back(std::move(owner));
    target.building = true;
    return index;
}

void Registry::CloseRegistrar() noexcept {
    Target().building = false;
}

Engine::Base::Result<void, KernelError> Registry::ReplaceExtractSource(const Registry& source) {
    GYO_ASSERT(data_->frozen && "ReplaceExtractSource: before the first BuildPlans, SetExtractSource");
    GYO_ASSERT(&source != this && "ReplaceExtractSource: a Registry is not its own extract source");
    if (auto opened = OpenRound("ReplaceExtractSource"); !opened) return opened;
    // The round reads the new source; the tables keep the current one until the round is taken in.
    pending_->extractSource = source.data_.get();
    pending_->extractSourceSet = true;
    pending_->registered = true;
    return {};
}

Engine::Base::Result<void, StartupReport> Registry::StagePlans() {
    GYO_ASSERT(!data_->building && !(pending_ && pending_->building));  // not inside build (rule 3)
    GYO_ASSERT(!data_->failed);
    GYO_ASSERT(pending_ && !staged_ && "StagePlans: no later round to build");
    RequireNoWorldRunning();
    // A later round: built on the copy; the tables change only when it is taken in. A round that fails, or
    // that an exception leaves, is dropped (DI-KS 38): it may read a source its caller is about to let go.
    Detail::RegistryData& round = *pending_;
    bool planned = false;
    try {
        Detail::ResolveStage(round, true);
        planned = Detail::PlanStage(round);
    } catch (...) {
        pending_.reset();
        throw;
    }
    if (!planned) {
        StartupReport report(std::move(round.errors));
        pending_.reset();
        return Engine::Base::Err(std::move(report));
    }
    round.version = data_->version + 1;
    round.registered = false;
    staged_ = true;
    return {};
}

void Registry::CommitPlans() {
    GYO_ASSERT(staged_ && "CommitPlans: no staged round");
    RequireNoWorldRunning();
    Detail::Adopt(*data_, std::move(*pending_));
    pending_.reset();
    staged_ = false;
}

Engine::Base::Result<void, StartupReport> Registry::BuildPlans() {
    GYO_ASSERT(!data_->building && !(pending_ && pending_->building));  // not inside build (rule 3)
    GYO_ASSERT(!data_->failed);
    GYO_ASSERT(!staged_ && "BuildPlans: a staged round waits for CommitPlans or DiscardRegistrations");
    RequireNoWorldRunning();
    if (pending_) {
        auto staged = StagePlans();
        if (!staged) return staged;
        CommitPlans();
        return {};
    }
    Detail::RegistryData& data = *data_;
    if (data.frozen || !data.registered) return {};  // nothing registered since the last BuildPlans
    // The first round, on the tables themselves.
    data.finished = true;
    Detail::ResolveStage(data, false);
    if (!Detail::PlanStage(data)) {
        data.failed = true;
        return Engine::Base::Err(StartupReport(std::move(data.errors)));
    }
    data.registered = false;
    data.version = 1;
    data.frozen = true;
    return {};
}

void Registry::DiscardRegistrations() {
    GYO_ASSERT(!data_->building && !(pending_ && pending_->building));  // not inside build (rule 3)
    GYO_ASSERT(!data_->failed);
    RequireNoWorldRunning();
    if (pending_) {
        pending_.reset();
        staged_ = false;
        return;
    }
    if (data_->frozen || !data_->registered) return;
    // The first round writes the tables: back to a Registry with only the
    // kernel's registrations (its extract source kept).
    Detail::RegistryData fresh;
    fresh.extractSource = data_->extractSource;
    fresh.extractSourceSet = data_->extractSourceSet;
    fresh.schedulerShuffle = data_->schedulerShuffle;
    Detail::AddKernelRegistrations(fresh);
    Detail::Adopt(*data_, std::move(fresh));
}

} // namespace GYO::Kernel
