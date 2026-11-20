#pragma once

#include <string_view>

#include "engine/logging/Level.hpp"
#include "engine/logging/RecordBuilder.hpp"
#include "engine/world_time/TimeTypes.hpp"

// How TimeDiscarded is written as a log record (logging declaration docs/architecture/target/v2/
// logging-declaration.zh-Hant.md, question 1, approved: D). The time Module describes its own message here;
// whether a World logs it is the wiring's choice, one line in its build:
//     WorldLogging::Forward<WorldTime::TimeDiscardedLog>(setup, {.schedule = "Main", .stage = "Last"});
// The time Module itself depends only on the record types of engine/logging, not on the World end of it.

namespace GYO::Engine::WorldTime {

struct TimeDiscardedLog final {
    using Message = TimeDiscarded;
    static constexpr std::string_view name = "TimeDiscarded";
    static constexpr std::string_view category = "world_time";
    static constexpr Logging::Level level = Logging::Level::Debug;
    // Text: "discarded <amount> ns (<cause>)"; fields amount_ns, real_delta_ns, cause.
    static void Describe(const TimeDiscarded& message, Logging::RecordBuilder& out);
};

} // namespace GYO::Engine::WorldTime
