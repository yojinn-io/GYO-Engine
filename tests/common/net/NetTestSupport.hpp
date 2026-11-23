#pragma once

// Small helpers shared by the net Module's tests.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "engine/net/Connection.hpp"

namespace GYO::Test::Net {

namespace N = GYO::Engine::Net;

inline std::vector<std::byte> Bytes(std::initializer_list<int> values) {
    std::vector<std::byte> out;
    for (const int value : values) out.push_back(static_cast<std::byte>(value));
    return out;
}

// A message whose first four bytes are index (little-endian) and whose rest is a fill derived from it, so
// the receiver can tell which message it got and whether it is whole.
inline std::vector<std::byte> NumberedMessage(const std::uint32_t index, const std::size_t size) {
    std::vector<std::byte> out(size < 4 ? 4 : size);
    for (std::size_t i = 0; i < 4; ++i) out[i] = static_cast<std::byte>(index >> (8 * i));
    for (std::size_t i = 4; i < out.size(); ++i) out[i] = static_cast<std::byte>((index * 31u + i * 7u) & 0xFFu);
    return out;
}

inline std::uint32_t IndexOf(const std::vector<std::byte>& message) {
    std::uint32_t index = 0;
    for (std::size_t i = 0; i < 4 && i < message.size(); ++i) index |= std::to_integer<std::uint32_t>(message[i]) << (8 * i);
    return index;
}

inline N::ConnectionSettings TwoChannels(const std::uint32_t connectionId = 7) {
    N::ConnectionSettings settings;
    settings.connectionId = connectionId;
    settings.channels = {{N::ChannelKind::OrderedReliable, 1}, {N::ChannelKind::Unreliable, 0}};
    return settings;
}

constexpr std::size_t kReliable = 0;
constexpr std::size_t kUnreliable = 1;

// The failure code of a result, or NetFailureCode{} (no code is 0) for a success, so a wrong success fails a
// check instead of reading the error of a success.
template <class R>
N::NetFailureCode CodeOf(const R& result) {
    return result.has_value() ? N::NetFailureCode{} : result.error().code;
}

inline N::NetTime Ms(const std::int64_t ms) { return std::chrono::milliseconds(ms); }

} // namespace GYO::Test::Net
