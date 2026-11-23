#include "doctest/doctest.h"

#include <cstring>
#include <optional>
#include <string>

#include "TextTestSupport.hpp"
#include "engine/asset/LoadContext.hpp"
#include "engine/text/FontLoader.hpp"

// The "font" Data Contract (version 1; declaration 5.2, Design Invariant
// TX-10): every rule, the loader, AddFont and Info.

using namespace GYO::Engine;
using GYO::Test::Text::LoadFont;
using GYO::Test::Text::ReadFont;
using Text::FontData;
using Text::TextFailureCode;

namespace {

using Bytes = std::vector<std::byte>;

std::uint32_t Read32(const Bytes& bytes, const std::size_t at) {
    return (std::to_integer<std::uint32_t>(bytes[at]) << 24) | (std::to_integer<std::uint32_t>(bytes[at + 1]) << 16) |
           (std::to_integer<std::uint32_t>(bytes[at + 2]) << 8) | std::to_integer<std::uint32_t>(bytes[at + 3]);
}

// The table directory entry of `tag` in a single sfnt.
std::size_t TableRecord(const Bytes& bytes, const char (&tag)[5]) {
    const std::size_t tables = (std::to_integer<std::size_t>(bytes[4]) << 8) | std::to_integer<std::size_t>(bytes[5]);
    for (std::size_t i = 0; i < tables; ++i) {
        const std::size_t record = 12 + 16 * i;
        if (std::memcmp(&bytes[record], tag, 4) == 0) {
            return record;
        }
    }
    FAIL("no table " << tag);
    return 0;
}

std::optional<TextFailureCode> Rejected(Bytes bytes) {
    auto font = FontData::FromBytes(std::move(bytes));
    if (font) {
        return std::nullopt;
    }
    return font.error().code;
}

Bytes Tagged(const char (&tag)[5], const std::size_t size = 64) {
    Bytes bytes(size, std::byte{0});
    std::memcpy(bytes.data(), tag, 4);
    return bytes;
}

} // namespace

TEST_CASE("text font: the three test fonts are taken, the collection has two faces") {
    CHECK(LoadFont("GyoTextTestLatin.ttf").FaceCount() == 1);
    CHECK(LoadFont("GyoTextTestKana.otf").FaceCount() == 1);
    CHECK(LoadFont("GyoTextTestPair.ttc").FaceCount() == 2);
    CHECK(LoadFont("BIZUDPGothic-Subset.ttf").FaceCount() == 1);
}

TEST_CASE("text font: rule 1, only sfnt fonts and collections") {
    CHECK(Rejected({}) == TextFailureCode::NotSfnt);
    CHECK(Rejected(Bytes(11, std::byte{0})) == TextFailureCode::NotSfnt);
    CHECK(Rejected(Tagged("wOFF")) == TextFailureCode::NotSfnt);
    CHECK(Rejected(Tagged("wOF2")) == TextFailureCode::NotSfnt);
    CHECK(Rejected(Tagged("typ1")) == TextFailureCode::NotSfnt);
    CHECK(Rejected(Tagged("%!PS")) == TextFailureCode::NotSfnt);
}

TEST_CASE("text font: rule 1, the 'true' tag of old Apple TrueType fonts is an sfnt") {
    Bytes bytes = ReadFont("GyoTextTestLatin.ttf");
    std::memcpy(bytes.data(), "true", 4);
    const auto font = FontData::FromBytes(std::move(bytes));
    CHECK(font.has_value());
}

TEST_CASE("text font: rule 2, a collection with no face") {
    Bytes bytes = Tagged("ttcf", 16);
    bytes[5] = std::byte{1}; // version 1.0, numFonts 0
    CHECK(Rejected(bytes) == TextFailureCode::EmptyCollection);
}

