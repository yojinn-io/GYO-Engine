#pragma once

#include <filesystem>
#include <string>

// Engine takes paths as UTF-8 text (docs/architecture/io.md). path::string()
// is the ANSI code page on Windows, so tests hand paths over through this.

namespace Engine::Test {

// The path in UTF-8, with the separators the path holds.
[[nodiscard]] inline std::string Utf8Path(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

// The path in UTF-8 '/' form.
[[nodiscard]] inline std::string GenericUtf8Path(const std::filesystem::path& path) {
    const std::u8string text = path.generic_u8string();
    return {text.begin(), text.end()};
}

} // namespace Engine::Test
