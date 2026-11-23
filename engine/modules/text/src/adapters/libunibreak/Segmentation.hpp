#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

// The libunibreak adapter's interface, in the text Module's words (Design
// Invariant TX-1: libunibreak's headers, types and calls stay in
// src/adapters/libunibreak/; decision v8-arch-gyo2-sdl-t, the user's choice (d)).
namespace GYO::Engine::Text::Detail {

enum class LineBreakAfter : std::uint8_t {
    // A line must end after this character (a line feed and the like, and the
    // end of the text).
    Mandatory = 0,
    // A line may end after it.
    Allowed = 1,
    None = 2,
};

// UAX #14, one entry per code point: out[i] is the break after text[i]. The
// language (a BCP 47 tag, empty for none) resolves the ambiguous class AI (as
// ID for zh, ja and ko, as AL otherwise); strict resolves CJ as NS (no small
// kana at the start of a line), otherwise as ID. No language and strict is
// UAX #14's default resolution.
void FindLineBreaks(std::span<const char32_t> text, std::string_view language, bool strict,
                    std::vector<LineBreakAfter>& out);

// UAX #29 extended grapheme clusters: out[i] is 1 when a grapheme ends after
// text[i] (always at the end of the text).
void FindGraphemeBoundaries(std::span<const char32_t> text, std::vector<std::uint8_t>& out);

} // namespace GYO::Engine::Text::Detail
