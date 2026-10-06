#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/net/GyopDatagram.hpp"

namespace fps::pvp::wire {
// The GYOP header and its transport checks are the engine's (Engine::Net);
// the protocol version, the message types and the TCP frame stay here.
inline constexpr std::uint16_t ProtocolVersion = 6;
inline constexpr std::size_t HeaderSize = Engine::Net::GyopHeaderSize, MaxDatagram = Engine::Net::GyopMaxDatagram,
    MaxFrame = 65536;
enum class Type : std::uint16_t { Hello=1, Welcome=2, Input=3, Snapshot=4, Error=5, Actions=6, ActionResults=7 };
inline std::uint64_t Read(std::span<const std::uint8_t> bytes) {
    std::uint64_t value{};
    for (const auto byte : bytes) value = (value << 8) | byte;
    return value;
}
inline void Write(std::span<std::uint8_t> bytes, std::uint64_t value) {
    for (std::size_t n=bytes.size(); n>0; --n) { bytes[n-1]=static_cast<std::uint8_t>(value); value >>= 8; }
}
struct Packet { Type type{}; std::uint64_t session{}; std::uint32_t sequence{}; std::string payload; };
inline std::vector<std::uint8_t> Encode(const Packet& packet) {
    const auto encoded = Engine::Net::EncodeGyopDatagram(
        {ProtocolVersion, static_cast<std::uint16_t>(packet.type), packet.session, packet.sequence, 0},
        std::span(reinterpret_cast<const std::uint8_t*>(packet.payload.data()), packet.payload.size()));
    if (!encoded) throw std::length_error("UDP payload exceeds 1200 bytes");
    return *encoded;
}
inline std::optional<Packet> Decode(std::span<const std::uint8_t> bytes) {
    const auto decoded = Engine::Net::DecodeGyopDatagram(bytes);
    if (!decoded || decoded->header.version != ProtocolVersion) return {};
    const auto type = decoded->header.type;
    if (type < 1 || type > 7) return {};
    return Packet{static_cast<Type>(type), decoded->header.session, decoded->header.sequence,
        std::string(reinterpret_cast<const char*>(decoded->payload.data()), decoded->payload.size())};
}
inline bool Newer(std::uint32_t next, std::uint32_t previous) noexcept {
    return next!=previous && static_cast<std::uint32_t>(next-previous)<0x80000000u;
}
inline std::vector<std::uint8_t> Frame(const std::string& payload) {
    if(payload.empty() || payload.size()>MaxFrame) throw std::length_error("Invalid IPC frame size");
    std::vector<std::uint8_t> out(4+payload.size());
    Write(std::span(out).first(4),payload.size());
    std::copy(payload.begin(),payload.end(),out.begin()+4);
    return out;
}
}
