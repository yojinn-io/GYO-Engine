#pragma once

// The authority Module (authority declaration
// docs/architecture/target/v2/authority-module-declaration.zh-Hant.md; the
// authority-rollback declaration, section 4): who decides the state of the
// game, chosen by the author per World as a strategy. Its duties (authority
// declaration 4.1): the AuthorityStrategy interface and the official
// ServerAuthorityStrategy; the registrations of decision data; the startup
// checks of the chosen strategy; the consistency fingerprint and the digest.
// Not its: rollback (the rollback Module, read through its public
// registrations), the tick (the time Module), the network.
//
// Step 3 of the batch: the unit and the strategy socket. The registrations and
// the official strategy come in step 5.

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Authority {

class AuthorityModule final : public Orchestration::Module {
public:
    AuthorityModule() = default;

    // Nothing yet: the registrations come in step 5.
    void Build(Orchestration::WorldSetup& setup) override;
    void Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) override;
};

} // namespace GYO::Engine::Authority
