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
    case StartupCode::DE2: return "D-E2";
    case StartupCode::DE3: return "D-E3";
    case StartupCode::DE4: return "D-E4";
    case StartupCode::DE5: return "D-E5";
    case StartupCode::DE6: return "D-E6";
    case StartupCode::DE7: return "D-E7";
    case StartupCode::DE9: return "D-E9";
    case StartupCode::DE11: return "D-E11";
    case StartupCode::DE14: return "D-E14";
    case StartupCode::DE15: return "D-E15";
    case StartupCode::DE16: return "D-E16";
    case StartupCode::DE17: return "D-E17";
    case StartupCode::DE19: return "D-E19";
    case StartupCode::DE20: return "D-E20";
    case StartupCode::RE2: return "R-E2";
    case StartupCode::RE4: return "R-E4";
    case StartupCode::RE5: return "R-E5";
    case StartupCode::RE7: return "R-E7";
    case StartupCode::XE1: return "X-E1";
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
    case TypeRole::Message: return "message";
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
    void AnnotatedWrite(TypeKey resource) override {
        if (const auto id = Lookup(resource, TypeRole::Resource, "resource")) access.annotatedWrites.push_back(*id);
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
                                         options.writePolicy, options.zone, options.role, options.rollback,
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
    data.resources.push_back(ResourceInfo{std::move(options.name), shape.size, shape.align, options.writePolicy,
                                          options.zone, options.role, options.rollback, options.external,
                                          options.threadBound, data.OwnerName(owner_)});
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
        MessageInfo{std::move(options.name), options.zone, data.OwnerName(owner_), options.capacity, {}});
    data.messageMakers.push_back(make);
    return id;
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
    data.namedSets.push_back(Detail::NamedSetRecord{std::move(name), key, owner_, {}, {}, {}, false});
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

void Registrar::MarkDigestSet(TypeKey set) {
    GYO_ASSERT(data_->building);
    Detail::RegistryData& data = *data_;
    ++data.digestMarks;
    for (Detail::NamedSetRecord& record : data.namedSets) {
        if (record.key == set) {
            record.digest = true;
            return;
        }
    }
    data.Error(StartupCode::RE4, "R-6", data.OwnerName(owner_) + " marks a named set that is not registered as the digest set",
               {data.OwnerName(owner_)});
}

void Registrar::DeclareAllotter(TypeKey resource, Detail::AllotterOps ops) {
    GYO_ASSERT(data_->building);
    data_->allotterDeclarations.push_back(resource);
    data_->allotterDeclarationOps.push_back(ops);
    data_->allotterOwners.push_back(owner_);
}

