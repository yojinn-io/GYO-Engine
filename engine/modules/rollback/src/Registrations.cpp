#include "engine/rollback/Registrations.hpp"

#include "engine/rollback/Runtime.hpp"

#include <algorithm>
#include <utility>

namespace GYO::Engine::Rollback {

namespace {

const char* SourceText(Source source) { return source == Source::Author ? "the author's" : "made on behalf"; }

// The one record of a key or name that counts: the author's over one made on behalf; among one source, the
// first, the others having to agree (same()); otherwise RB-E1.
template <class Record, class Key, class Same, class Describe>
std::vector<Record> PickOne(const std::vector<Record>& records, Key key, Same same, Describe describe,
                            std::vector<RegistrationError>& errors) {
    std::vector<Record> picked;
    std::vector<bool> done(records.size(), false);
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (done[i]) continue;
        std::vector<std::size_t> group;
        for (std::size_t j = i; j < records.size(); ++j) {
            if (!done[j] && key(records[j]) == key(records[i])) {
                group.push_back(j);
                done[j] = true;
            }
        }
        const bool anyAuthor = std::any_of(group.begin(), group.end(),
                                           [&](std::size_t g) { return records[g].source == Source::Author; });
        const Source counts = anyAuthor ? Source::Author : Source::OnBehalf;
        const Record* chosen = nullptr;
        for (std::size_t g : group) {
            const Record& record = records[g];
            if (record.source != counts) continue;
            if (chosen == nullptr) {
                chosen = &record;
            } else if (!same(*chosen, record)) {
                errors.push_back(describe(*chosen, std::string("two of ") + SourceText(counts) +
                                                       " registrations with different options"));
            }
        }
        picked.push_back(*chosen);
    }
    return picked;
}

} // namespace

const DataRecord* RollbackRegistry::Data(Kernel::TypeKey key) const noexcept {
    for (const DataRecord& record : data) {
        if (record.key == key) return &record;
    }
    return nullptr;
}

bool RollbackRegistry::IsDerived(Kernel::TypeKey key) const noexcept {
    return std::any_of(derived.begin(), derived.end(), [key](const DerivedRecord& r) { return r.key == key; });
}

const StepScheduleRecord* RollbackRegistry::StepSchedule(std::string_view name) const noexcept {
    for (const StepScheduleRecord& record : stepSchedules) {
        if (record.name == name) return &record;
    }
    return nullptr;
}

bool RollbackRegistry::Skipped(std::string_view system) const noexcept {
    return std::any_of(skipped.begin(), skipped.end(), [system](const SkipRecord& r) { return r.system == system; });
}

void HashValue(Kernel::Hasher& h, const RollbackRegistry& registry) {
    using Kernel::HashValue;
    h.U64(registry.data.size());
    for (const DataRecord& record : registry.data) {
        HashValue(h, record.kind);
        HashValue(h, record.source);
        HashValue(h, record.comparison != nullptr);
    }
    h.U64(registry.derived.size());
    for (const DerivedRecord& record : registry.derived) HashValue(h, record.source);
    h.U64(registry.stepSchedules.size());
    for (const StepScheduleRecord& record : registry.stepSchedules) {
        HashValue(h, record.name);
        HashValue(h, record.recordStage);
    }
    h.U64(registry.nestedSchedules.size());
    for (const NestedScheduleRecord& record : registry.nestedSchedules) {
        HashValue(h, record.name);
        HashValue(h, record.stepSchedule);
    }
    h.U64(registry.skipped.size());
    for (const SkipRecord& record : registry.skipped) HashValue(h, record.system);
    h.U64(registry.inputs.size());
    for (const InputRecord& record : registry.inputs) HashValue(h, record.capacity);
}

void RollbackRegistrations::Open() const {
    GYO_ASSERT(!buffer_->Frozen() && "a rollback registration after Build: the registrations are frozen");
}

void RollbackRegistrations::Append(DataRecord record) {
    Open();
    data_.push_back(std::move(record));
}
void RollbackRegistrations::Append(DerivedRecord record) {
    Open();
    derived_.push_back(std::move(record));
}
void RollbackRegistrations::Append(StepScheduleRecord record) {
    Open();
    stepSchedules_.push_back(std::move(record));
}
void RollbackRegistrations::Append(NestedScheduleRecord record) {
    Open();
    nestedSchedules_.push_back(std::move(record));
}
void RollbackRegistrations::Append(SkipRecord record) {
    Open();
    skipped_.push_back(std::move(record));
}
void RollbackRegistrations::Append(InputRecord record) {
    Open();
    inputs_.push_back(std::move(record));
}

