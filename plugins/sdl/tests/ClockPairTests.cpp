// Event times on the engine time base (Design Invariant SDL-3, docs/architecture/target/v2/sdl.zh-Hant.md; SDL
// declaration section 8). The pairing and the conversion are pure: scripted readers check them with exact equalities
// (master's platform-sdl EventTimeTests, D34). Each CHECK carries a tag ("[sc-...]") the mutants expect.
#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>

#include "plugins/sdl/ClockPair.hpp"

// The whole file is in the Plugin's namespace (naming N17: a unit opens only its own root, aliases included).
namespace GYO::Plugins::SDL {

namespace S = GYO::Plugins::SDL;
using GYO::Engine::Time::TimePoint;
using namespace std::chrono_literals;

TEST_CASE("clock: MakeClockPair puts the engine time at the midpoint of its two reads") {
    const TimePoint before = TimePoint{} + 1000s;
    const S::ClockPair pair = S::MakeClockPair(before, 77'000'000'000ULL, before + 900ns);
    CHECK_MESSAGE(pair.engine == before + 450ns, "[sc-midpoint] the engine time is the midpoint of the two reads");
    CHECK_MESSAGE(pair.sdlNs == 77'000'000'000ULL, "[sc-sdl-kept] the SDL read is kept as it is");
    CHECK_MESSAGE(pair.bracket == 900ns, "[sc-bracket] the bracket spans the two engine reads");
}

namespace {

std::string readLog;
int engineReads = 0;

TimePoint FakeEngineRead() noexcept {
    readLog += 'E';
    return TimePoint{} + (engineReads++ == 0 ? 100ns : 900ns);
}

std::uint64_t FakeSDLRead() noexcept {
    readLog += 'S';
    return 5'000;
}

} // namespace

TEST_CASE("clock: SampleClockPair reads engine, then SDL, then engine") {
    readLog.clear();
    engineReads = 0;
    const S::ClockPair pair = S::SampleClockPair({&FakeEngineRead, &FakeSDLRead});
    CHECK_MESSAGE(readLog == "ESE", "[sc-read-order] the reads are engine, SDL, engine");
    CHECK_MESSAGE(pair.engine == TimePoint{} + 500ns, "[sc-sample-midpoint] the midpoint of its own reads");
    CHECK_MESSAGE(pair.sdlNs == 5'000, "[sc-sample-sdl] its own SDL read");
    CHECK_MESSAGE(pair.bracket == 800ns, "[sc-sample-bracket] its own bracket");
}

TEST_CASE("clock: an SDL time is placed relative to the pair") {
    const TimePoint engine = TimePoint{} + 1000s;
    const S::ClockPair pair{engine, 50'000'000'000ULL, 2us};
    const TimePoint ceiling = engine + 60s;
    CHECK_MESSAGE(S::ConvertSDLTime(50'000'000'000ULL - 3'000'000ULL, pair, ceiling) == engine - 3ms,
                  "[sc-before] an SDL time before the pair maps before the pair's engine time");
    CHECK_MESSAGE(S::ConvertSDLTime(50'000'000'000ULL + 7'000'000ULL, pair, ceiling) == engine + 7ms,
                  "[sc-after] an SDL time after the pair maps after it");
}

TEST_CASE("clock: promise (1), never later than the delivery time") {
    const TimePoint engine = TimePoint{} + 1000s;
    const S::ClockPair pair{engine, 50'000'000'000ULL, 2us};
    const TimePoint ceiling = engine + 5ms;
    CHECK_MESSAGE(S::ConvertSDLTime(50'000'000'000ULL + 9'000'000ULL, pair, ceiling) == ceiling,
                  "[sc-ceiling] a time later than the ceiling is the ceiling");
    CHECK_MESSAGE(S::ConvertSDLTime(50'000'000'000ULL + 5'000'000ULL, pair, ceiling) == ceiling,
                  "[sc-ceiling-equal] a time equal to the ceiling is the ceiling");
}

TEST_CASE("clock: promise (2), non-decreasing SDL times stay non-decreasing within one pump") {
    const TimePoint engine = TimePoint{} + 1000s;
    const S::ClockPair pair{engine, 50'000'000'000ULL, 2us};
    const TimePoint ceiling = engine + 4ms;
    TimePoint last{};
    bool ordered = true;
    for (std::uint64_t t = 50'000'000'000ULL - 2'000'000ULL; t <= 50'000'000'000ULL + 6'000'000ULL; t += 250'000ULL) {
        const TimePoint converted = S::ConvertSDLTime(t, pair, ceiling);
        if (converted < last) ordered = false;
        last = converted;
    }
    CHECK_MESSAGE(ordered, "[sc-monotone] converted times do not decrease");
}

TEST_CASE("clock: promise (3), SDL time 0 is the delivery time") {
    const TimePoint engine = TimePoint{} + 1000s;
    const S::ClockPair pair{engine, 50'000'000'000ULL, 2us};
    const TimePoint ceiling = engine + 40ms;
    CHECK_MESSAGE(S::ConvertSDLTime(0, pair, ceiling) == ceiling, "[sc-zero] SDL time 0 is the delivery time");
}

TEST_CASE("clock: an SDL clock that wrapped still maps by the modular difference") {
    const TimePoint engine = TimePoint{} + 1000s;
    const S::ClockPair pair{engine, 5ULL, 2us};
    const TimePoint ceiling = engine + 60s;
    CHECK_MESSAGE(S::ConvertSDLTime(UINT64_MAX - 4ULL, pair, ceiling) == engine - 10ns,
                  "[sc-wrap] the difference is taken modulo 2^64");
}

} // namespace GYO::Plugins::SDL
