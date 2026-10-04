#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include "engine/net/GyopDatagram.hpp"

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

using namespace Engine::Net;

namespace {

// Shared with the Go gateway framing tests (services/gyo_gateway/framing).
nlohmann::json Vectors() {
    std::ifstream file(GYO_TEST_GYOP_VECTORS);
    REQUIRE(file);
    return nlohmann::json::parse(file);
}

std::vector<std::uint8_t> FromHex(const std::string& hex) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t index = 0; index + 1 < hex.size(); index += 2)
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(index, 2), nullptr, 16)));
    return bytes;
}

GyopHeader HeaderOf(const nlohmann::json& vector) {
    return {vector.at("version").get<std::uint16_t>(), vector.at("type").get<std::uint16_t>(),
        std::stoull(vector.at("session").get<std::string>(), nullptr, 16), vector.at("sequence").get<std::uint32_t>(),
        static_cast<std::uint16_t>(vector.value("channel", 0))};
}

} // namespace

TEST_CASE("GYOP datagrams encode and decode the shared vectors byte for byte") {
    const auto vectors = Vectors();
    CHECK(vectors.at("header_size").get<std::size_t>() == GyopHeaderSize);
    CHECK(vectors.at("max_datagram").get<std::size_t>() == GyopMaxDatagram);
    REQUIRE(!vectors.at("valid").empty());
    for (const auto& vector : vectors.at("valid")) {
        CAPTURE(vector.at("name").get<std::string>());
        const auto header = HeaderOf(vector);
        const auto payload = FromHex(vector.at("payload").get<std::string>());
        const auto datagram = FromHex(vector.at("datagram").get<std::string>());
        const auto encoded = EncodeGyopDatagram(header, payload);
        REQUIRE(encoded);
        CHECK(*encoded == datagram);
        const auto decoded = DecodeGyopDatagram(datagram);
        REQUIRE(decoded);
        CHECK(decoded->header == header);
        CHECK(std::vector<std::uint8_t>(decoded->payload.begin(), decoded->payload.end()) == payload);
    }
}

TEST_CASE("GYOP decode rejects every malformed shared vector") {
    const auto vectors = Vectors();
    REQUIRE(!vectors.at("malformed").empty());
    for (const auto& vector : vectors.at("malformed")) {
        CAPTURE(vector.at("name").get<std::string>());
        const auto decoded = DecodeGyopDatagram(FromHex(vector.at("datagram").get<std::string>()));
        REQUIRE_FALSE(decoded);
        CHECK(decoded.error().code == NetErrorCode::MalformedDatagram);
    }
}

TEST_CASE("GYOP encode refuses oversize payloads and channels other than 0") {
    const auto vectors = Vectors();
    REQUIRE(!vectors.at("encode_rejected").empty());
    for (const auto& vector : vectors.at("encode_rejected")) {
        CAPTURE(vector.at("name").get<std::string>());
        const std::vector<std::uint8_t> payload(vector.at("payload_size").get<std::size_t>(), 0x70);
        const auto encoded = EncodeGyopDatagram(HeaderOf(vector), payload);
        REQUIRE_FALSE(encoded);
        CHECK(std::string_view(ToString(encoded.error().code)) == vector.at("code").get<std::string>());
    }
}

TEST_CASE("GYOP decode leaves version and type policy to the caller") {
    // Any version and type pass the transport checks unchanged.
    const std::vector<std::uint8_t> payload{1, 2};
    for (const std::uint16_t value : {std::uint16_t{0}, std::uint16_t{0xffff}}) {
        const auto encoded = EncodeGyopDatagram({value, value, 1, 1, 0}, payload);
        REQUIRE(encoded);
        const auto decoded = DecodeGyopDatagram(*encoded);
        REQUIRE(decoded);
        CHECK(decoded->header.version == value);
        CHECK(decoded->header.type == value);
    }
}