Resolved Resolve(const RollbackRegistrations& table) {
    Resolved out;
    std::vector<RegistrationError>& errors = out.errors;
    out.registry.data = PickOne(
        table.Data(), [](const DataRecord& r) { return r.key.address; },
        [](const DataRecord& a, const DataRecord& b) { return a.kind == b.kind && a.comparison == b.comparison; },
        [](const DataRecord& r, std::string what) {
            return RegistrationError{"RB-E1", "rollback.Component or Resource: " + std::move(what), r.ops->idIn, r.kind};
        },
        errors);
    out.registry.derived = PickOne(
        table.Derived(), [](const DerivedRecord& r) { return r.key.address; },
        [](const DerivedRecord&, const DerivedRecord&) { return true; },
        [](const DerivedRecord& r, std::string what) {
            return RegistrationError{"RB-E1", "rollback.Derived: " + std::move(what), r.idIn, DataKind::Component};
        },
        errors);
    out.registry.stepSchedules = PickOne(
        table.StepSchedules(), [](const StepScheduleRecord& r) { return r.name; },
        [](const StepScheduleRecord& a, const StepScheduleRecord& b) { return a.recordStage == b.recordStage; },
        [](const StepScheduleRecord& r, std::string what) {
            return RegistrationError{"RB-E1", "rollback.StepSchedule '" + r.name + "': " + std::move(what), nullptr, DataKind::Component};
        },
        errors);
    out.registry.nestedSchedules = PickOne(
        table.NestedSchedules(), [](const NestedScheduleRecord& r) { return r.name; },
        [](const NestedScheduleRecord& a, const NestedScheduleRecord& b) { return a.stepSchedule == b.stepSchedule; },
        [](const NestedScheduleRecord& r, std::string what) {
            return RegistrationError{"RB-E1", "rollback.NestedSchedule '" + r.name + "': " + std::move(what), nullptr, DataKind::Component};
        },
        errors);
    out.registry.skipped = PickOne(
        table.Skipped(), [](const SkipRecord& r) { return r.system; },
        [](const SkipRecord&, const SkipRecord&) { return true; },
        [](const SkipRecord& r, std::string what) {
            return RegistrationError{"RB-E1", "rollback.SkipDuringResimulation '" + r.system + "': " + std::move(what), nullptr, DataKind::Component};
        },
        errors);
    out.registry.inputs = PickOne(
        table.Inputs(), [](const InputRecord& r) { return r.key.address; },
        [](const InputRecord& a, const InputRecord& b) { return a.capacity == b.capacity; },
        [](const InputRecord& r, std::string what) {
            return RegistrationError{"RB-E1", "rollback.Input: " + std::move(what), r.idIn, DataKind::Resource};
        },
        errors);
    return out;
}

bool NotResimulating(Kernel::Res<WorldTime::Resimulation> resimulation) { return resimulation->active == 0; }

RollbackEntry::RollbackEntry(Orchestration::WorldSetup& setup, Source source)
    : table_(setup.Buffer().Find<RollbackRegistrations>()), registrar_(&setup.Registrar()), source_(source),
      strategy_(setup.Options<RollbackOptions>() != nullptr && setup.Options<RollbackOptions>()->strategy != nullptr) {
    GYO_ASSERT(table_ != nullptr && "rollback registrations in a World the rollback Module is not built into");
}

void RollbackEntry::StepSchedule(std::string name, std::string recordStage) {
    const bool first = std::none_of(table_->StepSchedules().begin(), table_->StepSchedules().end(),
                                    [&name](const StepScheduleRecord& r) { return r.name == name; });
    if (first && strategy_) {
        // The record after each step (the registration's side effect in the kernel, at the call: declaration 6 point 3).
        Kernel::SystemOptions record;
        record.name = "RollbackRecord:" + name;
        record.mounts.push_back(Kernel::Mount{name, recordStage, {}, {}, {}, {}});
        registrar_->AddSystem<&RecordStep>(record);
    }
    table_->Append(StepScheduleRecord{std::move(name), std::move(recordStage), source_});
}

void RollbackEntry::NestedSchedule(std::string name, std::string stepSchedule) {
    table_->Append(NestedScheduleRecord{std::move(name), std::move(stepSchedule), source_});
}

void RollbackEntry::SkipDuringResimulation(Kernel::SystemOptions& options) {
    for (Kernel::Mount& mount : options.mounts) {
        mount.conditions.push_back(Kernel::Condition<&NotResimulating>("NotResimulating"));
    }
    table_->Append(SkipRecord{options.name, source_});
}

} // namespace GYO::Engine::Rollback
