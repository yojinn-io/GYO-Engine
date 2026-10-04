#pragma once

// GYO's macros assume a standard-conforming preprocessor on every compiler.
// build/cmake/GyoBuild.cmake passes /Zc:preprocessor to MSVC; a target that
// misses it fails here instead of expanding macros differently. clang-cl
// defines _MSC_VER but always conforms.
#if defined(_MSC_VER) && !defined(__clang__) && \
    (!defined(_MSVC_TRADITIONAL) || _MSVC_TRADITIONAL)
#error "GYO requires MSVC's standard-conforming preprocessor (/Zc:preprocessor, set in build/cmake/GyoBuild.cmake)"
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <source_location>

// GYO_ASSERT reports a Programmer Error: a broken precondition, a broken
// internal invariant or API misuse. A Runtime Error (bad external data, a
// failing environment) is a Result instead; see docs/architecture/error-handling.md.
//
// Unlike <cassert>, GYO_ASSERT is evaluated in every build configuration. The
// condition must have no side effects and is evaluated exactly once. On
// failure the installed AssertionHandler runs; if it returns, the process
// aborts. The default handler prints the condition and its location.
//
// A function that contains GYO_ASSERT is not noexcept: tests install a handler
// that throws AssertionFailure, and a throw out of a noexcept function
// terminates instead of reaching the test.
//
// In a constexpr function a failing GYO_ASSERT during constant evaluation is a
// compile error, because the failure path is not a constant expression.
//
// C++26 mapping: GYO_ASSERT corresponds to contract_assert, and
// SetAssertionHandler to the replaceable contract-violation handler.

namespace Engine::Base {

// Deliberately not derived from std::exception, so a catch (const
// std::exception&) boundary never swallows a failure thrown by a test handler.
struct AssertionFailure final {
    const char* expression;
    std::source_location location;
};

using AssertionHandler = void (*)(const AssertionFailure&);

namespace Detail {

inline constinit std::atomic<AssertionHandler> assertionHandler{nullptr};
inline constinit thread_local bool reportingAssertion = false;

// Marks this thread as running a handler; the destructor also runs when a
// test handler throws, so the next failure on this thread is not mistaken
// for a nested one.
class ReportingScope final {
public:
    ReportingScope() noexcept { reportingAssertion = true; }
    ~ReportingScope() { reportingAssertion = false; }
    ReportingScope(const ReportingScope&) = delete;
    ReportingScope& operator=(const ReportingScope&) = delete;
};

inline void PrintAssertionFailure(const AssertionFailure& failure) noexcept {
    std::fprintf(stderr, "GYO_ASSERT failed: %s\n  at %s:%u in %s\n",
                 failure.expression,
                 failure.location.file_name(),
                 static_cast<unsigned>(failure.location.line()),
                 failure.location.function_name());
    std::fflush(stderr);
}

} // namespace Detail

inline void DefaultAssertionHandler(const AssertionFailure& failure) noexcept {
    Detail::PrintAssertionFailure(failure);
}

// Installs handler for the whole program and returns the previous one;
// nullptr selects DefaultAssertionHandler. Install before starting threads.
// A product handler may log and must not throw; only tests throw.
inline AssertionHandler SetAssertionHandler(AssertionHandler handler) noexcept {
    return Detail::assertionHandler.exchange(handler);
}

[[noreturn]] inline void ReportAssertionFailure(const AssertionFailure& failure) {
    if (Detail::reportingAssertion) {
        // A handler failed an assertion of its own: report it and stop.
        Detail::PrintAssertionFailure(failure);
        std::abort();
    }
    {
        Detail::ReportingScope scope;
        const AssertionHandler handler = Detail::assertionHandler.load();
        (handler ? handler : &DefaultAssertionHandler)(failure);
    }
    std::abort();
}

namespace Detail {

[[noreturn]] inline void AssertionFailed(
    const char* expression,
    const std::source_location location = std::source_location::current()) {
    ReportAssertionFailure(AssertionFailure{expression, location});
}

} // namespace Detail

} // namespace Engine::Base

// Variadic so a condition containing commas (template arguments) needs no
// extra parentheses. Relies on the standard-conforming preprocessor checked
// at the top of this header.
#define GYO_ASSERT(...)                                                        \
    ((__VA_ARGS__) ? static_cast<void>(0)                                      \
                   : ::Engine::Base::Detail::AssertionFailed(#__VA_ARGS__))

// Marks a branch that a correct program never reaches (for example after a
// switch that handles every enumerator). Reaching it is a Programmer Error
// reported like a failed GYO_ASSERT; the call does not return.
#define GYO_UNREACHABLE() ::Engine::Base::Detail::AssertionFailed("unreachable")
