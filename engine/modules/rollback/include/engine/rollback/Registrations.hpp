#pragma once

// The rollback Module's public registrations (rollback declaration
// docs/architecture/target/v2/rollback-module-declaration.zh-Hant.md, section
// 5.4; authority-rollback declaration sections 5.1 and 6): what may roll back
// (Component, Resource), what is recomputed each step (Derived), the step
// schedule and the schedules run nested from it, the Systems skipped while
// resimulating, and the inputs whose history the strategy keeps (Input).
//
// The author calls them, or the server authority strategy on the author's
// behalf (Source::OnBehalf). While the World is built (Build) every call is
// appended to the table in the ModuleBuffer, with its source; the side effects
// in the kernel happen at the call (the run condition of SkipDuringResimulation;
// declaration 6 point 3). Which of several registrations of one type counts is
// decided after Build, by Resolve, a pure function of the table: the author's
// over one made on behalf, whatever the order; two of the author's (or two made
// on behalf) with different options are an error (RB-E1). The resolved
// registrations are put into the World as the Resource RollbackRegistry, read
// at run time by the strategy and by the authority Module's checks (R-6).

#include "engine/base/Assert.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace GYO::Engine::Rollback {

// Who made a registration: the author, or a strategy on the author's behalf
// (an author's registration overrides one made on behalf; not a privilege:
// anyone may say OnBehalf).
enum class Source : std::uint8_t { Author, OnBehalf };

enum class DataKind : std::uint8_t { Component, Resource };

// Finds a registered type's id in the read-only registry, by the type the
// registration named (nothing when it is not registered there as that kind).
using IdLookup = std::optional<std::uint32_t> (*)(const Kernel::FrozenRegistry&);

// A registered comparison, kept as a function pointer of no particular type
// (compared for identity; cast back to bool (*)(const T&, const T&) to call).
using Comparison = void (*)();

// The type-erased operations of one type that may roll back, made when it is
// registered (the registration knows the type; the kernel needs no untyped copy).
struct DataOps {
    IdLookup idIn = nullptr;
    // The values are equal: the registered comparison, or ==.
    bool (*equal)(const void* a, const void* b, Comparison comparison) = nullptr;
};

// The options of Component<T> and Resource<T>. equal: the comparison a restore
// uses to leave an unchanged value unwritten; nullptr is ==.
template <class T>
struct DataOptions {
    bool (*equal)(const T&, const T&) = nullptr;
};

// One appended registration of a type that may roll back.
struct DataRecord {
    Kernel::TypeKey key;
    DataKind kind = DataKind::Component;
    Source source = Source::Author;
    const DataOps* ops = nullptr;
    // The options: the registered comparison (compared for identity only), or nullptr for ==.
    Comparison comparison = nullptr;
};

struct DerivedRecord {
    Kernel::TypeKey key;
    IdLookup idIn = nullptr;
    Source source = Source::Author;
};

// The unit of rollback: the record runs after each of its steps, a
// resimulation runs it k times nested.
struct StepScheduleRecord {
    std::string name;
    std::string recordStage;
    Source source = Source::Author;
};

// A schedule run nested from a step schedule (it runs again while
// resimulating; not recorded, not a unit of rollback; only checked).
struct NestedScheduleRecord {
    std::string name;
    std::string stepSchedule;
    Source source = Source::Author;
};

struct SkipRecord {
    std::string system;
    Source source = Source::Author;
};

// An input whose history the strategy keeps by tick. The type itself is
// registered in the kernel by one entry only (the author's Registrar, or the
// authority Module's input registration); Input<T> does not register it.
struct InputRecord {
    Kernel::TypeKey key;
    IdLookup idIn = nullptr;
    std::uint32_t capacity = 0;
    Source source = Source::Author;
};

// A registration that cannot hold (reported by the rollback Module's Check
// with its own code; declaration 4.2: a Build has no way to report).
struct RegistrationError {
    std::string code;
    std::string message;
    // The type it is about, when it is about one (its name is read from the registry when reported).
    IdLookup idIn = nullptr;
    DataKind kind = DataKind::Component;
};

// The resolved registrations of one World: put into the World as a Resource
// when it is prepared, read at run time (R-6: readable through this, unchanged
// after the startup checks). Its hash function hashes each entry's kind,
// source, schedule and System names and capacity, never a type key or a
// function pointer (declaration 3.3).
struct RollbackRegistry {
    std::vector<DataRecord> data;
    std::vector<DerivedRecord> derived;
    std::vector<StepScheduleRecord> stepSchedules;
    std::vector<NestedScheduleRecord> nestedSchedules;
    std::vector<SkipRecord> skipped;
    std::vector<InputRecord> inputs;

    [[nodiscard]] const DataRecord* Data(Kernel::TypeKey key) const noexcept;
    [[nodiscard]] bool IsDerived(Kernel::TypeKey key) const noexcept;
    [[nodiscard]] const StepScheduleRecord* StepSchedule(std::string_view name) const noexcept;
    [[nodiscard]] bool Skipped(std::string_view system) const noexcept;
};

void HashValue(Kernel::Hasher& h, const RollbackRegistry& registry);

// The table in the ModuleBuffer: every registration appended while the World
// is built, with its source. Appending after Build is a GYO_ASSERT.
class RollbackRegistrations {
public:
    explicit RollbackRegistrations(const Orchestration::ModuleBuffer& buffer) : buffer_(&buffer) {}

    void Append(DataRecord record);
    void Append(DerivedRecord record);
    void Append(StepScheduleRecord record);
    void Append(NestedScheduleRecord record);
    void Append(SkipRecord record);
    void Append(InputRecord record);

