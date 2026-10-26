#pragma once

#include "engine/os/Console.hpp"

// Internal to engine/os: the process's shutdown-request flag, the one process
// state engine/os owns (Design Invariant 8). Only the OS handlers write it.

namespace Engine::Os::Detail {

// Keeps the first reason; later requests change nothing. Lock-free, so a
// signal handler may call it.
void RecordShutdown(Console::ShutdownReason reason) noexcept;

} // namespace Engine::Os::Detail
