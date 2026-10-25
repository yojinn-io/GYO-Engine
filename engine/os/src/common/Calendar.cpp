#include "Calendar.hpp"

#include <cstdint>

namespace Engine::Os::Detail {

namespace {

// Days from 1970-01-01 to year-month-day in the proleptic Gregorian calendar
// (H. Hinnant's days_from_civil).
std::int64_t DaysFromCivil(std::int64_t year, const unsigned month, const unsigned day) noexcept {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const auto yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468;
}

} // namespace

Process::LocalTime LocalTimeFromCalendar(const std::tm& local, const std::time_t time) noexcept {
    const int year = local.tm_year + 1900;
    const int month = local.tm_mon + 1;
    // The local fields read as if they were UTC, minus the instant itself.
    const std::int64_t asUtc = DaysFromCivil(year, static_cast<unsigned>(month), static_cast<unsigned>(local.tm_mday)) *
                                   86400 +
                               std::int64_t{local.tm_hour} * 3600 + std::int64_t{local.tm_min} * 60 + local.tm_sec;
    return {.year = year,
            .month = month,
            .day = local.tm_mday,
            .hour = local.tm_hour,
            .minute = local.tm_min,
            .second = local.tm_sec,
            .utcOffset = std::chrono::seconds{asUtc - static_cast<std::int64_t>(time)}};
}

} // namespace Engine::Os::Detail
