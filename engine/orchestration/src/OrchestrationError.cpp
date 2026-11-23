// Text of the orchestration layer's error codes and the startup report
// (declaration section 3.5).

#include "engine/orchestration/OrchestrationError.hpp"

#include <algorithm>

namespace GYO::Engine::Orchestration {

std::string_view ToString(OrchestrationCode code) noexcept {
    switch (code) {
    case OrchestrationCode::OE1: return "O-E1";
    case OrchestrationCode::OE2: return "O-E2";
    case OrchestrationCode::OE3: return "O-E3";
    case OrchestrationCode::OE4: return "O-E4";
    case OrchestrationCode::OE5: return "O-E5";
    case OrchestrationCode::OE6: return "O-E6";
    case OrchestrationCode::OE7: return "O-E7";
    case OrchestrationCode::OE9: return "O-E9";
    }
    return "O-E?";
}

std::string_view ToString(ProductStartupReportCode code) noexcept {
    switch (code) {
    case ProductStartupReportCode::StartupFailed: return "StartupFailed";
    }
    return "?";
}

std::string_view ToString(WorldFailureKind kind) noexcept {
    switch (kind) {
    case WorldFailureKind::Interrupted: return "Interrupted";
    case WorldFailureKind::ReplacementFailed: return "ReplacementFailed";
    case WorldFailureKind::ResourceExhausted: return "ResourceExhausted";
    }
    return "unknown";
}

std::string WorldFailure::Text() const {
    std::string text = "Product '" + product + "' ended: World '" + world + "' " + std::string(ToString(kind)) +
                       " in " + entry;
    if (!schedule.empty()) {
        text += ", schedule '" + schedule + "' (sync point " + std::to_string(syncEpoch) + ")";
    }
    if (kernelCode) text += ", kernel " + std::string(Kernel::ToString(*kernelCode));
    text += ", last schedule run: " + (lastSchedule.empty() ? std::string("none") : "'" + lastSchedule + "'");
    if (!where.empty()) text += "; " + where;
    return text;
}

std::string_view ToString(OrchestrationErrorCode code) noexcept {
    switch (code) {
    case OrchestrationErrorCode::ScheduleFailed: return "ScheduleFailed";
    case OrchestrationErrorCode::WorldInterrupted: return "WorldInterrupted";
    case OrchestrationErrorCode::WorldMissing: return "WorldMissing";
    case OrchestrationErrorCode::UnknownWorld: return "UnknownWorld";
    case OrchestrationErrorCode::ReplaceRegistrationFailed: return "ReplaceRegistrationFailed";
    case OrchestrationErrorCode::ReplaceCarryOverFailed: return "ReplaceCarryOverFailed";
    case OrchestrationErrorCode::ReplacePrepareFailed: return "ReplacePrepareFailed";
    case OrchestrationErrorCode::ReplaceScheduleFailed: return "ReplaceScheduleFailed";
    case OrchestrationErrorCode::InsertRefused: return "InsertRefused";
    case OrchestrationErrorCode::InsertBuildFailed: return "InsertBuildFailed";
    case OrchestrationErrorCode::OwnerTaken: return "OwnerTaken";
    case OrchestrationErrorCode::PrepareFailed: return "PrepareFailed";
    case OrchestrationErrorCode::ExchangeFailed: return "ExchangeFailed";
    }
    return "?";
}

ProductStartupReport::ProductStartupReport(std::vector<WorldStartupResult> worldResults,
                                           std::vector<OrchestrationStartupError> found,
                                           std::vector<UnitReportedError> reported)
    : worlds(std::move(worldResults)), errors(std::move(found)), unitErrors(std::move(reported)) {
    std::size_t kernelFailures = 0;
    for (const WorldStartupResult& world : worlds) {
        if (!world.kernel) continue;
        ++kernelFailures;
        detail += "World '" + world.world + "': kernel startup errors\n";
        for (const Kernel::StartupError& error : world.kernel->errors) {
            detail += "  " + std::string(Kernel::ToString(error.code)) + " (" + error.rule + "): " + error.message + "\n";
        }
        if (world.notChecked) detail += "  not checked because of the kernel errors: Finish, Check, lists\n";
    }
    for (const OrchestrationStartupError& error : errors) {
        detail += std::string(ToString(error.code)) + ": " + error.message + "\n";
    }
    for (const UnitReportedError& error : unitErrors) {
        detail += "unit '" + error.unit + "' on World '" + error.world + "' reports " + error.code + ": " + error.message + "\n";
    }
    message = "Product startup failed: " + std::to_string(errors.size()) + " orchestration error(s), " +
              std::to_string(kernelFailures) + " World(s) with kernel errors, " + std::to_string(unitErrors.size()) +
              " unit error(s)";
}

bool ProductStartupReport::Has(OrchestrationCode wanted) const noexcept {
    return Count(wanted) > 0;
}

std::size_t ProductStartupReport::Count(OrchestrationCode wanted) const noexcept {
    return static_cast<std::size_t>(std::count_if(errors.begin(), errors.end(),
                                                  [wanted](const OrchestrationStartupError& e) { return e.code == wanted; }));
}

const Kernel::StartupReport* ProductStartupReport::KernelReportOf(std::string_view world) const noexcept {
    for (const WorldStartupResult& result : worlds) {
        if (result.world == world && result.kernel) return &*result.kernel;
    }
    return nullptr;
}

} // namespace GYO::Engine::Orchestration
