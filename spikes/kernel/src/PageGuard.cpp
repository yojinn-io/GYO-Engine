#include "kernel_spike/PageGuard.hpp"

#include "kernel_spike/Assert.hpp"
#include "kernel_spike/Descriptor.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

// Platform layer of the page guard (PageGuard.hpp).
//
// The fault handler does only async-signal-safe work: a binary search of a
// fixed array, plain stores into the running System's RunContext, one
// protection change (mprotect is not on POSIX's async-signal-safe list, but
// it is a plain system call on Linux and macOS and is what guard-page
// handlers commonly use there; VirtualProtect in a vectored handler is
// ordinary Windows practice), write(2) and abort.

namespace KernelSpike::PageGuard {

namespace {

struct Range {
    std::uintptr_t begin;
    std::uintptr_t end;
    const void* world;
    std::uint32_t column;
};

// One table for every World in the process, sorted by begin. AddRange and
// RemoveRange update it under rangeMutex; the handler reads it without the
// lock (it cannot take one). Simplification: an update is not atomic with
// respect to a handler running at the same moment on another thread. The
// spike only updates the table from host code (column allocation is a
// structural change, which fails KERNEL_SPIKE_ASSERT during a System run), and
// its tests run one World at a time on one thread, so that race does not
// occur here; several Worlds on several threads would need a lock-free
// publication scheme (for example a new array published by one atomic store).
constexpr std::size_t kMaxRanges = 4096;
Range ranges[kMaxRanges];
std::atomic<std::size_t> rangeCount{0};
std::mutex rangeMutex;

std::atomic<std::uint64_t> handledFaults{0};
std::atomic<std::uint64_t> faultsWithoutSystem{0};

const Range* FindRange(std::uintptr_t address) noexcept {
    const std::size_t count = rangeCount.load(std::memory_order_acquire);
    // The first range that begins after address; the candidate is the one before it.
    std::size_t low = 0;
    std::size_t high = count;
    while (low < high) {
        const std::size_t middle = low + (high - low) / 2;
        if (ranges[middle].begin <= address) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low == 0) {
        return nullptr;
    }
    const Range& candidate = ranges[low - 1];
    return address < candidate.end ? &candidate : nullptr;
}

void WriteBytes(const char* data, std::size_t size) noexcept {
#if defined(_WIN32)
    DWORD written = 0;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), data, static_cast<DWORD>(size), &written, nullptr);
#else
    const ssize_t written = ::write(STDERR_FILENO, data, size);
    static_cast<void>(written);
#endif
}

void WriteText(const char* text) noexcept {
    WriteBytes(text, std::strlen(text));
}

void WriteHex(std::uintptr_t value) noexcept {
    constexpr std::size_t digits = 2 * sizeof(std::uintptr_t);
    char text[2 + digits];
    text[0] = '0';
    text[1] = 'x';
    for (std::size_t index = 0; index < digits; ++index) {
        const auto digit = static_cast<unsigned>((value >> (4 * (digits - 1 - index))) & 0xFu);
        text[2 + index] = "0123456789abcdef"[digit];
    }
    WriteBytes(text, sizeof text);
}

[[noreturn]] void Fatal(const char* what, std::uintptr_t address) noexcept {
    WriteText("kernel spike page guard: ");
    WriteText(what);
    WriteText(" at ");
    WriteHex(address);
    WriteText("\n");
    std::abort();
}

void RecordFault(Detail::RunContext& context, const Range& range, std::uintptr_t address) noexcept {
    const std::size_t slot = context.faultCount.load(std::memory_order_relaxed);
    if (slot >= Detail::RunContext::kFaultSlots) {
        context.faultsDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    context.faults[slot] = Detail::PageFault{range.world, range.column, address};
    context.faultCount.store(slot + 1, std::memory_order_release);
}

// Shared by the POSIX signal handler and the Windows vectored handler. True
// when address is on a guarded column and the fault is resolved: recorded in
// the running System's context and the column made writable, so the faulting
// instruction succeeds when it runs again. False when the address is not on
// any guarded column (not ours to handle).
bool HandleFault(std::uintptr_t address) noexcept {
    const Range* range = FindRange(address);
    if (range == nullptr) {
        return false;
    }
    handledFaults.fetch_add(1, std::memory_order_relaxed);
    Detail::RunContext* context = Detail::runningContext;
    if (context == nullptr) {
        // Host code during a Run, or a thread other than the System's (spike
        // limit L7). Where the spec files this is a question for the report.
        faultsWithoutSystem.fetch_add(1, std::memory_order_relaxed);
        Fatal("write to a guarded column with no System running on this thread", address);
    }
    const std::size_t recorded =
        std::min(context->faultCount.load(std::memory_order_relaxed), Detail::RunContext::kFaultSlots);
    for (std::size_t index = 0; index < recorded; ++index) {
        if (context->faults[index].world == range->world && context->faults[index].column == range->column) {
            // The first fault on this column lifted its protection; faulting
            // again means it did not, and returning would retry forever.
            Fatal("repeated fault on a guarded column whose protection was already lifted", address);
        }
    }
    RecordFault(*context, *range, address);
    if (!SetWritable(reinterpret_cast<std::byte*>(range->begin), range->end - range->begin, true)) {
        Fatal("could not lift the protection of a guarded column", address);
    }
    return true;
}

#if defined(_WIN32)

LONG CALLBACK OnException(EXCEPTION_POINTERS* pointers) {
    const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
    // ExceptionInformation[0] is 1 for a write; [1] is the address.
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2 ||
        record->ExceptionInformation[0] != 1) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    return HandleFault(static_cast<std::uintptr_t>(record->ExceptionInformation[1])) ? EXCEPTION_CONTINUE_EXECUTION
                                                                                      : EXCEPTION_CONTINUE_SEARCH;
}

