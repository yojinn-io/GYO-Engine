#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The explicit half of a declaration (spec B.2): what an owner states when it
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

// The determinism zone (spec G; target/determinism.md section 2). A zone left
// Unset is zone B: a product that has no (c) data never writes a zone. A
// product that needs every zone written out calls
// Registrar::RequireExplicitZones; Unset is then reported at finish (D-E7,
// spec S-6), so a forgotten zone never silently becomes (b).
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

// A clock domain (spec section 0).
enum class Domain : std::uint8_t {
    None,
    Fixed,
    Frame,
    Loading,
};

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
    // Bound to the thread that puts the value in (DI-KS 14; Bevy's NonSend): a
    // touch from any other thread is a GYO_ASSERT, destroying the World with a
    // value included; removing the value forgets the thread. Which thread runs
    // what is the orchestration layer's (v8-arch-gyo2-thread-binding-by-runner).
    bool threadBound = false;
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
    // The most hook rounds after the last sync point of a Run (spec F-12). Empty:
    // the World's KernelSettings::hookRoundLimit. 0 is D-E20. In the Plan
    // fingerprint and, for a schedule that runs a zone C System, the C fingerprint.
    std::optional<std::uint32_t> hookRoundLimit;
};

} // namespace GYO::Kernel
