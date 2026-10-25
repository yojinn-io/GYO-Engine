#include "Paths.hpp"

#include "engine/os/Files.hpp"

#include <string>

namespace Engine::Os {

namespace Detail {

bool IsValidUtf8(const std::string_view text) noexcept {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 0;
        char32_t code = 0;
        if (lead < 0x80) {
            ++i;
            continue;
        }
        if (lead >= 0xC2 && lead <= 0xDF) {
            length = 2;
            code = lead & 0x1Fu;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            length = 3;
            code = lead & 0x0Fu;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            length = 4;
            code = lead & 0x07u;
        } else {
            return false;
        }
        if (i + length > text.size()) return false;
        for (std::size_t k = 1; k < length; ++k) {
            const auto next = static_cast<unsigned char>(text[i + k]);
            if ((next & 0xC0u) != 0x80u) return false;
            code = (code << 6) | (next & 0x3Fu);
        }
        // Overlong three- and four-byte forms, surrogates, beyond U+10FFFF.
        if ((length == 3 && code < 0x800) || (length == 4 && code < 0x10000) || (code >= 0xD800 && code <= 0xDFFF) ||
            code > 0x10FFFF) {
            return false;
        }
        i += length;
    }
    return true;
}

Base::Result<void, OsError> CheckPath(const std::string_view utf8Path) {
    const std::string detail = "path_bytes=" + std::to_string(utf8Path.size());
    if (utf8Path.empty()) return Base::Err(OsError::Make(OsErrorCode::InvalidPath, "path is empty", detail));
    if (utf8Path.find('\0') != std::string_view::npos) {
        return Base::Err(OsError::Make(OsErrorCode::InvalidPath, "path contains a NUL byte", detail));
    }
    if (!IsValidUtf8(utf8Path)) {
        return Base::Err(OsError::Make(OsErrorCode::InvalidEncoding, "path is not valid UTF-8", detail));
    }
    return {};
}

} // namespace Detail

namespace Files {

std::string PathToUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path PathFromUtf8(const std::string_view utf8) {
    std::u8string text;
    text.reserve(utf8.size());
    for (const char c : utf8) text.push_back(static_cast<char8_t>(c));
    return std::filesystem::path(text);
}

} // namespace Files

} // namespace Engine::Os
