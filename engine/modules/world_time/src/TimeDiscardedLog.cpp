#include "engine/world_time/TimeDiscardedLog.hpp"

#include <string>

namespace GYO::Engine::WorldTime {

namespace {

std::string_view CauseName(const DiscardCause cause) noexcept {
    switch (cause) {
    case DiscardCause::None: return "None";
    case DiscardCause::StepLimit: return "StepLimit";
    case DiscardCause::FrameLimit: return "FrameLimit";
    case DiscardCause::NotDriven: return "NotDriven";
    }
    return "Unknown";
}

} // namespace

void TimeDiscardedLog::Describe(const TimeDiscarded& message, Logging::RecordBuilder& out) {
    const std::string_view cause = CauseName(message.cause);
    out.Text("discarded " + std::to_string(message.amount) + " ns (" + std::string(cause) + ")");
    out.Add("amount_ns", message.amount);
    out.Add("real_delta_ns", message.realDelta);
    out.Add("cause", cause);
}

} // namespace GYO::Engine::WorldTime
