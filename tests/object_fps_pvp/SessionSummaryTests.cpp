#include <doctest/doctest.h>

#include "RetroFPS/Pvp/SessionSummary.hpp"

#include <array>

using namespace fps::pvp;

namespace {
// The counter fields of WeaponFeedbackObservation (PvpApplication.hpp).
struct Weapon final {
    std::uint64_t submittedActions{}, acceptedDecisions{}, rejectedDecisions{}, hitDecisions{}, localCooldownBlocks{};
    std::array<std::uint64_t, ShotRejectionCount> rejectionsByReason{};
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

// log-contract: the summary splits rejections by the Match's reason, with the
// same window and restart rule as the totals.
TEST_CASE("PvP summary counts rejections by reason per window") {
    Weapon weapon;
    SummaryBaseline baseline;
    const auto cooldown = static_cast<std::size_t>(ShotRejection::Cooldown);
    const auto dead = static_cast<std::size_t>(ShotRejection::Dead);
    weapon.rejectionsByReason[cooldown] = 2;
    CHECK(baseline.Take(SummaryCountsOf(weapon)).rejectedBy[cooldown] == 2);
    weapon.rejectionsByReason[dead] = 1;
    const auto second = baseline.Take(SummaryCountsOf(weapon));
    CHECK(second.rejectedBy[cooldown] == 0);
    CHECK(second.rejectedBy[dead] == 1);
    RestartWeaponCounters(weapon, baseline);
    weapon.rejectionsByReason[cooldown] = 1;
    CHECK(baseline.Take(SummaryCountsOf(weapon)).rejectedBy[cooldown] == 1);
    CHECK(ShotRejectionName(ShotRejection::EmptyMagazine) == "empty_magazine");
}

