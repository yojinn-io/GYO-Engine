// The behaviour of the two external components that the JSON Module's adapter
// relies on (step 1 of docs/architecture/target/v2/json-module-declaration.zh-Hant.md).
// These tests name the components directly: tests/common/json is the adapter's
// own test directory, the one place outside engine/json/src that Design
// Invariant J-1 allows to. A component upgrade that changes one of these
// behaviours fails here before it reaches the adapter (Replacement Criteria
// RC-1 and RC-6).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

#include <fast_float/fast_float.h>
#include <yyjson.h>

#include "engine/base/Fnv1a.hpp"

namespace {

struct Converted {
    double value;
    std::errc ec;
    std::size_t consumed;
};

Converted FastFloat(const std::string_view text) {
    double value = 12345.0; // sentinel: shows whether the call wrote a value
    const auto result = fast_float::from_chars(text.data(), text.data() + text.size(), value);
    return {value, result.ec, static_cast<std::size_t>(result.ptr - text.data())};
}

std::uint64_t Bits(const double value) { return std::bit_cast<std::uint64_t>(value); }

} // namespace

TEST_CASE("fast_float: correctly rounded decimals, signed zero and subnormals") {
    CHECK(Bits(FastFloat("0.1").value) == Bits(0.1));
    CHECK(Bits(FastFloat("20.1").value) == Bits(20.1));
    CHECK(Bits(FastFloat("1e23").value) == Bits(1e23));
    CHECK(Bits(FastFloat("-0").value) == Bits(-0.0));
    CHECK(Bits(FastFloat("5e-324").value) == 1u);
    // Just above half of the smallest subnormal rounds up to it.
    CHECK(Bits(FastFloat("2.4703282292062328e-324").value) == 1u);
    CHECK(Bits(FastFloat("1.7976931348623157e308").value) == Bits(std::numeric_limits<double>::max()));
    // The largest decimal that still rounds to the largest double.
    CHECK(Bits(FastFloat("1.7976931348623158079e308").value) == Bits(std::numeric_limits<double>::max()));
}

TEST_CASE("fast_float: how it reports overflow and underflow") {
    // The adapter judges range itself (finite, and non-zero text never rounds
    // to zero); these record what fast_float reports so that a change shows.
    const Converted overflow = FastFloat("1e400");
    CHECK(overflow.ec == std::errc::result_out_of_range);
    CHECK(std::isinf(overflow.value));
    const Converted overflowEdge = FastFloat("1.797693134862315808e308");
    CHECK(overflowEdge.ec == std::errc::result_out_of_range);
    const Converted underflow = FastFloat("1e-400");
    CHECK(underflow.ec == std::errc::result_out_of_range);
    CHECK(underflow.value == 0.0);
    const Converted belowHalf = FastFloat("2.4703282292062327e-324");
    CHECK(belowHalf.ec == std::errc::result_out_of_range);
    CHECK(belowHalf.value == 0.0);
    // Zero text with a huge exponent is zero, not underflow.
    const Converted zero = FastFloat("0e999999");
    CHECK(zero.ec == std::errc{});
    CHECK(Bits(zero.value) == Bits(0.0));
}

namespace {

constexpr yyjson_read_flag kRawRead = YYJSON_READ_NUMBER_AS_RAW;

bool Accepts(const std::string_view text, const yyjson_read_flag flags = kRawRead) {
    yyjson_read_err err{};
    yyjson_doc* doc = yyjson_read_opts(const_cast<char*>(text.data()), text.size(), flags, nullptr, &err);
    const bool ok = doc != nullptr;
    yyjson_doc_free(doc);
    return ok;
}

} // namespace

TEST_CASE("yyjson: strictness under the raw-number flag") {
    using namespace std::string_view_literals;
    CHECK(Accepts("[1, 2.5, \"x\"]"sv));
    CHECK_FALSE(Accepts("123\0"sv));            // a number followed by NUL
    CHECK_FALSE(Accepts("\xEF\xBB\xBF{}"sv));   // UTF-8 BOM
    CHECK_FALSE(Accepts("{} x"sv));             // trailing content
    CHECK_FALSE(Accepts(""sv));
    CHECK_FALSE(Accepts("[NaN]"sv));
    CHECK_FALSE(Accepts("[1,]"sv));
    CHECK_FALSE(Accepts("[\"\\uD800\"]"sv));    // lone high surrogate escape
    CHECK_FALSE(Accepts("[\"\xC0\xAF\"]"sv));   // overlong UTF-8
    CHECK_FALSE(Accepts("[01]"sv));
    // The raw flag accepts any grammatical number; range is the adapter's job.
    CHECK(Accepts("[1e400]"sv));
    CHECK(Accepts("[1e-400]"sv));
}

