#pragma once

#include "engine/kernel/KernelError.hpp"
#include "engine/orchestration/OrchestrationFailure.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

// A broken World ends the Product (v8-arch-gyo2-broken-world-ends-product; error
// level E1, error-levels declaration D3 and D11: a type named ...Error cannot be
// recovered from; whoever receives it stops calling the Product's entry points).
// Not Java's Error (that is GYO's E0, an assertion) and not Go's XxxError (that
// is mostly GYO's ...Failure).

namespace GYO::Engine::Orchestration {

// How a World broke.
enum class WorldErrorKind : std::uint8_t {
    // A schedule the Product ran returned a kernel Err, or an exception left it:
    // the kernel interrupted the World (spec E.5).
    Interrupted = 1,
    // A DropOldFirst replacement failed after the old World was gone: the place is empty.
    ReplacementFailed,
    // std::bad_alloc left a Run or a DropOldFirst replacement.
    ResourceExhausted,
};

[[nodiscard]] std::string_view ToString(WorldErrorKind kind) noexcept;

// What broke, gathered by the orchestration layer and the kernel at the moment it
// broke, never read back from the broken World afterwards. The Product keeps one;
// every entry point refused afterwards returns a copy of it (O-11).
struct WorldError {
    // How the World broke (interrupted, or its place empty).
    WorldErrorKind code;
    // "World 'X' is interrupted: the Product has ended" (or "is empty"); set by the
    // Product when it hands the error out.
    std::string message;
    // Text(), set with message.
    std::string detail;
    std::string product;
    std::string world;
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
    // The request failure that became this error (E2 raised to E1): the step of a
    // DropOldFirst replacement that failed after the old World was gone, with its
    // code, reporterCode and detail unchanged (O-5).
    std::optional<OrchestrationFailure> cause;
    // The kernel error that interrupted the World, when there was one.
    std::optional<Kernel::KernelError> kernel;

    explicit WorldError(WorldErrorKind kind) noexcept : code(kind) {}

    // The whole error as one line (also detail).
    [[nodiscard]] std::string Text() const;
};

enum class WorldErrorOrFailureCode : std::uint8_t {
    // A World is broken: the Product has ended (Error()).
    WorldError = 1,
    // This request failed; the Product is healthy (Failure()).
    Failure,
};

[[nodiscard]] std::string_view ToString(WorldErrorOrFailureCode code) noexcept;

// What ReplaceWorld and InsertSystems return: either level (error-levels D3 (b)).
// The name carries both suffixes because it can be either (D11).
class WorldErrorOrFailure {
public:
    WorldErrorOrFailureCode code;
    // The held error's message and detail.
    std::string message;
    std::string detail;

    WorldErrorOrFailure(WorldError error)
        : code(WorldErrorOrFailureCode::WorldError), message(error.message), detail(error.detail),
          value_(std::move(error)) {}
    WorldErrorOrFailure(OrchestrationFailure failure)
        : code(WorldErrorOrFailureCode::Failure), message(failure.message), detail(failure.detail),
          value_(std::move(failure)) {}

    // The broken World's error, or nullptr when this is a request failure.
    [[nodiscard]] const WorldError* Error() const noexcept { return std::get_if<WorldError>(&value_); }
    // The request failure, or nullptr when a World is broken.
    [[nodiscard]] const OrchestrationFailure* Failure() const noexcept {
        return std::get_if<OrchestrationFailure>(&value_);
    }

private:
    std::variant<WorldError, OrchestrationFailure> value_;
};

} // namespace GYO::Engine::Orchestration
