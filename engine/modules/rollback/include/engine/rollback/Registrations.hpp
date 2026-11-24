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
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/rollback/History.hpp"
#include "engine/world_time/TimeTypes.hpp"

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
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
    // The official strategy's history of the type, with the registered comparison.
    std::unique_ptr<ColumnHistoryBase> (*makeHistory)(Comparison comparison) = nullptr;
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

// An input whose history the strategy keeps by tick. The type itself is
// registered in the kernel by one entry only (the author's Registrar, or the
// authority Module's input registration); Input<T> does not register it.
// How many steps of a recorded input are kept: a tier, or any number of steps written directly (not refused).
// The tiers follow the batch 4 budget (authority-rollback declaration 5.4): Short is the depth of the P1 and P3
// cases and lightyear's default rollback depth (20), Medium the P2 case's (32), Long one second at the time
// Module's default 64 Hz (64). StrategyDepth (0) takes the strategy's own depth when the World is prepared, so it
// never falls short of it (RB-E9).
enum class InputHistoryLength : std::uint32_t {
    StrategyDepth = 0,
    Short = 20,
    Medium = 32,
    Long = 64,
};

struct InputRecord {
    Kernel::TypeKey key;
    IdLookup idIn = nullptr;
    // The official strategy's history of the input.
    std::unique_ptr<InputHistoryBase> (*makeHistory)(std::uint32_t capacity) = nullptr;
    // Steps kept; 0: the strategy's depth (InputHistoryLength::StrategyDepth).
    std::uint32_t capacity = 0;
    Source source = Source::Author;
    // A resource input, or a component input (one value per entity; net Module declaration D-11).
    DataKind kind = DataKind::Resource;
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
    std::vector<InputRecord> inputs;

    [[nodiscard]] const DataRecord* Data(Kernel::TypeKey key) const noexcept;
    [[nodiscard]] bool IsDerived(Kernel::TypeKey key) const noexcept;
    [[nodiscard]] const StepScheduleRecord* StepSchedule(std::string_view name) const noexcept;
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
    void Append(InputRecord record);

