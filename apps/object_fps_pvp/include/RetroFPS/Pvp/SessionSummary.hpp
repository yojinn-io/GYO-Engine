#pragma once

#include <cstdint>

namespace fps::pvp {

// The weapon counters of the Client's once-per-second "PvP summary" log line,
// and the baseline each window subtracts. Diagnostics only.
struct SummaryCounts final {
    std::uint64_t submitted{}, accepted{}, rejected{}, hits{}, localBlocks{};
    bool operator==(const SummaryCounts&) const = default;
};

// Weapon is WeaponFeedbackObservation (PvpApplication.hpp); a template so that
// the domain tests need not link the presentation.
template <class Weapon>
[[nodiscard]] constexpr SummaryCounts SummaryCountsOf(const Weapon& weapon) noexcept {
    return {weapon.submittedActions, weapon.acceptedDecisions, weapon.rejectedDecisions, weapon.hitDecisions,
            weapon.localCooldownBlocks};
}

class SummaryBaseline final {
public:
    // The counters it measures restarted from zero.
    void Restart() noexcept { baseline_ = {}; }
    // The counts since the previous window; the current counts become the baseline.
    [[nodiscard]] SummaryCounts Take(const SummaryCounts& current) noexcept {
        const SummaryCounts delta{current.submitted - baseline_.submitted, current.accepted - baseline_.accepted,
                                  current.rejected - baseline_.rejected, current.hits - baseline_.hits,
                                  current.localBlocks - baseline_.localBlocks};
        baseline_ = current;
        return delta;
    }

private:
    SummaryCounts baseline_;
};

// Restarts the weapon counters (a new connection generation, a new viewed
// player, leaving Playing) together with the summary baseline, so a window
// never subtracts the counts of the previous session.
template <class Weapon>
void RestartWeaponCounters(Weapon& weapon, SummaryBaseline& baseline) noexcept {
    weapon = {};
    baseline.Restart();
}

} // namespace fps::pvp
