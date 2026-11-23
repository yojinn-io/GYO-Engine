#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

// What a TextContext shapes and lays out: the fonts, the size and the line
// height of each span of a text, and the options of a layout.
namespace GYO::Engine::Text {

// A font added to one TextContext (TextContext::AddFont). It means nothing to
// another context.
struct FontId {
    std::uint32_t value = 0;

    [[nodiscard]] friend bool operator==(FontId, FontId) = default;
};

enum class LineHeightKind : std::uint8_t {
    // A multiple of the font size (Bevy's LineHeight::RelativeToFont).
    RelativeToFont = 1,
    // Pixels.
    Px = 2,
};

struct LineHeight {
    LineHeightKind kind = LineHeightKind::RelativeToFont;
    float value = 1.2f;

    [[nodiscard]] static constexpr LineHeight Relative(const float factor) noexcept {
        return {LineHeightKind::RelativeToFont, factor};
    }
    [[nodiscard]] static constexpr LineHeight Pixels(const float px) noexcept { return {LineHeightKind::Px, px}; }
};

struct TextStyle {
    // The fallback order; fonts[0] is the primary font. Not owned: Shape copies it.
    std::span<const FontId> fonts;
    // Pixels per em (the caller applies any display scale). Positive, at most 16384.
    float sizePx = 16.0f;
    LineHeight lineHeight{};
};

// One span of a text: the bytes [byteBegin, byteEnd) of the UTF-8 text. The
// spans of one Shape call cover the text in order, without gaps.
struct TextSpan {
    std::uint32_t byteBegin = 0;
    std::uint32_t byteEnd = 0;
    TextStyle style{};
};

// How strictly Japanese and Chinese line-start rules apply (UAX #14's CJ class:
// small kana and the prolonged sound mark).
enum class LineBreakStrictness : std::uint8_t {
    // CJ as NS: no small kana or prolonged sound mark at the start of a line
    // (UAX #14's default resolution).
    Strict = 1,
    // CJ as ID: they may start a line (the browsers' default for Japanese).
    Normal = 2,
};

// What applies to a whole Shape call.
struct ShapeOptions {
    // A BCP 47 language tag ("ja", "zh-Hant", "en"); empty: none. It selects
    // HarfBuzz's language-specific forms and resolves UAX #14's ambiguous
    // characters (AI): as ideographs for Chinese, Japanese and Korean (※, ①
    // break like kanji), as letters otherwise.
    std::string_view language;
    LineBreakStrictness strictness = LineBreakStrictness::Strict;
};

enum class HorizontalAlign : std::uint8_t { Left = 1, Center = 2, Right = 3 };

struct LayoutOptions {
    // Lines are broken to fit this width; none: only at mandatory breaks.
    std::optional<float> maxWidth;
    // Each line is aligned within maxWidth, or within the widest line when
    // there is no maxWidth.
    HorizontalAlign align = HorizontalAlign::Left;
};

} // namespace GYO::Engine::Text
