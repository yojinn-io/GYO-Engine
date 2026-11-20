// Text of a broken World's error (error-levels declaration D3, D11).

#include "engine/orchestration/WorldError.hpp"

namespace GYO::Engine::Orchestration {

std::string_view ToString(WorldErrorKind kind) noexcept {
    switch (kind) {
    case WorldErrorKind::Interrupted: return "Interrupted";
    case WorldErrorKind::ReplacementFailed: return "ReplacementFailed";
    case WorldErrorKind::ResourceExhausted: return "ResourceExhausted";
    }
    return "unknown";
}

std::string WorldError::Text() const {
    std::string text = "Product '" + product + "' ended: World '" + world + "' " + std::string(ToString(code)) +
                       " in " + entry;
    if (!schedule.empty()) {
        text += ", schedule '" + schedule + "' (sync point " + std::to_string(syncEpoch) + ")";
    }
    if (kernelCode) text += ", kernel " + std::string(Kernel::ToString(*kernelCode));
    text += ", last schedule run: " + (lastSchedule.empty() ? std::string("none") : "'" + lastSchedule + "'");
    if (!where.empty()) text += "; " + where;
    return text;
}

std::string_view ToString(WorldErrorOrFailureCode code) noexcept {
    switch (code) {
    case WorldErrorOrFailureCode::WorldError: return "WorldError";
    case WorldErrorOrFailureCode::Failure: return "Failure";
    }
    return "?";
}

} // namespace GYO::Engine::Orchestration
