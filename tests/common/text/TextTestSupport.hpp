#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "doctest/doctest.h"
#include "engine/text/TextContext.hpp"

// Shared helpers of the text Module's tests. The fonts are described in
// tests/common/text/fonts/README.md; every expected value of the exact tests
// comes from the numbers in generate_test_fonts.py: unitsPerEm 1024, so at
// 16 px one font unit is 1/64 px (26.6 fixed point is the font unit).
namespace GYO::Test::Text {

namespace TextApi = ::GYO::Engine::Text;

inline std::vector<std::byte> ReadFont(const std::string_view name) {
    const std::filesystem::path path = std::filesystem::path(GYO_TEXT_TEST_FONTS) / std::string(name);
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << path.string());
    const std::vector<char> chars((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes(chars.size());
    for (std::size_t i = 0; i < chars.size(); ++i) {
        bytes[i] = static_cast<std::byte>(chars[i]);
    }
    return bytes;
}

inline TextApi::FontData LoadFont(const std::string_view name) {
    auto font = TextApi::FontData::FromBytes(ReadFont(name));
    REQUIRE_MESSAGE(font.has_value(), "font " << name << " rejected");
    return std::move(font).value();
}

// Font units of the synthetic fonts at 16 px, as pixels.
constexpr float Units16(const int units) { return static_cast<float>(units) / 64.0f; }

// A context with the Latin font (FontId 0) and the Kana font (FontId 1).
struct Fixture {
    TextApi::TextContext context;
    TextApi::FontId latin;
    TextApi::FontId kana;
    std::vector<TextApi::FontId> latinFirst;
    std::vector<TextApi::FontId> kanaFirst;

    Fixture() {
        latin = context.AddFont(LoadFont("GyoTextTestLatin.ttf"), 0).value();
        kana = context.AddFont(LoadFont("GyoTextTestKana.otf"), 0).value();
        latinFirst = {latin, kana};
        kanaFirst = {kana, latin};
    }

    TextApi::TextStyle Style(const std::vector<TextApi::FontId>& fonts, const float size = 16.0f,
                             const TextApi::LineHeight lineHeight = TextApi::LineHeight::Pixels(16.0f)) const {
        return TextApi::TextStyle{fonts, size, lineHeight};
    }

    TextApi::ShapedText Shape(const std::string_view text, const TextApi::TextStyle& style) {
        const TextApi::TextSpan span{0, static_cast<std::uint32_t>(text.size()), style};
        return context.Shape(text, std::span<const TextApi::TextSpan>(&span, 1));
    }

    TextApi::TextLayout Lay(const std::string_view text, const TextApi::TextStyle& style,
                            const TextApi::LayoutOptions& options = {}) {
        return context.Layout(Shape(text, style), options);
    }
};

} // namespace GYO::Test::Text
