#pragma once

#include <cstdint>

#include "engine/logging/WriteFatalRecord.hpp"
#include "engine/orchestration/OrchestrationError.hpp"
#include "engine/orchestration/Setup.hpp"

// Entrance ⑤ of section 3 of docs/architecture/target/v2/logging-declaration.zh-Hant.md: the structured
// error of a broken World, handed to logging directly by the runner or main (the orchestration layer does
// not write logs, O-11). A WorldFailure cannot be mended: the runner writes it once and exits, calling no
// entry point of the Product again (user, 2026-11-20: "log it, then exit"; nothing is de-duplicated,
// because nothing should repeat). Three ways to call it (section 5.6):
//
//     // (1) an entry point returns the error:
//     auto ran = product.RunFrame();
//     if (!ran && ran.error().failure) {
//         WorldLogging::WriteFailure(*ran.error().failure);
//         return {Orchestration::RunnerExitCode::Failure, "a World broke"};
//     }
//     // (2) a System throws: RunFrame does not return, the exception leaves it.
//     try { ran = product.RunFrame(); } catch (...) {
//         if (const auto* f = product.Failure()) {
//             WorldLogging::WriteFailure(*f);
//             return {Orchestration::RunnerExitCode::Failure, "a World broke"};
//         }
//         throw;  // not a broken World
//     }
//     // (3) the default runner: Product::Run returns only RunnerExit; main looks at Failure() and exits.
//     const auto exit = product->Run();
//     if (const auto* f = product->Failure()) { WorldLogging::WriteFailure(*f); return 1; }
//
// It reads only the WorldFailure it is given (gathered by the orchestration layer when the World broke),
// never the World or the Kernel, and writes one Fatal record of category "world" through WriteFatalRecord.
// Each call writes. Never throws. Design Invariant LG-2.

namespace GYO::Engine::WorldLogging {

Logging::FatalWriteReport WriteFailure(const Orchestration::WorldFailure& failure) noexcept;

// Wraps a runner in "log it, then exit" (question 2 (d), approved: each product's runner or main wires it;
// this is the helper). The wrapped runner returns when an entry point gives it a WorldFailure; when the
// Product has a failure, the wrapper writes it once and returns a Failure exit. An exception that leaves the
// wrapped runner while the Product has a failure is written the same way and becomes a Failure exit; an
// exception without one is thrown on.
[[nodiscard]] Orchestration::Runner WithFailureLog(Orchestration::Runner runner);

} // namespace GYO::Engine::WorldLogging
