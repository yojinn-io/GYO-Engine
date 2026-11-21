#pragma once

#include "engine/base/Error.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Two error types of the kernel.
//
// KernelError: what a System returns on its Err side (target/kernel DI-KS 2,
// target/system-contract section 2, target/error-handling.md). Result<void,
// KernelError> is the return contract of every System, not an error channel:
// Ok is the normal path. KernelError is the one kernel error type with a chain
// (target/error-handling "Relation to no error chains"), in two directions:
//   context   the context chain, vertical: each layer on the way out (kernel
//             API, System, Executor) adds one line, innermost first;
//   siblings  the sibling list, horizontal: independent errors of one System
//             call (each access-verification violation, the System's own Err)
//             side by side, never wrapped into one another (code Siblings).
// rule names the broken rule ("W-5", "W-9", "X-11", ...); world names the
// World the error was found in (spec A.6: a violation carries the World's
// identity). The code enum is the kernel module's own (no global error enum).
//
// StartupReport: every startup error of one Registry::BuildPlans,
// collected and reported at once (spec D.2, D-5). A startup error is a Runtime Error of the
// composer (the registrations are wrong): the Host refuses to
// start and decides what to print (spec D.4).

namespace GYO::Kernel {

enum class KernelErrorCode : std::uint8_t {
    // A System's own Err (the System's code chose this one).
    SystemFailed = 1,
    // Several independent errors of one call; see siblings.
    Siblings,
    // Access verification layer 1 (spec A.6, W-5): an undeclared (id, mode) through the World API.
    AccessViolation,
    // Access verification layer 2 after a call (spec A.6, E.2 step 5): storage the System did not declare as
    // written changed.
    BoundaryChange,
    // Access verification layer 2 before a call (spec A.6, E.2 step 0): storage changed outside the World API
    // since the last comparison; blamed on the previous step or the Ready window, not on the next System.
    PreCallChange,
    // Spec W-4: a handle used outside the System call that fetched it, or after a sync point.
    StaleHandle,
    // Spec A.1, W-9: the allotter's range is used up (reported, never wrapped).
    AllotterExhausted,
    // Spec W-1 checked after a sync point with verification on.
    StructureBroken,
    // Spec F-12: the hook rounds after the last sync point of a Run did not empty
    // the schedule's hook queue within the limit.
    HookRoundLimit,
    // The codes below come with nested runs (ExclusiveWorld::RunSchedule; K0,
    // v8-arch-gyo2-nested-schedule). Each refusal interrupts the World and is
    // reported once, by the outermost Run.
    // Spec X-11: the schedule is running already (this level or an outer one).
    ScheduleRunning,
    // Spec X-12: no schedule of that name is registered.
    UnknownSchedule,
    // Spec X-15: an extract schedule is not run from inside a System.
    ExtractNotNestable,
    // Spec X-13: a hook runs while a sync point applies its records, so it runs no schedule.
    HookRunsSchedule,
    // Spec X-14: a schedule is not run from inside a Read or Modify closure.
    ClosureRunsSchedule,
    // Spec X-16: an exception left a nested run (the outer System caught it).
    NestedRunAborted,
    // DI-KS 26 (v8-arch-gyo2-dynamic-insert-per-world): Registry::Register while a
    // live World made from the Registry declares no dynamic insert
    // (WorldOptions::dynamicInsert = false). Nothing was registered; the error goes
    // back to the caller and the kernel logs nothing (CCR-3; error levels D2).
    DynamicInsertRefused,
    // The codes below come with ExclusiveWorld's immediate structural changes
    // (declaration v8-arch-gyo2-kernel-subtract-structure 3.5; DI-KS 18, 21).
    // ExclusiveWorld::Spawn: the id the World's allotter gave is alive. Nothing
    // is spawned and the spawn is recorded as a RejectedSpawn (spec F-13,
    // fromAllotter). Returned to the System only: like a spawn dropped at a sync
    // point it is no error of the Run (it is not added to the call's errors), and
    // the World is not interrupted.
    AllottedIdAlive,
    // ExclusiveWorld::Spawn with no allotter declared (D-E3 cannot see at
    // startup that an exclusive System spawns). The change is not made and the
    // call goes on; the Run ends in this Err when the System returns (DI-KS 2).
    NoAllotter,
    // An immediate change inside a Read or Modify closure: the reference the
    // closure holds would not survive it (like X-14). The World is interrupted.
    ClosureChangesMembership,
    // An immediate change through a hook's ExclusiveWorld<>: a hook runs while
    // records are applied (like X-13). The World is interrupted.
    HookChangesMembership,
    // An exception left an immediate change (a hook or an InitialValue function
    // it ran threw) and the System caught it (like X-16, NestedRunAborted). The
    // World was interrupted when the exception left the change.
    ImmediateChangeAborted,
    // World::Summary: a type with a value in the World has no hash function
    // (Hash.hpp; authority-rollback declaration 3.3, the user's decision (d)).
    // detail names every such type. The World is not interrupted.
    HashMissing,
};

[[nodiscard]] std::string_view ToString(KernelErrorCode code) noexcept;

struct KernelError {
    KernelErrorCode code;
    std::string message;
    std::string detail;
    std::string rule;
    std::string world;
    std::vector<std::string> context;
    std::vector<KernelError> siblings;

