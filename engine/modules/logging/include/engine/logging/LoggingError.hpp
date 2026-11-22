#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace GYO::Engine::Logging {

// Failure classes of the logging Module. Zero is not a valid code; the numeric
// values are not a data contract.
enum class LoggingErrorCode : std::uint16_t {
    // A file sink's file could not be opened for writing.
    OpenFailed = 1,
    // A file sink's writer thread could not start.
    StartFailed,
};

[[nodiscard]] constexpr const char* ToString(const LoggingErrorCode code) noexcept {
    switch (code) {
    case LoggingErrorCode::OpenFailed: return "OpenFailed";
    case LoggingErrorCode::StartFailed: return "StartFailed";
    }
    return "Unknown";
}

using LoggingError = ::GYO::Engine::Base::Error<LoggingErrorCode>;
static_assert(::GYO::Engine::Base::CodedError<LoggingError>);

template <class T>
using LoggingResult = ::GYO::Engine::Base::Result<T, LoggingError>;

} // namespace GYO::Engine::Logging
