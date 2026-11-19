// Numbers: the raw-number path (Design Invariant J-3) and the range rule of
// strict parsing (J-2); docs/architecture/target/v2/json-module-declaration.zh-Hant.md 5.3.
#include <doctest/doctest.h>

#include <bit>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/json/Document.hpp"

using namespace std::string_view_literals;
using GYO::Engine::Json::Document;
using GYO::Engine::Json::JsonErrorCode;
using GYO::Engine::Json::Parse;
using GYO::Engine::Json::Value;

namespace {

Document MustParse(const std::string_view text) {
    auto document = Parse(text);
    REQUIRE(document.has_value());
    return std::move(*document);
}

std::uint64_t Bits(const double value) { return std::bit_cast<std::uint64_t>(value); }

std::uint64_t DoubleBits(const std::string_view text) {
    const Document document = MustParse(text);
    auto value = document.Root().AsDouble();
    REQUIRE(value.has_value());
    return Bits(*value);
}

JsonErrorCode ParseError(const std::string_view text) {
    auto document = Parse(text);
    REQUIRE_FALSE(document.has_value());
    return document.error().code;
}

} // namespace

TEST_CASE("Parse: a number that overflows or whose non-zero text rounds to zero is NumberOutOfRange") {
    CHECK(ParseError("1e400"sv) == JsonErrorCode::NumberOutOfRange);
    CHECK(ParseError("-1e400"sv) == JsonErrorCode::NumberOutOfRange);
    CHECK(ParseError("1.797693134862315808e308"sv) == JsonErrorCode::NumberOutOfRange);  // just past the largest
    CHECK(ParseError("1e-400"sv) == JsonErrorCode::NumberOutOfRange);
    CHECK(ParseError("2.4703282292062327e-324"sv) == JsonErrorCode::NumberOutOfRange); // below half the smallest
    CHECK(ParseError("-1e-400"sv) == JsonErrorCode::NumberOutOfRange);
    CHECK(ParseError("1e99999999999999999999"sv) == JsonErrorCode::NumberOutOfRange);  // exponent beyond 64 bits
}

TEST_CASE("Parse: a number nobody reads is still checked; the detail points at it") {
    auto document = Parse("{\"ok\":1,\n \"list\":[0, 1e400]}"sv);
    REQUIRE_FALSE(document.has_value());
    CHECK(document.error().code == JsonErrorCode::NumberOutOfRange);
    CHECK(document.error().detail == "line 2, column 13; /list/1");
    auto root = Parse("1e-400"sv);
    REQUIRE_FALSE(root.has_value());
    CHECK(root.error().detail == "line 1, column 1");
}

TEST_CASE("Parse: zero text is zero at any exponent, not an underflow") {
    CHECK(DoubleBits("0e999999"sv) == Bits(0.0));
    CHECK(DoubleBits("-0e-999"sv) == Bits(-0.0));
    CHECK(DoubleBits("0.000"sv) == Bits(0.0));
    CHECK(DoubleBits("0e99999999999999999999"sv) == Bits(0.0));
}

TEST_CASE("AsDouble: correctly rounded at the edges") {
    CHECK(DoubleBits("1.7976931348623158079e308"sv) == Bits(std::numeric_limits<double>::max()));
    CHECK(DoubleBits("2.4703282292062328e-324"sv) == 1u);  // rounds up to the smallest subnormal
    CHECK(DoubleBits("4.9406564584124654e-324"sv) == 1u);
    CHECK(DoubleBits("0.1"sv) == Bits(0.1));
    CHECK(DoubleBits("-0"sv) == Bits(-0.0));
    CHECK(DoubleBits("-0.0"sv) == Bits(-0.0));
    // Integer literals read as doubles too, rounded to the nearest double.
    CHECK(DoubleBits("9007199254740993"sv) == Bits(9007199254740992.0));
    CHECK(DoubleBits("18446744073709551616"sv) == Bits(18446744073709551616.0));
}

namespace {

// "i:<value>" or the error code's name: a failed read shows in the CHECK
// instead of stopping the test at value().
std::string I(const Value value) {
    auto read = value.AsInt64();
    return read ? "i:" + std::to_string(*read) : ToString(read.error().code);
}

std::string U(const Value value) {
    auto read = value.AsUint64();
    return read ? "u:" + std::to_string(*read) : ToString(read.error().code);
}

} // namespace

TEST_CASE("AsInt64 and AsUint64: integer literals only, in range") {
    const Document document = MustParse(
        R"([1.0, 1e2, 9223372036854775807, -9223372036854775808, 9223372036854775808, -0, -5, 18446744073709551615, 18446744073709551616])"sv);
    std::vector<Value> values;
    for (const Value value : document.Root().AsArray().value()) values.push_back(value);
    REQUIRE(values.size() == 9);

    CHECK(I(values[0]) == "WrongKind");
    CHECK(values[0].AsInt64().error().detail == "expected an integer literal, found 1.0");
    CHECK(U(values[1]) == "WrongKind");
    CHECK(I(values[2]) == "i:9223372036854775807");
    CHECK(I(values[3]) == "i:-9223372036854775808");
    CHECK(I(values[4]) == "NumberOutOfRange");
    CHECK(U(values[4]) == "u:9223372036854775808");
    CHECK(I(values[5]) == "i:0");
    CHECK(U(values[5]) == "u:0");  // the adapter's own case: std::from_chars rejects "-0" unsigned
    CHECK(I(values[6]) == "i:-5");
    CHECK(U(values[6]) == "NumberOutOfRange");
    CHECK(U(values[7]) == "u:18446744073709551615");
    CHECK(U(values[8]) == "NumberOutOfRange");
    CHECK(I(values[8]) == "NumberOutOfRange");
    CHECK(values[8].AsDouble().has_value());
}

TEST_CASE("IsIntegerLiteral and AsNumberText") {
    const Document document = MustParse(R"([12, -0, 1.0, 1e2, 1E2, "12", 0.10])"sv);
    std::string pattern;
    std::string_view last;
    for (const Value value : document.Root().AsArray().value()) {
        pattern += value.IsIntegerLiteral() ? 'i' : '-';
        if (auto text = value.AsNumberText()) last = *text;
    }
    CHECK(pattern == "ii-----");
    CHECK(last == "0.10");
}