    KernelError(KernelErrorCode errorCode, std::string errorMessage, std::string errorDetail = {})
        : code(errorCode), message(std::move(errorMessage)), detail(std::move(errorDetail)) {}

    // Adds one layer of context (the context chain) and returns *this.
    KernelError& Wrap(std::string layer) & {
        context.push_back(std::move(layer));
        return *this;
    }
    KernelError&& Wrap(std::string layer) && {
        context.push_back(std::move(layer));
        return std::move(*this);
    }

    // Every leaf error: this one, or each sibling's leaves for code Siblings.
    [[nodiscard]] std::vector<const KernelError*> Leaves() const;
    // Whether any leaf has this code.
    [[nodiscard]] bool Has(KernelErrorCode wanted) const;
    // The whole chain as text, one line per layer and sibling (the whole chain).
    [[nodiscard]] std::string Text() const;
};

// The startup error codes of spec D.2's table that the kernel still has: its
// kept list and the codes later batches take out (DI-KS 15; the guard
// "DI-KS 15: the StartupCode values ..." in tests/common/kernel/RegistryTests.cpp
// holds both lists). The enum value is not part of any fingerprint or file: the
// text form (ToString) is.
enum class StartupCode : std::uint8_t {
    DE1 = 1, DE3, DE4, DE5, DE6, DE7,
    DE14, DE15, DE16, DE17, DE19, DE20,
    RE2, RE4, RE5,
    // A later round of Registry::Register / BuildPlans registered what only the
    // first round may (declaration v8-arch-gyo2-kernel-subtract-structure 3.2 rule 6).
    RE7,
};

[[nodiscard]] std::string_view ToString(StartupCode code) noexcept;

struct StartupError {
    // The code of this error.
    StartupCode code;
    // Spec D.2's step the error was found in, when the step has its own code
    // distinct from code. No code has its own step since X-E1 and D-E11 left
    // (v8-arch-gyo2-authority-rollback step 2): equal to code.
    StartupCode step;
    // The rule broken, for people: "Z6", "H-1", ...
    std::string rule;
    std::string message;
    // Every party the error names (System, component, owner, schedule names),
    // so a report always says who is involved (spec D-5: "names both sides").
    std::vector<std::string> subjects;
    // Registration sites of the Systems and hooks involved, "file:line".
    std::vector<std::string> sources;

    [[nodiscard]] bool Is(StartupCode wanted) const noexcept { return code == wanted || step == wanted; }
};

enum class StartupReportCode : std::uint8_t {
    StartupFailed = 1,
};

[[nodiscard]] std::string_view ToString(StartupReportCode code) noexcept;

// The Err side of Registry::BuildPlans and Finish: all errors, in the order the analysis found
// them (deterministic: owner order, registration order, schedule order).
struct StartupReport {
    StartupReportCode code;
    std::string message;
    std::string detail;
    std::vector<StartupError> errors;

    explicit StartupReport(std::vector<StartupError> found);

    [[nodiscard]] bool Has(StartupCode wanted) const noexcept;
    [[nodiscard]] std::size_t Count(StartupCode wanted) const noexcept;
};

} // namespace GYO::Kernel
