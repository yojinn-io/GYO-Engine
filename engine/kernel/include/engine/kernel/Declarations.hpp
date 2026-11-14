#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// The explicit half of a declaration (spec B.2): what a plugin states when it
// registers a component, resource, event, effect, schedule or System. The other
// half, a System's data access, is derived from its parameter types
// (Params.hpp); nothing here can declare access by hand (DI-KS 1).

namespace GYO::Kernel {

// Ids are handed out in registration order (DI-KS 4, spec R-1): the n-th
// component registered gets ComponentId n. They are in-process only.
using ComponentId = std::uint32_t;
using ResourceId = std::uint32_t;
using EventTypeId = std::uint32_t;
using EffectId = std::uint32_t;
using SetId = std::uint32_t;
using RegistrationSeq = std::uint32_t;

// The determinism zone (spec G; target/determinism.md section 2). There is no
// default: Unset is reported at finish (D-E7, spec S-6), so a forgotten zone
// never silently becomes (b) or (c).
enum class Zone : std::uint8_t {
    Unset,
    C,
    B,
};

// Spec A.1 ColumnInfo. Exclusive by default (target/world.zh-Hant.md section 3).
enum class WritePolicy : std::uint8_t {
    Exclusive,
    Shared,
};

// Spec A.1: Input is (c) data that an ingress System may write (Z3); it must
// not roll back (Z8).
enum class Role : std::uint8_t {
    State,
    Input,
};

enum class Rollback : std::uint8_t {
    No,
    Yes,
    // Recomputed each tick before every reader (spec A.1, D-E16).
    Derived,
};

// A clock domain (spec section 0). Startup schedules have none.
enum class Domain : std::uint8_t {
    None,
    Fixed,
    Frame,
    Loading,
};

// Where a System or resource may run: any thread of the schedule's runner, or
// one named thread role (spec E.3). The role "Main" is the platform thread.
struct Affinity {
    // Empty: Any.
    std::string role;

    [[nodiscard]] static Affinity Any() { return {}; }
    [[nodiscard]] static Affinity Named(std::string name) { return Affinity{std::move(name)}; }
    [[nodiscard]] bool IsAny() const noexcept { return role.empty(); }

    friend bool operator==(const Affinity&, const Affinity&) = default;
};

// The thread role a schedule's Run is called on (spec D.1 runner).
inline constexpr const char* kMainRole = "Main";

struct ComponentOptions {
    std::string name;
    Zone zone = Zone::Unset;
    WritePolicy writePolicy = WritePolicy::Exclusive;
    Role role = Role::State;
    Rollback rollback = Rollback::No;
};

struct ResourceOptions {
    std::string name;
    Zone zone = Zone::Unset;
    WritePolicy writePolicy = WritePolicy::Exclusive;
    Role role = Role::State;
    Rollback rollback = Rollback::No;
    // Exchanged with the outside of the Product (spec I.2); always zone B (Z1, Z7).
    bool external = false;
    Affinity affinity;
};

struct EventOptions {
    std::string name;
    Zone zone = Zone::Unset;
    // The clock domain the event belongs to; its writers and readers run in
    // that domain's clock schedule (spec F-9, F-E1).
    Domain domain = Domain::None;
};

// A named group of a schedule's Systems (spec C.1 sets): order edges and run
// conditions given once for every member.
struct SetOptions {
    std::string name;
    std::string schedule;
    std::vector<std::string> before;
    std::vector<std::string> after;
};

enum class ExecutorKind : std::uint8_t {
    Serial,
    // Not implemented in v1: any Parallel schedule is D-E17 (LIM-2).
    Parallel,
};

// A sync point declared inside a stage (spec D.1 syncPoints). Systems refer to
// it by name in before / after; every stage also ends with one.
struct SyncPointOptions {
    std::string name;
    std::string stage;
};

// Given by the Host (spec D.1); the kernel builds in no schedule or stage name
// (DI-KS 15).
struct ScheduleOptions {
    std::string name;
    Domain domain = Domain::None;
    // Runs once before every clock schedule; its writers do not count as
    // writers of an Exclusive column (spec B.3).
    bool startup = false;
    // Its Run advances the domain tick; one per domain (H-1, D-E12).
    bool clock = false;
    // The only schedule kind where Extract parameters are legal (spec H.6).
    bool extract = false;
    bool rollbackEnabled = false;
    std::vector<std::string> stages;
    // The stage where ingress Systems run (spec Z3). Empty: no ingress stage.
    std::string ingressStage;
    std::vector<SyncPointOptions> syncPoints;
    // The sync point after which the domain's events are trimmed (spec D.1
    // eventTrim). Empty: the end of the last stage. Clock schedules only.
    std::string eventTrim;
    ExecutorKind executor = ExecutorKind::Serial;
    // Parallel only: the CTest that compares Serial and Parallel (spec X-2).
    std::string parallelGuardTest;
    // The thread role that calls Run (spec D.1 runner).
    std::string runner = kMainRole;
};

} // namespace GYO::Kernel
