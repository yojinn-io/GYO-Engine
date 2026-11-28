#pragma once

// Text cut to a byte budget without splitting a UTF-8 sequence.

#include <cstddef>
#include <string_view>

namespace object_fps_pvp::data {

// The longest start of text that is at most maxBytes long and ends between two code points (a continuation byte,
// 10xxxxxx, is never the first byte left out).
[[nodiscard]] inline std::string_view Utf8Prefix(const std::string_view text, const std::size_t maxBytes) noexcept {
    if (text.size() <= maxBytes) return text;
    std::size_t end = maxBytes;
    while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) --end;
    return text.substr(0, end);
}

} // namespace object_fps_pvp::data
