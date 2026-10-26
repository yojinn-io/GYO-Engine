#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OsError.hpp"

#include <string>
#include <string_view>

// Internal to engine/os on Windows: UTF-16 text to UTF-8, unchanged otherwise
// (no separator rewriting; Files.cpp keeps its own path form).

namespace Engine::Os::Detail {

// An unpaired surrogate is InvalidEncoding, never a silent U+FFFD.
[[nodiscard]] Base::Result<std::string, OsError> WideToUtf8(std::wstring_view wide);

} // namespace Engine::Os::Detail
