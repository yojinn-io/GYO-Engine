#include "engine/net/PacketHeader.hpp"

#include <string>

#include "ByteCursor.hpp"

namespace GYO::Engine::Net {

void EncodePacketHeader(const PacketHeader& header, std::vector<std::byte>& out) {
    Detail::ByteWriter writer(out);
    writer.U16(kPacketMagic);
    writer.U8(kProtocolVersion);
    writer.U8(static_cast<std::uint8_t>(header.kind));
    writer.U32(header.connectionId);
    writer.U32(header.worldGeneration);
    writer.U16(header.packetId);
    writer.U16(header.ack);
    writer.U32(header.ackBits);
    writer.U32(header.tick);
}

NetResult<PacketHeader> DecodePacketHeader(const std::span<const std::byte> datagram) {
    if (datagram.size() < kPacketHeaderSize) {
        return Base::Err(NetFailure::Make(NetFailureCode::Truncated, "the datagram is shorter than a packet header",
                                          "bytes=" + std::to_string(datagram.size())));
    }
    Detail::ByteReader reader(datagram);
    std::uint16_t magic = 0;
    std::uint8_t version = 0, kind = 0;
    PacketHeader header;
    (void)reader.U16(magic);
    (void)reader.U8(version);
    (void)reader.U8(kind);
    if (magic != kPacketMagic) {
        return Base::Err(NetFailure::Make(NetFailureCode::NotOurProtocol, "the datagram has no packet magic",
                                          "magic=" + std::to_string(magic)));
    }
    if (version != kProtocolVersion) {
        return Base::Err(NetFailure::Make(NetFailureCode::ProtocolVersionMismatch,
                                          "the datagram is of another protocol version",
                                          "version=" + std::to_string(version)));
    }
    switch (static_cast<PacketKind>(kind)) {
    case PacketKind::Data:
    case PacketKind::AckOnly:
    case PacketKind::Disconnect:
    case PacketKind::ConnectRequest:
    case PacketKind::Challenge:
    case PacketKind::ConnectResponse:
    case PacketKind::Accepted:
    case PacketKind::Rejected:
        header.kind = static_cast<PacketKind>(kind);
        break;
    default:
        return Base::Err(NetFailure::Make(NetFailureCode::UnknownPacketKind, "the packet kind is not known",
                                          "kind=" + std::to_string(kind)));
    }
    (void)reader.U32(header.connectionId);
    (void)reader.U32(header.worldGeneration);
    (void)reader.U16(header.packetId);
    (void)reader.U16(header.ack);
    (void)reader.U32(header.ackBits);
    (void)reader.U32(header.tick);
    return header;
}

} // namespace GYO::Engine::Net
