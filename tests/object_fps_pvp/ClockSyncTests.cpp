// The Match's clock sampling (pv7 §3); the Gateway's clock_sync_test.go pins
// the same cases on the same algorithm.
#include "RetroFPS/Pvp/ClockSync.hpp"

#include <doctest/doctest.h>

namespace {
using namespace fps::pvp;

// One sample with the given offset and delay, our transmit at `at`.
void Measure(ClockSync& clock, std::uint64_t at, std::int64_t offset, std::int64_t delay) {
    const auto t1 = clock.Stamp(at);
    const auto t2 = static_cast<std::uint64_t>(static_cast<std::int64_t>(t1) + offset + delay / 2);
    clock.Observe({.originUs = t1, .receiveUs = t2, .transmitUs = t2}, t1 + static_cast<std::uint64_t>(delay));
}
} // namespace

TEST_CASE("PvP clock sample follows the four-timestamp formula") {
    ClockSync clock;
    const auto t1 = clock.Stamp(1'000);
    const auto echo = clock.Observe({.originUs = t1, .receiveUs = 5'000, .transmitUs = 5'100}, 1'300);
    REQUIRE(echo);
    CHECK(echo->first == 5'100);
    CHECK(echo->second == 1'300);
    const auto window = clock.Roll(ClockSync::WindowUs);
    REQUIRE(window);
    CHECK(window->samples == 1);
    CHECK(window->delayMinUs == 200);
    CHECK(window->delayMaxUs == 200);
    CHECK(window->offsetUs == 3'900);
}

TEST_CASE("PvP clock sample violations are counted and ignored") {
    ClockSync clock;
    for (std::uint64_t i = 1; i <= ClockSync::RememberedTransmits + 1; ++i) clock.Stamp(i * 100);
    CHECK(clock.Observe({.originUs = 100, .receiveUs = 10, .transmitUs = 20}, 2'000)); // forgotten origin
    CHECK(clock.Observe({.originUs = 900, .transmitUs = 30}, 2'000));                  // origin without receive
    CHECK(clock.Observe({.originUs = 900, .receiveUs = 50, .transmitUs = 40}, 2'000)); // receive after transmit
    CHECK_FALSE(clock.Observe({.transmitUs = 40}, 2'000));                             // not increasing
    CHECK_FALSE(clock.Observe({.transmitUs = 0}, 2'000));                              // below 1
    CHECK(clock.Observe({.originUs = 900, .receiveUs = 50, .transmitUs = 60}, 2'000)); // measured
    CHECK(clock.Observe({.originUs = 900, .receiveUs = 70, .transmitUs = 2'000'000}, 1'000)); // negative delay
    const auto window = clock.Roll(ClockSync::WindowUs);
    REQUIRE(window);
    CHECK(window->invalid == 5);
    CHECK(window->samples == 1);
    CHECK(window->negativeDelay == 1);
    REQUIRE(clock.Echo());
    CHECK(clock.Echo()->first == 2'000'000);
}

TEST_CASE("PvP clock stamps are strictly increasing from one") {
    ClockSync clock;
    CHECK(clock.Stamp(0) == 1);
    CHECK(clock.Stamp(5) == 5);
    CHECK(clock.Stamp(5) == 6);
}

TEST_CASE("PvP clock window takes the minimum-delay sample") {
    ClockSync clock;
    std::uint64_t at = 1'000'000;
    for (const std::int64_t delay : {300, 100, 200}) {
        Measure(clock, at, 40, delay);
        at += 1'000'000;
    }
    const auto window = clock.Roll(ClockSync::WindowUs);
    REQUIRE(window);
    CHECK(window->samples == 3);
    CHECK(window->delayMinUs == 100);
    CHECK(window->delayMaxUs == 300);
    CHECK(window->offsetUs == 40);
}

TEST_CASE("PvP clock windows report missing samples, jumps and drift") {
    ClockSync clock;
    auto window = clock.Roll(ClockSync::WindowUs);
    REQUIRE(window);
    CHECK(ClockWindowLine(*window) ==
          "clock sync window_ms=10000 samples=0 missing=1 invalid=0 negative_delay=0 replaced=0 offset_us=na "
          "delay_min_us=na delay_max_us=na drift_ppm=na jump=0 final=0");
    constexpr auto W = ClockSync::WindowUs;
    // 1 ppm of drift: the offset grows 10 µs per 10 s window.
    for (std::uint64_t i = 2; i <= 4; ++i) {
        CAPTURE(i);
        Measure(clock, i * W - 5'000'000, static_cast<std::int64_t>(i) * 10, 100);
        window = clock.Roll(i * W);
        REQUIRE(window);
        CHECK_FALSE(window->jump);
        if (i >= 3) {
            REQUIRE(window->driftPpm);
            CHECK(*window->driftPpm == doctest::Approx(1.0).epsilon(0.001));
        }
    }
    // A step beyond both uncertainties (50 µs each) and the 1 ms floor.
    Measure(clock, 5 * W - 5'000'000, 40 + 1'200, 100);
    window = clock.Roll(5 * W);
    REQUIRE(window);
    CHECK(window->jump);
    CHECK_FALSE(window->driftPpm);
    // Within the floor: no jump, and the drift restarts from the jump.
    Measure(clock, 6 * W - 5'000'000, 40 + 1'200 + 900, 100);
    clock.NoteReplaced();
    window = clock.Roll(6 * W, true);
    REQUIRE(window);
    CHECK(ClockWindowLine(*window) ==
          "clock sync window_ms=10000 samples=1 missing=0 invalid=0 negative_delay=0 replaced=1 offset_us=2140 "
          "delay_min_us=100 delay_max_us=100 drift_ppm=90.000 jump=0 final=1");
}

TEST_CASE("PvP clock jumps allow each sample's uncertainty and the drift allowance") {
    constexpr auto W = ClockSync::WindowUs;
    ClockSync uncertain;
    Measure(uncertain, 5'000'000, 0, 2'000);
    static_cast<void>(uncertain.Roll(W));
    Measure(uncertain, 15'000'000, 2'500, 2'000); // within 1 ms + (1000 + 1000) µs
    const auto first = uncertain.Roll(2 * W);
    REQUIRE(first);
    CHECK_FALSE(first->jump);
    ClockSync drifting;
    Measure(drifting, 5'000'000, 0, 100);
    static_cast<void>(drifting.Roll(W));
    for (std::uint64_t i = 2; i <= 10; ++i) static_cast<void>(drifting.Roll(i * W)); // no samples
    Measure(drifting, 105'000'000, 5'000, 100); // 100 s later: within 10 ms + 100 µs
    const auto later = drifting.Roll(11 * W);
    REQUIRE(later);
    CHECK_FALSE(later->jump);
}

TEST_CASE("PvP clock checks come before anything is taken") {
    ClockSync clock;
    CHECK_FALSE(clock.Observe({.transmitUs = 0}, 10));
    CHECK_FALSE(clock.Echo());
    CHECK(clock.Observe({.transmitUs = 5}, 20));
    CHECK_FALSE(clock.Observe({.transmitUs = 5}, 30)); // a repeat, read later
    REQUIRE(clock.Echo());
    CHECK(clock.Echo()->second == 20);
    const auto t1 = clock.Stamp(100);
    CHECK(clock.Observe({.originUs = t1, .receiveUs = 150, .transmitUs = 160}, 110)); // delay 0
    const auto window = clock.Roll(ClockSync::WindowUs);
    REQUIRE(window);
    CHECK(window->samples == 1);
    CHECK(window->negativeDelay == 0);
    CHECK(window->delayMinUs == 0);
}
