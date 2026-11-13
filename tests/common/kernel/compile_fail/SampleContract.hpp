#pragma once

// Test-only rejection condition for the two compile-failure test shapes of
// spec J-10 (migration plan U5: (a) try_compile expected to fail, (b) a trait
// checked by doctest). It is the smallest stand-in for target/kernel DI-KS 1
// ("a System is a stateless callable"): a callable with state is not an empty
// class. The real System contract and every J-10 entry arrive with the
// registry-schedule batch; this header stays in the test directory and is not
// part of engine/kernel.

#include <type_traits>

namespace KernelScaffoldSample {

// Shape (b): the condition as a trait a test can read.
template <typename Callable>
inline constexpr bool kAccepted = std::is_empty_v<Callable>;

// Shape (a): the same condition as a static_assert at the registration point.
// The message is what the try_compile check looks for, so a failure for any
// other reason (a missing include, a wrong flag) is not counted as expected.
template <typename Callable>
constexpr bool Register(const Callable&) noexcept {
    static_assert(kAccepted<Callable>, "GYO_KERNEL_SAMPLE_REJECTED: a system callable has state");
    return true;
}

} // namespace KernelScaffoldSample
