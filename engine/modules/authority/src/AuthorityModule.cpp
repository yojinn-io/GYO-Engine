#include "engine/authority/AuthorityModule.hpp"

namespace GYO::Engine::Authority {

void AuthorityModule::Build(Orchestration::WorldSetup&) {}

void AuthorityModule::Check(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& context) {
    const AuthorityOptions* options = context.Options<AuthorityOptions>();
    if (options == nullptr || !options->strategy) return;
    CheckContext check(registry, context);
    options->strategy->Check(check);
}

void PeerAuthorityStrategy::Check(CheckContext& context) {
    context.Report("AU-E1", "PeerAuthorityStrategy is not implemented yet; a strategy of your own can implement "
                            "AuthorityStrategy and be chosen in AuthorityOptions");
}

// Never reached: Check refuses every World that chooses this strategy.
void PeerAuthorityStrategy::OnRegister(RegisterContext&) {}
Kernel::Fingerprint PeerAuthorityStrategy::ConsistencyFingerprint(FingerprintContext&) { return {}; }

} // namespace GYO::Engine::Authority
