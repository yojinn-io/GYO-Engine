// The calling thread's name and priority hint on Linux. Moved out of the one
// source with per-OS branches (os-threads-backend D69) into sys/<platform>.
#include "engine/os/Thread.hpp"

#include <string>

#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace GYO::Engine::OS::Thread {

bool SetCurrentName(const std::string_view utf8Name) noexcept {
    const std::string name(utf8Name);
    return pthread_setname_np(pthread_self(), name.substr(0, 15).c_str()) == 0;
}

bool SetCurrentPriority(const PriorityHint hint) noexcept {
    // Raising a thread's priority needs privileges; only lowering is attempted.
    if (hint == PriorityHint::Normal) return true;
    if (hint == PriorityHint::Interactive) return false;
    return setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 10) == 0;
}

} // namespace GYO::Engine::OS::Thread
