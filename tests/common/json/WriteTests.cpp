// Writing: structure, escaping, number text, data JSON cannot represent,
// structural misuse, WriteValue and the round trip (Design Invariant J-5 of
// docs/architecture/target/v2/json-module-declaration.zh-Hant.md, 5.4 and 8.3).
#include <doctest/doctest.h>

// MSVC needs <ostream> for doctest to print std::string_view in CHECK.
#include <ostream>

#include <bit>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/base/Fnv1a.hpp"
#include "engine/json/Document.hpp"
#include "engine/json/Writer.hpp"

using namespace std::string_view_literals;
using GYO::Engine::Json::Document;
using GYO::Engine::Json::JsonErrorCode;
using GYO::Engine::Json::Parse;
using GYO::Engine::Json::Value;
using GYO::Engine::Json::WriteValue;
using GYO::Engine::Json::Writer;

namespace {

std::string Write(void (*body)(Writer&)) {
    std::string out;
    Writer writer(out);
    body(writer);
    auto done = writer.Finish();
    REQUIRE(done.has_value());
    return out;
}

std::string DoubleText(const double value) {
    std::string out;
    Writer writer(out);
    writer.Double(value);
    auto done = writer.Finish();
    REQUIRE(done.has_value());
    return out;
}

std::string StringText(const std::string_view text) {
    std::string out;
    Writer writer(out);
    writer.String(text);
    auto done = writer.Finish();
    REQUIRE(done.has_value());
    return out;
}

JsonErrorCode StringError(const std::string_view text) {
    std::string out;
    Writer writer(out);
    writer.String(text);
    auto done = writer.Finish();
    REQUIRE_FALSE(done.has_value());
    return done.error().code;
}

std::uint64_t Bits(const double value) { return std::bit_cast<std::uint64_t>(value); }

} // namespace

TEST_CASE("Writer: compact output in call order") {
    CHECK(Write([](Writer& w) {
              w.BeginObject();
              w.Key("tick");
              w.Uint(1207);
              w.Key("player");
              w.Int(-3);
              w.Key("moves");
              w.BeginArray();
              w.Bool(true);
              w.Null();
              w.BeginObject();
              w.EndObject();
              w.BeginArray();
              w.EndArray();
              w.EndArray();
              w.Key("name");
              w.String("p3");
              w.EndObject();
          }) == R"({"tick":1207,"player":-3,"moves":[true,null,{},[]],"name":"p3"})");
    CHECK(Write([](Writer& w) { w.Int(std::numeric_limits<std::int64_t>::min()); }) == "-9223372036854775808");
    CHECK(Write([](Writer& w) { w.Uint(std::numeric_limits<std::uint64_t>::max()); }) == "18446744073709551615");
    CHECK(Write([](Writer& w) { w.Bool(false); }) == "false");
}

TEST_CASE("Writer: strings are escaped per RFC 8259, non-ASCII stays UTF-8") {
    CHECK(StringText("\"\\") == R"("\"\\")");
    CHECK(StringText("\b\f\n\r\t") == R"("\b\f\n\r\t")");
    CHECK(StringText("\x01\x1f\x7f") == "\"\\u0001\\u001f\x7f\"");
    CHECK(StringText(std::string_view("a\0b", 3)) == R"("a\u0000b")");
    CHECK(StringText("/") == R"("/")");
    CHECK(StringText("\xC3\xA9\xF0\x9D\x84\x9E") == "\"\xC3\xA9\xF0\x9D\x84\x9E\"");
}

TEST_CASE("Writer: UTF-8 is checked with the same rules as Parse") {
    // Valid boundaries pass and read back byte for byte.
    for (const std::string_view text :
         {"\x7F"sv, "\xC2\x80"sv, "\xDF\xBF"sv, "\xE0\xA0\x80"sv, "\xED\x9F\xBF"sv, "\xEE\x80\x80"sv,
          "\xEF\xBF\xBF"sv, "\xF0\x90\x80\x80"sv, "\xF4\x8F\xBF\xBF"sv}) {
        const std::string written = StringText(text);
        auto document = Parse(written);
        REQUIRE(document.has_value());
        CHECK(document->Root().AsString().value() == text);
    }
    // Invalid sequences are NotRepresentable, as Parse would call them Syntax.
    for (const std::string_view text :
         {"\x80"sv, "\xC0\xAF"sv, "\xC1\xBF"sv, "\xE0\x80\x80"sv, "\xE0\x9F\xBF"sv, "\xED\xA0\x80"sv,
          "\xED\xBF\xBF"sv, "\xF0\x80\x80\x80"sv, "\xF0\x8F\xBF\xBF"sv, "\xF4\x90\x80\x80"sv, "\xF5\x80\x80\x80"sv,
          "\xFF"sv, "\xE2\x82"sv, "\xF0\x9D\x84"sv, "\xC3"sv, "a\xC3"sv}) {
        CHECK(StringError(text) == JsonErrorCode::NotRepresentable);
        auto parsed = Parse("\"" + std::string(text) + "\"");
        CHECK_FALSE(parsed.has_value());
    }
    // Keys are checked too.
    std::string out;
    Writer writer(out);
    writer.BeginObject();
    writer.Key("\xFF");
    writer.Null();
    writer.EndObject();
    auto keyDone = writer.Finish();
    REQUIRE_FALSE(keyDone.has_value());
    CHECK(keyDone.error().code == JsonErrorCode::NotRepresentable);
}

