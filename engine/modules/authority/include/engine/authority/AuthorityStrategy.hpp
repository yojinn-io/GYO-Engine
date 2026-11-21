#pragma once

// The authority strategy interface (authority declaration
// docs/architecture/target/v2/authority-module-declaration.zh-Hant.md, 4.4, the
// contract; the user's v8-arch-gyo2-strategy-interface-impl): an abstract base
// class with virtual functions. The official ServerAuthorityStrategy and an
// author's go through the same socket, with no privilege.
//
// Each hook takes one context object of its own, so a hook can be given more
// later without its signature changing (authority-rollback declaration 10.1).
// The registration and fingerprint contexts come with the registrations and
// the official strategy (step 5); the startup check's is here. Digest and
// OnMismatch wait for the network Module's declaration (authority declaration
// 4.4: their shape is not in this contract).
//
// A strategy object is chosen per World (AuthorityOptions in the World
// description's options) and holds no per-World state (authority-rollback
// declaration 4.2).

#include "engine/kernel/Plan.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/orchestration/Setup.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace GYO::Engine::Authority {

// What a strategy's startup check reads, and where it reports (after every
// unit's Finish, before the World is built; O-5: with the authority Module's
// string codes, AU-E...).
class CheckContext {
public:
    CheckContext(const Kernel::FrozenRegistry& registry, Orchestration::WorldCheck& check)
        : registry_(&registry), check_(&check) {}

    [[nodiscard]] const Kernel::FrozenRegistry& Registry() const noexcept { return *registry_; }
    [[nodiscard]] const Orchestration::WorldCheck& World() const noexcept { return *check_; }
    void Report(std::string code, std::string message) { check_->Report(std::move(code), std::move(message)); }

private:
    const Kernel::FrozenRegistry* registry_;
    Orchestration::WorldCheck* check_;
};

// The contexts of the registration hook and of the consistency fingerprint (step 5).
class RegisterContext;
class FingerprintContext;

class AuthorityStrategy {
public:
    virtual ~AuthorityStrategy() = default;

    // Its name and version: in the consistency fingerprint (authority declaration 5.5).
    [[nodiscard]] virtual std::string_view Name() const = 0;
    [[nodiscard]] virtual std::uint32_t Version() const = 0;
    // At an authority registration (always before the Plans are built): what the strategy registers on the
    // author's behalf, in the kernel and in the rollback Module (authority declaration 4.3).
    virtual void OnRegister(RegisterContext& context) = 0;
    // Startup: reads the read-only registry and the registrations; reports what cannot hold.
    virtual void Check(CheckContext& context) = 0;
    // Startup, before connecting: what both ends must have in common (authority declaration 5.5).
    [[nodiscard]] virtual Kernel::Fingerprint ConsistencyFingerprint(FingerprintContext& context) = 0;

protected:
    AuthorityStrategy() = default;
};

// The authority Module's setting in one World (WorldDescription::options).
struct AuthorityOptions {
    std::shared_ptr<AuthorityStrategy> strategy;
};

// The peer-to-peer strategy (a fighting game; placeholder only, authority
// declaration 4.6): its startup check reports that it is not implemented
// (AU-E1), so a World never runs with it.
class PeerAuthorityStrategy final : public AuthorityStrategy {
public:
    [[nodiscard]] std::string_view Name() const override { return "PeerAuthorityStrategy"; }
    [[nodiscard]] std::uint32_t Version() const override { return 0; }
    void OnRegister(RegisterContext& context) override;
    void Check(CheckContext& context) override;
    [[nodiscard]] Kernel::Fingerprint ConsistencyFingerprint(FingerprintContext& context) override;
};

} // namespace GYO::Engine::Authority
