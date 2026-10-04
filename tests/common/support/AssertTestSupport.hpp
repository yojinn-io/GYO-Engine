#pragma once

#include "doctest/doctest.h"

#include "engine/base/Assert.hpp"

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

// Test helpers for GYO_ASSERT. Outside GYO_CHECK_ASSERTS the default handler
// stays installed, so an unexpected Programmer Error still aborts the test
// executable loudly instead of being reported as an ordinary failure.

namespace Engine::Test {

[[noreturn]] inline void ThrowAssertionFailure(const Base::AssertionFailure& failure) {
    throw failure;
}

// Makes GYO_ASSERT throw AssertionFailure for the lifetime of the scope and
// restores the previous handler afterwards. Use only on the test's own
// thread; a throw out of a noexcept function or another thread terminates.
class ScopedAssertionHandler final {
public:
    ScopedAssertionHandler() noexcept
        : previous_(Base::SetAssertionHandler(&ThrowAssertionFailure)) {}
    ~ScopedAssertionHandler() { Base::SetAssertionHandler(previous_); }
    ScopedAssertionHandler(const ScopedAssertionHandler&) = delete;
    ScopedAssertionHandler& operator=(const ScopedAssertionHandler&) = delete;

private:
    Base::AssertionHandler previous_;
};

#if defined(_MSC_VER)
// abort() would otherwise open a Windows Error Reporting dialog and stall CI.
inline const bool abortReportingDisabled = [] {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    return true;
}();
#endif

} // namespace Engine::Test

// Checks that evaluating the expression fails a GYO_ASSERT. Variadic so the
// expression may contain commas; like GYO_ASSERT it relies on the
// standard-conforming preprocessor (engine/base/Assert.hpp).
#define GYO_CHECK_ASSERTS(...)                                                 \
    do {                                                                       \
        bool gyoAsserted = false;                                              \
        {                                                                      \
            ::Engine::Test::ScopedAssertionHandler gyoAssertScope;             \
            try {                                                              \
                static_cast<void>(__VA_ARGS__);                                \
            } catch (const ::Engine::Base::AssertionFailure&) {                \
                gyoAsserted = true;                                            \
            }                                                                  \
        }                                                                      \
        CHECK_MESSAGE(gyoAsserted, "expected GYO_ASSERT to fail: " #__VA_ARGS__); \
    } while (false)
