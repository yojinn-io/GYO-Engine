#pragma once

#include "doctest/doctest.h"

#include "kernel_spike/Assert.hpp"
#include "kernel_spike/Registry.hpp"

#include <vector>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

// The repository's way of checking a Programmer Error
// (tests/common/support/AssertTestSupport.hpp), applied to the spike's own
// KERNEL_SPIKE_ASSERT: a scoped handler turns the failure into a throw.

namespace KernelSpikeTest {

[[noreturn]] inline void ThrowAssertionFailure(const KernelSpike::AssertionFailure& failure) {
    throw failure;
}

class ScopedAssertionHandler final {
public:
    ScopedAssertionHandler() noexcept : previous_(KernelSpike::SetAssertionHandler(&ThrowAssertionFailure)) {}
    ~ScopedAssertionHandler() { KernelSpike::SetAssertionHandler(previous_); }
    ScopedAssertionHandler(const ScopedAssertionHandler&) = delete;
    ScopedAssertionHandler& operator=(const ScopedAssertionHandler&) = delete;

private:
    KernelSpike::AssertionHandler previous_;
};

#if defined(_MSC_VER)
inline const bool abortReportingDisabled = [] {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    return true;
}();
#endif

// ---- Types that exist only in the tests.

struct Position {
    float x;
    float y;
};
struct Velocity {
    float dx;
    float dy;
};
struct Health {
    int points;
};

struct Time {
    int tick;
};
struct Gravity {
    float g;
};
struct Stats {
    int count;
};
// Owns heap memory: the access check compares its inline bytes only.
struct Inventory {
    std::vector<int> items;
};

struct Hit {
    int target;
    int amount;
};
struct Spawned {
    int id;
};

// Registration order fixes the ids the tests write by hand:
// components Position 0, Velocity 1, Health 2;
// resources Time 0, Gravity 1, Stats 2, Inventory 3; events Hit 0, Spawned 1.
inline KernelSpike::Registry MakeRegistry() {
    KernelSpike::Registry registry;
    registry.RegisterComponent<Position>("Position");
    registry.RegisterComponent<Velocity>("Velocity");
    registry.RegisterComponent<Health>("Health");
    registry.RegisterResource<Time>("Time");
    registry.RegisterResource<Gravity>("Gravity");
    registry.RegisterResource<Stats>("Stats");
    registry.RegisterResource<Inventory>("Inventory");
    registry.RegisterEvent<Hit>("Hit");
    registry.RegisterEvent<Spawned>("Spawned");
    return registry;
}

} // namespace KernelSpikeTest

// Checks that evaluating the expression fails a KERNEL_SPIKE_ASSERT.
#define SPIKE_CHECK_ASSERTS(...)                                                   \
    do {                                                                           \
        bool spikeAsserted = false;                                                \
        {                                                                          \
            ::KernelSpikeTest::ScopedAssertionHandler spikeAssertScope;            \
            try {                                                                  \
                static_cast<void>(__VA_ARGS__);                                    \
            } catch (const ::KernelSpike::AssertionFailure&) {                     \
                spikeAsserted = true;                                              \
            }                                                                      \
        }                                                                          \
        CHECK_MESSAGE(spikeAsserted, "expected KERNEL_SPIKE_ASSERT to fail: " #__VA_ARGS__); \
    } while (false)

// Checks that evaluating the expression does not fail a KERNEL_SPIKE_ASSERT
// (a failure is reported as a test failure instead of aborting the run).
#define SPIKE_CHECK_NO_ASSERT(...)                                                 \
    do {                                                                           \
        bool spikeAsserted = false;                                                \
        {                                                                          \
            ::KernelSpikeTest::ScopedAssertionHandler spikeAssertScope;            \
            try {                                                                  \
                static_cast<void>(__VA_ARGS__);                                    \
            } catch (const ::KernelSpike::AssertionFailure& failure) {             \
                spikeAsserted = true;                                              \
                MESSAGE("unexpected KERNEL_SPIKE_ASSERT: " << failure.message);    \
            }                                                                      \
        }                                                                          \
        CHECK_MESSAGE(!spikeAsserted, "expected no KERNEL_SPIKE_ASSERT: " #__VA_ARGS__); \
    } while (false)
