// Compile-failure samples (a) of spec J-10 (migration plan U5: the three core
// rejections are try_compile samples; the rest are traits checked by doctest in
// SignatureTests.cpp). tests/common/kernel/CMakeLists.txt compiles this file
// at configure time once per variant:
//   GYO_KERNEL_SAMPLE_ACCEPTED   a captureless lambda and a function with kernel
//                                handles; must compile (control: include paths
//                                and flags work, so a failure elsewhere is the
//                                sample's own).
//   GYO_KERNEL_SAMPLE_CAPTURE    a lambda with a capture; must fail with
//                                GYO_KERNEL_REJECT_CAPTURE.
//   GYO_KERNEL_SAMPLE_WORLD      a World& parameter; must fail with
//                                GYO_KERNEL_REJECT_PARAMETER.
//   GYO_KERNEL_SAMPLE_RETURN     a System returning void; must fail with
//                                GYO_KERNEL_REJECT_RETURN.
#include "engine/kernel/Kernel.hpp"

namespace {

using namespace GYO::Kernel;

struct SamplePos {
    int x;
};
struct World {};  // stands for the kernel World type

SystemResult ReadSample(Query<Read<SamplePos>>) { return {}; }
SystemResult TakesWorld(World&) { return {}; }
void ReturnsVoid(Query<Read<SamplePos>>) {}

} // namespace

void KernelSampleRegistration(Registrar& registrar, int value) {
    (void)value;
    (void)&ReadSample;
    (void)&TakesWorld;
    (void)&ReturnsVoid;
#if defined(GYO_KERNEL_SAMPLE_ACCEPTED)
    const auto system = [](Query<Read<SamplePos>>) -> SystemResult { return {}; };
    registrar.AddSystem<decltype(system)>({.name = "Lambda"});
    registrar.AddSystem<&ReadSample>({.name = "Function"});
#elif defined(GYO_KERNEL_SAMPLE_CAPTURE)
    const auto system = [value](Query<Read<SamplePos>>) -> SystemResult { return value > 0 ? SystemResult{} : SystemResult{}; };
    registrar.AddSystem<decltype(system)>({.name = "Capturing"});
#elif defined(GYO_KERNEL_SAMPLE_WORLD)
    registrar.AddSystem<&TakesWorld>({.name = "TakesWorld"});
#elif defined(GYO_KERNEL_SAMPLE_RETURN)
    registrar.AddSystem<&ReturnsVoid>({.name = "ReturnsVoid"});
#else
    (void)registrar;
#endif
}
