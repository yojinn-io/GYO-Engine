#include "engine/net/GyopDatagram.hpp"

#include <algorithm>
#include <string>

namespace Engine::Net {
namespace {

constexpr std::uint8_t Magic[4]{'G', 'Y', 'O', 'P'};

std::uint64_t ReadBigEndian(std::span<const std::uint8_t> bytes) noexcept {
    std::uint64_t value{};
    for (const auto byte : bytes) value = (value << 8U) | byte;
    return value;
}

void WriteBigEndian(std::span<std::uint8_t> bytes, std::uint64_t value) noexcept {
    for (std::size_t index = bytes.size(); index > 0; --index) {
        bytes[index - 1] = static_cast<std::uint8_t>(value);
        value >>= 8U;
    }
}

Base::Err<NetError> Malformed(std::string detail) {
    return Base::Err(NetError::Make(NetErrorCode::MalformedDatagram, "malformed GYOP datagram", std::move(detail)));
}

} // namespace

Base::Result<std::vector<std::uint8_t>, NetError> EncodeGyopDatagram(
    const GyopHeader& header, const std::span<const std::uint8_t> payload) {
    if (payload.size() > GyopMaxDatagram - GyopHeaderSize)
        return Base::Err(NetError::Make(NetErrorCode::DatagramTooLarge, "GYOP datagram exceeds 1200 bytes",
            std::to_string(GyopHeaderSize + payload.size()) + " bytes"));
    if (header.channel != 0)
        return Base::Err(NetError::Make(NetErrorCode::UnsupportedChannel, "GYOP v1 has only channel 0",
            "channel " + std::to_string(header.channel)));
    std::vector<std::uint8_t> bytes(GyopHeaderSize + payload.size());
    const std::span<std::uint8_t> view(bytes);
    std::copy(std::begin(Magic), std::end(Magic), bytes.begin());
    WriteBigEndian(view.subspan(4, 2), header.version);
    WriteBigEndian(view.subspan(6, 2), header.type);
    WriteBigEndian(view.subspan(8, 8), header.session);
    WriteBigEndian(view.subspan(16, 4), header.sequence);
    WriteBigEndian(view.subspan(20, 2), payload.size());
    WriteBigEndian(view.subspan(22, 2), header.channel);
    std::copy(payload.begin(), payload.end(), bytes.begin() + static_cast<std::ptrdiff_t>(GyopHeaderSize));
    return bytes;
}

Base::Result<GyopDatagram, NetError> DecodeGyopDatagram(const std::span<const std::uint8_t> datagram) {
    if (datagram.size() < GyopHeaderSize) return Malformed("shorter than the 24-byte header");
    if (datagram.size() > GyopMaxDatagram) return Malformed("longer than 1200 bytes");
    if (!std::equal(std::begin(Magic), std::end(Magic), datagram.begin())) return Malformed("missing GYOP magic");
    const auto channel = static_cast<std::uint16_t>(ReadBigEndian(datagram.subspan(22, 2)));
    if (channel != 0) return Malformed("channel " + std::to_string(channel));
    if (ReadBigEndian(datagram.subspan(20, 2)) != datagram.size() - GyopHeaderSize)
        return Malformed("payload length differs from the datagram");
    return GyopDatagram{
        GyopHeader{static_cast<std::uint16_t>(ReadBigEndian(datagram.subspan(4, 2))),
            static_cast<std::uint16_t>(ReadBigEndian(datagram.subspan(6, 2))),
            ReadBigEndian(datagram.subspan(8, 8)),
            static_cast<std::uint32_t>(ReadBigEndian(datagram.subspan(16, 4))),
            channel},
        datagram.subspan(GyopHeaderSize)};
}

} // namespace Engine::Net
