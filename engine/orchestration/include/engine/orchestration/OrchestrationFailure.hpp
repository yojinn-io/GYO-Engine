#pragma once

#include "engine/kernel/Declarations.hpp"
#include "engine/kernel/KernelError.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Failures of the orchestration layer (error level E2; declaration
// docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
// sections 3.5, 3.9, 3.10, and the error-levels declaration D3, D11). Two groups
// that never share a code: the startup codes of Product::Build (O-E1 to O-E7,
// collected into one ProductStartupReport together with every World's kernel
// StartupReport and the errors registration units report), and the runtime codes
// of a request to the Product that failed (OrchestrationFailure). The kernel's
// StartupCode is not extended (O-5). A broken World (E1) is WorldError.hpp.

namespace GYO::Engine::Orchestration {

// Startup codes (section 3.5).
enum class OrchestrationCode : std::uint8_t {
    // O-E1: one register row is paired with two unit instances.
    OE1 = 1,
    // O-E2: a unit instance names a row that does not exist or is library_only
    // (only a hand-written RegisteredUnits can do this).
    OE2,
    // O-E3: a dependency is missing, comes later, or a Module depends on a Plugin.
    OE3,
    // O-E4: two runners are installed.
    OE4,
    // O-E5: a list error (an insertion anchor that is not there, a name that is
    // not a schedule of that World).
    OE5,
    // O-E6: a World description error (no World, two Worlds of one name, a units
    // entry naming a unit the Product does not carry).
    OE6,
    // O-E7: a prepare function returned an error at startup.
    OE7,
};

[[nodiscard]] std::string_view ToString(OrchestrationCode code) noexcept;

struct OrchestrationStartupError {
    OrchestrationCode code;
    std::string message;
    // Every party the error names (unit, World, schedule names).
    std::vector<std::string> subjects;
};

// An error a registration unit reports from Finish or Check: its own code (a
// string, the unit's own vocabulary, never a kernel or orchestration code).
struct UnitReportedError {
    std::string unit;
    std::string world;
    std::string code;
    std::string message;
};

// One World's part of the startup report.
struct WorldStartupResult {
    std::string world;
    // The kernel's report of the World's BuildPlans, when it failed.
    std::optional<Kernel::StartupReport> kernel;
    // Set when the kernel failed: Finish, Check and the list checks did not run
    // for this World (O-5: "not checked because of a kernel error").
    bool notChecked = false;
};

enum class ProductStartupReportCode : std::uint8_t {
    StartupFailed = 1,
};

[[nodiscard]] std::string_view ToString(ProductStartupReportCode code) noexcept;

// The Err side of Product::Build: everything found, in one report (O-5). A
// prepare error (O-E7) comes alone: prepare functions run on built Worlds, so
// only when everything else passed.
struct ProductStartupReport {
    ProductStartupReportCode code = ProductStartupReportCode::StartupFailed;
    std::string message;
    // The whole report as text (one line per error).
    std::string detail;
    std::vector<WorldStartupResult> worlds;
    std::vector<OrchestrationStartupError> errors;
    std::vector<UnitReportedError> unitErrors;

    ProductStartupReport(std::vector<WorldStartupResult> worldResults, std::vector<OrchestrationStartupError> found,
                         std::vector<UnitReportedError> reported);

    [[nodiscard]] bool Has(OrchestrationCode wanted) const noexcept;
    [[nodiscard]] std::size_t Count(OrchestrationCode wanted) const noexcept;
    // The World's kernel report, or nullptr when its BuildPlans succeeded or it was not built.
    [[nodiscard]] const Kernel::StartupReport* KernelReportOf(std::string_view world) const noexcept;
};

// Runtime codes of a request to the Product that failed while the Product stayed
// healthy (error level E2, error-levels declaration D3 and D11: a type named
// ...Failure lets the caller decide). A broken World is not a code here: it is
// WorldError (E1, WorldError.hpp), whose code says interrupted or empty.
enum class OrchestrationFailureCode : std::uint8_t {
    // Replacement: registering the new World again failed (startup in the report detail).
    ReplaceRegistrationFailed = 1,
    // Replacement: a type marked to carry over is not registered in the new World.
    ReplaceCarryOverFailed,
    // Replacement: a prepare function returned an error (its own code in reporterCode).
    ReplacePrepareFailed,
    // Replacement: a schedule on the new World returned a kernel Err (in kernel).
    ReplaceScheduleFailed,
    // Dynamic insert refused by the kernel (DynamicInsertRefused, in kernel).
    InsertRefused,
    // Dynamic insert: BuildPlans failed (the kernel's report in detail).
    InsertBuildFailed,
    // Dynamic insert: the owner name is a unit's or the Product's own.
    OwnerTaken,
    // What a prepare function returns to say it failed; the author's own code goes in reporterCode.
    PrepareFailed,
};

[[nodiscard]] std::string_view ToString(OrchestrationFailureCode code) noexcept;

// E2: this request failed; the Product and every World are healthy. A prepare
// function returns it too: it has no way to end the Product (error levels P16).
struct OrchestrationFailure {
    OrchestrationFailureCode code;
    std::string message;
    std::string detail;
    // The reporter's own code (a prepare function's, for example "save-version-unsupported"),
    // carried unchanged through ReplaceWorld (section 3.9).
    std::string reporterCode;
    // The kernel error behind ReplaceScheduleFailed or InsertRefused (zero or one).
    std::vector<Kernel::KernelError> kernel;

    OrchestrationFailure(OrchestrationFailureCode failureCode, std::string failureMessage, std::string failureDetail = {},
                         std::string failureReporterCode = {})
        : code(failureCode), message(std::move(failureMessage)), detail(std::move(failureDetail)),
          reporterCode(std::move(failureReporterCode)) {}
};

} // namespace GYO::Engine::Orchestration
