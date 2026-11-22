#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "engine/net/Connection.hpp"
#include "engine/net/Link.hpp"
#include "engine/net/NetFailure.hpp"
#include "engine/net/PacketHeader.hpp"

// The handshake and the connection settings above the transport (declaration D-6, D-8). The payloads of the
// five handshake kinds and what both ends compare. Genealogy: netcode.io's request, challenge, response
// (lightyear's netcode crate), made stateless on the server like netcode's challenge token: the challenge is a
// keyed hash the server can recompute, so it keeps nothing for a handshake that is not finished.

namespace GYO::Engine::Net {

// The net Module's own protocol version (what the messages above the header mean), compared in the handshake.
// The header's kProtocolVersion is the framing; a datagram of another framing cannot be read at all.
inline constexpr std::uint32_t kNetProtocolVersion = 1;

// Why a connection ended or was never made. Zero is not a reason.
enum class DisconnectReason : std::uint8_t {
    // This end closed it.
    Closed = 1,
    // Nothing arrived for longer than SessionSettings::timeoutAfter.
    Timeout,
    // The remote end closed it (a Disconnect packet arrived).
    ClosedByRemote,
    // The net protocol version or the protocol hash differs (the detail says which).
    ProtocolMismatch,
    StepLengthMismatch,
    // The two Worlds' allotter ranges overlap (the detail gives both).
    IdRangeOverlap,
    // The client sent a consistency fingerprint the server does not have, or a different one.
    FingerprintMismatch,
    // The response did not carry the challenge the server would give for that request from that endpoint.
    ChallengeFailed,
    // The handshake did not finish within SessionSettings::handshakeTimeout.
    HandshakeTimeout,
    // The server's product refused the client (the detail is the product's text).
    Refused,
};

[[nodiscard]] const char* ToString(DisconnectReason reason) noexcept;

// An allotter range [first, last] (entity_ids' RangeAllotter, or one the author gives).
struct IdRange final {
    std::uint64_t first = 0;
    std::uint64_t last = 0;

    [[nodiscard]] bool Overlaps(const IdRange& other) const noexcept { return first <= other.last && other.first <= last; }
    [[nodiscard]] std::string Text() const;
    friend constexpr bool operator==(const IdRange&, const IdRange&) noexcept = default;
};

// What one end brings to the comparison (declaration D-6, F15).
struct HandshakeFacts final {
    std::uint32_t protocolVersion = kNetProtocolVersion;
    // The hash of the net Module's registrations (step 2 adds replication; step 1b hashes the empty list).
    std::uint64_t protocolHash = 0;
    // The World's fixed step length.
    std::int64_t stepNanoseconds = 0;
    IdRange idRange;
    // The consistency fingerprint's text, when the World has an authority strategy (K2, K3); none for K1.
    std::optional<std::string> fingerprint;

    friend bool operator==(const HandshakeFacts&, const HandshakeFacts&) = default;
};

struct ConnectRequest final {
    HandshakeFacts facts;
    // Opaque to the Engine: the product maps it to its own player (P12).
    std::uint64_t userId = 0;
    // Carried, not checked (Q7: no authentication in this line).
    std::vector<std::byte> token;
    // Drawn per handshake by the client: a second response of the same handshake (its Accepted was lost) carries
    // the same salt, a new handshake from the same endpoint (a reconnect) another one (D-8).
    std::uint64_t salt = 0;

    friend bool operator==(const ConnectRequest&, const ConnectRequest&) = default;
};

struct ConnectAccepted final {
    std::uint32_t connectionId = 0;
    std::uint32_t tick = 0;
    std::int64_t stepNanoseconds = 0;
    std::uint32_t worldGeneration = 0;
    IdRange idRange;

    friend bool operator==(const ConnectAccepted&, const ConnectAccepted&) = default;
};

struct Rejection final {
    DisconnectReason reason = DisconnectReason::Refused;
    std::string detail;
};

// The thresholds of a connection (all settings; the defaults and their genealogy in net.zh-Hant.md).
struct SessionSettings final {
    // An idle end sends a keepalive when nothing was sent for longer than this; a connecting client resends its
    // request or response at the same rate (lightyear crates/connection/netcode/src/lib.rs:116,
    // PACKET_SEND_RATE_SEC = 1/10).
    NetTime keepaliveInterval = std::chrono::milliseconds(100);
    // A client hears nothing for longer than this: Interrupted (ten keepalive intervals; set by the card).
    NetTime interruptAfter = std::chrono::seconds(1);
    // Nothing for longer than this: the connection ends with Timeout (lightyear netcode server.rs:34,
    // CLIENT_TIMEOUT_SECS = 10).
    NetTime timeoutAfter = std::chrono::seconds(10);
    // A handshake not finished after this long ends with HandshakeTimeout (lightyear netcode lib.rs:115,
    // CONNECTION_TIMEOUT_SEC = 15).
    NetTime handshakeTimeout = std::chrono::seconds(15);
    // The transport of every connection; connectionId is set by the handshake.
    ConnectionSettings transport;
};

// The first mismatch between a client's facts and the server's, in the order version, protocol hash, step
// length, id ranges, fingerprint; nullopt when they agree. A client without a fingerprint (K1) is not
// compared on it; a client with one needs the server to have the same.
[[nodiscard]] std::optional<Rejection> CompareFacts(const HandshakeFacts& client, const HandshakeFacts& server);

namespace Detail {

// The challenge: a keyed hash of the request's bytes and the endpoint it came from (the first 64 bits of a
// SHA-256 over the secret, the endpoint and the bytes).
[[nodiscard]] std::uint64_t ChallengeOf(std::uint64_t secret, Endpoint from, std::span<const std::byte> requestBody);

// The payloads after the header. Requests are padded to at least kMinRequestDatagram bytes, so a server never
// answers a request with more bytes than it got (the challenge is smaller).
inline constexpr std::size_t kChallengeDatagram = kPacketHeaderSize + 8;
inline constexpr std::size_t kMinRequestDatagram = kChallengeDatagram;

void EncodeRequestBody(const ConnectRequest& request, std::vector<std::byte>& out);
[[nodiscard]] NetResult<ConnectRequest> DecodeRequestBody(std::span<const std::byte> body, std::size_t& used);
void EncodeAccepted(const ConnectAccepted& accepted, std::vector<std::byte>& out);
[[nodiscard]] NetResult<ConnectAccepted> DecodeAccepted(std::span<const std::byte> body);
void EncodeRejection(const Rejection& rejection, std::vector<std::byte>& out);
[[nodiscard]] NetResult<Rejection> DecodeRejection(std::span<const std::byte> body);

// A handshake datagram: the header (packet id and acks 0) and the payload.
[[nodiscard]] std::vector<std::byte> HandshakeDatagram(PacketKind kind, std::uint32_t connectionId, Stamp stamp,
                                                       std::span<const std::byte> payload);

} // namespace Detail

} // namespace GYO::Engine::Net