TEST_CASE("Writer: doubles in the shortest round-trip text, '.0' kept") {
    CHECK(DoubleText(100000.0) == "100000.0");
    CHECK(DoubleText(0.0001) == "0.0001");
    CHECK(DoubleText(1e21) == "1e21");
    CHECK(DoubleText(1e23) == "1e23");
    CHECK(DoubleText(-0.0) == "-0.0");
    CHECK(DoubleText(5e-324) == "5e-324");
    CHECK(DoubleText(0.1) == "0.1");
    CHECK(DoubleText(1.5e-7) == "1.5e-7");
    auto document = Parse(DoubleText(100000.0));
    REQUIRE(document.has_value());
    CHECK_FALSE(document->Root().IsIntegerLiteral());
}

TEST_CASE("Writer: a non-finite number fails Finish and cuts the string back") {
    for (const double value : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
        std::string out = "earlier line\n";
        Writer writer(out);
        writer.BeginObject();
        writer.Key("speed");
        writer.Double(value);
        writer.Key("after");
        writer.Int(1);
        writer.EndObject();
        auto done = writer.Finish();
        CHECK_FALSE(done.has_value());
        CHECK(out == "earlier line\n");
        if (done) continue;
        CHECK(done.error().code == JsonErrorCode::NotRepresentable);
        CHECK(done.error().detail == "value 3");
    }
}

// Structural misuse is a Programmer Error: one case per rule, so that a
// failure names the rule.
TEST_CASE("Writer misuse: a value in an object without a Key") {
    std::string out;
    Writer writer(out);
    writer.BeginObject();
    GYO_CHECK_ASSERTS(writer.Int(1));
}

TEST_CASE("Writer misuse: a Key outside an object") {
    std::string out;
    Writer inArray(out);
    inArray.BeginArray();
    GYO_CHECK_ASSERTS(inArray.Key("k"));
    std::string top;
    Writer atTop(top);
    GYO_CHECK_ASSERTS(atTop.Key("k"));
}

TEST_CASE("Writer misuse: an end that does not match") {
    std::string out;
    Writer writer(out);
    writer.BeginObject();
    GYO_CHECK_ASSERTS(writer.EndArray());
}

TEST_CASE("Writer misuse: ending an object after a Key") {
    std::string out;
    Writer writer(out);
    writer.BeginObject();
    writer.Key("k");
    GYO_CHECK_ASSERTS(writer.EndObject());
}

TEST_CASE("Writer misuse: a second top-level value") {
    std::string out;
    Writer writer(out);
    writer.Int(1);
    GYO_CHECK_ASSERTS(writer.Int(2));
}

TEST_CASE("Writer misuse: Finish with an open container") {
    std::string out;
    Writer writer(out);
    writer.BeginArray();
    GYO_CHECK_ASSERTS(writer.Finish());
}

TEST_CASE("Writer misuse: Finish with nothing written") {
    std::string out;
    Writer writer(out);
    GYO_CHECK_ASSERTS(writer.Finish());
}

TEST_CASE("Writer misuse: Finish twice, and writing after Finish") {
    std::string out;
    Writer writer(out);
    writer.Null();
    CHECK(writer.Finish().has_value());
    GYO_CHECK_ASSERTS(writer.Finish());
    GYO_CHECK_ASSERTS(writer.Null());
}

TEST_CASE("Writer: deeper than the inline levels still works") {
    std::string out;
    Writer writer(out);
    for (std::size_t i = 0; i < Writer::kInlineDepth * 3; ++i) writer.BeginArray();
    for (std::size_t i = 0; i < Writer::kInlineDepth * 3; ++i) writer.EndArray();
    REQUIRE(writer.Finish().has_value());
    CHECK(out == std::string(Writer::kInlineDepth * 3, '[') + std::string(Writer::kInlineDepth * 3, ']'));
}

