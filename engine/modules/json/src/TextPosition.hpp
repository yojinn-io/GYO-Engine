#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace GYO::Engine::Json::Detail {

// "line L, column C" for a byte offset into text: 1-based, columns counted in
// bytes. An offset past the end names the position just after the last byte.
[[nodiscard]] std::string LineAndColumn(std::string_view text, std::size_t offset);

// Appends one reference token of a JSON Pointer (RFC 6901): '/' then the token
// with '~' written as "~0" and '/' as "~1".
void AppendPointerToken(std::string& pointer, std::string_view token);

} // namespace GYO::Engine::Json::Detail
