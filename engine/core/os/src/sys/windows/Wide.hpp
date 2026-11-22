#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OSError.hpp"

#include <string>
#include <string_view>

// Internal to engine/os on Windows: text between UTF-16 and UTF-8, unchanged
// otherwise (no separator rewriting; Files.cpp keeps its own path form).

namespace GYO::Engine::OS::Detail {

// An unpaired surrogate is InvalidEncoding, never a silent U+FFFD.
[[nodiscard]] Base::Result<std::string, OSError> WideToUtf8(std::wstring_view wide);

// Text that is not UTF-8 is InvalidEncoding.
[[nodiscard]] Base::Result<std::wstring, OSError> Utf8ToWide(std::string_view utf8);

} // namespace GYO::Engine::OS::Detail
