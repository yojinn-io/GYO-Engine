#pragma once

#include "engine/base/Log.hpp"

namespace fps::pvp {

#if defined(__GNUC__) || defined(__clang__)
#define FPS_PVP_PRINTF_FORMAT(formatIndex, firstArgument) __attribute__((format(printf, formatIndex, firstArgument)))
#else
#define FPS_PVP_PRINTF_FORMAT(formatIndex, firstArgument)
#endif

// One Client diagnostics line, printf-formatted as the SDL log calls it replaces
// formatted it, sent to the Engine log facade with the category "app". The
// Client's log handler writes "[level] text" for this category, the bytes the
// SDL log path wrote before (log-contract: the A-class hit feedback line).
FPS_PVP_PRINTF_FORMAT(2, 3) void ClientLog(Engine::Base::LogLevel level, const char* format, ...) noexcept;

// Waits about the given number of milliseconds on the Engine time base (a
// pause in loops that have nothing to wait for, such as a minimized window).
void PauseMilliseconds(int milliseconds) noexcept;

} // namespace fps::pvp
