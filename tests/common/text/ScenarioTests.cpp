#include "doctest/doctest.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "TextTestSupport.hpp"
#include "engine/text/Caret.hpp"

// The declaration's scenarios (8.5): a HUD rebuilt every frame (F1), a text
// box with an IME composition (F2), a wrapped description (F3); and the
// threading contract (Design Invariant TX-7).

using namespace GYO::Engine;
using GYO::Test::Text::Fixture;
using GYO::Test::Text::LoadFont;

namespace {

std::string Remaining(const int n, const int m) {
    return "残り " + std::to_string(n) + " / " + std::to_string(m);
}

std::uint32_t VisibleGlyphs(const Text::TextLayout& layout) {
    return static_cast<std::uint32_t>(
        std::count_if(layout.glyphs.begin(), layout.glyphs.end(), [](const auto& g) { return g.visible; }));
}

} // namespace

TEST_CASE("text scenario: a HUD rebuilt every frame draws each glyph once") {
    Text::TextContext context;
    const Text::FontId biz = context.AddFont(LoadFont("BIZUDPGothic-Subset.ttf"), 0).value();
    const std::vector<Text::FontId> fonts{biz};
    Text::GlyphAtlas atlas;
    std::set<char> digitsSeen;
    std::uint32_t drawn = 0;
    std::uint64_t followed = 0;
    std::size_t changedRects = 0;
    for (int frame = 0; frame < 1000; ++frame) {
        std::uint32_t newThisFrame = 0;
        std::set<char> newDigits;
        for (int line = 0; line < 30; ++line) {
            const int n = (frame * 7 + line * 13) % 100;
            const std::string text = Remaining(n, 99);
            for (const char c : text) {
                if (c >= '0' && c <= '9' && digitsSeen.insert(c).second) {
                    newDigits.insert(c);
                }
            }
            const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()),
                                      Text::TextStyle{fonts, 24.0f, Text::LineHeight::Relative(1.2f)}};
            const Text::TextLayout layout = context.Layout(context.Shape(text, {&span, 1}),
                                                           Text::LayoutOptions{200.0f, Text::HorizontalAlign::Center});
            const Text::RasterResult result = context.Rasterize(layout, atlas);
            REQUIRE(result.placements.size() == layout.glyphs.size());
            newThisFrame += result.newGlyphs;
            CHECK(VisibleGlyphs(layout) == layout.glyphs.size());
        }
        drawn += newThisFrame;
        if (frame > 0) {
            // After the first frame only digits never seen before are new.
            CHECK(newThisFrame == newDigits.size());
        }
        const Text::AtlasChanges changes = atlas.ChangesSince(followed);
        followed = changes.revision;
        changedRects += changes.changes.size();
    }
    CHECK(atlas.GlyphCount() == drawn);
    // Every placed glyph and every page appeared once in the changes.
    CHECK(changedRects == drawn + atlas.PageCount());
}

TEST_CASE("text scenario: a label shrinks one pixel at a time until it fits") {
    Fixture f;
    // "STAGE 12": seven 512-unit glyphs and a 256-unit space, 3.75 px per px of size.
    float size = 40.0f;
    while (f.Lay("STAGE 12", f.Style(f.latinFirst, size)).width > 100.0f) {
        size -= 1.0f;
    }
    CHECK(size == 26.0f);
}

TEST_CASE("text scenario: a text box with an IME composition and a ligature") {
    Fixture f;
    // "fi" + composition "がっこう" + "A": bytes fi [0,2), がっこう [2,14), A [14,15).
    const std::string text = "fi\xE3\x81\x8C\xE3\x81\xA3\xE3\x81\x93\xE3\x81\x86" "A";
    const Text::TextLayout layout = f.Lay(text, f.Style(f.latinFirst));
    std::vector<Text::Rect> underline;
    Text::RangeRects(layout, 2, 14, underline);
    REQUIRE(underline.size() == 1);
    // The underline covers exactly the composition's glyphs.
    CHECK(underline[0].x == layout.glyphs[1].penX);
    CHECK(underline[0].width == 64.0f);
    // The caret after っ (byte 8).
    CHECK(Text::CaretAt(layout, Text::Caret{8}).x == layout.glyphs[1].penX + 32.0f);
    // Every boundary: caret -> hit test -> the same caret.
    for (const std::uint32_t offset : layout.graphemeBoundaries) {
        const Text::CaretRect rect = Text::CaretAt(layout, Text::Caret{offset});
        CAPTURE(offset);
        CHECK(Text::HitTest(layout, rect.x, rect.top + 1.0f).byteOffset == offset);
    }
    // Inside the ligature: halfway.
    CHECK(Text::CaretAt(layout, Text::Caret{1}).x == layout.glyphs[0].advance / 2.0f);
}

