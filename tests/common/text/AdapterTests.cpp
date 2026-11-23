#include "doctest/doctest.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "Utf8.hpp"
#include "adapters/freetype/FreeTypeRaster.hpp"
#include "adapters/harfbuzz/HarfBuzzShaping.hpp"
#include "adapters/libunibreak/Segmentation.hpp"
#include "engine/base/Sha256.hpp"

// The adapters through their own interfaces (engine/modules/text/src):
// - Unicode's conformance tests for line breaking (UAX #14) and grapheme
//   breaking (UAX #29), every line (Design Invariant TX-4, criterion 3);
// - the two adapters read the same advances (Design Invariant TX-2);
// - the UTF-8 decoder's U+FFFD per maximal subpart.

using namespace GYO::Engine;
using namespace GYO::Engine::Text::Detail;

namespace {

std::vector<std::byte> ReadBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << path.string());
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(chars.size());
    for (std::size_t i = 0; i < chars.size(); ++i) {
        bytes[i] = static_cast<std::byte>(chars[i]);
    }
    return bytes;
}

struct Case {
    std::vector<char32_t> text;
    // breaks[i]: a break after text[i].
    std::vector<bool> breaks;
    int line = 0;
};

// Unicode's test format: "÷ 0023 × 0020 ÷ # comment", one break mark before,
// between and after the code points.
std::vector<Case> ReadCases(const std::vector<std::byte>& bytes) {
    std::vector<Case> cases;
    std::istringstream in(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        line = line.substr(0, line.find('#'));
        std::istringstream fields(line);
        std::string token;
        Case c;
        c.line = number;
        bool first = true;
        while (fields >> token) {
            if (token == "\xC3\xB7" || token == "\xC3\x97") { // ÷ or ×
                if (!first) {
                    c.breaks.push_back(token == "\xC3\xB7");
                }
                first = false;
            } else {
                c.text.push_back(static_cast<char32_t>(std::stoul(token, nullptr, 16)));
            }
        }
        if (!c.text.empty()) {
            REQUIRE(c.breaks.size() == c.text.size());
            cases.push_back(std::move(c));
        }
    }
    return cases;
}

const std::filesystem::path kUnicode = GYO_TEXT_UNICODE_TESTS;
const std::filesystem::path kFonts = GYO_TEXT_TEST_FONTS;

} // namespace

TEST_CASE("text adapter: Unicode 17.0 LineBreakTest, every case (UAX #14 default resolution)") {
    const std::vector<std::byte> bytes = ReadBytes(kUnicode / "LineBreakTest.txt");
    // The same file unicode.org publishes as LineBreakTest-17.0.0.txt.
    CHECK(Base::Sha256(bytes) == "e69884e0dde6a8724873f885d68c52dc14518abf9ae4ca9e2283b8773db3b752");
    const std::vector<Case> cases = ReadCases(bytes);
    CHECK(cases.size() == 19338);
    std::size_t compared = 0;
    std::size_t failed = 0;
    std::vector<LineBreakAfter> breaks;
    for (const Case& c : cases) {
        FindLineBreaks(c.text, {}, true, breaks);
        REQUIRE(breaks.size() == c.text.size());
        bool same = true;
        for (std::size_t i = 0; i < c.text.size(); ++i) {
            same = same && ((breaks[i] != LineBreakAfter::None) == c.breaks[i]);
        }
        ++compared;
        if (!same) {
            ++failed;
            if (failed <= 5) {
                MESSAGE("LineBreakTest.txt line " << c.line << " differs");
            }
        }
    }
    CHECK(compared == cases.size());
    CHECK(failed == 0);
}

TEST_CASE("text adapter: Unicode 17.0 GraphemeBreakTest, every case (UAX #29)") {
    const std::vector<std::byte> bytes = ReadBytes(kUnicode / "GraphemeBreakTest.txt");
    CHECK(Base::Sha256(bytes) == "e2d134d2c52919bace503ebb6a551c1855fe1a1faec18478c78fff254a1793ec");
    const std::vector<Case> cases = ReadCases(bytes);
    CHECK(cases.size() == 766);
    std::size_t failed = 0;
    std::vector<std::uint8_t> ends;
    for (const Case& c : cases) {
        FindGraphemeBoundaries(c.text, ends);
        REQUIRE(ends.size() == c.text.size());
        for (std::size_t i = 0; i < c.text.size(); ++i) {
            if ((ends[i] != 0) != c.breaks[i]) {
                ++failed;
                MESSAGE("GraphemeBreakTest.txt line " << c.line << " differs at " << i);
                break;
            }
        }
    }
    CHECK(failed == 0);
}

