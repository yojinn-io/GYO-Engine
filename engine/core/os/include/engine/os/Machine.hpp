#pragma once

#include "engine/base/Result.hpp"
#include "engine/os/OSError.hpp"

#include <string>

// Facts of the machine the process runs on (docs/architecture/os.md). Every
// query asks the OS again; nothing is cached. What a caller makes of the
// answer (a player's name, a log line) is the caller's.

namespace GYO::Engine::OS::Machine {

// The name this machine has on the network, as the OS reports it: gethostname
// on macOS and Linux (macOS may end it with ".local", or give the name the
// network handed it), GetComputerNameExW(ComputerNamePhysicalDnsHostname) on
// Windows. Not trimmed, not case-folded, not shortened. InvalidEncoding when
// it has no UTF-8 form; IOFailed with the native error when the OS refuses.
[[nodiscard]] Base::Result<std::string, OSError> HostName();

} // namespace GYO::Engine::OS::Machine
