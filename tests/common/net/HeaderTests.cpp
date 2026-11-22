// The packet header (net Module declaration D-2; wire format table in docs/architecture/target/v2/net.zh-Hant.md).
#include <doctest/doctest.h>

#include <vector>

#include "NetTestSupport.hpp"
#include "engine/net/PacketHeader.hpp"

namespace N = GYO::Engine::Net;
using GYO::Test::Net::Bytes;

namespace {

N::PacketHeader Sample() {
    N::PacketHeader header;
    header.kind = N::PacketKind::AckOnly;
    header.connectionId = 0x11223344u;
    header.worldGeneration = 0x55667788u;
    header.packetId = 0x99AAu;
    header.ack = 0xBBCCu;
    header.ackBits = 0xDDEEFF01u;
    header.tick = 0x02030405u;
    return header;
}

} // namespace

TEST_CASE("header: encoding and decoding give back every field") {
    std::vector<std::byte> bytes;
    N::EncodePacketHeader(Sample(), bytes);
    REQUIRE(bytes.size() == N::kPacketHeaderSize);
    const auto decoded = N::DecodePacketHeader(bytes);
    REQUIRE(decoded.has_value());
    CHECK(*decoded == Sample());
}

TEST_CASE("header: the bytes are the documented little-endian layout") {
    std::vector<std::byte> bytes;
    N::EncodePacketHeader(Sample(), bytes);
    // magic 'G' 'N', version 1, kind 2 (AckOnly), connection id, World generation, packet id, ack, ack bits, tick.
    const auto expected = Bytes({0x47, 0x4E, 0x01, 0x02, 0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
                                 0xAA, 0x99, 0xCC, 0xBB, 0x01, 0xFF, 0xEE, 0xDD, 0x05, 0x04, 0x03, 0x02});
    CHECK_MESSAGE(bytes == expected, "the header bytes are the documented layout");
}

TEST_CASE("header: a datagram that is not a header of this version is a failure, not an assertion") {
    std::vector<std::byte> good;
    N::EncodePacketHeader(Sample(), good);

    auto failure = [](const std::vector<std::byte>& bytes) { return GYO::Test::Net::CodeOf(N::DecodePacketHeader(bytes)); };

    std::vector<std::byte> shortened(good.begin(), good.end() - 1);
    CHECK(failure(shortened) == N::NetFailureCode::Truncated);
    CHECK(failure({}) == N::NetFailureCode::Truncated);

    auto magic = good;
    magic[1] = std::byte{0x4F};
    CHECK(failure(magic) == N::NetFailureCode::NotOurProtocol);

    auto version = good;
    version[2] = std::byte{2};
    CHECK(failure(version) == N::NetFailureCode::ProtocolVersionMismatch);

    for (const int kind : {0, 4, 16, 19, 255}) {
        CAPTURE(kind);
        auto unknown = good;
        unknown[3] = static_cast<std::byte>(kind);
        CHECK_MESSAGE(failure(unknown) == N::NetFailureCode::UnknownPacketKind,
                      "a kind outside the table (a reserved handshake kind included) is UnknownPacketKind");
    }
}

TEST_CASE("header: sequence order wraps") {
    CHECK(N::SequenceNewer(1, 0));
    CHECK(N::SequenceNewer(0, 0xFFFF));
    CHECK_FALSE(N::SequenceNewer(0xFFFF, 0));
    CHECK_FALSE(N::SequenceNewer(5, 5));
    CHECK(N::SequenceNewer(0x7FFF, 0));
    CHECK_FALSE(N::SequenceNewer(0x8000, 0));
}

TEST_CASE("header: failure codes have names") {
    CHECK(std::string_view(N::ToString(N::NetFailureCode::Truncated)) == "Truncated");
    CHECK(std::string_view(N::ToString(N::NetFailureCode::LinkFailed)) == "LinkFailed");
    CHECK(static_cast<int>(N::NetFailureCode::Truncated) == 1);
}