    const std::vector<DataRecord>& Data() const noexcept { return data_; }
    const std::vector<DerivedRecord>& Derived() const noexcept { return derived_; }
    const std::vector<StepScheduleRecord>& StepSchedules() const noexcept { return stepSchedules_; }
    const std::vector<NestedScheduleRecord>& NestedSchedules() const noexcept { return nestedSchedules_; }
    const std::vector<SkipRecord>& Skipped() const noexcept { return skipped_; }
    const std::vector<InputRecord>& Inputs() const noexcept { return inputs_; }

private:
    void Open() const;
    const Orchestration::ModuleBuffer* buffer_;
    std::vector<DataRecord> data_;
    std::vector<DerivedRecord> derived_;
    std::vector<StepScheduleRecord> stepSchedules_;
    std::vector<NestedScheduleRecord> nestedSchedules_;
    std::vector<SkipRecord> skipped_;
    std::vector<InputRecord> inputs_;
};

// Resolve's result: the registrations that count, and what cannot hold.
struct Resolved {
    RollbackRegistry registry;
    std::vector<RegistrationError> errors;
};

// The registrations that count (a pure function of the table; declaration 6
// point 3): for each type, the author's over one made on behalf; two of the
// author's, or two made on behalf, with different options are RB-E1. A
// schedule or a System named twice is one. First-registration order.
[[nodiscard]] Resolved Resolve(const RollbackRegistrations& table);

// The kernel run condition SkipDuringResimulation puts on a mount: false while
// the World resimulates (the time Module's Resimulation).
bool NotResimulating(Kernel::Res<WorldTime::Resimulation> resimulation);

namespace Detail {

template <class T>
std::optional<std::uint32_t> ComponentIdIn(const Kernel::FrozenRegistry& registry) {
    return registry.ComponentIdOf<T>();
}
template <class T>
std::optional<std::uint32_t> ResourceIdIn(const Kernel::FrozenRegistry& registry) {
    return registry.ResourceIdOf<T>();
}

template <class T>
bool EqualAs(const void* a, const void* b, Comparison comparison) {
    const T& left = *static_cast<const T*>(a);
    const T& right = *static_cast<const T*>(b);
    if (comparison != nullptr) return reinterpret_cast<bool (*)(const T&, const T&)>(comparison)(left, right);
    if constexpr (std::equality_comparable<T>) {
        return left == right;
    } else {
        GYO_ASSERT(false && "a type without == rolls back only with a comparison (DataOptions::equal)");
        return false;
    }
}

template <class T>
inline constexpr DataOps kComponentOps{&ComponentIdIn<T>, &EqualAs<T>};
template <class T>
inline constexpr DataOps kResourceOps{&ResourceIdIn<T>, &EqualAs<T>};

} // namespace Detail

// The author's way in (rollback declaration 5.4): made from the WorldSetup of
// any Build of a World the rollback Module is built into (the product's own
// included); without the Module it is a GYO_ASSERT. source: OnBehalf for a
// strategy registering on the author's behalf.
class RollbackEntry {
public:
    explicit RollbackEntry(Orchestration::WorldSetup& setup, Source source = Source::Author);

    // T may roll back. T is registered in the kernel elsewhere (Check reports
    // RB-E2 when it is not). It is copied into the history: copyable.
    // A type without == needs options.equal (GYO_ASSERT).
    template <class T>
    void Component(DataOptions<T> options = {}) {
        static_assert(std::is_copy_constructible_v<T>, "rollback.Component<T>: T is copied into the history");
        GYO_ASSERT((std::equality_comparable<T> || options.equal != nullptr) &&
                   "rollback.Component<T>: a type without == needs DataOptions::equal");
        table_->Append(DataRecord{Kernel::KeyOf<T>(), DataKind::Component, source_, &Detail::kComponentOps<T>,
                                  reinterpret_cast<Comparison>(options.equal)});
    }
    template <class T>
    void Resource(DataOptions<T> options = {}) {
        static_assert(std::is_copy_constructible_v<T>, "rollback.Resource<T>: T is copied into the history");
        GYO_ASSERT((std::equality_comparable<T> || options.equal != nullptr) &&
                   "rollback.Resource<T>: a type without == needs DataOptions::equal");
        table_->Append(DataRecord{Kernel::KeyOf<T>(), DataKind::Resource, source_, &Detail::kResourceOps<T>,
                                  reinterpret_cast<Comparison>(options.equal)});
    }
    // Component T is recomputed each step before every reader; not recorded, not restored.
    template <class T>
    void Derived() {
        table_->Append(DerivedRecord{Kernel::KeyOf<T>(), &Detail::ComponentIdIn<T>, source_});
    }
    // The step schedule: the record runs in its recordStage after each step.
    void StepSchedule(std::string name, std::string recordStage);
    // A schedule run nested from stepSchedule.
    void NestedSchedule(std::string name, std::string stepSchedule);
    // The System of options is skipped while the World resimulates: a run
    // condition on each of its mounts, given now (before AddSystem), and the
    // registration recorded. Call it with the options passed to AddSystem.
    void SkipDuringResimulation(Kernel::SystemOptions& options);
    // The strategy keeps T's history by tick, at most capacity ticks. T is
    // registered in the kernel elsewhere (Check reports RB-E3 when it is not).
    template <class T>
    void Input(std::uint32_t capacity) {
        table_->Append(InputRecord{Kernel::KeyOf<T>(), &Detail::ResourceIdIn<T>, capacity, source_});
    }

private:
    RollbackRegistrations* table_;
    Source source_;
};

} // namespace GYO::Engine::Rollback
