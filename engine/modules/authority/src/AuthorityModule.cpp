#include "engine/authority/AuthorityModule.hpp"

#include "engine/rollback/Registrations.hpp"

#include <utility>

namespace GYO::Engine::Authority {

void AuthorityModule::Build(Orchestration::WorldSetup& setup) {
    AuthorityRegistrations& table = setup.Buffer().Create<AuthorityRegistrations>(setup.Buffer());
    setup.Registrar().Resource<AuthorityRegistry>({.name = "AuthorityRegistry"});
    const AuthorityOptions* options = setup.Options<AuthorityOptions>();
    std::shared_ptr<AuthorityStrategy> strategy = options != nullptr ? options->strategy : nullptr;
    // When the World is prepared: the registrations go into it (the digest and the fingerprint read them there,
    // so a failed replacement leaves the old World's as they were).
    setup.Prepare([&table, strategy](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        const auto list = [](const auto& kept) { return std::vector(kept.begin(), kept.end()); };
        world.SetResource(AuthorityRegistry{list(table.Data()), list(table.Marks()), list(table.Ingress()), list(table.Runs()),
                                            list(table.OneEnd()), strategy});
        return {};
    });
}

void AuthorityModule::Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) {
    const AuthorityRegistrations* table = context.Buffer().Find<AuthorityRegistrations>();
    const AuthorityOptions* options = context.Options<AuthorityOptions>();
    if (options == nullptr || !options->strategy) {
        // A World with authority registrations chooses a strategy (authority declaration 4.7); one without (a
        // rendering World) need not.
        if (table != nullptr && table->Any()) {
            context.Report("AU-E2", "World '" + context.WorldName() +
                                        "' has authority registrations but chose no authority strategy (AuthorityOptions)");
        }
        return;
    }
    const Rollback::RollbackRegistrations* rollbackTable = context.Buffer().Find<Rollback::RollbackRegistrations>();
    const Rollback::RollbackRegistry rollback = rollbackTable != nullptr ? Rollback::Resolve(*rollbackTable).registry
                                                                         : Rollback::RollbackRegistry{};
    // The table is made by Build in every World the Module is built into.
    GYO_ASSERT(table != nullptr && "AuthorityModule::Check: no registrations table (Build did not run)");
    CheckContext check(registry, context, *table, rollback);
    options->strategy->Check(check);
}

void PeerAuthorityStrategy::Check(CheckContext& context) {
    context.Report("AU-E1", "PeerAuthorityStrategy is not implemented yet; a strategy of your own can implement "
                            "AuthorityStrategy and be chosen in AuthorityOptions");
}

// The types still go into the kernel and an entry is still added, so the World reaches Check and AU-E1 says why
// it does not run (not the kernel's R-E4).
void PeerAuthorityStrategy::OnRegister(RegisterContext& context) {
    if (context.Data() != nullptr) context.RegisterInKernel();
    if (context.Ingress() != nullptr) context.AddIngress();
}
// Never reached: Check refuses every World that chooses this strategy.
Kernel::Fingerprint PeerAuthorityStrategy::ConsistencyFingerprint(FingerprintContext&) { return {}; }

} // namespace GYO::Engine::Authority
