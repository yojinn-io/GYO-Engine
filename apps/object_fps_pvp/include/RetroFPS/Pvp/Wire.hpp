#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace fps::pvp::wire {
inline constexpr std::size_t HeaderSize = 24, MaxDatagram = 1200, MaxFrame = 65536;
enum class Type : std::uint16_t { Hello=1, Welcome=2, Input=3, Snapshot=4, Error=5 };
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
    if (packet.payload.size()+HeaderSize>MaxDatagram) throw std::length_error("UDP payload exceeds 1200 bytes");
    std::vector<std::uint8_t> bytes(HeaderSize+packet.payload.size());
    bytes[0]='G'; bytes[1]='Y'; bytes[2]='O'; bytes[3]='P';
    Write(std::span(bytes).subspan(4,2),3);
    Write(std::span(bytes).subspan(6,2),static_cast<std::uint16_t>(packet.type));
    Write(std::span(bytes).subspan(8,8),packet.session);
    Write(std::span(bytes).subspan(16,4),packet.sequence);
    Write(std::span(bytes).subspan(20,2),packet.payload.size());
    std::copy(packet.payload.begin(),packet.payload.end(),bytes.begin()+HeaderSize);
    return bytes;
}
inline std::optional<Packet> Decode(std::span<const std::uint8_t> bytes) {
    if(bytes.size()<HeaderSize || bytes.size()>MaxDatagram || bytes[0]!='G' || bytes[1]!='Y' || bytes[2]!='O' || bytes[3]!='P' ||
       Read(bytes.subspan(4,2))!=3 || Read(bytes.subspan(22,2))!=0 || Read(bytes.subspan(20,2))!=bytes.size()-HeaderSize) return {};
    const auto type=Read(bytes.subspan(6,2));
    if(type<1 || type>5) return {};
    return Packet{static_cast<Type>(type),Read(bytes.subspan(8,8)),static_cast<std::uint32_t>(Read(bytes.subspan(16,4))),
        std::string(reinterpret_cast<const char*>(bytes.data()+HeaderSize),bytes.size()-HeaderSize)};
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
