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
// target/system-contract section 2). Result<void, KernelError> is the return
// contract of every System, not an error channel: Ok is the normal path. The
// Executor, the context chain and sibling lists arrive with world-exec; this
// batch only fixes the type a System signature must return (spec S-2, J-10).
//
// StartupReport: every startup error of one Registry::Finish, collected and
// reported at once (spec D.2, D-5). A startup error is a Runtime Error of the
// composer (the plugin list and declarations are wrong): the Host refuses to
// start and decides what to print (spec D.4).

namespace GYO::Kernel {

enum class KernelErrorCode : std::uint8_t {
    // Spec F-10 / S-13 / Z5 and the access verification (spec A.6) fill this
    // list in world-exec.
    SystemFailed = 1,
};

[[nodiscard]] std::string_view ToString(KernelErrorCode code) noexcept;

struct KernelError {
    KernelErrorCode code;
    std::string message;
    std::string detail;

    KernelError(KernelErrorCode errorCode, std::string errorMessage, std::string errorDetail = {})
        : code(errorCode), message(std::move(errorMessage)), detail(std::move(errorDetail)) {}
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
