#pragma once

#include "engine/os/Process.hpp"

#include <ctime>

// Internal to engine/os: the part of ToLocalTime that does not depend on the
// OS, shared by the per-OS sources.

namespace GYO::Engine::OS::Detail {

// The fields of a std::tm that localtime_r / localtime_s filled for time, and
// the offset those fields imply (local minus UTC, in whole seconds).
[[nodiscard]] Process::LocalTime LocalTimeFromCalendar(const std::tm& local, std::time_t time) noexcept;

} // namespace GYO::Engine::OS::Detail
