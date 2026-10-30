#pragma once
#include <cstdint>
#include <limits>
#include <optional>

namespace fps::pvp {
// protocol-v8 §2a: every counter that crosses the wire (tick, player id,
// epoch, life, command sequence, action id) ends at the uint32 maximum,
// although C++ keeps it in std::uint64_t. Each exhaustion site keeps its
// own behaviour and decides with these functions instead of the uint64 limit.
// Domain code includes this header directly: Wire.hpp needs GYO::Net.
inline constexpr std::uint64_t WireCounterMax = (std::numeric_limits<std::uint32_t>::max)();

constexpr bool WireCounterExhausted(std::uint64_t value) noexcept { return value >= WireCounterMax; }

// value + amount, or nullopt when the sum would pass WireCounterMax.
constexpr std::optional<std::uint64_t> WireCounterAdd(std::uint64_t value, std::uint64_t amount) noexcept {
    if (value > WireCounterMax || amount > WireCounterMax - value) return std::nullopt;
    return value + amount;
}
}
