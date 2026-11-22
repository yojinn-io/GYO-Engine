#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "engine/net/NetFailure.hpp"

// The packet header (declaration D-2): lightyear's five fields (kind, packet id, ack, ack bitfield, tick;
// crates/transport/transport/src/packet/header.rs at lightyear 736e1695) with a magic number, the protocol
// version, the connection id and the World generation in front. Every field is little-endian; the layout
// is the table in docs/architecture/target/v2/net.zh-Hant.md ("wire format") and changes only with
// kProtocolVersion.

namespace GYO::Engine::Net {

// 'G' 'N' on the wire (0x47 0x4E), read as a little-endian u16.
inline constexpr std::uint16_t kPacketMagic = 0x4E47;
inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::size_t kPacketHeaderSize = 24;

// The packet kinds of this version. Zero is not a kind. 16 to 19 are reserved for the handshake of step 1b
// (request, challenge, response, accepted) and are UnknownPacketKind until then.
enum class PacketKind : std::uint8_t {
    // Carries messages (possibly none) and asks the remote end to acknowledge it.
    Data = 1,
    // Carries only the ack fields; it does not ask for an acknowledgement, so two idle ends do not trade
    // acknowledgements forever (lightyear packet_type.rs:27-37).
    AckOnly = 2,
    // The sending end closes the connection; sent several times since it may be lost.
    Disconnect = 3,
};

struct PacketHeader final {
    PacketKind kind = PacketKind::Data;
    // Chosen by the handshake (step 1b); a datagram of another connection is dropped.
    std::uint32_t connectionId = 0;
    // The sending World's generation: it changes when the server replaces its World (D-5).
    std::uint32_t worldGeneration = 0;
    // Increases by one for every packet sent and is never reused within the window (it wraps at 2^16).
    std::uint16_t packetId = 0;
    // The newest packet id received from the remote end, and bit i set when packetId (ack - 1 - i) was
    // received too (the 32 before it).
    std::uint16_t ack = 0;
    std::uint32_t ackBits = 0;
    // The sending World's tick (D-9: the only time used for decisions).
    std::uint32_t tick = 0;

    friend constexpr bool operator==(const PacketHeader&, const PacketHeader&) noexcept = default;
};

// Appends the header's kPacketHeaderSize bytes.
void EncodePacketHeader(const PacketHeader& header, std::vector<std::byte>& out);

// Reads the header at the front of datagram: Truncated, NotOurProtocol, ProtocolVersionMismatch or
// UnknownPacketKind when it is not a header of this version.
[[nodiscard]] NetResult<PacketHeader> DecodePacketHeader(std::span<const std::byte> datagram);

// Wrap-aware order of 16-bit sequence numbers (Gaffer On Games, "Reliability and Congestion Avoidance over
// UDP"): a is newer than b when it is ahead by less than half the range.
[[nodiscard]] constexpr bool SequenceNewer(const std::uint16_t a, const std::uint16_t b) noexcept {
    return a != b && static_cast<std::uint16_t>(a - b) < 0x8000u;
}

} // namespace GYO::Engine::Net