#else

// The dispositions OnFault chains to: the ones installed before it (or, after
// EnsureFaultHandler reinstalled it, the ones it found instead). Written under
// installMutex while no fault is being handled (simplification: a reinstall
// racing a foreign fault on another thread is not handled).
struct sigaction previousSegv;
struct sigaction previousBus;
std::atomic<std::uint64_t> foreignSignals{0};
std::atomic<std::uint64_t> handlerReinstalls{0};
// Non-zero while this thread runs a previous handler from OnFault. A previous
// handler that calls back into OnFault (it saved OnFault as its own previous
// before a reinstall put OnFault in front of it again) then gets the default
// action instead of an endless loop.
constinit thread_local int chaining = 0;

// A fault sets a small positive SEGV_ or BUS_ code. A signal sent by kill(),
// raise() or sigqueue() has si_code <= 0 on Linux (SI_USER 0, SI_QUEUE and
// SI_TKILL negative); on macOS kill() and raise() give 0 (measured on macOS
// 26, x86_64) and the SI_ constants are 0x10001 and up.
bool SentByProcess(const siginfo_t* info) noexcept {
#if defined(__APPLE__)
    return info->si_code <= 0 || info->si_code >= SI_USER;
#else
    return info->si_code <= 0;
#endif
}

// The default action for a signal that is not ours: reset the disposition to
// SIG_DFL and return. A real fault recurs when the instruction runs again and
// ends the process; a sent signal is raised again (it stays blocked until this
// handler returns). The process is ending, so uninstalling the page guard here
// loses nothing.
void DefaultAction(int signal, const siginfo_t* info) noexcept {
    struct sigaction fallback {};
    fallback.sa_handler = SIG_DFL;
    sigemptyset(&fallback.sa_mask);
    sigaction(signal, &fallback, nullptr);
    if (SentByProcess(info)) {
        raise(signal);
    }
}

// Known limit: siginfo carries no access kind, so on POSIX an instruction
// fetch from a guarded page (never done on purpose: column pages hold data)
// is taken for a write; lifting the protection does not make the page
// executable, the fetch faults again and the handler aborts with "repeated
// fault". The Windows handler checks the access kind (ExceptionInformation[0]).
void OnFault(int signal, siginfo_t* info, void* context) {
    if (!SentByProcess(info) && HandleFault(reinterpret_cast<std::uintptr_t>(info->si_addr))) {
        return;  // the faulting instruction runs again and now succeeds
    }
    // Not a guarded column: hand it to the previous handler and stay installed.
    foreignSignals.fetch_add(1, std::memory_order_relaxed);
    if (chaining != 0) {
        DefaultAction(signal, info);
        return;
    }
    const struct sigaction previous = signal == SIGBUS ? previousBus : previousSegv;
    if ((previous.sa_flags & SA_SIGINFO) != 0) {
        ++chaining;
        previous.sa_sigaction(signal, info, context);
        --chaining;
        return;
    }
    if (previous.sa_handler == SIG_IGN && SentByProcess(info)) {
        return;
    }
    if (previous.sa_handler == SIG_DFL || previous.sa_handler == SIG_IGN) {
        // SIG_IGN for a real fault: returning would retry the instruction
        // forever, and the kernel does not let a fault signal be ignored
        // either, so it gets the default action.
        DefaultAction(signal, info);
        return;
    }
    ++chaining;
    previous.sa_handler(signal);
    --chaining;
}

struct sigaction HandlerAction() noexcept {
    struct sigaction action {};
    action.sa_sigaction = &OnFault;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    return action;
}

// Installs OnFault for signal unless it is already there; what was there
// becomes the disposition OnFault chains to. True when it installed.
bool InstallFor(int signal, struct sigaction& previous) {
    struct sigaction current {};
    KERNEL_SPIKE_ASSERT(sigaction(signal, nullptr, &current) == 0, "page guard: sigaction query failed");
    if ((current.sa_flags & SA_SIGINFO) != 0 && current.sa_sigaction == &OnFault) {
        return false;
    }
    const struct sigaction action = HandlerAction();
    KERNEL_SPIKE_ASSERT(sigaction(signal, &action, &previous) == 0, "page guard: sigaction failed");
    return true;
}

