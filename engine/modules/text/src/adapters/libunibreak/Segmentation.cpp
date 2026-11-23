#include "adapters/libunibreak/Segmentation.hpp"

#include <graphemebreak.h>
#include <linebreak.h>

#include <string>
#include <vector>

#include "engine/base/Assert.hpp"

namespace GYO::Engine::Text::Detail {

namespace {

// libunibreak's language string: the tag, and "-strict" for CJ as NS. With no
// tag and "-strict" it is UAX #14's default resolution, the one Unicode's
// LineBreakTest.txt is checked with (libunibreak's tests.c). libunibreak has no
// global state: both calls are safe on any thread.
std::string LibunibreakLanguage(const std::string_view language, const bool strict) {
    std::string lang(language);
    if (strict) {
        lang += "-strict";
    }
    return lang;
}

std::vector<utf32_t> ToUtf32(const std::span<const char32_t> text) {
    std::vector<utf32_t> out(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        out[i] = static_cast<utf32_t>(text[i]);
    }
    return out;
}

} // namespace

void FindLineBreaks(const std::span<const char32_t> text, const std::string_view language, const bool strict,
                    std::vector<LineBreakAfter>& out) {
    out.assign(text.size(), LineBreakAfter::None);
    if (text.empty()) {
        return;
    }
    const std::vector<utf32_t> utf32 = ToUtf32(text);
    std::vector<char> breaks(text.size());
    const std::string lang = LibunibreakLanguage(language, strict);
    set_linebreaks_utf32(utf32.data(), utf32.size(), lang.empty() ? nullptr : lang.c_str(), breaks.data());
    for (std::size_t i = 0; i < text.size(); ++i) {
        switch (breaks[i]) {
        case LINEBREAK_MUSTBREAK:
        case LINEBREAK_INDETERMINATE: out[i] = LineBreakAfter::Mandatory; break;
        case LINEBREAK_ALLOWBREAK: out[i] = LineBreakAfter::Allowed; break;
        case LINEBREAK_NOBREAK: out[i] = LineBreakAfter::None; break;
        default: GYO_UNREACHABLE();
        }
    }
}

void FindGraphemeBoundaries(const std::span<const char32_t> text, std::vector<std::uint8_t>& out) {
    out.assign(text.size(), 0);
    if (text.empty()) {
        return;
    }
    const std::vector<utf32_t> utf32 = ToUtf32(text);
    std::vector<char> breaks(text.size());
    set_graphemebreaks_utf32(utf32.data(), utf32.size(), nullptr, breaks.data());
    for (std::size_t i = 0; i < text.size(); ++i) {
        GYO_ASSERT(breaks[i] == GRAPHEMEBREAK_BREAK || breaks[i] == GRAPHEMEBREAK_NOBREAK);
        out[i] = breaks[i] == GRAPHEMEBREAK_BREAK ? 1 : 0;
    }
    out.back() = 1;
}

} // namespace GYO::Engine::Text::Detail
