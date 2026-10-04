#include "AssertTestSupport.hpp"

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"

#include <string>
#include <utility>

namespace {

enum class SampleErrorCode { None = 0, Failed };
using SampleError = Engine::Base::Error<SampleErrorCode>;
using IntResult = Engine::Base::Result<int, SampleError>;
using VoidResult = Engine::Base::Result<void, SampleError>;

SampleError Failure() {
    return SampleError::Make(SampleErrorCode::Failed, "failed", "sample");
}

} // namespace

TEST_CASE("Result: the held side is readable") {
    auto success = IntResult::Ok(7);
    REQUIRE(success);
    CHECK(success.value() == 7);
    CHECK(std::move(success).value() == 7);

    auto failure = IntResult::Err(Failure());
    REQUIRE_FALSE(failure);
    CHECK(failure.error().code == SampleErrorCode::Failed);
    CHECK(std::move(failure).error().message == "failed");

    const auto voidSuccess = VoidResult::Ok();
    CHECK(voidSuccess);
    voidSuccess.value();
}

TEST_CASE("Result: reading the side that is not held is a Programmer Error") {
    auto success = IntResult::Ok(7);
    const auto& constSuccess = success;
    GYO_CHECK_ASSERTS(success.error());
    GYO_CHECK_ASSERTS(constSuccess.error());
    GYO_CHECK_ASSERTS(std::move(success).error());

    auto failure = IntResult::Err(Failure());
    const auto& constFailure = failure;
    GYO_CHECK_ASSERTS(failure.value());
    GYO_CHECK_ASSERTS(constFailure.value());
    GYO_CHECK_ASSERTS(std::move(failure).value());

    const auto voidSuccess = VoidResult::Ok();
    GYO_CHECK_ASSERTS(voidSuccess.error());
    const auto voidFailure = VoidResult::Err(Failure());
    GYO_CHECK_ASSERTS(voidFailure.value());
}
