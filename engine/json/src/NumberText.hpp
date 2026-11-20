#pragma once

#include <cstdint>
#include <string_view>

// Reading the raw text of a JSON number, without any external component. The
// text has passed JSON's number grammar (yyjson checks it) before it gets here.
namespace GYO::Engine::Json::Detail {

// Written without '.', 'e' or 'E'.
[[nodiscard]] bool IsIntegerLiteral(std::string_view text) noexcept;

// Some digit before the exponent is not 0: the value is not zero, so a
// conversion that gives zero has underflowed (J-2).
[[nodiscard]] bool HasNonZeroSignificand(std::string_view text) noexcept;

enum class IntegerRead : std::uint8_t { Ok = 1, OutOfRange };

// Integer literals only (IsIntegerLiteral). "-0" reads as 0 for both; a
// negative literal is OutOfRange for the unsigned read.
[[nodiscard]] IntegerRead ReadInt64(std::string_view text, std::int64_t& out) noexcept;
[[nodiscard]] IntegerRead ReadUint64(std::string_view text, std::uint64_t& out) noexcept;

} // namespace GYO::Engine::Json::Detail