TEST_CASE("WriteValue: members in document order, numbers as written") {
    const std::string_view text =
        R"({"z":0.10,"big":18446744073709551616,"neg":-0,"exp":1E+2,"s":"a\"\u00e9\n","n":null,"a":[[],{},true]})";
    auto document = Parse(text);
    REQUIRE(document.has_value());
    std::string out;
    Writer writer(out);
    WriteValue(writer, document->Root());
    REQUIRE(writer.Finish().has_value());
    CHECK(out == "{\"z\":0.10,\"big\":18446744073709551616,\"neg\":-0,\"exp\":1E+2,\"s\":\"a\\\"\xC3\xA9\\n\",\"n\":null,"
                 "\"a\":[[],{},true]}");
}

TEST_CASE("WriteValue: deep documents need no recursion") {
    constexpr std::size_t depth = 200000;
    std::string text(depth, '[');
    text.append(depth, ']');
    auto document = Parse(text);
    REQUIRE(document.has_value());
    std::string out;
    Writer writer(out);
    WriteValue(writer, document->Root());
    REQUIRE(writer.Finish().has_value());
    CHECK(out == text);
}

namespace {

std::uint64_t SplitMix64(std::uint64_t& state) {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

} // namespace

TEST_CASE("Writer and Parse: 200,000 doubles round-trip bit for bit") {
    std::vector<double> values;
    values.reserve(200000);
    std::uint64_t state = 20261120;
    while (values.size() < 200000) {
        const double value = std::bit_cast<double>(SplitMix64(state));
        if (value == value && value - value == 0.0) values.push_back(value);  // finite
    }
    std::string out;
    Writer writer(out);
    writer.BeginArray();
    for (const double value : values) writer.Double(value);
    writer.EndArray();
    REQUIRE(writer.Finish().has_value());
    auto document = Parse(out);
    REQUIRE(document.has_value());
    std::size_t index = 0;
    std::size_t mismatched = 0;
    for (const Value value : document->Root().AsArray().value()) {
        auto read = value.AsDouble();
        if (!read || Bits(*read) != Bits(values[index])) ++mismatched;
        ++index;
    }
    CHECK(index == values.size());
    CHECK(mismatched == 0);
}

TEST_CASE("Writer: the same number text on every platform (structured golden)") {
    // Inputs are decimal texts read by Parse (correctly rounded, the same on
    // every platform), so the doubles are identical everywhere; the golden is
    // the FNV-1a of what the Writer makes of them.
    std::vector<std::string> texts;
    for (int exponent = -324; exponent <= 308; ++exponent) texts.push_back("1e" + std::to_string(exponent));
    const char* digits = "12345678901234567";
    for (int length = 1; length <= 17; ++length) {
        for (int exponent = -330; exponent <= 308; exponent += 7) {
            texts.push_back(std::string(digits, length) + "e" + std::to_string(exponent));
        }
    }
    for (std::uint64_t integer = 1; integer < 100000000000000000ULL; integer *= 7) texts.push_back(std::to_string(integer));
    for (const char* text : {"-0", "0", "4.9406564584124654e-324", "2.2250738585072009e-308",
                             "2.2250738585072014e-308", "1.7976931348623157e308", "100000", "0.0001", "1e21",
                             "1e22", "9007199254740993", "0.30000000000000004"}) {
        texts.emplace_back(text);
    }
    std::string out;
    Writer writer(out);
    writer.BeginArray();
    std::size_t written = 0;
    for (const std::string& text : texts) {
        auto document = Parse(text);
        if (!document) continue;  // past the range: not part of the golden
        writer.Double(document->Root().AsDouble().value());
        ++written;
    }
    writer.EndArray();
    REQUIRE(writer.Finish().has_value());
    INFO(out);
    CHECK(written == 2198);
    CHECK(Engine::Base::Fnv1a64(out) == 0x0483ADE1A5CA673BULL);
}

// Added after the adversarial check of the mutation list (Fable/high, 2026-11-20;
// build/evidence/gyo2-json-module/mutation-check-fable/).

TEST_CASE("Writer misuse: two Keys in a row") {
    std::string out;
    Writer writer(out);
    writer.BeginObject();
    writer.Key("a");
    GYO_CHECK_ASSERTS(writer.Key("b"));
}

TEST_CASE("Writer: Finish returns the first error, not the last") {
    std::string out;
    Writer writer(out);
    writer.BeginArray();
    writer.String("\xFF");
    writer.Double(std::numeric_limits<double>::quiet_NaN());
    writer.EndArray();
    auto done = writer.Finish();
    REQUIRE_FALSE(done.has_value());
    CHECK(done.error().message == "invalid UTF-8");
    CHECK(done.error().detail == "value 2");
}

TEST_CASE("Writer: a level past the inline ones is dropped when it ends") {
    // The innermost array is empty, so its state ("nothing written yet")
    // differs from its parent's; a sibling written after it needs the comma.
    const std::size_t depth = Writer::kInlineDepth + 8;
    std::string out;
    Writer writer(out);
    for (std::size_t i = 0; i < depth; ++i) writer.BeginArray();
    writer.EndArray();
    writer.Int(2);
    for (std::size_t i = 1; i < depth; ++i) writer.EndArray();
    REQUIRE(writer.Finish().has_value());
    CHECK(out == std::string(depth, '[') + "],2" + std::string(depth - 1, ']'));
}

namespace {

std::string HexOf(const std::string& bytes) {
    std::string hex;
    char buffer[4];
    for (const char c : bytes) {
        std::snprintf(buffer, sizeof buffer, "%02x ", static_cast<unsigned>(static_cast<unsigned char>(c)));
        hex += buffer;
    }
    return hex;
}

} // namespace

TEST_CASE("Writer and Parse accept the same UTF-8: every sequence of 1 to 4 boundary bytes") {
    static constexpr unsigned char kBytes[] = {0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2, 0xDF,
                                               0xE0, 0xE1, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3, 0xF4, 0xF5, 0xFF};
    constexpr std::size_t kCount = sizeof kBytes;
    std::size_t sequences = 0;
    std::size_t accepted = 0;
    std::size_t disagreements = 0;
    std::string firstWriterOnly;  // the Writer writes it, Parse rejects it
    std::string firstParseOnly;   // Parse reads it, the Writer refuses it
    std::string text;
    std::string out;
    for (std::size_t length = 1; length <= 4; ++length) {
        std::size_t total = 1;
        for (std::size_t i = 0; i < length; ++i) total *= kCount;
        for (std::size_t n = 0; n < total; ++n) {
            text.clear();
            for (std::size_t i = 0, rest = n; i < length; ++i, rest /= kCount) text += static_cast<char>(kBytes[rest % kCount]);
            ++sequences;
            out.clear();
            Writer writer(out);
            writer.String(text);
            const bool writes = writer.Finish().has_value();
            const bool reads = Parse("\"" + text + "\"").has_value();
            if (writes) ++accepted;
            if (writes == reads) continue;
            ++disagreements;
            if (writes && firstWriterOnly.empty()) firstWriterOnly = HexOf(text);
            if (reads && firstParseOnly.empty()) firstParseOnly = HexOf(text);
        }
    }
    CHECK(sequences == 24 + 576 + 13824 + 331776);
    CHECK_MESSAGE(disagreements == 0, "written but not read back: " << firstWriterOnly << "| read but not written: "
                                                                  << firstParseOnly);
    CHECK(accepted == 1926);
}

TEST_CASE("Writer: a space and other printable ASCII are written as they are") {
    CHECK(StringText(" !~") == R"(" !~")");
}

TEST_CASE("WriteValue and AsBool: false stays false") {
    auto document = Parse("[true,false]");
    REQUIRE(document.has_value());
    std::vector<bool> read;
    for (const Value value : document->Root().AsArray().value()) read.push_back(value.AsBool().value());
    CHECK(read == std::vector<bool>{true, false});
    std::string out;
    Writer writer(out);
    WriteValue(writer, document->Root());
    REQUIRE(writer.Finish().has_value());
    CHECK(out == "[true,false]");
}

TEST_CASE("Writer misuse: an end without its begin") {
    std::string out;
    Writer writer(out);
    GYO_CHECK_ASSERTS(writer.EndArray());
}

TEST_CASE("Writer: a key written twice is not detected; Parse rejects the result") {
    std::string out;
    Writer writer(out);
    writer.BeginObject();
    writer.Key("k");
    writer.Int(1);
    writer.Key("k");
    writer.Int(2);
    writer.EndObject();
    REQUIRE(writer.Finish().has_value());
    CHECK(out == R"({"k":1,"k":2})");
    auto document = Parse(out);
    REQUIRE_FALSE(document.has_value());
    CHECK(document.error().code == JsonErrorCode::RepeatedKey);
}
