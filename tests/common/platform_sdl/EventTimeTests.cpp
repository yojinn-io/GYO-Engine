// IP-3 ip3c: event times on the Engine time base (docs/architecture/platform-sdl.md,
// timestamp contract; judgments declared in plans/input-and-present/ip3-declaration.md).
#include <doctest/doctest.h>

#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/time/Waiter.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using Engine::Platform::Sdl::ClockPair;
using Engine::Platform::Sdl::ConvertSdlTimestamp;
using Engine::Platform::Sdl::CurrentClockPairForTesting;
using Engine::Platform::Sdl::NativeEvent;
using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;
using Engine::Time::TimePoint;
using namespace std::chrono_literals;

namespace {

std::unique_ptr<SdlPlatform> MakeTimedPlatform() {
    auto platform = SdlPlatform::Create();
    REQUIRE(platform);
    return std::move(platform.value());
}

// A user event with a marker; sdlNs 0 lets SDL stamp the push time.
void PushMarked(const std::int32_t marker, const std::uint64_t sdlNs = 0) {
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    event.user.code = marker;
    event.common.timestamp = sdlNs;
    REQUIRE(SDL_PushEvent(&event));
}

bool IsMarked(const SDL_Event& event, const std::int32_t marker) {
    return event.type == SDL_EVENT_USER && event.user.code == marker;
}

// SDL time 1 ns lies before every pair, so its conversion is never clamped and
// shows exactly which pair a pump used.
constexpr std::uint64_t EarlySdlTime = 1;

TimePoint ExpectedWith(const ClockPair& pair, const std::uint64_t sdlNs) {
    return pair.engine + std::chrono::nanoseconds{static_cast<std::int64_t>(sdlNs - pair.sdlNs)};
}

bool SamePair(const ClockPair& left, const ClockPair& right) {
    return left.engine == right.engine && left.sdlNs == right.sdlNs && left.bracket == right.bracket;
}

} // namespace

