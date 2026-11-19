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
// Added after the batch's review (registration-point checks beyond J-10's three
// core shapes, so removing a static_assert is caught at the registration point):
//   GYO_KERNEL_SAMPLE_SELF_CONFLICT     J-11: one column written twice -> GYO_KERNEL_REJECT_SELF_CONFLICT
//   GYO_KERNEL_SAMPLE_CONDITION_WRITE   J-13: a run condition that writes -> GYO_KERNEL_REJECT_CONDITION_WRITE
//   GYO_KERNEL_SAMPLE_MEMBER            J-10: a member function -> GYO_KERNEL_REJECT_MEMBER_FUNCTION
//   GYO_KERNEL_SAMPLE_HOOK_PARAMETER    a hook with a World& parameter -> GYO_KERNEL_REJECT_PARAMETER
// Added by world-exec (spec J-4, "a handle kept in Local or a static does not compile"):
//   GYO_KERNEL_SAMPLE_HANDLE_COPY       requires a handle to be copyable or movable; must fail with
//                                       GYO_KERNEL_SAMPLE_HANDLE_NOT_COPYABLE (a handle that could be
//                                       copied or moved could be kept past its call)
//   GYO_KERNEL_SAMPLE_KERNEL_DIAGNOSTICS a System writing the kernel's own RejectedSpawn records
//                                       (spec F-13) -> GYO_KERNEL_REJECT_KERNEL_DIAGNOSTICS
// Added by v8-arch-gyo2-time-events step 3 (event cursor declaration 3.2: a hook is not a reader):
//   GYO_KERNEL_SAMPLE_HOOK_MESSAGE_READER a hook with a MessageReader parameter
//                                       -> GYO_KERNEL_REJECT_HOOK_MESSAGE_READER
#include "engine/kernel/Kernel.hpp"

#include <type_traits>

namespace {

using namespace GYO::Kernel;

struct SamplePos {
    int x;
};
struct World {};  // stands for the kernel World type

SystemResult ReadSample(Query<Read<SamplePos>>) { return {}; }
SystemResult TakesWorld(World&) { return {}; }
void ReturnsVoid(Query<Read<SamplePos>>) {}
SystemResult WritesTwice(Query<Write<SamplePos>>, Query<Write<SamplePos>>) { return {}; }
bool ConditionWrites(Query<Write<SamplePos>>) { return true; }
struct SampleMessage {
    int n;
};
SystemResult HookReadsMessages(HookCtx, MessageReader<SampleMessage>) { return {}; }
struct Holder {
    SystemResult Member(Query<Read<SamplePos>>) { return {}; }
};

} // namespace

void KernelSampleRegistration(Registrar& registrar, int value) {
    (void)value;
    (void)&ReadSample;
    (void)&TakesWorld;
    (void)&ReturnsVoid;
    (void)&WritesTwice;
    (void)&ConditionWrites;
    (void)&HookReadsMessages;
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
#elif defined(GYO_KERNEL_SAMPLE_SELF_CONFLICT)
    registrar.AddSystem<&WritesTwice>({.name = "WritesTwice"});
#elif defined(GYO_KERNEL_SAMPLE_CONDITION_WRITE)
    (void)Condition<&ConditionWrites>("ConditionWrites");
#elif defined(GYO_KERNEL_SAMPLE_MEMBER)
    registrar.AddSystem<&Holder::Member>({.name = "Member"});
#elif defined(GYO_KERNEL_SAMPLE_HOOK_PARAMETER)
    registrar.Hook<SamplePos, &TakesWorld>(HookTrigger::OnAdd, {.name = "HookTakesWorld"});
#elif defined(GYO_KERNEL_SAMPLE_HOOK_MESSAGE_READER)
    registrar.Hook<SamplePos, &HookReadsMessages>(HookTrigger::OnAdd, {.name = "HookReadsMessages"});
#elif defined(GYO_KERNEL_SAMPLE_HANDLE_COPY)
    (void)registrar;
    static_assert(std::is_copy_constructible_v<Query<Read<SamplePos>>> || std::is_move_constructible_v<Query<Read<SamplePos>>> ||
                      std::is_copy_constructible_v<Res<SamplePos>> || std::is_move_constructible_v<Res<SamplePos>> ||
                      std::is_copy_constructible_v<Commands> || std::is_move_constructible_v<Commands>,
                  "GYO_KERNEL_SAMPLE_HANDLE_NOT_COPYABLE: kernel handles are neither copyable nor movable");
#elif defined(GYO_KERNEL_SAMPLE_KERNEL_DIAGNOSTICS)
    const auto system = [](DiagnosticsWriter<RejectedSpawn>) -> SystemResult { return {}; };
    registrar.AddSystem<decltype(system)>({.name = "WritesKernelRecords"});
#else
    (void)registrar;
#endif
}