TEST_CASE("text scenario: a description in Japanese and English wraps without breaking the rules") {
    Text::TextContext context;
    const Text::FontId biz = context.AddFont(LoadFont("BIZUDPGothic-Subset.ttf"), 0).value();
    const std::vector<Text::FontId> fonts{biz};
    const std::string text =
        "操作方法：ゲームパッドの「A」ボタンで決定します。Press Start to begin. 残りステージのページで決定します。";
    const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()),
                              Text::TextStyle{fonts, 17.0f, Text::LineHeight::Relative(1.2f)}};
    const Text::ShapedText shaped = context.Shape(text, {&span, 1});
    for (const float width : {300.0f, 200.0f, 120.0f}) {
        CAPTURE(width);
        const Text::TextLayout layout = context.Layout(shaped, Text::LayoutOptions{width, {}});
        CHECK(layout.lines.size() > 1);
        for (std::size_t l = 0; l < layout.lines.size(); ++l) {
            const Text::LayoutLine& line = layout.lines[l];
            const std::string content = text.substr(line.byteBegin, line.byteEnd - line.byteBegin);
            CAPTURE(content);
            CHECK(line.width <= width);
            for (const char* closing : {"。", "、", "」", "：", "ー", "ッ", "ッ"}) {
                CHECK(content.rfind(closing, 0) != 0);
            }
            CHECK((content.size() < 3 || content.compare(content.size() - 3, 3, "「") != 0));
            // English words are never split.
            for (const char* word : {"Press", "Start", "begin."}) {
                const std::size_t at = text.find(word);
                const bool starts = line.byteBegin > at && line.byteBegin < at + std::strlen(word);
                CHECK_FALSE(starts);
            }
        }
    }
}

TEST_CASE("text threads: one context per thread over shared fonts gives the single-thread result") {
    const Text::FontData biz = LoadFont("BIZUDPGothic-Subset.ttf");
    const std::string text = "操作方法：ゲームパッドの「A」ボタンで決定します。";
    const auto layOut = [&](std::vector<float>& out) {
        Text::TextContext context;
        const Text::FontId id = context.AddFont(biz, 0).value();
        const std::vector<Text::FontId> fonts{id};
        const Text::TextSpan span{0, static_cast<std::uint32_t>(text.size()), Text::TextStyle{fonts, 24.0f, {}}};
        Text::GlyphAtlas atlas;
        for (int i = 0; i < 50; ++i) {
            const Text::TextLayout layout = context.Layout(context.Shape(text, {&span, 1}), {160.0f, {}});
            static_cast<void>(context.Rasterize(layout, atlas));
            out.clear();
            for (const auto& g : layout.glyphs) {
                out.push_back(g.x);
                out.push_back(g.y);
                out.push_back(static_cast<float>(g.glyph));
            }
        }
    };
    std::vector<float> single;
    layOut(single);
    std::vector<std::vector<float>> results(8);
    std::vector<std::thread> threads;
    for (auto& result : results) {
        threads.emplace_back([&layOut, &result] { layOut(result); });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (const auto& result : results) {
        CHECK(result == single);
    }
}

TEST_CASE("text threads: fonts load on many threads at once") {
    const std::vector<std::byte> bytes = GYO::Test::Text::ReadFont("GyoTextTestPair.ttc");
    std::vector<int> faces(8, 0);
    std::vector<std::thread> threads;
    for (int& count : faces) {
        threads.emplace_back([&bytes, &count] {
            for (int i = 0; i < 20; ++i) {
                auto font = Text::FontData::FromBytes(std::vector<std::byte>(bytes));
                count = font ? static_cast<int>(font.value().FaceCount()) : -1;
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    for (const int count : faces) {
        CHECK(count == 2);
    }
}
