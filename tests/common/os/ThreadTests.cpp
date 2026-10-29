#include <doctest/doctest.h>

#include "engine/os/Thread.hpp"
#include "engine/threads/RoleThread.hpp"

#include <string>
#include <thread>
#include <utility>

// The thread name and priority calls (docs/architecture/os.md, Thread), read
// back through the platform's own queries: the name the OS keeps and the class,
// priority or niceness it reports. Each case runs on a thread of its own, so
// the test runner's thread keeps its name and priority; what the thread reads
// is asserted back on the test's thread (a failed REQUIRE on another thread
// would end the process).
#if defined(__APPLE__)
#include <pthread.h>
#include <sys/qos.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cerrno>
#endif

using namespace Engine::Os;

namespace {

template <class Body>
void OnOwnThread(Body&& body) {
    std::thread thread(std::forward<Body>(body));
    thread.join();
}

#if defined(_WIN32)
// The description Windows keeps for the calling thread; empty when the query
// does not exist (before Windows 10 1607).
std::wstring CurrentDescription() {
    using Query = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    const auto module = ::GetModuleHandleW(L"kernel32.dll");
    const auto query = module ? reinterpret_cast<Query>(::GetProcAddress(module, "GetThreadDescription")) : nullptr;
    if (!query) return {};
    PWSTR text = nullptr;
    if (FAILED(query(::GetCurrentThread(), &text)) || text == nullptr) return {};
    std::wstring description(text);
    ::LocalFree(text);
    return description;
}
#elif defined(__APPLE__) || defined(__linux__)
std::string CurrentName() {
    char name[64] = {};
    if (pthread_getname_np(pthread_self(), name, sizeof name) != 0) return {};
    return name;
}
#endif

// What the calling thread's priority reads back as, and what the hint should
// read back as: a QoS class on macOS, a thread priority on Windows, the nice
// value on Linux (where only Background changes it). -1 where not read.
struct PriorityRead final {
    long actual = -1;
    long expected = -1;
};

PriorityRead ReadPriority([[maybe_unused]] const Thread::PriorityHint hint, [[maybe_unused]] const long niceBefore) {
#if defined(__APPLE__)
    const qos_class_t expected = hint == Thread::PriorityHint::Interactive ? QOS_CLASS_USER_INTERACTIVE :
        hint == Thread::PriorityHint::Background ? QOS_CLASS_UTILITY : QOS_CLASS_DEFAULT;
    return {static_cast<long>(qos_class_self()), static_cast<long>(expected)};
#elif defined(_WIN32)
    const int expected = hint == Thread::PriorityHint::Interactive ? THREAD_PRIORITY_ABOVE_NORMAL :
        hint == Thread::PriorityHint::Background ? THREAD_PRIORITY_BELOW_NORMAL : THREAD_PRIORITY_NORMAL;
    return {::GetThreadPriority(::GetCurrentThread()), expected};
#elif defined(__linux__)
    errno = 0;
    const int nice = getpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)));
    if (errno != 0) return {};
    // nice 10, which an unprivileged process can take only from a nice value up to 10.
    const bool lowered = hint == Thread::PriorityHint::Background && niceBefore <= 10;
    return {nice, lowered ? 10 : niceBefore};
#else
    return {};
#endif
}

long NiceNow() {
#if defined(__linux__)
    errno = 0;
    const int nice = getpriority(PRIO_PROCESS, static_cast<id_t>(syscall(SYS_gettid)));
    return errno == 0 ? nice : -100;
#else
    return 0;
#endif
}

// Whether the platform takes the hint (Design Invariant 10): everywhere on macOS
// and Windows; on Linux Normal (no call) and Background from a nice value up to
// 10; Interactive needs privileges there. Elsewhere only Normal.
bool ExpectApplied([[maybe_unused]] const Thread::PriorityHint hint, [[maybe_unused]] const long niceBefore) {
#if defined(__APPLE__) || defined(_WIN32)
    return true;
#elif defined(__linux__)
    return hint == Thread::PriorityHint::Normal || (hint == Thread::PriorityHint::Background && niceBefore <= 10);
#else
    return hint == Thread::PriorityHint::Normal;
#endif
}