TEST_CASE("yyjson: raw numbers and keys live in the document's own copy of the input") {
    std::string text = R"({"k":-0.10,"a":[18446744073709551616]})";
    yyjson_read_err err{};
    yyjson_doc* doc = yyjson_read_opts(text.data(), text.size(), kRawRead, nullptr, &err);
    REQUIRE(doc != nullptr);
    std::memset(text.data(), 'x', text.size()); // the input may go away after parsing
    yyjson_val* root = yyjson_doc_get_root(doc);
    yyjson_val* k = yyjson_obj_get(root, "k");
    REQUIRE(yyjson_is_raw(k));
    CHECK(std::string_view(yyjson_get_raw(k), yyjson_get_len(k)) == "-0.10");
    yyjson_val* big = yyjson_arr_get_first(yyjson_obj_get(root, "a"));
    REQUIRE(yyjson_is_raw(big));
    CHECK(std::string_view(yyjson_get_raw(big), yyjson_get_len(big)) == "18446744073709551616");
    // Duplicate members stay in the document; the adapter detects them.
    yyjson_doc_free(doc);
    const std::string_view dup = R"({"a":1,"a":2})";
    doc = yyjson_read_opts(const_cast<char*>(dup.data()), dup.size(), kRawRead, nullptr, &err);
    REQUIRE(doc != nullptr);
    CHECK(yyjson_obj_size(yyjson_doc_get_root(doc)) == 2);
    yyjson_doc_free(doc);
}

namespace {

std::string WriteNumber(const double value) {
    yyjson_mut_val number;
    yyjson_mut_set_real(&number, value);
    char buffer[64];
    char* end = yyjson_mut_write_number(&number, buffer);
    return end == nullptr ? std::string("(null)") : std::string(buffer, end);
}

} // namespace

TEST_CASE("yyjson_write_number: shortest round-trip text, '.0' kept, no NaN") {
    CHECK(WriteNumber(100000.0) == "100000.0");
    CHECK(WriteNumber(1e21) == "1e21");
    CHECK(WriteNumber(1e23) == "1e23");
    CHECK(WriteNumber(0.0001) == "0.0001");
    CHECK(WriteNumber(-0.0) == "-0.0");
    CHECK(WriteNumber(5e-324) == "5e-324");
    CHECK(WriteNumber(1.5e-7) == "1.5e-7");
    CHECK(WriteNumber(std::numeric_limits<double>::quiet_NaN()) == "(null)");
    CHECK(WriteNumber(std::numeric_limits<double>::infinity()) == "(null)");
}

TEST_CASE("yyjson_write_number: the same text on every platform (small golden)") {
    // Literals, not computed values, so the inputs are the same bits on every
    // compiler. The golden is the FNV-1a of the outputs joined by '\n'.
    static constexpr double kValues[] = {
        1e-10, 1e-9, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 1e-1, 1.0, 1e1, 1e2, 1e3, 1e4, 1e5,
        1e6, 1e7, 1e8, 1e9, 1e10, 1e15, 1e16, 1e17, 1e20, 1e21, 1e22, 1e100, 1e-100, 1e308, 1e-308,
        12.0, 123.0, 1234567.0, 123456789012345678.0, 1.2, 1.23, 0.3, 0.30000000000000004,
        2.2250738585072014e-308, 2.2250738585072009e-308, 4.9406564584124654e-324,
        1.7976931348623157e308, 9007199254740993.0, 0.1, 0.7, 20.1, 3.0517578125e-5,
        7.2057594037927933e16, -1.5, -123.456, 65536.0, 4294967296.0, 1.0 / 3.0, 2.0 / 3.0,
    };
    std::string joined;
    for (const double value : kValues) {
        joined += WriteNumber(value);
        joined += '\n';
    }
    INFO(joined); // shown only when the golden fails
    CHECK(Engine::Base::Fnv1a64(joined) == 0x8100A3C1F6B4B980ULL);
}
