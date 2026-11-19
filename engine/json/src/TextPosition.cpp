#include "TextPosition.hpp"

#include <algorithm>

namespace GYO::Engine::Json::Detail {

std::string LineAndColumn(const std::string_view text, const std::size_t offset) {
    const std::size_t end = std::min(offset, text.size());
    std::size_t line = 1;
    std::size_t lineStart = 0;
    for (std::size_t i = 0; i < end; ++i) {
        if (text[i] == '\n') {
            ++line;
            lineStart = i + 1;
        }
    }
    return "line " + std::to_string(line) + ", column " + std::to_string(end - lineStart + 1);
}

void AppendPointerToken(std::string& pointer, const std::string_view token) {
    pointer += '/';
    for (const char c : token) {
        if (c == '~') {
            pointer += "~0";
        } else if (c == '/') {
            pointer += "~1";
        } else {
            pointer += c;
        }
    }
}

} // namespace GYO::Engine::Json::Detail
