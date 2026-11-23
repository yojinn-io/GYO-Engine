#pragma once

#include <optional>
#include <string_view>

namespace object_connect::adapters {

// The whole text read as one decimal number (std::chars_format::general:
// optional '-', digits, optional fraction, optional exponent; no '+', no
// leading whitespace), rounded to the nearest float; nullopt when any of the
// text is left over or the text is not a number. Infinity and NaN spellings are
// returned as such; callers check finiteness.
//
// The floating-point std::from_chars is not available for the macOS 13.3
// deployment target, so the conversion is fast_float's, and its header appears
// only in this directory.
[[nodiscard]] std::optional<float> DecimalToFloat(std::string_view text) noexcept;

} // namespace object_connect::adapters