TEST_CASE("MakeClockPair puts the engine time at the midpoint of its two reads") {
    const TimePoint before = TimePoint{} + 1000s;
    const ClockPair pair = Engine::Platform::Sdl::MakeClockPair(before, 77'000'000'000ULL, before + 900ns);
    CHECK_MESSAGE(pair.engine == before + 450ns, "the pair's engine time is the midpoint of the two engine reads");
    CHECK_MESSAGE(pair.sdlNs == 77'000'000'000ULL, "the pair keeps the SDL read unchanged");
    CHECK_MESSAGE(pair.bracket == 900ns, "the bracket is the span between the two engine reads");
}

namespace {

// Fake readers: each read is logged in order and returns the next scripted value.
std::string readLog;
int engineReads = 0;

TimePoint FakeEngineRead() noexcept {
    readLog += 'E';
    return TimePoint{} + (engineReads++ == 0 ? 100ns : 900ns);
}

std::uint64_t FakeSdlRead() noexcept {
    readLog += 'S';
    return 5'000;
}

} // namespace

TEST_CASE("SampleClockPairWith reads Engine then SDL then Engine and takes the midpoint") {
    readLog.clear();
    engineReads = 0;
    const ClockPair pair = Engine::Platform::Sdl::SampleClockPairWith({&FakeEngineRead, &FakeSdlRead});
    CHECK_MESSAGE(readLog == "ESE", "the pair reads Engine, then SDL, then Engine");
    CHECK_MESSAGE(pair.engine == TimePoint{} + 500ns, "the pair's engine time is the midpoint of its own reads");
    CHECK_MESSAGE(pair.sdlNs == 5'000, "the pair keeps its own SDL read");
    CHECK_MESSAGE(pair.bracket == 800ns, "the pair's bracket spans its own engine reads");
}

TEST_CASE("SampleClockPair reads both clocks between the caller's reads") {
    for (int round = 0; round < 100; ++round) {
        const TimePoint engineBefore = Engine::Time::Now();
        const std::uint64_t sdlBefore = SDL_GetTicksNS();
        const ClockPair pair = Engine::Platform::Sdl::SampleClockPair();
        const std::uint64_t sdlAfter = SDL_GetTicksNS();
        const TimePoint engineAfter = Engine::Time::Now();
        CHECK_MESSAGE((pair.engine >= engineBefore && pair.engine <= engineAfter),
                      "the pair's engine time lies between the caller's engine reads");
        CHECK_MESSAGE((pair.sdlNs >= sdlBefore && pair.sdlNs <= sdlAfter),
                      "the pair's SDL time lies between the caller's SDL reads");
        CHECK_MESSAGE(pair.bracket <= engineAfter - engineBefore, "the bracket fits inside the caller's reads");
    }
}

TEST_CASE("ConvertSdlTimestamp places an SDL time relative to the clock pair") {
    const TimePoint engine = TimePoint{} + 1000s;
    const ClockPair pair{engine, 50'000'000'000ULL, 2us};
    const TimePoint ceiling = engine + 60s;
    CHECK_MESSAGE(ConvertSdlTimestamp(50'000'000'000ULL - 3'000'000ULL, pair, ceiling) == engine - 3ms,
                  "an SDL time before the pair maps before the pair's engine time");
    CHECK_MESSAGE(ConvertSdlTimestamp(50'000'000'000ULL + 7'000'000ULL, pair, ceiling) == engine + 7ms,
                  "an SDL time after the pair maps after the pair's engine time");
}

TEST_CASE("ConvertSdlTimestamp never returns a time later than the ceiling") {
    const TimePoint engine = TimePoint{} + 1000s;
    const ClockPair pair{engine, 50'000'000'000ULL, 2us};
    const TimePoint ceiling = engine + 5ms;
    CHECK_MESSAGE(ConvertSdlTimestamp(50'000'000'000ULL + 9'000'000ULL, pair, ceiling) == ceiling,
                  "a converted time later than the ceiling is the ceiling");
}

TEST_CASE("ConvertSdlTimestamp maps a missing SDL time to the delivery time") {
    const TimePoint engine = TimePoint{} + 1000s;
    const ClockPair pair{engine, 50'000'000'000ULL, 2us};
    const TimePoint ceiling = engine + 40ms;
    CHECK_MESSAGE(ConvertSdlTimestamp(0, pair, ceiling) == ceiling, "SDL time 0 maps to the delivery time");
}

TEST_CASE("A pumped event carries the time it was pushed") {
    auto platform = MakeTimedPlatform();
    auto waiter = Engine::Time::Waiter::Create();
    REQUIRE(waiter);

    // Proposal (pending approval): the tolerance comes from the test's own reads:
    // a pair sampled just before the push and the pump's pair measure how far
    // the two clocks drifted apart (rate difference, rounding) in between.
    const ClockPair atPush = Engine::Platform::Sdl::SampleClockPair();
    const TimePoint before = Engine::Time::Now();
    PushMarked(1);
    const TimePoint after = Engine::Time::Now();
    (void)waiter.value().WaitUntil(Engine::Time::Now() + 5ms);

    std::optional<TimePoint> stamped;
    std::optional<TimePoint> delivered;
    std::optional<ClockPair> sampled;
    (void)platform->PumpEvents([&](const PlatformEvent& event) {
        if (!IsMarked(NativeEvent(event), 1)) return;
        stamped = event.Timestamp();
        delivered = Engine::Time::Now();
        sampled = Engine::Platform::Sdl::CurrentClockPairForTesting(*platform);
    });
    REQUIRE(stamped);
    REQUIRE(sampled);
    const auto engineSpan = std::chrono::duration_cast<std::chrono::nanoseconds>(sampled->engine - atPush.engine);
    const auto sdlSpan = std::chrono::nanoseconds{static_cast<std::int64_t>(sampled->sdlNs - atPush.sdlNs)};
    const auto drift = engineSpan - sdlSpan;
    const auto tolerance = (drift < 0ns ? -drift : drift) + atPush.bracket + sampled->bracket + 2ns;
    MESSAGE("tolerance_ns=" << tolerance.count() << " drift_ns=" << drift.count()
            << " bracket_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(sampled->bracket).count()
            << " error_lo_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(*stamped - before).count()
            << " error_hi_ns=" << std::chrono::duration_cast<std::chrono::nanoseconds>(after - *stamped).count());
    CHECK_MESSAGE(*stamped >= before - tolerance, "the event time is not earlier than its push");
    CHECK_MESSAGE(*stamped <= after + tolerance, "the event time is not later than its push");
    CHECK_MESSAGE(*stamped <= *delivered, "the event time is not later than its delivery");
}

TEST_CASE("An SDL time in the future is delivered no later than its delivery") {
    auto platform = MakeTimedPlatform();
    PushMarked(2, SDL_GetTicksNS() + 10'000'000'000ULL);
    std::optional<TimePoint> stamped;
    std::optional<TimePoint> delivered;
    (void)platform->PumpEvents([&](const PlatformEvent& event) {
        if (!IsMarked(NativeEvent(event), 2)) return;
        stamped = event.Timestamp();
        delivered = Engine::Time::Now();
    });
    REQUIRE(stamped);
    CHECK_MESSAGE(*stamped <= *delivered, "a future SDL time is clamped to the delivery time");
}

TEST_CASE("Within one pump non-decreasing SDL times stay non-decreasing") {
    auto platform = MakeTimedPlatform();
    const std::uint64_t base = SDL_GetTicksNS();
    PushMarked(3, base - 3'000'000ULL);
    PushMarked(4, base - 2'000'000ULL);
    PushMarked(5, base - 2'000'000ULL);
    PushMarked(6, base - 1'000'000ULL);
    std::vector<TimePoint> stamps;
    (void)platform->PumpEvents([&](const PlatformEvent& event) {
        const SDL_Event& native = NativeEvent(event);
        if (native.type == SDL_EVENT_USER && native.user.code >= 3 && native.user.code <= 6)
            stamps.push_back(event.Timestamp());
    });
    REQUIRE(stamps.size() == 4);
    for (std::size_t index = 1; index < stamps.size(); ++index)
        CHECK_MESSAGE(stamps[index - 1] <= stamps[index], "event times keep the order of their SDL times");
}

TEST_CASE("Each pump converts with its own clock pair") {
    auto platform = MakeTimedPlatform();
    CHECK_FALSE(Engine::Platform::Sdl::CurrentClockPairForTesting(*platform));

    std::optional<TimePoint> first;
    std::optional<ClockPair> firstPair;
    PushMarked(7, EarlySdlTime);
    (void)platform->PumpEvents([&](const PlatformEvent& event) {
        if (!IsMarked(NativeEvent(event), 7)) return;
        first = event.Timestamp();
        firstPair = Engine::Platform::Sdl::CurrentClockPairForTesting(*platform);
    });
    std::optional<TimePoint> second;
    std::optional<ClockPair> secondPair;
    PushMarked(8, EarlySdlTime);
    (void)platform->PumpEvents([&](const PlatformEvent& event) {
        if (!IsMarked(NativeEvent(event), 8)) return;
        second = event.Timestamp();
        secondPair = Engine::Platform::Sdl::CurrentClockPairForTesting(*platform);
    });
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(firstPair);
    REQUIRE(secondPair);
    CHECK_MESSAGE(*first == ExpectedWith(*firstPair, EarlySdlTime), "the first pump converts with its pair");
    CHECK_MESSAGE(*second == ExpectedWith(*secondPair, EarlySdlTime), "the second pump converts with its pair");
    CHECK_MESSAGE((secondPair->sdlNs > firstPair->sdlNs && secondPair->engine > firstPair->engine),
                  "the second pump samples a new pair");
    CHECK_FALSE(Engine::Platform::Sdl::CurrentClockPairForTesting(*platform));
}

TEST_CASE("A nested pump samples its own clock pair and restores the outer one") {
    auto platform = MakeTimedPlatform();
    // Both are queued before the outer pump's pass: the nested pump, called while
    // the outer one handles 9, polls 11 from the same pass. Events pushed during
    // a pass come after its end marker (10 below reaches the outer pump).
    PushMarked(9, EarlySdlTime);
    PushMarked(11, EarlySdlTime);
    std::optional<ClockPair> outerPair;
    std::optional<ClockPair> innerPair;
    std::optional<ClockPair> restoredPair;
    std::optional<TimePoint> inner;
    std::optional<TimePoint> afterNested;
    bool nested = false;
    (void)platform->PumpEvents([&](const PlatformEvent& event) {
        const SDL_Event& native = NativeEvent(event);
        if (IsMarked(native, 9) && !nested) {
            nested = true;
            outerPair = Engine::Platform::Sdl::CurrentClockPairForTesting(*platform);
            (void)platform->PumpEvents([&](const PlatformEvent& innerEvent) {
                if (!IsMarked(NativeEvent(innerEvent), 11)) return;
                inner = innerEvent.Timestamp();
                innerPair = Engine::Platform::Sdl::CurrentClockPairForTesting(*platform);
            });
            restoredPair = Engine::Platform::Sdl::CurrentClockPairForTesting(*platform);
            PushMarked(10, EarlySdlTime);  // polled by the outer pump
        } else if (IsMarked(native, 10)) {
            afterNested = event.Timestamp();
        }
    });
    REQUIRE(nested);
    REQUIRE(outerPair);
    REQUIRE(innerPair);
    REQUIRE(restoredPair);
    REQUIRE(inner);
    REQUIRE(afterNested);
    CHECK_MESSAGE(innerPair->sdlNs > outerPair->sdlNs, "a nested pump samples its own pair");
    CHECK_MESSAGE(*inner == ExpectedWith(*innerPair, EarlySdlTime), "a nested pump converts with its own pair");
    CHECK_MESSAGE(SamePair(*restoredPair, *outerPair), "after a nested pump the outer pump's pair is back");
    CHECK_MESSAGE(*afterNested == ExpectedWith(*outerPair, EarlySdlTime),
                  "after a nested pump the outer pump converts with its own pair");
}