    const std::vector<DataRecord>& Data() const noexcept { return data_; }
    const std::vector<DerivedRecord>& Derived() const noexcept { return derived_; }
    const std::vector<StepScheduleRecord>& StepSchedules() const noexcept { return stepSchedules_; }
    const std::vector<NestedScheduleRecord>& NestedSchedules() const noexcept { return nestedSchedules_; }
    const std::vector<InputRecord>& Inputs() const noexcept { return inputs_; }

private:
    void Open() const;
    const Orchestration::ModuleBuffer* buffer_;
    std::vector<DataRecord> data_;
    std::vector<DerivedRecord> derived_;
    std::vector<StepScheduleRecord> stepSchedules_;
    std::vector<NestedScheduleRecord> nestedSchedules_;
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

// The schedules that run again while resimulating: the step schedules and the
// schedules nested in them.
[[nodiscard]] std::set<std::string> ResimulatedSchedules(const RollbackRegistry& registry);
// Whether a run condition is the one SkipDuringResimulation puts on a mount,
// and whether a mount carries it.
[[nodiscard]] bool IsSkipCondition(const Kernel::ConditionRef& condition);
[[nodiscard]] bool SkippedWhileResimulating(const Kernel::Mount& mount);
// Whether a System runs while the World resimulates: one of its mounts is in a
// resimulated schedule and does not carry that condition. Asked of each mount
// (a mount added after SkipDuringResimulation runs again), not of the name.
[[nodiscard]] bool RunsWhileResimulating(const Kernel::SystemInfo& system, const std::set<std::string>& resimulated);

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
std::unique_ptr<ColumnHistoryBase> MakeComponentHistory(Comparison comparison) {
    return std::make_unique<ComponentHistory<T>>(reinterpret_cast<bool (*)(const T&, const T&)>(comparison));
}
template <class T>
std::unique_ptr<ColumnHistoryBase> MakeResourceHistory(Comparison comparison) {
    return std::make_unique<ResourceHistory<T>>(reinterpret_cast<bool (*)(const T&, const T&)>(comparison));
}
template <class T>
std::unique_ptr<InputHistoryBase> MakeInputHistory(std::uint32_t capacity) {
    return std::make_unique<InputHistory<T>>(capacity);
}
template <class T>
std::unique_ptr<InputHistoryBase> MakeComponentInputHistory(std::uint32_t capacity) {
    return std::make_unique<ComponentInputHistory<T>>(capacity);
}

template <class T>
inline constexpr DataOps kComponentOps{&ComponentIdIn<T>, &EqualAs<T>, &MakeComponentHistory<T>};
template <class T>
inline constexpr DataOps kResourceOps{&ResourceIdIn<T>, &EqualAs<T>, &MakeResourceHistory<T>};

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
    // The step schedule: the record runs in its recordStage after each step (in a World with a strategy, the
    // record System is mounted there now, by the first registration of the schedule). The defaults follow the
    // time Module (authority-rollback declaration 2.3): its fixed-step schedule (WorldTimeOptions::fixedSchedule)
    // and that schedule's Last.
    void StepSchedule();
    void StepSchedule(std::string name, std::string recordStage = std::string(Orchestration::DefaultSchedules::last));
    // A schedule run nested from stepSchedule.
    void NestedSchedule(std::string name, std::string stepSchedule);
    // The System of options is skipped while the World resimulates: a run
    // condition on each of its mounts, given now (before AddSystem), and the
    // registration recorded. Call it with the options passed to AddSystem.
    void SkipDuringResimulation(Kernel::SystemOptions& options);
    // T is an outside input whose value the strategy records by tick and writes back before each step it runs
    // again (a recorded input; not rollback state). The length is a tier or a number of steps; the default is the
    // strategy's depth. kind: a resource (the default), or a component, recorded per entity (one value of each
    // entity by tick, written back cell by cell; net Module declaration D-11). T is registered in the kernel
    // elsewhere (Check reports RB-E3 when it is not); the authority Module registers it on the author's behalf for a
    // decision input (authority.Resource<T> or authority.Component<T> with {.input = true}).
    template <class T>
    void Input(InputHistoryLength length = InputHistoryLength::StrategyDepth, DataKind kind = DataKind::Resource) {
        Input<T>(static_cast<std::uint32_t>(length), kind);
    }
    template <class T>
    void Input(std::uint32_t steps, DataKind kind = DataKind::Resource) {
        static_assert(std::is_copy_constructible_v<T>, "rollback.Input<T>: T is copied into the history");
        if (kind == DataKind::Component) {
            table_->Append(InputRecord{Kernel::KeyOf<T>(), &Detail::ComponentIdIn<T>, &Detail::MakeComponentInputHistory<T>,
                                       steps, source_, DataKind::Component});
        } else {
            table_->Append(InputRecord{Kernel::KeyOf<T>(), &Detail::ResourceIdIn<T>, &Detail::MakeInputHistory<T>, steps,
                                       source_, DataKind::Resource});
        }
    }

private:
    RollbackRegistrations* table_;
    Kernel::Registrar* registrar_;
    Source source_;
    // The World chose a strategy: StepSchedule mounts the record (none without one: R-4).
    bool strategy_;
    // The time Module's fixed-step schedule in this World: StepSchedule()'s default.
    std::string fixedSchedule_;
};

// An entity a rollback keeps in being: the official strategy keeps it out of the alive sets it records, and a
// restore neither despawns it nor puts it back. Only its being: its rollback-registered components, if it has any,
// roll back like any other entity's (one it did not have at the point is taken off), so give it none. The idea of
// lightyear's DisableRollback (crates/replication/prediction/src/rollback.rs:272-275), which also leaves the
// components out and hides the entity during a resimulation; not its name, since the meaning is narrower. For what
// lives beside the game's state, made outside the step: a connection's Peer entity (the net Module puts it on a
// client's own Peer), a client's camera or the pictures of what it shows. Registered in a World that chose a
// strategy (R-4: none without one).
struct KeepThroughRollback final {};
inline void HashValue(Kernel::Hasher&, const KeepThroughRollback&) {}

} // namespace GYO::Engine::Rollback
