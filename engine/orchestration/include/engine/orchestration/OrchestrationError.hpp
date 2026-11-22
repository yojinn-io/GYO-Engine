#pragma once

#include "engine/kernel/Declarations.hpp"
#include "engine/kernel/KernelError.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Errors of the orchestration layer (declaration
// docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
// sections 3.5, 3.9, 3.10). Two groups that never share a code: the startup
// codes of Product::Build (O-E1 to O-E7, collected into one
// ProductStartupReport together with every World's kernel StartupReport and the
// errors registration units report), and the runtime codes of the Product's
// entry points (OrchestrationError). The kernel's StartupCode is not extended
// (O-5).

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

// How a World broke (v8-arch-gyo2-broken-world-ends-product).
enum class WorldFailureKind : std::uint8_t {
    // A schedule the Product ran returned a kernel Err, or an exception left it:
    // the kernel interrupted the World (spec E.5).
    Interrupted = 1,
    // A DropOldFirst replacement failed after the old World was gone: the place is empty.
    ReplacementFailed,
    // std::bad_alloc left a Run or a DropOldFirst replacement.
    ResourceExhausted,
};

[[nodiscard]] std::string_view ToString(WorldFailureKind kind) noexcept;

// One World breaking ends the Product (v8-arch-gyo2-broken-world-ends-product):
// what broke, gathered by the orchestration layer and the kernel at the moment
// it broke, never read back from the broken World afterwards.
struct WorldFailure {
    std::string product;
    std::string world;
    WorldFailureKind kind = WorldFailureKind::Interrupted;
    // The Product's entry point that broke it ("RunGameInit", "RunFrame",
    // "RunSchedule", "ReplaceWorld"); "outside the Product" when the World was
    // found interrupted without the Product running it (WorldExecutor::Run called directly).
    std::string entry;
    // The schedule that failed; empty when no schedule was running (a prepare
    // function, a registration or a carry-over of a replacement failed).
    std::string schedule;
    // The World's sync point number (Kernel::World::SyncEpoch), read before the
    // schedule ran. The orchestration layer knows no clock and no game time (O-4):
    // an author who wants the tick in the report puts it in its own error (G6).
    std::uint64_t syncEpoch = 0;
    // The kernel error code, when the kernel reported one.
    std::optional<Kernel::KernelErrorCode> kernelCode;
    // The kernel error as text (its layers name the System and its step), or
    // the orchestration layer's report of a failed replacement.
    std::string where;
    // What happened before: the last schedule the Product ran with success on
    // this World; empty when none had.
    std::string lastSchedule;

    // The whole failure as one line (also the refusal's detail).
    [[nodiscard]] std::string Text() const;
};

// Runtime codes of the Product's entry points (sections 3.4, 3.9, 3.10).
enum class OrchestrationErrorCode : std::uint8_t {
    // A schedule run by RunGameInit, RunFrame or RunSchedule returned a kernel Err (in kernel).
    ScheduleFailed = 1,
    // A World of the Product is interrupted: the Product has ended, nothing runs
    // and nothing is replaced (v8-arch-gyo2-broken-world-ends-product; in failure).
    WorldInterrupted,
    // A World's place is empty (a DropOldFirst replacement failed): the Product
    // has ended, as for WorldInterrupted (in failure).
    WorldMissing,
    // No World of that name.
    UnknownWorld,
    // Replacement: registering the new World again failed (startup in the report detail).
    ReplaceRegistrationFailed,
    // Replacement: a type marked to carry over is not registered in the new World.
    ReplaceCarryOverFailed,
    // Replacement: a prepare function returned an error (its own code in reporterCode).
    ReplacePrepareFailed,
    // Replacement: a beforeSwap schedule returned a kernel Err (in kernel).
    ReplaceScheduleFailed,
    // Dynamic insert refused by the kernel (DynamicInsertRefused, in kernel).
    InsertRefused,
    // Dynamic insert: BuildPlans failed (the kernel's report in detail).
    InsertBuildFailed,
    // Dynamic insert: the owner name is a unit's or the Product's own.
    OwnerTaken,
    // What a prepare function returns to say it failed; the author's own code goes in reporterCode.
    PrepareFailed,
    // RunFrame: an exchange function returned an error (exchange declaration 5.1). The message names
    // the World, "in" or "out" and the unit; the function's own code is in reporterCode. The World is
    // not broken and the Product goes on (E2); the Worlds before it ran this round.
    ExchangeFailed,
};

[[nodiscard]] std::string_view ToString(OrchestrationErrorCode code) noexcept;

struct OrchestrationError {
    OrchestrationErrorCode code;
    std::string message;
    std::string detail;
    // The reporter's own code (a prepare function's, for example "save-version-unsupported"),
    // carried unchanged through ReplaceWorld (section 3.9).
    std::string reporterCode;
    // The kernel error behind ScheduleFailed, ReplaceScheduleFailed or InsertRefused (zero or one).
    std::vector<Kernel::KernelError> kernel;
    // Set when a World of the Product is broken (this call broke it, or an
    // earlier one did): the Product has ended.
    std::optional<WorldFailure> failure;

    OrchestrationError(OrchestrationErrorCode errorCode, std::string errorMessage, std::string errorDetail = {},
                       std::string errorReporterCode = {})
        : code(errorCode), message(std::move(errorMessage)), detail(std::move(errorDetail)),
          reporterCode(std::move(errorReporterCode)) {}
};

} // namespace GYO::Engine::Orchestration
