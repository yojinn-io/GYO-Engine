#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

namespace Engine::Net {

// The GYOP datagram header: the engine's UDP transport contract, shared byte
// for byte with the Go gateway framing (services/gyo_gateway/framing).
// Big-endian fields:
//   @0  "GYOP" magic
//   @4  Version  u16 (the caller's protocol version)
//   @6  Type     u16 (the caller's message kind)
//   @8  Session  u64
//   @16 Sequence u32
//   @20 payload length u16 (must equal the bytes after the header)
//   @22 Channel  u16 (v1 has only channel 0, unreliable sequenced)
// The engine checks transport conditions only. Which versions and message
// types are accepted is the caller's protocol policy.
inline constexpr std::size_t GyopHeaderSize = 24;
// The whole datagram, header included. Callers may impose a smaller limit,
// never a larger one.
inline constexpr std::size_t GyopMaxDatagram = 1200;

// Zero is not a valid code; the numeric values are not a data contract.
enum class NetErrorCode : std::uint16_t {
    MalformedDatagram = 1,
    DatagramTooLarge,
    UnsupportedChannel,
};

[[nodiscard]] constexpr const char* ToString(const NetErrorCode code) noexcept {
    switch (code) {
    case NetErrorCode::MalformedDatagram:  return "MalformedDatagram";
    case NetErrorCode::DatagramTooLarge:   return "DatagramTooLarge";
    case NetErrorCode::UnsupportedChannel: return "UnsupportedChannel";
    }
    return "Unknown";
}

using NetError = Base::Error<NetErrorCode>;
static_assert(Base::CodedError<NetError>);

struct GyopHeader final {
    std::uint16_t version{};
    std::uint16_t type{};
    std::uint64_t session{};
    std::uint32_t sequence{};
    std::uint16_t channel{};

    friend bool operator==(const GyopHeader&, const GyopHeader&) noexcept = default;
};

struct GyopDatagram final {
    GyopHeader header;
    // A view into the decoded bytes; valid while they are.
    std::span<const std::uint8_t> payload;
};

// DatagramTooLarge when header and payload exceed GyopMaxDatagram;
// UnsupportedChannel for any channel but 0.
[[nodiscard]] Base::Result<std::vector<std::uint8_t>, NetError> EncodeGyopDatagram(
    const GyopHeader& header, std::span<const std::uint8_t> payload);

// MalformedDatagram when the bytes are shorter than the header, longer than
// GyopMaxDatagram, lack the magic, use a channel but 0, or carry a payload
// length that differs from the bytes after the header.
[[nodiscard]] Base::Result<GyopDatagram, NetError> DecodeGyopDatagram(
    std::span<const std::uint8_t> datagram);

} // namespace Engine::Net
