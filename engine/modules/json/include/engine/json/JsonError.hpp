#pragma once

#include <cstdint>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace GYO::Engine::Json {

// Failure classes of the JSON Module. Zero is not a valid code; the numeric
// values are not a data contract.
enum class JsonErrorCode : std::uint16_t {
    // Not JSON: grammar, invalid UTF-8 or escapes, a byte order mark, trailing
    // content, empty text.
    Syntax = 1,
    // A key appears twice in one object (compared after unescaping).
    RepeatedKey,
    // A number overflows a double, non-zero text rounds to zero, or an integer
    // read does not fit the requested type.
    NumberOutOfRange,
    // A value read as a kind it is not.
    WrongKind,
    // The Writer was given a value JSON cannot represent (a non-finite number,
    // invalid UTF-8).
    NotRepresentable,
    // Parsing or writing could not allocate memory.
    OutOfMemory,
};

[[nodiscard]] const char* ToString(JsonErrorCode code) noexcept;

using JsonError = ::GYO::Engine::Base::Error<JsonErrorCode>;
static_assert(::GYO::Engine::Base::CodedError<JsonError>);

template <class T>
using JsonResult = ::GYO::Engine::Base::Result<T, JsonError>;

} // namespace GYO::Engine::Json