TEST_CASE("text adapter: line breaks after mandatory breaks, and at the text's end") {
    std::vector<LineBreakAfter> breaks;
    const std::u32string text = U"a b\nc";
    FindLineBreaks(text, {}, true, breaks);
    CHECK(breaks == std::vector<LineBreakAfter>{LineBreakAfter::None, LineBreakAfter::Allowed, LineBreakAfter::None,
                                                LineBreakAfter::Mandatory, LineBreakAfter::Mandatory});
    // CJ (small kana) resolves to NS: no break before っ.
    const std::u32string kana = U"かっこ";
    FindLineBreaks(kana, {}, true, breaks);
    CHECK(breaks[0] == LineBreakAfter::None);
    CHECK(breaks[1] == LineBreakAfter::Allowed);
}

TEST_CASE("text adapter: HarfBuzz and FreeType read the same advances in font units") {
    for (const char* name : {"GyoTextTestLatin.ttf", "GyoTextTestKana.otf", "BIZUDPGothic-Subset.ttf"}) {
        CAPTURE(name);
        const std::vector<std::byte> bytes = ReadBytes(kFonts / name);
        const auto face = HarfBuzzFace::Create(bytes, 0);
        REQUIRE(face != nullptr);
        FreeTypeLibrary freeType;
        FreeTypeOpen status = FreeTypeOpen::Unreadable;
        const auto handle = freeType.OpenFace(bytes, 0, status);
        REQUIRE(handle.has_value());
        const std::uint32_t glyphs = face->GlyphCount();
        REQUIRE(glyphs > 1);
        std::uint32_t compared = 0;
        for (std::uint32_t glyph = 0; glyph < glyphs; ++glyph) {
            const std::int32_t harfBuzz = face->UnscaledAdvance(glyph);
            const std::int32_t freeTypeAdvance = freeType.UnscaledAdvance(*handle, glyph);
            CAPTURE(glyph);
            CHECK(harfBuzz == freeTypeAdvance);
            ++compared;
        }
        CHECK(compared == glyphs);
    }
}

TEST_CASE("text adapter: UTF-8 decoding, U+FFFD per maximal subpart") {
    std::vector<char32_t> cps;
    std::vector<std::uint32_t> offsets;
    DecodeUtf8("A\xE3\x81\x82\xF0\x9F\x98\x80", cps, offsets);
    CHECK(cps == std::vector<char32_t>{U'A', U'あ', U'\U0001F600'});
    CHECK(offsets == std::vector<std::uint32_t>{0, 1, 4, 8});
    // Unicode's example (Table 3-8): 61 F1 80 80 E1 80 C2 62 80 63 80 BF 64.
    DecodeUtf8("\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64", cps, offsets);
    CHECK(cps == std::vector<char32_t>{U'a', 0xFFFD, 0xFFFD, 0xFFFD, U'b', 0xFFFD, U'c', 0xFFFD, 0xFFFD, U'd'});
    CHECK(offsets == std::vector<std::uint32_t>{0, 1, 4, 6, 7, 8, 9, 10, 11, 12, 13});
    // Overlong, surrogate, past U+10FFFF, C0/C1, F5.
    DecodeUtf8("\xC0\xAF\xED\xA0\x80\xF4\x90\x80\x80\xF5", cps, offsets);
    CHECK(cps == std::vector<char32_t>(10, 0xFFFD));
    // An overlong three-byte sequence: E0 needs A0..BF second, so each byte is its own subpart.
    DecodeUtf8("\xE0\x80\x80", cps, offsets);
    CHECK(cps == std::vector<char32_t>(3, 0xFFFD));
    // An overlong four-byte sequence: F0 needs 90..BF second.
    DecodeUtf8("\xF0\x80\x80\x80", cps, offsets);
    CHECK(cps == std::vector<char32_t>(4, 0xFFFD));
}
