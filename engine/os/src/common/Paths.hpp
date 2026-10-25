#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <cstddef>
#include <string>
#include <string_view>

// Internal to engine/os: the checks every path argument passes before it
// reaches the OS, shared by the per-OS sources.

namespace Engine::Os::Detail {

// Well-formed UTF-8 (RFC 3629): no overlong forms, no surrogates, at most U+10FFFF.
[[nodiscard]] bool IsValidUtf8(std::string_view text) noexcept;

// An argument the layer refuses before any OS call: there is no native error,
// so the detail is "path_bytes=<n>" alone (docs/architecture/os.md).
[[nodiscard]] OsError RefusedArgument(OsErrorCode code, std::string message, std::size_t pathBytes);

// Not empty, no NUL, valid UTF-8.
[[nodiscard]] Base::Result<void, OsError> CheckPath(std::string_view utf8Path);

} // namespace Engine::Os::Detail
