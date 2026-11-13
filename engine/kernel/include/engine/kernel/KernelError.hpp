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
// rule names the broken rule ("W-5", "F-10", "Z5", "S-13", ...); world names the
// World the error was found in (spec A.6: a violation carries the World's
// identity). The code enum is the kernel module's own (no global error enum).
//
// StartupReport: every startup error of one Registry::Finish, collected and
// reported at once (spec D.2, D-5). A startup error is a Runtime Error of the
// composer (the plugin list and declarations are wrong): the Host refuses to
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
    // Spec F.1, F-10: untyped Commands on a zone C or Exclusive column.
    UntypedWrite,
    // Spec Z5: a zone B emitter despawns an entity with zone C columns (when recorded, or the sync point's recheck).
    ZoneCDespawn,
    // Spec S-13: an ExclusiveWorld write outside zone B Shared data, or an untyped spawn.
    ExclusiveWorldWrite,
    // Spec A.1, W-9: the allotter's range is used up (reported, never wrapped).
    AllotterExhausted,
    // Spec W-1 checked after a sync point with verification on.
    StructureBroken,
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
    // The whole chain as text, one line per layer and sibling (what the Executor's handler logs).
    [[nodiscard]] std::string Text() const;
};

// The startup error codes of spec D.2's table (D-E1 to D-E19, R-E1 to R-E6,
// X-E1 to X-E3, F-E1). The enum value is not part of any fingerprint or file:
// the text form (ToString) is.
enum class StartupCode : std::uint8_t {
    DE1 = 1, DE2, DE3, DE4, DE5, DE6, DE7, DE8, DE9, DE10,
    DE11, DE12, DE13, DE14, DE15, DE16, DE17, DE18, DE19,
    RE1, RE2, RE3, RE4, RE5, RE6,
    XE1, XE2, XE3,
    FE1,
};

[[nodiscard]] std::string_view ToString(StartupCode code) noexcept;

struct StartupError {
    // The code of this error.
    StartupCode code;
    // Spec D.2's step the error was found in, when the step has its own code
    // distinct from code (affinity: code X-E2 or X-E3, step D-E10; a (c)
    // Parallel schedule without a guard: code X-E1, step D-E11). Otherwise
    // equal to code.
    StartupCode step;
    // The rule broken, for people: "Z1", "S-12", "H-1", ...
    std::string rule;
    std::string message;
    // Every party the error names (System, component, plugin, schedule names),
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

// The Err side of Registry::Finish: all errors, in the order the analysis found
// them (deterministic: plugin list order, registration order, schedule order).
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