void Registrar::RequireExplicitZones() {
    GYO_ASSERT(data_->building);
    data_->explicitZones = true;
    ++data_->explicitZoneCalls;
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
    data_->hooks.push_back(Detail::HookRecord{std::move(options.name), options.zone, component, trigger, resolve, run,
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

Fingerprint FrozenRegistry::RegistryFingerprint() const {
    return Detail::RegistryFingerprintOf(*data_);
}

Fingerprint FrozenRegistry::CFingerprint() const {
    return Detail::CFingerprintOf(*data_);
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
                                          WritePolicy::Exclusive, Zone::B, Role::State, Rollback::No, true, false,
                                          kKernelOwnerName});
    data.resourceMakers.push_back(&MakeResource<KernelSettings>);
    data.effects.push_back(EffectRecord{kDiagnosticsEffect, kKernelOwner});
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

// A zone left Unset is zone B unless an owner asked for explicit zones
// (Registrar::RequireExplicitZones). Runs once registration is closed and
// before every check, so the zone rules, the Plans and the fingerprints all
// see the zone the declaration ends up with.
void ApplyDefaultZones(RegistryData& data) {
    if (data.explicitZones) return;
    const auto fill = [](Zone& zone) {
        if (zone == Zone::Unset) zone = Zone::B;
    };
    for (ColumnInfo& info : data.components) fill(info.zone);
    for (ResourceInfo& info : data.resources) fill(info.zone);
    for (MessageInfo& info : data.messages) fill(info.zone);
    for (SystemRecord& system : data.systems) fill(system.options.zone);
    for (HookRecord& hook : data.hooks) fill(hook.zone);
}

void CheckData(RegistryData& data) {
    for (const ColumnInfo& info : data.components) {
        if (info.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", "component '" + info.name + "' declares no zone (there is no default)",
                       {info.name, info.owner});
        }
        if (info.role == Role::Input && info.rollback != Rollback::No) {
            data.Error(StartupCode::DE7, "Z8", "component '" + info.name + "' has role Input and must not roll back",
                       {info.name, info.owner});
        }
    }
    for (const ResourceInfo& info : data.resources) {
        if (info.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", "resource '" + info.name + "' declares no zone (there is no default)",
                       {info.name, info.owner});
        }
        if (info.external && info.zone == Zone::C) {
            data.Error(StartupCode::DE7, "Z1", "external resource '" + info.name + "' is zone C; external is always zone B",
                       {info.name, info.owner});
        }
        if (info.role == Role::Input && info.rollback != Rollback::No) {
            data.Error(StartupCode::DE7, "Z8", "resource '" + info.name + "' has role Input and must not roll back",
                       {info.name, info.owner});
        }
    }
    for (const MessageInfo& info : data.messages) {
        if (info.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", "message '" + info.name + "' declares no zone (there is no default)",
                       {info.name, info.owner});
        }
    }
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

// Registrar::InitialValue: resolves the two components of every declaration and
// leaves the valid ones in (requestId, targetId) order. Who writes a request
// writes, in effect, its component: the zone rules check that in CheckSystems
// (FunnelWrites), not here.
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
        // The value of a zone C component is computed from zone C data only (Z1):
        // Fn is (c) computation whatever System writes the request.
        if (targetInfo.zone == Zone::C && requestInfo.zone != Zone::C) {
            data.Error(StartupCode::DE7, "Z1",
                       "the initial value of zone C component '" + targetInfo.name + "' comes from '" + requestInfo.name +
                           "', which is zone " + ZoneName(requestInfo.zone),
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
        if (!s.ingressStage.empty() && stages.count(s.ingressStage) != 0 && s.stages.front() != s.ingressStage) {
            data.Error(StartupCode::DE7, "Z3", "schedule '" + s.name + "' names ingress stage '" + s.ingressStage +
                                                   "', which is not its first stage", {s.name});
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
        if (s.executor == ExecutorKind::Parallel) {
            data.Error(StartupCode::DE17, "X-2", "schedule '" + s.name + "' declares Parallel; v1 has only Serial (LIM-2)",
                       {s.name});
        }
    }
}

// A run condition's access joins its System's (spec S-4, E.4): every read,
// filter, set read and the extract flag, so the System's checks
// (S-9, Z1, D-E14) see it. Conditions are read-only
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
    for (MessageInfo& info : data.messages) info.readers.clear();
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
            for (const ConditionRef& condition : mount.conditions) {
                if (condition.Resolver() == nullptr) continue;
                SystemResolver conditionResolver(data, subject + " run condition '" + condition.Name() + "'",
                                                 SourceText(system.source));
                condition.Resolver()(conditionResolver);
                AddMessageReaders(data, conditionResolver.access.reads, system, mount, condition.Name());
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
        if (hook.zone == Zone::C) {
            const auto forbid = [&](bool present, const char* rule, const char* what) {
                if (present) {
                    data.Error(StartupCode::DE7, rule, subject + " is zone C and " + what, {hook.name},
                               {SourceText(hook.source)});
                }
            };
            forbid(resolver.access.local, "S-7", "has Local<T> (state outside the World)");
            forbid(resolver.access.changeFilters, "S-9", "filters by Changed or Added");
            forbid(resolver.access.exclusive, "S-13", "takes ExclusiveWorld");
        }
        Normalize(resolver.access);
        hook.access = std::move(resolver.access);
        if (hook.zone == Zone::Unset) {
            data.Error(StartupCode::DE7, "S-6", subject + " declares no zone (there is no default)", {hook.name},
                       {SourceText(hook.source)});
        }
    }
}

