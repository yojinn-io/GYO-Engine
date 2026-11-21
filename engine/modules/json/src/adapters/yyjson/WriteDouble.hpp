#pragma once

#include <cstddef>

// The number half of the Writer, behind the Module's own words (J-1): the
// shortest round-trip text of a finite double, written by yyjson's number
// writer (one implementation on every platform; question 2 of the
// declaration's section 12).
namespace GYO::Engine::Json::Detail {

inline constexpr std::size_t kDoubleTextCapacity = 40;

// Writes the text of a finite double into buffer and returns its length.
// A non-finite value is a Programmer Error: the Writer rejects it first.
[[nodiscard]] std::size_t WriteDouble(double value, char (&buffer)[kDoubleTextCapacity]);

} // namespace GYO::Engine::Json::Detail
