#include <doctest/doctest.h>

#include "RetroFPS/Pvp/SessionSummary.hpp"

using namespace fps::pvp;

namespace {
// The counter fields of WeaponFeedbackObservation (PvpApplication.hpp).
struct Weapon final {
    std::uint64_t submittedActions{}, acceptedDecisions{}, rejectedDecisions{}, hitDecisions{}, localCooldownBlocks{};
};
} // namespace

// N-summary-counter-wrap (2026-10-16 Windows LAN): a Client that rejoined wrote
// actions_submitted=18446744073709551466. The weapon counters restart whenever
// the weapon feedback restarts, also within one frame while the phase stays
// Playing (a new connection generation), and the next window then subtracted
// the old baseline.
TEST_CASE("PvP summary counts restart with the weapon counters, also within one Playing frame") {
    Weapon weapon;
    SummaryBaseline baseline;
    weapon.submittedActions = 7;
    weapon.acceptedDecisions = 5;
    weapon.localCooldownBlocks = 3;
    CHECK(baseline.Take(SummaryCountsOf(weapon)) == SummaryCounts{7, 5, 0, 0, 3});
    // The connection generation changes between two summary windows.
    RestartWeaponCounters(weapon, baseline);
    weapon.submittedActions = 2;
    weapon.acceptedDecisions = 1;
    CHECK(baseline.Take(SummaryCountsOf(weapon)) == SummaryCounts{2, 1, 0, 0, 0});
    CHECK(baseline.Take(SummaryCountsOf(weapon)) == SummaryCounts{});
}
