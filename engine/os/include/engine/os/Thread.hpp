#pragma once

#include <string_view>

// The calling thread's name and scheduling hint (docs/architecture/os.md,
// Thread). engine/threads' RoleThread applies both on a new role's thread
// before its body runs; what a role is, and its lifetime, stay with
// engine/threads. Neither call is an error when the platform does not take it:
// each answers whether it was applied (Design Invariant 10).

namespace Engine::Os::Thread {

// A scheduling hint, not a timing guarantee.
enum class PriorityHint : unsigned char { Background, Normal, Interactive };

// Names the calling thread for debuggers, profilers and trace. The name is
// UTF-8. macOS: pthread_setname_np. Windows: SetThreadDescription (Windows 10
// 1607 and later, looked up at run time), from the UTF-8 name; a name that is
// not UTF-8 is not applied. Linux: pthread_setname_np with the first 15 bytes.
// Other platforms have no call, so false.
[[nodiscard]] bool SetCurrentName(std::string_view utf8Name) noexcept;

// Applies the hint to the calling thread. macOS: QoS classes (Background as
// utility, Normal as default, Interactive as user-interactive). Windows:
// below normal, normal and above normal thread priorities. Linux: only
// Background is applied (nice 10, which fails if the process already runs
// nicer); Interactive needs privileges and is not; Normal makes no call and
// counts as applied, as on other platforms.
[[nodiscard]] bool SetCurrentPriority(PriorityHint hint) noexcept;

} // namespace Engine::Os::Thread
