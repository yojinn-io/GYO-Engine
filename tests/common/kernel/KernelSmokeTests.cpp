// Smoke test of the kernel scaffold and compile-failure sample (b)
// (docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md).
// The executable links only GYO::Kernel and gyo_test_support (GYO::Base and
// doctest), so a kernel header that needs anything else does not compile here.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "engine/kernel/Kernel.hpp"

#include "compile_fail/SampleContract.hpp"

#include <doctest/doctest.h>

TEST_CASE("Kernel scaffold: the kernel header compiles on its own") {
    // Nothing to call yet: building this translation unit is the check.
    CHECK(true);
}

TEST_CASE("Kernel compile-failure sample (b): the trait rejects a callable with state") {
    int captured = 2;
    const auto stateless = [](int input) { return input + 1; };
    const auto stateful = [captured](int input) { return input + captured; };
    const auto by_reference = [&captured](int input) { return input + captured; };
    CHECK(KernelScaffoldSample::kAccepted<decltype(stateless)>);
    CHECK_FALSE(KernelScaffoldSample::kAccepted<decltype(stateful)>);
    // Beyond what sample (a) builds: a capture by reference is state too.
    CHECK_FALSE(KernelScaffoldSample::kAccepted<decltype(by_reference)>);
    CHECK(stateless(1) + stateful(1) + by_reference(1) == 8);
}
