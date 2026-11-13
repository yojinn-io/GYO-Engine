// Source of compile-failure sample (a) (spec J-10, migration plan U5). The
// kernel tests' CMakeLists compiles it twice with try_compile:
//   GYO_KERNEL_SAMPLE_ACCEPTED defined   -> a captureless callable; must compile
//                                           (control: include paths and flags work).
//   GYO_KERNEL_SAMPLE_ACCEPTED undefined -> a callable with a capture; must fail
//                                           with GYO_KERNEL_SAMPLE_REJECTED.
#include "engine/kernel/Kernel.hpp"

#include "SampleContract.hpp"

int KernelScaffoldSampleRegistration(int value) {
#if defined(GYO_KERNEL_SAMPLE_ACCEPTED)
    const auto system = [](int input) { return input + 1; };
    (void)value;
#else
    const auto system = [value](int input) { return input + value; };
#endif
    return KernelScaffoldSample::Register(system) ? system(1) : 0;
}