#endif

std::mutex installMutex;
bool installed = false;

} // namespace

std::size_t PageSize() noexcept {
    static const std::size_t size = [] {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return static_cast<std::size_t>(info.dwPageSize);
#else
        return static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
#endif
    }();
    return size;
}

std::size_t RoundUpToPages(std::size_t bytes) noexcept {
    const std::size_t page = PageSize();
    return (bytes + page - 1) / page * page;
}

std::byte* AllocatePages(std::size_t bytes) {
    KERNEL_SPIKE_ASSERT(bytes != 0 && bytes % PageSize() == 0, "page guard: allocation is not whole pages");
#if defined(_WIN32)
    void* pages = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (pages == nullptr) {
        throw std::bad_alloc();
    }
#else
    void* pages = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pages == MAP_FAILED) {
        throw std::bad_alloc();
    }
#endif
    return static_cast<std::byte*>(pages);
}

void FreePages(std::byte* pages, std::size_t bytes) noexcept {
#if defined(_WIN32)
    static_cast<void>(bytes);
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, bytes);
#endif
}

bool SetWritable(std::byte* pages, std::size_t bytes, bool writable) noexcept {
#if defined(_WIN32)
    DWORD previous = 0;
    return VirtualProtect(pages, bytes, writable ? PAGE_READWRITE : PAGE_READONLY, &previous) != 0;
#else
    return mprotect(pages, bytes, writable ? PROT_READ | PROT_WRITE : PROT_READ) == 0;
#endif
}

void InstallFaultHandler() {
    const std::lock_guard lock(installMutex);
    if (installed) {
        return;
    }
#if defined(_WIN32)
    // First in the chain, so it runs before any SEH frame or the
    // unhandled-exception filter (doctest installs one).
    KERNEL_SPIKE_ASSERT(AddVectoredExceptionHandler(1, &OnException) != nullptr,
                        "page guard: AddVectoredExceptionHandler failed");
#else
    // A write to a read-only page is SIGSEGV on Linux and SIGBUS on macOS
    // (measured on x86_64); both go through the same handler.
    static_cast<void>(InstallFor(SIGSEGV, previousSegv));
    static_cast<void>(InstallFor(SIGBUS, previousBus));
#endif
    installed = true;
}

void EnsureFaultHandler() {
    const std::lock_guard lock(installMutex);
    KERNEL_SPIKE_ASSERT(installed, "page guard: EnsureFaultHandler before InstallFaultHandler");
#if !defined(_WIN32)
    const bool segv = InstallFor(SIGSEGV, previousSegv);
    const bool bus = InstallFor(SIGBUS, previousBus);
    if (segv || bus) {
        handlerReinstalls.fetch_add(1, std::memory_order_relaxed);
    }
#endif
}

void AddRange(const void* world, std::uint32_t column, std::byte* pages, std::size_t bytes) {
    const std::lock_guard lock(rangeMutex);
    const std::size_t count = rangeCount.load(std::memory_order_relaxed);
    KERNEL_SPIKE_ASSERT(count < kMaxRanges, "page guard: more guarded columns than the spike's range table holds");
    const auto begin = reinterpret_cast<std::uintptr_t>(pages);
    std::size_t at = 0;
    while (at < count && ranges[at].begin < begin) {
        ++at;
    }
    for (std::size_t index = count; index > at; --index) {
        ranges[index] = ranges[index - 1];
    }
    ranges[at] = Range{begin, begin + bytes, world, column};
    rangeCount.store(count + 1, std::memory_order_release);
}

void RemoveRange(const std::byte* pages) noexcept {
    const std::lock_guard lock(rangeMutex);
    const std::size_t count = rangeCount.load(std::memory_order_relaxed);
    const auto begin = reinterpret_cast<std::uintptr_t>(pages);
    std::size_t at = 0;
    while (at < count && ranges[at].begin != begin) {
        ++at;
    }
    KERNEL_SPIKE_ASSERT(at < count, "page guard: removing a range that was never added");
    for (std::size_t index = at; index + 1 < count; ++index) {
        ranges[index] = ranges[index + 1];
    }
    rangeCount.store(count - 1, std::memory_order_release);
}

std::uint64_t HandledFaults() noexcept {
    return handledFaults.load(std::memory_order_relaxed);
}

std::uint64_t FaultsWithoutSystem() noexcept {
    return faultsWithoutSystem.load(std::memory_order_relaxed);
}

HandlerStats GetHandlerStats() noexcept {
    HandlerStats stats;
    stats.handledFaults = HandledFaults();
    stats.faultsWithoutSystem = FaultsWithoutSystem();
#if !defined(_WIN32)
    stats.foreignSignals = foreignSignals.load(std::memory_order_relaxed);
    stats.reinstalls = handlerReinstalls.load(std::memory_order_relaxed);
#endif
    return stats;
}

} // namespace KernelSpike::PageGuard
