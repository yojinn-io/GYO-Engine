#pragma once

// The official rollback strategy, triggered by state (rollback declaration 5.5;
// authority-rollback declaration 5.2, 5.3 and the entry shape of 5.5): the
// shape of lightyear's prediction. Its internals are not promised (L21).
//   - Record: after each step, per type that may roll back, the cells changed
//     since the last record and the cells gone; the alive set whole; the
//     inputs' values (History.hpp).
//   - Decide (the user's (a), 2026-11-20): when the inbox's confirmed point C
//     moves on, compare at C, per entity and component, the authoritative
//     values with the history; a difference (or mode Always) goes back to C.
//     C further back than the depth: the rollback is given up (Abandon).
//   - Restore: the state at C from the history, the authoritative values over
//     it, put back with the kernel's Restore (one stamp, hooks after).
//   - Resimulate: Tick back to C, then per step the inputs' values written
//     back and one fixed step run, up to now; "resimulating" lowered.
//   - Correct: nothing in this batch (no correction function is registered;
//     the view correction comes with the Client batch).

#include "engine/rollback/RollbackStrategy.hpp"

#include <cstdint>
#include <string_view>

namespace GYO::Engine::Rollback {

enum class RollbackMode : std::uint8_t {
    // Go back whenever the confirmed point moves on.
    Always,
    // Go back when an authoritative value at the confirmed point differs (the default).
    Check,
    // Never go back (the confirmed point is still followed).
    Disabled,
};

struct StateRollbackSettings {
    // The deepest rollback, in steps (rollback declaration 5.3: lightyear's 20; authority-rollback 5.4.4).
    std::uint32_t depth = 20;
    RollbackMode mode = RollbackMode::Check;
};

class StateRollbackStrategy : public RollbackStrategy {
public:
    explicit StateRollbackStrategy(StateRollbackSettings settings = {}) : settings_(settings) {}

    [[nodiscard]] std::string_view Name() const override { return "StateRollbackStrategy"; }
    void Check(CheckContext& context) override;
    ::GYO::Engine::Base::Result<void, Kernel::KernelError> Record(RecordContext& context) override;
    ::GYO::Engine::Base::Result<void, Kernel::KernelError> Decide(DecideContext& context) override;
    ::GYO::Engine::Base::Result<void, Kernel::KernelError> Restore(RestoreContext& context) override;
    ::GYO::Engine::Base::Result<void, Kernel::KernelError> Resimulate(ResimulateContext& context) override;
    ::GYO::Engine::Base::Result<void, Kernel::KernelError> Correct(CorrectContext& context) override;
    [[nodiscard]] std::uint32_t DefaultInputLength() const noexcept override { return settings_.depth; }

    [[nodiscard]] const StateRollbackSettings& Settings() const noexcept { return settings_; }

private:
    StateRollbackSettings settings_;
};

} // namespace GYO::Engine::Rollback
