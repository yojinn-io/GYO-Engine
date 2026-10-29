// The calling thread's name and priority hint. Moved from engine/threads'
// RoleThread.cpp (os-threads-backend, D69) as one source with per-OS branches,
// so the move reads line by line against the old code (os-subsystem open
// decision 11 (c)); the Windows name now goes through UTF-8 to UTF-16 instead
// of widening each byte (★ declared in D69).
#include "engine/os/Thread.hpp"

#include <string>

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/qos.h>
#elif defined(_WIN32)
#include "../windows/Wide.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace Engine::Os::Thread {

bool SetCurrentName(const std::string_view utf8Name) noexcept {
    const std::string name(utf8Name);
#if defined(__APPLE__)
    return pthread_setname_np(name.c_str()) == 0;
#elif defined(_WIN32)
    // SetThreadDescription exists from Windows 10 1607; look it up instead of linking it.
    using Describe = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const auto module = ::GetModuleHandleW(L"kernel32.dll");
    const auto describe = module ? reinterpret_cast<Describe>(::GetProcAddress(module, "SetThreadDescription")) : nullptr;
    if (!describe) return false;
    const auto wide = Detail::Utf8ToWide(name);
    if (!wide) return false;
    return SUCCEEDED(describe(::GetCurrentThread(), wide.value().c_str()));
#elif defined(__linux__)
    return pthread_setname_np(pthread_self(), name.substr(0, 15).c_str()) == 0;
#else
    static_cast<void>(name);
    return false;
#endif
}

bool SetCurrentPriority(const PriorityHint hint) noexcept {
#if defined(__APPLE__)
    const qos_class_t qos = hint == PriorityHint::Interactive ? QOS_CLASS_USER_INTERACTIVE :
        hint == PriorityHint::Background ? QOS_CLASS_UTILITY : QOS_CLASS_DEFAULT;
    return pthread_set_qos_class_self_np(qos, 0) == 0;
#elif defined(_WIN32)
    const int value = hint == PriorityHint::Interactive ? THREAD_PRIORITY_ABOVE_NORMAL :
        hint == PriorityHint::Background ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL;
    return ::SetThreadPriority(::GetCurrentThread(), value) != 0;
#elif defined(__linux__)
    // Raising a thread's priority needs privileges; only lowering is attempted.
    if (hint == PriorityHint::Normal) return true;
    if (hint == PriorityHint::Interactive) return false;
    return setpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)), 10) == 0;
#else
    return hint == PriorityHint::Normal;
#endif
}

} // namespace Engine::Os::Thread
