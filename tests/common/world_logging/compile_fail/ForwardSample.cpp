// Compile-fail samples of Design Invariant LG-3 (tests/common/world_logging/CMakeLists.txt): the control
// compiles; each variant must fail with its own static_assert marker.
#include <string_view>

#include "engine/kernel/Params.hpp"
#include "engine/logging/Level.hpp"
#include "engine/logging/RecordBuilder.hpp"
#include "engine/world_logging/Forward.hpp"

namespace WL = GYO::Engine::WorldLogging;
namespace K = GYO::Kernel;
namespace L = GYO::Engine::Logging;

struct Sample final {
    int value = 0;
};

struct SampleLog final {
    using Message = Sample;
    static constexpr std::string_view name = "Sample";
    static constexpr std::string_view category = "sample";
    static constexpr L::Level level = L::Level::Info;
    static void Describe(const Sample&, L::RecordBuilder&) {}
};

// No Describe: not a descriptor.
struct NotADescriptor final {
    using Message = Sample;
    static constexpr std::string_view name = "Sample";
    static constexpr std::string_view category = "sample";
    static constexpr L::Level level = L::Level::Info;
};

#if defined(GYO_WORLD_LOGGING_SAMPLE_ACCEPTED)
static_assert(WL::Detail::RequireReadOnlyParameters<K::MessageReader<Sample>, K::Res<WL::LogWorldLabel<SampleLog>>>());
static_assert(WL::Detail::RequireDescriptor<SampleLog>());
void Instantiate(GYO::Engine::Orchestration::WorldSetup& setup) { WL::Forward<SampleLog>(setup, {"Main", "Last"}); }
#elif defined(GYO_WORLD_LOGGING_SAMPLE_WRITE)
static_assert(WL::Detail::RequireReadOnlyParameters<K::MessageReader<Sample>, K::MessageWriter<Sample>>());
#elif defined(GYO_WORLD_LOGGING_SAMPLE_DESCRIPTOR)
void Instantiate(GYO::Engine::Orchestration::WorldSetup& setup) { WL::Forward<NotADescriptor>(setup, {"Main", "Last"}); }
#endif
