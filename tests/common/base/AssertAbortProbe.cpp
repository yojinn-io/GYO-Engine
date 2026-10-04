#include "engine/base/Assert.hpp"

#include <cstdio>
#include <cstring>
#include <type_traits>

#if defined(_MSC_VER)
#include <stdlib.h>
#endif

// Run by AssertAbortProbe.cmake. Every mode must end in abort(); reaching the
// end of main is a failure the script detects by its marker.
namespace {

void ReturningHandler(const Engine::Base::AssertionFailure& failure) {
    std::fprintf(stderr, "probe handler saw: %s\n", failure.expression);
    std::fflush(stderr);
}

void NestedHandler(const Engine::Base::AssertionFailure&) {
    const volatile int inner = 0;
    GYO_ASSERT(inner == 1);
}

} // namespace

int main(int argc, char** argv) {
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const char* mode = argc > 1 ? argv[1] : "default";
    if (std::strcmp(mode, "returning") == 0) {
        Engine::Base::SetAssertionHandler(&ReturningHandler);
    } else if (std::strcmp(mode, "nested") == 0) {
        Engine::Base::SetAssertionHandler(&NestedHandler);
    }

    const volatile int probeValue = 1;
    GYO_ASSERT(probeValue == 2 && std::is_same_v<int, int>);

    std::fprintf(stderr, "probe continued after a failed assertion\n");
    return 0;
}
