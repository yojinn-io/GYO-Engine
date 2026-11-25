#pragma once

// Hexadecimal text as bytes: how the services hand the admission token over (gateway declaration D5), read by the
// Match (its key on the standard input) and the Client (the token in the room's connect).

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace object_fps_pvp::data {

// Either case; nullopt for an odd length or another character.
[[nodiscard]] inline std::optional<std::vector<std::byte>> DecodeHex(const std::string_view text) {
    if (text.size() % 2 != 0) return std::nullopt;
    const auto nibble = [](const char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<std::byte> bytes(text.size() / 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const int high = nibble(text[2 * i]);
        const int low = nibble(text[2 * i + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        bytes[i] = static_cast<std::byte>((high << 4) | low);
    }
    return bytes;
}

} // namespace object_fps_pvp::data