TEST_CASE("text font: rule 2, a face that cannot be read") {
    Bytes truncated = ReadFont("GyoTextTestLatin.ttf");
    truncated.resize(40);
    CHECK(Rejected(truncated) == TextFailureCode::FaceUnreadable);
}

TEST_CASE("text font: rule 2, a face with no glyph") {
    Bytes bytes = ReadFont("GyoTextTestLatin.ttf");
    const std::size_t maxp = Read32(bytes, TableRecord(bytes, "maxp") + 8);
    bytes[maxp + 4] = std::byte{0}; // numGlyphs
    bytes[maxp + 5] = std::byte{0};
    CHECK(Rejected(bytes) == TextFailureCode::NoGlyphs);
}

TEST_CASE("text font: rule 2, a face without a Unicode character map") {
    Bytes bytes = ReadFont("GyoTextTestLatin.ttf");
    std::memcpy(&bytes[TableRecord(bytes, "cmap")], "xmap", 4);
    CHECK(Rejected(bytes) == TextFailureCode::NoUnicodeCmap);
}

TEST_CASE("text font: the loader takes the Data Contract and ignores settings") {
    const Text::FontLoader loader;
    CHECK(loader.ParseSettings(std::nullopt).has_value());
    CHECK(loader.ParseSettings(std::string_view(R"({"size":12})")).has_value());
    const Bytes bytes = ReadFont("GyoTextTestPair.ttc");
    Asset::LoadContext context(Asset::AssetId("test.font"), "fonts/pair.ttc", Text::FontLoader::kDefaultMaxBytes);
    const auto loaded = loader.Load(bytes, Text::FontSettings{}, context);
    REQUIRE(loaded.has_value());
    CHECK(loaded.value().FaceCount() == 2);
    CHECK(loaded.value().Bytes().size() == bytes.size());

    const Bytes woff = Tagged("wOFF");
    const auto rejected = loader.Load(woff, Text::FontSettings{}, context);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error().code == TextFailureCode::NotSfnt);
    CHECK(rejected.error().detail.find("test.font") != std::string::npos);
    CHECK(rejected.error().detail.find("fonts/pair.ttc") != std::string::npos);
}

TEST_CASE("text font: AddFont opens a face; an index past the faces is a failure") {
    Text::TextContext context;
    const FontData pair = LoadFont("GyoTextTestPair.ttc");
    const auto latin = context.AddFont(pair, 0);
    const auto kana = context.AddFont(pair, 1);
    REQUIRE(latin.has_value());
    REQUIRE(kana.has_value());
    CHECK(latin.value().value == 0);
    CHECK(kana.value().value == 1);
    CHECK(context.FontCount() == 2);
    const Text::FontInfo latinInfo = context.Info(latin.value());
    CHECK(latinInfo.familyName == "GYO Text Test Latin");
    CHECK(latinInfo.styleName == "Regular");
    CHECK(latinInfo.unitsPerEm == 1024);
    CHECK(context.Info(kana.value()).familyName == "GYO Text Test Kana");
    const auto missing = context.AddFont(pair, 2);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().code == TextFailureCode::FaceIndexOutOfRange);
}

TEST_CASE("text font: the font's bytes are shared by its copies and outlive the caller's") {
    Text::TextContext context;
    Text::FontId id;
    {
        const FontData font = LoadFont("GyoTextTestLatin.ttf");
        const FontData copy = font;
        CHECK(copy.Bytes().data() == font.Bytes().data());
        id = context.AddFont(font, 0).value();
    }
    // The context keeps the font: shaping after the caller's copies are gone works.
    const std::vector<Text::FontId> fonts{id};
    const Text::TextSpan span{0, 1, Text::TextStyle{fonts, 16.0f, Text::LineHeight::Pixels(16.0f)}};
    const Text::TextLayout layout = context.Layout(context.Shape("A", {&span, 1}), {});
    REQUIRE(layout.glyphs.size() == 1);
    CHECK(layout.glyphs[0].advance == 8.0f);
}
