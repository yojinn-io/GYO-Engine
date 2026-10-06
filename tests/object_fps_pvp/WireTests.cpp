#include <doctest/doctest.h>

#include "RetroFPS/Pvp/Wire.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace wire = fps::pvp::wire;

namespace {

std::vector<std::uint8_t> FromHex(std::string_view hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t index = 0; index + 1 < hex.size(); index += 2)
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(std::string(hex.substr(index, 2)), nullptr, 16)));
    return bytes;
}

wire::Packet Sample(const std::uint16_t type) {
    return {static_cast<wire::Type>(type), 0x0102030405060708ULL + type, 0xfffffff0U + type,
        std::string(type, static_cast<char>(type)) + std::string("\x00\xff", 2)};
}

std::vector<std::uint8_t> Valid() { return wire::Encode(Sample(3)); }

} // namespace

// Pinned on the product's own header code before the GYOP header moved to the
// engine: every message type must keep these exact bytes. pv6 changed only the
// version field (bytes 4-5) from 0x0005 to 0x0006.
TEST_CASE("PvP datagram encoding of every message type is byte-for-byte stable") {
    constexpr std::array<std::string_view, 7> expected{
        "47594f50000600010102030405060709fffffff1000300000100ff",
        "47594f5000060002010203040506070afffffff200040000020200ff",
        "47594f5000060003010203040506070bfffffff30005000003030300ff",
        "47594f5000060004010203040506070cfffffff4000600000404040400ff",
        "47594f5000060005010203040506070dfffffff500070000050505050500ff",
        "47594f5000060006010203040506070efffffff60008000006060606060600ff",
        "47594f5000060007010203040506070ffffffff7000900000707070707070700ff",
    };
    for (std::uint16_t type = 1; type <= 7; ++type) {
        CAPTURE(type);
        const auto bytes = wire::Encode(Sample(type));
        CHECK(bytes == FromHex(expected[type - 1]));
        const auto decoded = wire::Decode(bytes);
        REQUIRE(decoded);
        CHECK(decoded->type == static_cast<wire::Type>(type));
        CHECK(decoded->session == Sample(type).session);
        CHECK(decoded->sequence == Sample(type).sequence);
        CHECK(decoded->payload == Sample(type).payload);
    }
}

// The complete rejection set of the product's datagram decode. Transport
// conditions and the product's version and type policy are both covered, so
// moving either part elsewhere cannot change what is accepted.
TEST_CASE("PvP datagram decode accepts and rejects the same datagrams") {
    const auto valid = Valid();
    CHECK(wire::Decode(valid));
    const auto rejected = [](std::vector<std::uint8_t> bytes) { return !wire::Decode(bytes); };
    const auto with = [&](std::size_t offset, std::uint8_t value) {
        auto bytes = valid;
        bytes[offset] = value;
        return bytes;
    };

    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{23}})
        CHECK(rejected(std::vector<std::uint8_t>(valid.begin(), valid.begin() + static_cast<std::ptrdiff_t>(size))));
    CHECK(rejected(std::vector<std::uint8_t>(valid.begin(), valid.end() - 1)));  // length field too large
    auto extended = valid;
    extended.push_back(0);
    CHECK(rejected(extended));                                                   // length field too small
    CHECK(rejected(with(0, 'X')));                                               // magic
    CHECK(rejected(with(3, 'Q')));
    CHECK(rejected(with(5, static_cast<std::uint8_t>(wire::ProtocolVersion - 1)))); // previous version
    CHECK(rejected(with(5, static_cast<std::uint8_t>(wire::ProtocolVersion + 1)))); // next version
    CHECK(rejected(with(4, 1)));                                                 // version high byte set
    CHECK(rejected(with(22, 1)));                                                // channel / reserved @22
    CHECK(rejected(with(23, 1)));
    CHECK(rejected(with(7, 0)));                                                 // type 0
    CHECK(rejected(with(7, 8)));                                                 // type 8
    CHECK(rejected(with(6, 1)));                                                 // type 0x0103
    for (std::uint8_t type = 1; type <= 7; ++type) CHECK_FALSE(rejected(with(7, type)));

    // Exactly 1200 bytes decode; 1201 bytes are rejected even with a matching length field.
    wire::Packet largest = Sample(3);
    largest.payload.assign(wire::MaxDatagram - wire::HeaderSize, 'p');
    const auto full = wire::Encode(largest);
    CHECK(full.size() == 1200);
    CHECK(wire::Decode(full));
    auto oversize = full;
    oversize.push_back('p');
    oversize[20] = static_cast<std::uint8_t>((oversize.size() - wire::HeaderSize) >> 8U);
    oversize[21] = static_cast<std::uint8_t>(oversize.size() - wire::HeaderSize);
    CHECK(rejected(oversize));
}

TEST_CASE("PvP datagram encoding refuses payloads beyond the 1200-byte datagram") {
    wire::Packet packet = Sample(3);
    packet.payload.assign(wire::MaxDatagram - wire::HeaderSize + 1, 'p');
    CHECK_THROWS_AS(wire::Encode(packet), std::length_error);
}
