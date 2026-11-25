// The Match's admission (gateway declaration D5): the token its key signed, then one seat per user.

#include <doctest/doctest.h>

#include "object_fps_pvp/match/AdmissionToken.hpp"
#include "object_fps_pvp/world/PvpWorld.hpp"

#include <memory>
#include <string>

namespace PW = object_fps_pvp::world;
namespace N = GYO::Engine::Net;

namespace {

// The vector "accepted" of services/token/testdata/admission_vectors.txt: key a7 x 32, Match 0x8877665544332211,
// user 9, expiring at 1800000600.
constexpr std::uint64_t kMatch = 0x8877665544332211ULL;
constexpr std::int64_t kBeforeExpiry = 1'800'000'599;
constexpr N::Endpoint kHere{1};
constexpr N::Endpoint kThere{2};

std::vector<std::byte> Key() { return std::vector<std::byte>(32, std::byte{0xa7}); }

N::ConnectRequest Request(const std::uint64_t user, std::vector<std::byte> token) {
    N::ConnectRequest request;
    request.userId = user;
    request.token = std::move(token);
    return request;
}

std::string Detail(const N::NetResult<void>& result) { return result ? std::string("admitted") : result.error().message; }

} // namespace

TEST_CASE("world: a seat per user") {
    PW::Seats seats(2);
    CHECK_FALSE(seats.Take(1, kHere).has_value());
    CHECK_MESSAGE(seats.Take(1, kThere).value_or("seated") == "the user is already in the match",
                  "a second seat for one user from another endpoint");
    CHECK_FALSE(seats.Take(2, kThere).has_value());
    CHECK_MESSAGE(seats.Take(3, kHere).value_or("seated") == "the match is full", "a seat past the capacity");
    CHECK(seats.Taken() == 2);
    seats.Release(1);
    CHECK(seats.Taken() == 1);
    CHECK_MESSAGE(!seats.Take(1, kThere).has_value(), "a user who left cannot come back");
}

TEST_CASE("world: a reconnect from the same endpoint keeps its seat") {
    // The server ends the old connection and asks the admission before the World has seen the old one go: the new
    // connection comes in, and the old one's release afterwards leaves the new one seated.
    PW::Seats seats(1);
    CHECK_FALSE(seats.Take(5, kHere).has_value());
    CHECK_MESSAGE(!seats.Take(5, kHere).has_value(), "a reconnect from the same endpoint is refused");
    seats.Release(5);
    CHECK_MESSAGE(seats.Taken() == 1, "the old connection's release freed the new one's seat");
    CHECK_MESSAGE(seats.Take(6, kThere).value_or("seated") == "the match is full", "the seat was given away");
    seats.Release(5);
    CHECK(seats.Taken() == 0);
}

TEST_CASE("world: a keyed Match admits only its tokens") {
    auto seats = std::make_shared<PW::Seats>();
    std::int64_t now = kBeforeExpiry;
    // The vector's token, built from its fields (the MAC by the check the vectors test compares with the services).
    std::vector<std::byte> token(object_fps_pvp::match::kAdmissionSigned);
    const auto put = [&](const std::size_t at, const std::uint64_t value) {
        for (std::size_t i = 0; i < 8; ++i) token[at + i] = static_cast<std::byte>((value >> (56 - 8 * i)) & 0xff);
    };
    token[0] = std::byte{1};
    put(1, kMatch);
    put(9, 9);
    put(17, 1'800'000'600);
    const auto mac = object_fps_pvp::match::HmacSha256(Key(), token);
    token.insert(token.end(), mac.begin(), mac.end());

    const auto admission = PW::MatchAdmission(seats, PW::AdmissionCheck{Key(), kMatch, [&] { return now; }});
    CHECK_MESSAGE(Detail(admission(Request(8, token), kHere)) == "token user", "another user's token");
    CHECK_MESSAGE(Detail(admission(Request(9, {}), kHere)) == "token length", "no token");
    CHECK(seats->Taken() == 0);
    CHECK_MESSAGE(Detail(admission(Request(9, token), kHere)) == "admitted", "the user's own token");
    CHECK_MESSAGE(Detail(admission(Request(9, token), kThere)) == "the user is already in the match",
                  "the token used again from another endpoint");
    seats->Release(9);
    now = 1'800'000'600;
    CHECK_MESSAGE(Detail(admission(Request(9, token), kHere)) == "token expired", "a token at its expiry");
    const auto otherMatch = PW::MatchAdmission(seats, PW::AdmissionCheck{Key(), kMatch + 1, [] { return kBeforeExpiry; }});
    CHECK_MESSAGE(Detail(otherMatch(Request(9, token), kHere)) == "token match", "another Match's token");
}

TEST_CASE("world: a Match without a key admits by seat") {
    auto seats = std::make_shared<PW::Seats>(1);
    const auto admission = PW::SeatAdmission(seats);
    CHECK(Detail(admission(Request(5, {}), kHere)) == "admitted");
    CHECK(Detail(admission(Request(6, {}), kThere)) == "the match is full");
}
