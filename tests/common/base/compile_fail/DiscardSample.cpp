// Compile-failure sample of error levels D5
// (docs/architecture/target/v2/error-levels-declaration.zh-Hant.md): discarding
// a [[nodiscard]] value unread does not compile in GYO C++ code.
// tests/common/base/CMakeLists.txt compiles this file at configure time once per
// variant, with build/cmake/GyoBuild.cmake's GYO_DISCARD_IS_ERROR_OPTIONS:
//   GYO_DISCARD_SAMPLE_ACCEPTED  every value read, or discarded with an explicit
//                                (void); must compile (control: include paths and
//                                flags work, so a failure elsewhere is the sample's own).
//   GYO_DISCARD_SAMPLE_RESULT    a Base::Result (a [[nodiscard]] class) discarded;
//                                must fail with a diagnostic naming nodiscard.
//   GYO_DISCARD_SAMPLE_FUNCTION  a [[nodiscard]] function's int discarded; must
//                                fail with a diagnostic naming nodiscard.
// GCC, Clang and MSVC all name the attribute in these diagnostics, so the
// expected failure is "the compile failed and its output names nodiscard".
#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <cstdint>

namespace {

enum class SampleCode : std::uint8_t { Failed = 1 };

[[nodiscard]] const char* ToString(SampleCode) noexcept { return "Failed"; }

using SampleFailure = ::Engine::Base::Error<SampleCode>;

::Engine::Base::Result<int, SampleFailure> Produce() { return 1; }

[[nodiscard]] int Count() { return 1; }

} // namespace

int Use() {
#if defined(GYO_DISCARD_SAMPLE_ACCEPTED)
    auto produced = Produce();
    (void)Count(); // explicit: the count is not needed here
    return produced ? *produced : 0;
#elif defined(GYO_DISCARD_SAMPLE_RESULT)
    Produce();
    return 0;
#elif defined(GYO_DISCARD_SAMPLE_FUNCTION)
    Count();
    return 0;
#else
#error "define one GYO_DISCARD_SAMPLE_ variant"
#endif
}
