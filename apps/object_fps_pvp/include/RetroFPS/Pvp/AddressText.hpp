#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace fps::pvp {

// The characters of typed or pasted UTF-8 text that the lobby's Gateway
// address accepts: a-z A-Z 0-9 . : - [ ]. An input method in a full-width
// mode commits the full-width forms (U+FF01-U+FF5E) and the ideographic full
// stop (U+3002, as '.'); they become their ASCII characters first
// (v8-product-lobby-fullwidth). Everything else, and any byte that is not
// part of a complete UTF-8 sequence, is dropped.
[[nodiscard]] inline std::string FilterAddressText(const std::string_view utf8) {
    std::string accepted;
    for (std::size_t index = 0; index < utf8.size();) {
        const auto lead = static_cast<unsigned char>(utf8[index]);
        const std::size_t length = lead < 0x80 ? 1 : (lead >> 5) == 0x6 ? 2 : (lead >> 4) == 0xE ? 3 : (lead >> 3) == 0x1E ? 4 : 0;
        if (length == 0 || index + length > utf8.size()) {
            ++index;
            continue;
        }
        std::uint32_t codePoint = length == 1 ? lead : length == 2 ? lead & 0x1Fu : length == 3 ? lead & 0x0Fu : lead & 0x07u;
        bool complete = true;
        for (std::size_t next = 1; next < length; ++next) {
            const auto byte = static_cast<unsigned char>(utf8[index + next]);
            if ((byte & 0xC0) != 0x80) {
                complete = false;
                break;
            }
            codePoint = (codePoint << 6) | (byte & 0x3Fu);
        }
        if (!complete) {
            ++index;
            continue;
        }
        index += length;
        if (codePoint >= 0xFF01 && codePoint <= 0xFF5E) codePoint -= 0xFF01 - 0x21;
        else if (codePoint == 0x3002) codePoint = '.';
        if (codePoint >= 0x80) continue;
        const auto c = static_cast<char>(codePoint);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == ':' ||
            c == '-' || c == '[' || c == ']') {
            accepted += c;
        }
    }
    return accepted;
}

} // namespace fps::pvp
