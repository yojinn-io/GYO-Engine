#pragma once

// The rollback strategy interface (rollback declaration 5.3, the contract; the
// user's v8-arch-gyo2-strategy-interface-impl): an abstract base class with
// virtual functions. The official strategy and an author's go through the same
// socket, with no privilege; an author may override some hooks of the official
// one or write all of them.
//
// Each hook takes one context object of its own: what the hook reads and does
// goes through it, so a hook can be given more later without its signature
// changing (authority-rollback declaration 10.1). The run-time contexts are
// defined with the official strategy (step 4); the startup check's is here.
//
// A strategy object is chosen per World (RollbackOptions in the World
// description's options) and holds no per-World state: the description is
// reused by ReplaceWorld; a World's state lives in that World's Resources
// (authority-rollback declaration 4.2).

#include "engine/base/Result.hpp"
#include "engine/kernel/KernelError.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/rollback/Registrations.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace GYO::Engine::Rollback {

// What a strategy's startup check reads, and where it reports (after every
// unit's Finish, before the World is built; O-5: with the rollback Module's
// string codes, RB-E...).
class CheckContext {
public:
    CheckContext(const Kernel::FrozenRegistry& registry, const RollbackRegistry& registrations,
                 Orchestration::WorldCheck& check)
        : registry_(&registry), registrations_(&registrations), check_(&check) {}

    [[nodiscard]] const Kernel::FrozenRegistry& Registry() const noexcept { return *registry_; }
    [[nodiscard]] const RollbackRegistry& Registrations() const noexcept { return *registrations_; }
    [[nodiscard]] const std::string& WorldName() const noexcept { return check_->WorldName(); }
    void Report(std::string code, std::string message) { check_->Report(std::move(code), std::move(message)); }

private:
    const Kernel::FrozenRegistry* registry_;
    const RollbackRegistry* registrations_;
    Orchestration::WorldCheck* check_;
};

// The run-time hooks' contexts (defined with the official strategy, step 4).
class RecordContext;
class DecideContext;
class RestoreContext;
class ResimulateContext;
class CorrectContext;

class RollbackStrategy {
public:
    virtual ~RollbackStrategy() = default;

    // Its name, for reports and for the authority Module's fingerprint.
    [[nodiscard]] virtual std::string_view Name() const = 0;
    // Startup: reads the read-only registry and the rollback registrations; reports what cannot hold.
    virtual void Check(CheckContext& context) = 0;
    // After each step of a step schedule: keeps what the step left.
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Record(RecordContext& context) = 0;
    // Before the step schedule each frame: reads what came from outside and decides whether, and to which step,
    // to go back; deciding to, it raises "resimulating".
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Decide(DecideContext& context) = 0;
    // After deciding: puts the World back to that step (keeping first what a correction needs).
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Restore(RestoreContext& context) = 0;
    // After restoring: runs the step schedule again up to now, then lowers "resimulating".
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Resimulate(ResimulateContext& context) = 0;
    // Each frame, on the view side: what the frame adds to hide the difference a resimulation made.
    virtual ::Engine::Base::Result<void, Kernel::KernelError> Correct(CorrectContext& context) = 0;

protected:
    RollbackStrategy() = default;
};

// The rollback Module's setting in one World (WorldDescription::options). No
// strategy: the World does not roll back, and that is no error (a Match); the
// registrations are still made and readable (rollback declaration 5.5 point 10).
struct RollbackOptions {
    std::shared_ptr<RollbackStrategy> strategy;
};

// The peer-to-peer rollback strategy (a fighting game; named by the user,
// placeholder only, rollback declaration 5.6): its startup check reports that
// it is not implemented (RB-E6), so a World never runs with it.
class PeerRollbackStrategy final : public RollbackStrategy {
public:
    [[nodiscard]] std::string_view Name() const override { return "PeerRollbackStrategy"; }
    void Check(CheckContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Record(RecordContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Decide(DecideContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Restore(RestoreContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Resimulate(ResimulateContext& context) override;
    ::Engine::Base::Result<void, Kernel::KernelError> Correct(CorrectContext& context) override;
};

} // namespace GYO::Engine::Rollback
