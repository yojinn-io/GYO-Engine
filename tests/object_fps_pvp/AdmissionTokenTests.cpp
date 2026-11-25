// The Match's admission token check against the vectors the services' Go check wrote
// (games/object_fps_pvp/services/token/testdata/admission_vectors.txt; gateway declaration D5): both ends must agree
// on every vector.

#include <doctest/doctest.h>

#include "object_fps_pvp/data/Hex.hpp"
#include "object_fps_pvp/match/AdmissionToken.hpp"

#include <algorithm>
#include <fstream>
#include <span>
#include <sstream>
#include <string>

namespace M = object_fps_pvp::match;

namespace {

std::string Expected(const std::optional<M::AdmissionRefusal> refusal) {
    if (!refusal) return "accepted";
    switch (*refusal) {
    case M::AdmissionRefusal::Length: return "length";
    case M::AdmissionRefusal::Version: return "version";
    case M::AdmissionRefusal::Signature: return "signature";
    case M::AdmissionRefusal::Match: return "match";
    case M::AdmissionRefusal::User: return "user";
    case M::AdmissionRefusal::Expired: return "expired";
    }
    return "?";
}

} // namespace

TEST_CASE("admission token: the Match agrees with the services on every vector") {
    std::ifstream lines(PVP_ADMISSION_VECTORS);
    REQUIRE_MESSAGE(lines.good(), "the vectors file does not read");
    std::string line;
    int checked = 0;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        std::string name, keyHex, tokenHex, expected;
        long long now = 0;
        unsigned long long match = 0, user = 0;
        fields >> name >> keyHex >> tokenHex >> now >> match >> user >> expected;
        const auto key = object_fps_pvp::data::DecodeHex(keyHex);
        const auto token = object_fps_pvp::data::DecodeHex(tokenHex);
        REQUIRE(key.has_value());
        REQUIRE(token.has_value());
        const auto result = M::VerifyAdmission(*key, *token, now, match, user);
        CHECK_MESSAGE(Expected(result) == expected, "admission vector " << name << ": the Match says " << Expected(result)
                                                                          << ", the services say " << expected);
        ++checked;
    }
    CHECK_MESSAGE(checked == 11, "the vectors file has " << checked << " vectors, the services write 11");
}

TEST_CASE("admission token: HMAC-SHA256 matches RFC 4231 case 2") {
    const std::string key = "Jefe";
    const std::string data = "what do ya want for nothing?";
    const auto mac = M::HmacSha256(std::as_bytes(std::span(key.data(), key.size())), std::as_bytes(std::span(data.data(), data.size())));
    const auto expected = object_fps_pvp::data::DecodeHex("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    REQUIRE(expected.has_value());
    CHECK_MESSAGE(std::equal(mac.begin(), mac.end(), expected->begin()), "HMAC-SHA256 RFC 4231 case 2 differs");
}

TEST_CASE("admission token: hex text") {
    using object_fps_pvp::data::DecodeHex;
    CHECK(DecodeHex("00ffA0").value() == std::vector<std::byte>{std::byte{0}, std::byte{0xff}, std::byte{0xa0}});
    CHECK_FALSE(DecodeHex("abc").has_value());
    CHECK_FALSE(DecodeHex("zz").has_value());
}
