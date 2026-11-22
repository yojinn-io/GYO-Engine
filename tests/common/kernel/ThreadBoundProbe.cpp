// DI-KS 14, spec J-6 (v8-arch-gyo2-thread-binding-by-runner): destroying a World
// whose thread-bound resource holds a value put in on another thread fails a
// GYO_ASSERT. A destructor cannot throw to a test, so this probe is its own
// process: the assertion handler prints a line and ends the process with 0;
// returning from main means the destruction was not checked (exit 1).

#include "engine/kernel/Kernel.hpp"

#include "engine/base/Assert.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using namespace GYO::Kernel;

struct Bound {
    std::int64_t value;
};

void BuildProbe(Registrar& r) {
    r.Resource<Bound>({.name = "Bound", .threadBound = true});
    ScheduleOptions s;
    s.name = "Tick";
    s.stages = {"A"};
    r.Schedule(s);
}

void AssertedOnDestroy(const GYO::Engine::Base::AssertionFailure& failure) {
    std::printf("thread-bound destroy: GYO_ASSERT failed as expected: %s\n", failure.expression);
    std::fflush(stdout);
    std::_Exit(0);
}

} // namespace

int main() {
    Registry registry;
    static_cast<void>(registry.Register(RegistrationOwner{"probe", 0}, &BuildProbe));
    if (!registry.BuildPlans()) {
        std::printf("thread-bound destroy: BuildPlans failed\n");
        return 2;
    }
    auto executor = std::make_unique<WorldExecutor>(registry, "probe");
    std::thread([&] { executor->GetWorld().SetResource(Bound{1}); }).join();
    GYO::Engine::Base::SetAssertionHandler(&AssertedOnDestroy);
    executor.reset();
    std::printf("thread-bound destroy: no GYO_ASSERT when the World was destroyed on another thread\n");
    return 1;
}
