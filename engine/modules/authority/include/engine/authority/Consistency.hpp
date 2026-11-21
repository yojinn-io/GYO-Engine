#pragma once

// What both ends compare (authority declaration 5.5, 5.6, 5.9): read from a
// built World whose authority Module has registrations and a strategy.
//
//   ConsistencyFingerprintOf  the World's strategy's ConsistencyFingerprint.
//   Digest                    the official digest: for each decision component
//       and resource (not a message, not digestExclude; the types marked by
//       type included, those marked by name not in this batch), each value's
//       hash with the type's authority registration number, summed mod 2^64
//       (order free; equal cells do not cancel). No entity id (authority-rollback
//       declaration 4.4). Its shape is not in the contract (authority 4.4).
//   CollectedSystems          the Systems, hooks and observers the consistency
//       fingerprint takes: those seen at startup to write decision data, less
//       OneEndOnly (the dataset report's list).
//   ExportAuthoritySources    G-5: the source files of what writes decision data
//       (Systems, hooks, observers, the functions of Registrar::InitialValue for
//       a decision component), as gyo.authority-sources v1 JSON, for the
//       product's scan (tests/common/authority_scan).

#include "engine/authority/Registrations.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Plan.hpp"
#include "engine/kernel/World.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace GYO::Engine::Authority {

enum class AuthorityFailureCode : std::uint8_t {
    // The World has no AuthorityRegistry or no strategy (the authority Module was not built into it, or none chosen).
    NoAuthority = 1,
    // G-5: a registration site is outside the repository root or the owner root.
    SourceOutsideRoot = 2,
};

[[nodiscard]] inline std::string_view ToString(AuthorityFailureCode code) noexcept {
    switch (code) {
    case AuthorityFailureCode::NoAuthority: return "NoAuthority";
    case AuthorityFailureCode::SourceOutsideRoot: return "SourceOutsideRoot";
    }
    return "Unknown";
}

// What a function of the authority Module cannot do (an E2: returned, the caller decides).
struct AuthorityFailure {
    AuthorityFailureCode code;
    std::string message;
    std::string detail;

    AuthorityFailure(AuthorityFailureCode failureCode, std::string failureMessage, std::string failureDetail = {})
        : code(failureCode), message(std::move(failureMessage)), detail(std::move(failureDetail)) {}
};

[[nodiscard]] ::Engine::Base::Result<Kernel::Fingerprint, AuthorityFailure> ConsistencyFingerprintOf(const Kernel::World& world);
[[nodiscard]] ::Engine::Base::Result<std::uint64_t, AuthorityFailure> Digest(const Kernel::World& world);

// What writes decision data, seen at startup (the consistency fingerprint's definition, authority declaration 5.5).
struct Collected {
    // System names, in registrationSeq order.
    std::vector<std::string> systems;
    // Hook names, in registration order.
    std::vector<std::string> hooks;
    // Observer names, in registration order.
    std::vector<std::string> observers;
};
// Decision keys: the kernel ids of the decision data, by kind (the tick and other marks included).
struct DecisionKeys {
    std::vector<Kernel::AccessKey> keys;
    [[nodiscard]] bool Has(const Kernel::AccessKey& key) const;
};
[[nodiscard]] DecisionKeys DecisionKeysOf(const Kernel::FrozenRegistry& registry, const std::vector<DataRecord>& data,
                                          const std::vector<MarkRecord>& marks);
// Every key an access set writes, as the authority's checks see it: immediate writes, deferred writes,
// ExclusiveWorld<Writes<...>> annotations, and the components a sync point fills from the requests it inserts
// (Registrar::InitialValue, through chains).
[[nodiscard]] std::vector<Kernel::AccessKey> WrittenKeys(const Kernel::FrozenRegistry& registry, const Kernel::AccessSet& access);
[[nodiscard]] Collected CollectedOf(const Kernel::FrozenRegistry& registry, const AuthorityRegistry& registrations);

struct AuthoritySourcesOptions {
    // Absolute path of the repository root; registration sites are made relative to it.
    std::string repositoryRoot;
    std::string ownerRoot;
    std::vector<std::string> includeRoots;
    std::string repoCommit;
};
[[nodiscard]] ::Engine::Base::Result<std::string, AuthorityFailure> ExportAuthoritySources(
    const Kernel::World& world, const AuthoritySourcesOptions& options);

} // namespace GYO::Engine::Authority
