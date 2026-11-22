// The calling thread's name and priority hint on Windows. Moved out of the one
// source with per-OS branches (os-threads-backend D69) into sys/<platform>; the
// name goes through UTF-8 to UTF-16 instead of widening each byte (★ D69).
#include "engine/os/Thread.hpp"

#include "Wide.hpp"

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace GYO::Engine::OS::Thread {

bool SetCurrentName(const std::string_view utf8Name) noexcept {
    const std::string name(utf8Name);
    // SetThreadDescription exists from Windows 10 1607; look it up instead of linking it.
    using Describe = HRESULT(WINAPI*)(HANDLE, PCWSTR);
    const auto module = ::GetModuleHandleW(L"kernel32.dll");
    const auto describe = module ? reinterpret_cast<Describe>(::GetProcAddress(module, "SetThreadDescription")) : nullptr;
    if (!describe) return false;
    const auto wide = Detail::Utf8ToWide(name);
    if (!wide) return false;
    return SUCCEEDED(describe(::GetCurrentThread(), wide.value().c_str()));
}

bool SetCurrentPriority(const PriorityHint hint) noexcept {
    const int value = hint == PriorityHint::Interactive ? THREAD_PRIORITY_ABOVE_NORMAL :
        hint == PriorityHint::Background ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL;
    return ::SetThreadPriority(::GetCurrentThread(), value) != 0;
}

} // namespace GYO::Engine::OS::Thread
