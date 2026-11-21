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
// Its headers: AuthorityStrategy.hpp (the socket and the contexts),
// Registrations.hpp (AuthorityEntry), ServerAuthorityStrategy.hpp (the official
// strategy), Consistency.hpp (the fingerprint, the digest, the G-5 export).

#include "engine/authority/AuthorityStrategy.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Authority {

class AuthorityModule final : public Orchestration::Module {
public:
    AuthorityModule() = default;

    // The table of registrations in the ModuleBuffer (AuthorityEntry appends to it) and, when the World is
    // prepared, the Resource AuthorityRegistry.
    void Build(Orchestration::WorldSetup& setup) override;
    // AU-E2 when the World has registrations and no strategy; otherwise the strategy's Check.
    void Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) override;
};

} // namespace GYO::Engine::Authority
