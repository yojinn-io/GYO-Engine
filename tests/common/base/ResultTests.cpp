#include "AssertTestSupport.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using Engine::Base::Err;
using Engine::Base::Result;

namespace {

enum class SampleErrorCode { None = 0, Failed };
using SampleError = Engine::Base::Error<SampleErrorCode>;
using IntResult = Result<int, SampleError>;
using VoidResult = Result<void, SampleError>;

SampleError Failure() {
    return SampleError::Make(SampleErrorCode::Failed, "failed", "sample");
}

IntResult Parse(const bool succeed) {
    if (!succeed) return Err(Failure());
    return 7;
}

VoidResult Check(const bool succeed) {
    if (!succeed) return Err(Failure());
    return {};
}

IntResult Forward(const bool succeed) {
    auto parsed = Parse(succeed);
    if (!parsed) return Err(std::move(parsed).error());
    return *parsed + 1;
}

struct Explicit final {
    explicit Explicit(int input) : value(input) {}
    int value;
};

} // namespace

// Construction rules, checked at compile time.
static_assert(std::is_convertible_v<int, IntResult>, "success converts implicitly");
static_assert(std::is_convertible_v<Err<SampleError>, IntResult>, "Err converts implicitly");
static_assert(!std::is_constructible_v<IntResult, SampleError>, "a bare E is rejected: a forgotten Err does not compile");
static_assert(!std::is_default_constructible_v<IntResult>, "no default success for non-void T");
static_assert(std::is_default_constructible_v<VoidResult>, "`return {};` is the void success");
static_assert(std::is_constructible_v<Result<Explicit, SampleError>, int>, "explicit T constructors stay available");
static_assert(!std::is_convertible_v<int, Result<Explicit, SampleError>>, "...but only explicitly");
static_assert(std::is_convertible_v<Err<const char*>, Result<int, std::string>>,
              "Err<G> converts when E is constructible from G (string literals for string errors)");
static_assert(!std::is_constructible_v<Result<int, std::string>, Err<int>>);

TEST_CASE("Result: success and failure through return statements") {
    const auto success = Parse(true);
    REQUIRE(success);
    CHECK(success.has_value());
    CHECK(*success == 7);
    CHECK(success.value() == 7);

    const auto failure = Parse(false);
    REQUIRE_FALSE(failure);
    CHECK(failure.error().code == SampleErrorCode::Failed);
    CHECK(failure.error().message == "failed");

    CHECK(Forward(true).value() == 8);
    CHECK(Forward(false).error().detail == "sample");

    CHECK(Check(true));
    CHECK(Check(false).error().code == SampleErrorCode::Failed);
}

TEST_CASE("Result: string literal errors convert through Err<const char*>") {
    const auto make = [](const bool succeed) -> Result<int, std::string> {
        if (!succeed) return Err("no value");
        return 1;
    };
    CHECK(make(false).error() == "no value");
    CHECK(make(true).value() == 1);
}

TEST_CASE("Result: move-only values, arrow access and Result<bool>") {
    const auto make = []() -> Result<std::unique_ptr<int>, SampleError> {
        return std::make_unique<int>(5);
    };
    auto owned = make();
    REQUIRE(owned);
    std::unique_ptr<int> taken = std::move(owned).value();
    CHECK(*taken == 5);

    const Result<std::string, SampleError> text = std::string("abc");
    CHECK(text->size() == 3);

    // operator bool reports success, not the held bool.
    const Result<bool, SampleError> no = false;
    CHECK(no);
    CHECK_FALSE(no.value());

    const Result<std::optional<int>, SampleError> empty = std::nullopt;
    CHECK(empty);
    CHECK_FALSE(empty->has_value());
}

TEST_CASE("Result: reading the side that is not held is a Programmer Error") {
    auto success = Parse(true);
    const auto& constSuccess = success;
    GYO_CHECK_ASSERTS(success.error());
    GYO_CHECK_ASSERTS(constSuccess.error());
    GYO_CHECK_ASSERTS(std::move(success).error());

    auto failure = Parse(false);
    const auto& constFailure = failure;
    GYO_CHECK_ASSERTS(failure.value());
    GYO_CHECK_ASSERTS(constFailure.value());
    GYO_CHECK_ASSERTS(*failure);
    GYO_CHECK_ASSERTS(std::move(failure).value());

    GYO_CHECK_ASSERTS(Check(true).error());
    GYO_CHECK_ASSERTS(Check(false).value());
}
