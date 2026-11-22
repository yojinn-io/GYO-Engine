#include "engine/net/Handshake.hpp"

#include <string>

#include "ByteCursor.hpp"
#include "engine/base/Sha256.hpp"

namespace GYO::Engine::Net {

const char* ToString(const DisconnectReason reason) noexcept {
    switch (reason) {
    case DisconnectReason::Closed: return "Closed";
    case DisconnectReason::Timeout: return "Timeout";
    case DisconnectReason::ClosedByRemote: return "ClosedByRemote";
    case DisconnectReason::ProtocolMismatch: return "ProtocolMismatch";
    case DisconnectReason::StepLengthMismatch: return "StepLengthMismatch";
    case DisconnectReason::IdRangeOverlap: return "IdRangeOverlap";
    case DisconnectReason::FingerprintMismatch: return "FingerprintMismatch";
    case DisconnectReason::ChallengeFailed: return "ChallengeFailed";
    case DisconnectReason::HandshakeTimeout: return "HandshakeTimeout";
    case DisconnectReason::Refused: return "Refused";
    }
    return "Unknown";
}

std::string IdRange::Text() const { return "[" + std::to_string(first) + ", " + std::to_string(last) + "]"; }

std::optional<Rejection> CompareFacts(const HandshakeFacts& client, const HandshakeFacts& server) {
    if (client.protocolVersion != server.protocolVersion) {
        return Rejection{DisconnectReason::ProtocolMismatch, "version: client " + std::to_string(client.protocolVersion) +
                                                                 ", server " + std::to_string(server.protocolVersion)};
    }
    if (client.protocolHash != server.protocolHash) {
        return Rejection{DisconnectReason::ProtocolMismatch, "protocol hash: client " + std::to_string(client.protocolHash) +
                                                                 ", server " + std::to_string(server.protocolHash)};
    }
    if (client.stepNanoseconds != server.stepNanoseconds) {
        return Rejection{DisconnectReason::StepLengthMismatch, "step ns: client " + std::to_string(client.stepNanoseconds) +
                                                                   ", server " + std::to_string(server.stepNanoseconds)};
    }
    if (client.idRange.Overlaps(server.idRange)) {
        return Rejection{DisconnectReason::IdRangeOverlap,
                         "client " + client.idRange.Text() + ", server " + server.idRange.Text()};
    }
    if (client.fingerprint) {
        if (!server.fingerprint) {
            return Rejection{DisconnectReason::FingerprintMismatch, "the client has a fingerprint, the server none"};
        }
        if (*client.fingerprint != *server.fingerprint) {
            return Rejection{DisconnectReason::FingerprintMismatch,
                             "client " + *client.fingerprint + ", server " + *server.fingerprint};
        }
    }
    return std::nullopt;
}

namespace Detail {
namespace {

NetFailure Short(const char* what) { return NetFailure::Make(NetFailureCode::Truncated, what); }

void Bytes16(ByteWriter& writer, std::span<const std::byte> bytes) {
    writer.U16(static_cast<std::uint16_t>(bytes.size()));
    writer.Bytes(bytes);
}

bool ReadBytes16(ByteReader& reader, std::vector<std::byte>& out) {
    std::uint16_t length = 0;
    std::span<const std::byte> bytes;
    if (!reader.U16(length) || !reader.Bytes(length, bytes)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

void U64(ByteWriter& writer, const std::uint64_t value) {
    writer.U32(static_cast<std::uint32_t>(value));
    writer.U32(static_cast<std::uint32_t>(value >> 32));
}

bool ReadU64(ByteReader& reader, std::uint64_t& value) {
    std::uint32_t low = 0, high = 0;
    if (!reader.U32(low) || !reader.U32(high)) return false;
    value = (static_cast<std::uint64_t>(high) << 32) | low;
    return true;
}

} // namespace

std::uint64_t ChallengeOf(const std::uint64_t secret, const Endpoint from, const std::span<const std::byte> requestBody) {
    std::vector<std::byte> keyed;
    ByteWriter writer(keyed);
    U64(writer, secret);
    U64(writer, from.value);
    writer.Bytes(requestBody);
    const std::string hex = Base::Sha256(keyed);
    return std::stoull(hex.substr(0, 16), nullptr, 16);
}

void EncodeRequestBody(const ConnectRequest& request, std::vector<std::byte>& out) {
    ByteWriter writer(out);
    writer.U32(request.facts.protocolVersion);
    U64(writer, request.facts.protocolHash);
    U64(writer, static_cast<std::uint64_t>(request.facts.stepNanoseconds));
    U64(writer, request.facts.idRange.first);
    U64(writer, request.facts.idRange.last);
    U64(writer, request.userId);
    U64(writer, request.salt);
    writer.U8(request.facts.fingerprint ? 1 : 0);
    const std::string fingerprint = request.facts.fingerprint.value_or(std::string{});
    Bytes16(writer, std::as_bytes(std::span(fingerprint.data(), fingerprint.size())));
    Bytes16(writer, request.token);
}

NetResult<ConnectRequest> DecodeRequestBody(const std::span<const std::byte> body, std::size_t& used) {
    ByteReader reader(body);
    ConnectRequest request;
    std::uint64_t step = 0;
    std::uint8_t hasFingerprint = 0;
    std::vector<std::byte> fingerprint;
    if (!reader.U32(request.facts.protocolVersion) || !ReadU64(reader, request.facts.protocolHash) ||
        !ReadU64(reader, step) || !ReadU64(reader, request.facts.idRange.first) ||
        !ReadU64(reader, request.facts.idRange.last) || !ReadU64(reader, request.userId) || !ReadU64(reader, request.salt) ||
        !reader.U8(hasFingerprint) ||
        !ReadBytes16(reader, fingerprint) || !ReadBytes16(reader, request.token)) {
        return Base::Err(Short("a connect request is cut short"));
    }
    if (hasFingerprint > 1) return Base::Err(NetFailure::Make(NetFailureCode::MalformedPayload, "a connect request's fingerprint flag is not 0 or 1"));
    request.facts.stepNanoseconds = static_cast<std::int64_t>(step);
    if (hasFingerprint == 1) {
        request.facts.fingerprint = std::string(reinterpret_cast<const char*>(fingerprint.data()), fingerprint.size());
    }
    used = body.size() - reader.Remaining();
    return request;
}

void EncodeAccepted(const ConnectAccepted& accepted, std::vector<std::byte>& out) {
    ByteWriter writer(out);
    writer.U32(accepted.connectionId);
    U64(writer, accepted.salt);
    writer.U32(accepted.tick);
    U64(writer, static_cast<std::uint64_t>(accepted.stepNanoseconds));
    writer.U32(accepted.worldGeneration);
    U64(writer, accepted.idRange.first);
    U64(writer, accepted.idRange.last);
}

NetResult<ConnectAccepted> DecodeAccepted(const std::span<const std::byte> body) {
    ByteReader reader(body);
    ConnectAccepted accepted;
    std::uint64_t step = 0;
    if (!reader.U32(accepted.connectionId) || !ReadU64(reader, accepted.salt) || !reader.U32(accepted.tick) || !ReadU64(reader, step) ||
        !reader.U32(accepted.worldGeneration) || !ReadU64(reader, accepted.idRange.first) ||
        !ReadU64(reader, accepted.idRange.last)) {
        return Base::Err(Short("an accept is cut short"));
    }
    accepted.stepNanoseconds = static_cast<std::int64_t>(step);
    return accepted;
}

void EncodeRejection(const Rejection& rejection, std::vector<std::byte>& out) {
    ByteWriter writer(out);
    writer.U8(static_cast<std::uint8_t>(rejection.reason));
    const std::string detail = rejection.detail.substr(0, 512);
    Bytes16(writer, std::as_bytes(std::span(detail.data(), detail.size())));
}

NetResult<Rejection> DecodeRejection(const std::span<const std::byte> body) {
    ByteReader reader(body);
    std::uint8_t reason = 0;
    std::vector<std::byte> detail;
    if (!reader.U8(reason) || !ReadBytes16(reader, detail)) return Base::Err(Short("a rejection is cut short"));
    if (reason < static_cast<std::uint8_t>(DisconnectReason::Closed) || reason > static_cast<std::uint8_t>(DisconnectReason::Refused)) {
        return Base::Err(NetFailure::Make(NetFailureCode::MalformedPayload, "a rejection's reason is not known",
                                          "reason=" + std::to_string(reason)));
    }
    return Rejection{static_cast<DisconnectReason>(reason), std::string(reinterpret_cast<const char*>(detail.data()), detail.size())};
}

std::vector<std::byte> HandshakeDatagram(const PacketKind kind, const std::uint32_t connectionId, const Stamp stamp,
                                         const std::span<const std::byte> payload) {
    PacketHeader header;
    header.kind = kind;
    header.connectionId = connectionId;
    header.tick = stamp.tick;
    header.worldGeneration = stamp.worldGeneration;
    std::vector<std::byte> out;
    EncodePacketHeader(header, out);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

} // namespace Detail
} // namespace GYO::Engine::Net
