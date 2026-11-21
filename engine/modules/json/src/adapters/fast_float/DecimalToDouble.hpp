#pragma once

#include <cstdint>
#include <string_view>

// The fast_float half of the adapter, behind the Module's own words: callers
// see no fast_float type (Design Invariant J-1).
namespace GYO::Engine::Json::Detail {

enum class DecimalRead : std::uint8_t { Ok = 1, OutOfRange };

// Converts a JSON number's text to the correctly rounded double, the same on
// every platform (J-3). OutOfRange when the result is infinite, or zero while
// the text is not (J-2); the judgement is the Module's own, not fast_float's
// error report.
[[nodiscard]] DecimalRead DecimalToDouble(std::string_view text, double& out) noexcept;

} // namespace GYO::Engine::Json::Detail