// The zone of the data behind one access key.
Zone ZoneOf(const RegistryData& data, const AccessKey& key) {
    switch (key.kind) {
    case AccessKind::Component: return data.components[key.id].zone;
    case AccessKind::Resource: return data.resources[key.id].zone;
    case AccessKind::Message: return data.messages[key.id].zone;
    }
    return Zone::Unset;
}

std::string NameOf(const RegistryData& data, const AccessKey& key) {
    switch (key.kind) {
    case AccessKind::Component: return data.components[key.id].name;
    case AccessKind::Resource: return data.resources[key.id].name;
    case AccessKind::Message: return data.messages[key.id].name;
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

// Registrar::InitialValue: the kernel fills a component at the sync point for
// whoever added a cell of its request, so for the zone rules (Z1, Z2, Z3, Z6,
// Z7) a System or hook writes every component the requests it inserts or
// spawns fill, through chains of declarations. A write in place or a remove of
// a request adds no cell and fills nothing, so it does not count. Not for the
// writer count (D-E2): not counting them is what the funnel is for. Maps each
// such component to the request it is filled from; components the access set
// writes itself are not listed.
std::map<ComponentId, ComponentId> FunnelWrites(const RegistryData& data, const AccessSet& access) {
    std::map<ComponentId, ComponentId> reached;
    std::vector<ComponentId> pending(access.deferredInserts.begin(), access.deferredInserts.end());
    std::set<ComponentId> seen(pending.begin(), pending.end());
    while (!pending.empty()) {
        const ComponentId request = pending.back();
        pending.pop_back();
        for (const InitialValueRecord& rule : data.initialValues) {
            if (rule.requestId != request || !seen.insert(rule.targetId).second) continue;
            reached.emplace(rule.targetId, request);
            pending.push_back(rule.targetId);
        }
    }
    for (const AccessKey& key : WrittenKeys(access)) {
        if (key.kind == AccessKind::Component) reached.erase(key.id);
    }
    return reached;
}

// The access set the zone rules check: access plus the components it fills (FunnelWrites).
AccessSet ZoneAccess(const AccessSet& access, const std::map<ComponentId, ComponentId>& funnel) {
    AccessSet out = access;
    for (const auto& entry : funnel) out.deferredWrites.push_back(entry.first);
    SortUnique(out.deferredWrites);
    return out;
}

// " (filled from 'R' by Registrar::InitialValue)" for a component written only through the funnel.
std::string FunnelNote(const RegistryData& data, const std::map<ComponentId, ComponentId>& funnel, const AccessKey& key) {
    if (key.kind != AccessKind::Component) return {};
    const auto found = funnel.find(key.id);
    if (found == funnel.end()) return {};
    return " (filled from '" + data.components[found->second].name + "' by Registrar::InitialValue)";
}

// Z1, Z2 (first half), Z7 and the boundary rule for one System or hook. access
// includes the components the funnel fills for it (ZoneAccess); funnel names them.
void CheckZoneAccess(RegistryData& data, const std::string& subject, const std::string& name, Zone zone, bool ingress,
                     const AccessSet& access, const std::map<ComponentId, ComponentId>& funnel,
                     const std::string& source) {
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
                       subject + " is zone C but " + (writes ? "writes" : "reads") + " '" + NameOf(data, key) + "'" +
                           FunnelNote(data, funnel, key) + ", which is " +
                           (external ? "external (always zone B)" : "zone " + std::string(ZoneName(ZoneOf(data, key)))),
                       {name, NameOf(data, key)}, {source});
        }
        for (const AccessKey& key : written) {
            if (IsInput(data, key)) {
                data.Error(StartupCode::DE7, "Z3",
                           subject + " is zone C and writes role=Input data '" + NameOf(data, key) + "'" +
                               FunnelNote(data, funnel, key) + "; only the Host and ingress Systems write boundary input",
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
                       NameOf(data, key) + "'" + FunnelNote(data, funnel, key) + (ingress ? " whose role is not Input" : ""),
                   {name, NameOf(data, key)}, {source});
    }
}

void CheckSystems(RegistryData& data) {
    // One X-E1 per schedule (the first zone C System found names it), not one per System.
    std::set<std::string> unguardedParallel;
    bool anyRollback = false;
    for (const ScheduleRecord& schedule : data.schedules) anyRollback = anyRollback || schedule.options.rollbackEnabled;

    for (const SystemRecord& system : data.systems) {
        if (system.removed) continue;
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
        if (access.exclusive && system.options.ingress) {
            // Spec S-17 (K0): Z2's ingress exception would let it write zone C boundary input.
            data.Error(StartupCode::DE7, "S-17",
                       subject + " takes ExclusiveWorld and is ingress; an ingress System declares the boundary input "
                                 "it writes in its signature",
                       {name}, {source});
        }
        const std::map<ComponentId, ComponentId> funnel = FunnelWrites(data, access);
        const AccessSet zoneAccess = ZoneAccess(access, funnel);
        CheckZoneAccess(data, subject, name, zone, system.options.ingress, zoneAccess, funnel, source);

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
                for (const AccessKey& key : WrittenKeys(zoneAccess)) {
                    if (key.kind == AccessKind::Message) continue;
                    const Rollback rollback = RollbackOf(data, key);
                    if (rollback != Rollback::Yes && rollback != Rollback::Derived) {
                        data.Error(StartupCode::DE7, "Z6",
                                   subject + " is zone C in rollback-enabled schedule '" + s.name + "' and writes '" +
                                       NameOf(data, key) + "'" + FunnelNote(data, funnel, key) + ", which does not roll back",
                                   {name, NameOf(data, key), s.name}, {source});
                    }
                }
            }
            if (s.executor == ExecutorKind::Parallel && zone == Zone::C && s.parallelGuardTest.empty() &&
                unguardedParallel.insert(s.name).second) {
                data.Error(StartupCode::XE1, StartupCode::DE11, "X-2",
                           "schedule '" + s.name + "' declares Parallel and holds zone C System '" + name +
                               "', but names no Serial-versus-Parallel guard test",
                           {s.name, name}, {source});
            }
        }
    }
    for (const HookRecord& hook : data.hooks) {
        const std::string subject = "hook '" + hook.name + "'";
        const std::map<ComponentId, ComponentId> funnel = FunnelWrites(data, hook.access);
        const AccessSet zoneAccess = ZoneAccess(hook.access, funnel);
        CheckZoneAccess(data, subject, hook.name, hook.zone, false, zoneAccess, funnel, SourceText(hook.source));
        if (hook.zone == Zone::C && anyRollback) {
            for (const AccessKey& key : WrittenKeys(zoneAccess)) {
                if (key.kind != AccessKind::Message && RollbackOf(data, key) != Rollback::Yes) {
                    data.Error(StartupCode::DE7, "Z6",
                               subject + " is zone C and writes '" + NameOf(data, key) + "'" + FunnelNote(data, funnel, key) +
                                   ", which is not rollback=Yes, in a World with a rollback-enabled schedule",
                               {hook.name, NameOf(data, key)}, {SourceText(hook.source)});
                }
            }
        }
    }
}

