#pragma once

// The time Module's errors, kept apart from the code that returns them (as every
// Module keeps its errors in its own <Module>Error.hpp).

#include "engine/base/Error.hpp"

#include <cstdint>

namespace Engine::Time {

enum class TimeErrorCode : std::uint16_t {
    // The operating system refused the wait primitive (kqueue, timer or event).
    WaitPrimitiveUnavailable = 1,
};

[[nodiscard]] constexpr const char* ToString(const TimeErrorCode code) noexcept {
    switch (code) {
    case TimeErrorCode::WaitPrimitiveUnavailable: return "WaitPrimitiveUnavailable";
    }
    return "Unknown";
}

using TimeError = Base::Error<TimeErrorCode>;
static_assert(Base::CodedError<TimeError>);

} // namespace Engine::Time
