#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "AssertTestSupport.hpp"

#include <cstring>
#include <exception>
#include <type_traits>

using Engine::Base::AssertionFailure;
using Engine::Base::AssertionHandler;
using Engine::Base::SetAssertionHandler;

namespace {

int CountedTrue(int& evaluations) {
    ++evaluations;
    return 1;
}

int CountedFalse(int& evaluations) {
    ++evaluations;
    return 0;
}

void RecordingHandler(const AssertionFailure&) {}

// Asserts that its argument is positive. Not noexcept, as required for a
// function that asserts.
int RequirePositive(int value) {
    GYO_ASSERT(value > 0);
    return value;
}

} // namespace

TEST_CASE("GYO_ASSERT: a true condition is evaluated once and does nothing") {
    int evaluations = 0;
    GYO_ASSERT(CountedTrue(evaluations) == 1);
    CHECK(evaluations == 1);
}

TEST_CASE("GYO_ASSERT: a false condition is evaluated once and reports") {
    int evaluations = 0;
    GYO_CHECK_ASSERTS(GYO_ASSERT(CountedFalse(evaluations) == 1));
    CHECK(evaluations == 1);
}

TEST_CASE("GYO_ASSERT: conditions may contain commas") {
    GYO_ASSERT(std::is_same_v<int, int>);
    GYO_CHECK_ASSERTS(GYO_ASSERT(std::is_same_v<int, long> && sizeof(int) > 0));
}

TEST_CASE("GYO_ASSERT: the failure carries the condition text and location") {
    Engine::Test::ScopedAssertionHandler scope;
    const int value = 3;
    try {
        GYO_ASSERT(value == 4);
        FAIL("GYO_ASSERT did not report");
    } catch (const AssertionFailure& failure) {
        CHECK(std::strcmp(failure.expression, "value == 4") == 0);
        CHECK(std::strstr(failure.location.file_name(), "AssertTests.cpp") != nullptr);
        CHECK(failure.location.line() > 0);
    }
}

TEST_CASE("GYO_CHECK_ASSERTS: consecutive failures in one test case are each reported") {
    // The reentrancy guard must reset when the test handler throws.
    GYO_CHECK_ASSERTS(RequirePositive(0));
    GYO_CHECK_ASSERTS(RequirePositive(-1));
    CHECK(RequirePositive(2) == 2);
}

TEST_CASE("SetAssertionHandler: returns the previous handler and nullptr restores the default") {
    const AssertionHandler original = SetAssertionHandler(&RecordingHandler);
    CHECK(SetAssertionHandler(original) == &RecordingHandler);
    {
        Engine::Test::ScopedAssertionHandler scope;
        CHECK(SetAssertionHandler(&RecordingHandler) == &Engine::Test::ThrowAssertionFailure);
        SetAssertionHandler(&Engine::Test::ThrowAssertionFailure);
    }
    // The scope restored whatever was installed before it.
    CHECK(SetAssertionHandler(original) == original);
}

TEST_CASE("GYO_UNREACHABLE reports like a failed assertion") {
    Engine::Test::ScopedAssertionHandler scope;
    try {
        GYO_UNREACHABLE();
    } catch (const AssertionFailure& failure) {
        CHECK(std::strcmp(failure.expression, "unreachable") == 0);
    }
}

TEST_CASE("AssertionFailure is not a std::exception") {
    static_assert(!std::is_base_of_v<std::exception, AssertionFailure>);
    CHECK(true);
}

// Negative control: GYO_CHECK_ASSERTS must report a failure when the
// expression does not fail an assertion. should_fail inverts the outcome.
TEST_CASE("GYO_CHECK_ASSERTS fails when nothing asserts" * doctest::should_fail()) {
    GYO_CHECK_ASSERTS(RequirePositive(1));
}