// D-E2: at most one writer of an Exclusive column or resource over every
// schedule; writers are counted per System, hooks included (DI-KS 6).
void CheckExclusiveWriters(RegistryData& data) {
    std::map<AccessKey, std::vector<std::pair<std::string, std::string>>> writers;
    for (const SystemRecord& system : data.systems) {
        if (system.removed) continue;
        bool counted = false;
        for (const Mount& mount : system.options.mounts) {
            const ScheduleRecord* schedule = data.FindSchedule(mount.schedule);
            if (schedule != nullptr) counted = true;
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

// The read-only view's lists (FrozenRegistry; DI-KS 25), made again at every BuildPlans.
void FillSystemInfos(RegistryData& data) {
    data.systemInfos.clear();
    for (const SystemRecord& system : data.systems) {
        data.systemInfos.push_back(SystemInfo{system.options.name, system.seq, system.options.zone, system.options.ingress,
                                              system.options.mounts, system.access, SourceText(system.source),
                                              data.OwnerName(system.owner), system.effects, system.removed});
    }
    data.hookInfos.clear();
    for (const HookRecord& hook : data.hooks) {
        data.hookInfos.push_back(HookInfo{hook.name, hook.trigger, hook.componentId.value_or(0), hook.zone, hook.access,
                                          SourceText(hook.source), data.OwnerName(hook.owner)});
    }
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
    if (data.digestMarks > base.digestMarks) refuse("DigestSet", "a named set is marked as the digest set", {"DigestSet"});
    if (data.explicitZoneCalls > base.explicitZoneCalls) {
        refuse("RequireExplicitZones", "explicit zones are required", {"RequireExplicitZones"});
    }
}

// What every round resolves first (named set members, the allotter, Systems'
// access sets). Derived
// fields are cleared and made again (declaration 3.2 rule 2).
void ResolveStage(RegistryData& data, bool laterRound) {
    ApplyDefaultZones(data);
    CheckData(data);
    if (laterRound) {
        // Needs hooks' component ids: resolved here first, again by ResolveSystems.
        for (HookRecord& hook : data.hooks) {
            const TypeEntry* entry = data.FindType(hook.component);
            hook.componentId = entry != nullptr && entry->role == TypeRole::Component ? std::optional<ComponentId>(entry->id)
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
    CheckExclusiveWriters(data);
    CheckHookConflicts(data);
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
void Adopt(RegistryData& into, RegistryData&& round) {
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

Engine::Base::Result<std::uint32_t, KernelError> Registry::OpenRegistrar(RegistrationOwner owner) {
    GYO_ASSERT(!data_->building && !(pending_ && pending_->building));  // not inside build (rule 3)
    GYO_ASSERT(!data_->failed);  // a failed first round leaves the Registry unusable (rule 9)
    RequireNoWorldRunning();
    if (data_->refusingWorlds > 0) {
        // DI-KS 26: one World that declares no dynamic insert refuses for the whole Registry.
        KernelError error(KernelErrorCode::DynamicInsertRefused,
                          "Register by '" + owner.name + "' refused: " + std::to_string(data_->refusingWorlds) +
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
        base.initialValues = round.initialValues.size();
        base.allotterDeclarations = round.allotterDeclarations.size();
        base.digestMarks = round.digestMarks;
        base.explicitZoneCalls = round.explicitZoneCalls;
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

Engine::Base::Result<void, StartupReport> Registry::BuildPlans() {
    GYO_ASSERT(!data_->building && !(pending_ && pending_->building));  // not inside build (rule 3)
    GYO_ASSERT(!data_->failed);
    RequireNoWorldRunning();
    if (pending_) {
        // A later round: built on the copy; the tables change only when it succeeds.
        Detail::RegistryData& round = *pending_;
        Detail::ResolveStage(round, true);
        if (!Detail::PlanStage(round)) {
            StartupReport report(std::move(round.errors));
            pending_.reset();
            return Engine::Base::Err(std::move(report));
        }
        round.version = data_->version + 1;
        round.registered = false;
        Detail::Adopt(*data_, std::move(round));
        pending_.reset();
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
