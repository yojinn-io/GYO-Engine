#pragma once

#include <atomic>
#include <cstdio>
#include <cstdlib>

// Minimal stand-in for GYO_ASSERT: the spike includes no GYO module. Same
// shape as engine/base/Assert.hpp so the tests use the repository's way of
// checking a failure (a replaceable handler that tests turn into a throw):
// evaluated in every build configuration; on failure the installed handler
// runs, and if it returns the process aborts.
//
// Only the World access checks (KERNEL_SPIKE_ACCESS_CHECKS, World.hpp) depend
// on the build; KERNEL_SPIKE_ASSERT itself is always on.

namespace KernelSpike {

// Not derived from std::exception, like Engine::Base::AssertionFailure.
struct AssertionFailure final {
    const char* message;
    const char* file;
    unsigned line;
};

using AssertionHandler = void (*)(const AssertionFailure&);

namespace Detail {

inline constinit std::atomic<AssertionHandler> assertionHandler{nullptr};

} // namespace Detail

// Installs handler for the whole program and returns the previous one;
// nullptr selects the default (print and abort).
inline AssertionHandler SetAssertionHandler(AssertionHandler handler) noexcept {
    return Detail::assertionHandler.exchange(handler);
}

[[noreturn]] inline void AssertionFailed(const char* message, const char* file, unsigned line) {
    const AssertionFailure failure{message, file, line};
    if (const AssertionHandler handler = Detail::assertionHandler.load()) {
        handler(failure);
    }
    std::fprintf(stderr, "KERNEL_SPIKE_ASSERT failed: %s\n  at %s:%u\n", message, file, line);
    std::fflush(stderr);
    std::abort();
}

} // namespace KernelSpike

// Not variadic, so it needs no standard-conforming preprocessor.
#define KERNEL_SPIKE_ASSERT(condition, message)                                \
    ((condition) ? static_cast<void>(0)                                        \
                 : ::KernelSpike::AssertionFailed((message), __FILE__,         \
                                                  static_cast<unsigned>(__LINE__)))

#define KERNEL_SPIKE_FAIL(message)                                             \
    ::KernelSpike::AssertionFailed((message), __FILE__, static_cast<unsigned>(__LINE__))