constexpr Thread::PriorityHint kHints[] = {Thread::PriorityHint::Background, Thread::PriorityHint::Normal,
                                           Thread::PriorityHint::Interactive};

} // namespace

TEST_CASE("Os thread names the calling thread as the platform reads it back") {
#if defined(_WIN32)
    bool ascii = false, beyondAscii = false, invalid = true;
    std::wstring afterAscii, afterBeyond, afterInvalid;
    OnOwnThread([&] {
        ascii = Thread::SetCurrentName("gyo-os-thread");
        afterAscii = CurrentDescription();
        // UTF-8 becomes UTF-16, not one wide unit per byte (D69 ★).
        beyondAscii = Thread::SetCurrentName("gyo-\xE5\x9F\xB7\xE8\xA1\x8C\xE7\xB7\x92-\xED\x95\x9C");
        afterBeyond = CurrentDescription();
        // A name that is not UTF-8 is not applied, and the last one stays.
        invalid = Thread::SetCurrentName("gyo-\xFF");
        afterInvalid = CurrentDescription();
    });
    CHECK(ascii);
    CHECK(afterAscii == L"gyo-os-thread");
    CHECK(beyondAscii);
    CHECK(afterBeyond == L"gyo-\x57F7\x884C\x7DD2-\xD55C");
    CHECK_FALSE(invalid);
    CHECK(afterInvalid == L"gyo-\x57F7\x884C\x7DD2-\xD55C");
#elif defined(__APPLE__) || defined(__linux__)
    bool applied = false, longApplied = false;
    std::string after, afterLong;
    OnOwnThread([&] {
        applied = Thread::SetCurrentName("gyo-os-thread");
        after = CurrentName();
        longApplied = Thread::SetCurrentName("gyo-os-thread-with-a-long-name");
        afterLong = CurrentName();
    });
    CHECK(applied);
    CHECK(after == "gyo-os-thread");
    CHECK(longApplied);
#if defined(__linux__)
    // Linux keeps 15 bytes; the call cuts the name instead of failing.
    CHECK(afterLong == "gyo-os-thread-w");
#else
    CHECK(afterLong == "gyo-os-thread-with-a-long-name");
#endif
#else
    bool applied = true;
    OnOwnThread([&] { applied = Thread::SetCurrentName("gyo-os-thread"); });
    CHECK_FALSE(applied);
#endif
}

TEST_CASE("Os thread applies the priority hints as the platform reads them back") {
    for (const auto hint : kHints) {
        CAPTURE(static_cast<int>(hint));
        long before = 0;
        bool applied = false;
        PriorityRead read;
        OnOwnThread([&] {
            before = NiceNow();
            applied = Thread::SetCurrentPriority(hint);
            read = ReadPriority(hint, before);
        });
        CHECK(applied == ExpectApplied(hint, before));
        CHECK(read.actual == read.expected);
    }
}

// The role threads hand their priority to engine/os as the same level
// (engine/threads' RoleThread, threads.md Invariant 5): read back on the role's
// own thread.
TEST_CASE("Os thread receives a role's priority as the same hint") {
    using Engine::Threads::ThreadPriority;
    const std::pair<ThreadPriority, Thread::PriorityHint> levels[] = {
        {ThreadPriority::Background, Thread::PriorityHint::Background},
        {ThreadPriority::Normal, Thread::PriorityHint::Normal},
        {ThreadPriority::Interactive, Thread::PriorityHint::Interactive},
    };
    for (const auto& [priority, hint] : levels) {
        CAPTURE(static_cast<int>(priority));
        long before = 0;
        OnOwnThread([&] { before = NiceNow(); });  // a new thread starts with the process's niceness
        PriorityRead read;
        auto role = Engine::Threads::RoleThread::Start({"gyo-os-role", priority},
            [&](Engine::Threads::RoleContext&) { read = ReadPriority(hint, before); });
        REQUIRE(role);
        role->Join();
        CHECK(role->PriorityApplied() == ExpectApplied(hint, before));
        CHECK(read.actual == read.expected);
    }
}
