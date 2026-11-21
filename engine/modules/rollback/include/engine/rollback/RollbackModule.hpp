#pragma once

// The rollback Module (rollback declaration
// docs/architecture/target/v2/rollback-module-declaration.zh-Hant.md; the
// authority-rollback declaration, section 5): its duty is to put part of a
// World's state back to an earlier step and run it forward to now again. Its
// duties (rollback declaration 5.1): (1) the RollbackStrategy interface and the
// official strategy; (2) the public registrations (Registrations.hpp); (3) the
// startup checks; (4) the flow of record, decide, restore, resimulate, correct
// (the strategy's). Not its: the tick, the clock and "resimulating" (the time
// Module); who has authority and the digest (the authority Module); the network.
//
// In each World it is built into: Build creates the registration table in the
// ModuleBuffer and registers the Resource RollbackRegistry; the World's prepare
// function puts the resolved registrations into it; Check reports what cannot
// hold (RB-E...) and runs the strategy's startup check.

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/rollback/Registrations.hpp"
#include "engine/rollback/RollbackStrategy.hpp"

namespace GYO::Engine::Rollback {

class RollbackModule final : public Orchestration::Module {
public:
    RollbackModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
    void Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) override;
};

} // namespace GYO::Engine::Rollback
