// The calling thread's name and priority hint on macOS. Moved out of the one
// source with per-OS branches (os-threads-backend D69) into sys/<platform>.
#include "engine/os/Thread.hpp"

#include <string>

#include <pthread.h>
#include <sys/qos.h>

namespace Engine::Os::Thread {

bool SetCurrentName(const std::string_view utf8Name) noexcept {
    const std::string name(utf8Name);
    return pthread_setname_np(name.c_str()) == 0;
}

bool SetCurrentPriority(const PriorityHint hint) noexcept {
    const qos_class_t qos = hint == PriorityHint::Interactive ? QOS_CLASS_USER_INTERACTIVE :
        hint == PriorityHint::Background ? QOS_CLASS_UTILITY : QOS_CLASS_DEFAULT;
    return pthread_set_qos_class_self_np(qos, 0) == 0;
}

} // namespace Engine::Os::Thread
