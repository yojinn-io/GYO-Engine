#include "AssertTestSupport.hpp"

#include "engine/base/Error.hpp"

#include <string>
#include <type_traits>

namespace Sample {

enum class SampleCode { First = 1, Second };

[[nodiscard]] constexpr const char* ToString(const SampleCode code) noexcept {
    switch (code) {
    case SampleCode::First: return "First";
    case SampleCode::Second: return "Second";
    }
    return "Unknown";
}

using SampleError = Engine::Base::Error<SampleCode>;

enum class UnnamedCode { First = 1 }; // no ToString

struct DefaultConstructible final {
    SampleCode code{SampleCode::First};
    std::string message;
    std::string detail;
};

} // namespace Sample

using Engine::Base::CodedError;

static_assert(CodedError<Sample::SampleError>);
static_assert(!std::is_default_constructible_v<Sample::SampleError>, "an Error always describes a failure");
static_assert(!CodedError<Engine::Base::Error<Sample::UnnamedCode>>, "codes need a ToString for Describe");
static_assert(!CodedError<Sample::DefaultConstructible>, "a default-constructible type could mean no error");

TEST_CASE("Error: Make carries code, message and detail") {
    const auto error = Sample::SampleError::Make(Sample::SampleCode::Second, "went wrong", "a/b.txt");
    CHECK(error.code == Sample::SampleCode::Second);
    CHECK(error.message == "went wrong");
    CHECK(error.detail == "a/b.txt");
}

TEST_CASE("Error: the zero code is a Programmer Error") {
    GYO_CHECK_ASSERTS(Sample::SampleError::Make(Sample::SampleCode{}, "no code"));
}

TEST_CASE("Describe: code name and message, then the detail when present") {
    CHECK(Engine::Base::Describe(Sample::SampleError::Make(Sample::SampleCode::First, "missing"))
          == "First: missing");
    CHECK(Engine::Base::Describe(Sample::SampleError::Make(Sample::SampleCode::Second, "unreadable", "a/b.txt"))
          == "Second: unreadable (a/b.txt)");
}

TEST_CASE("CauseDetail: inner code name and detail, after the outer detail") {
    const auto inner = Sample::SampleError::Make(Sample::SampleCode::Second, "unreadable", "a/b.txt");
    CHECK(Engine::Base::CauseDetail(inner) == "Second: a/b.txt");
    CHECK(Engine::Base::CauseDetail(inner, "manifest.json") == "manifest.json; Second: a/b.txt");
    CHECK(Engine::Base::CauseDetail(Sample::SampleError::Make(Sample::SampleCode::First, "missing")) == "First");
}
