#pragma once

// The official server authority strategy (authority declaration 4.4, 5; the
// authority-rollback declaration 4.4): the decision data is the server's.
//
//   OnRegister  registers on the author's behalf: a decision type into the
//       kernel and, unless it is an input or the author chose otherwise, into the
//       rollback Module as one that may roll back; an entry System skipped while
//       resimulating, then added (DI-A 11).
//   Check (startup; authority declaration 5.4, 5.5, 5.11; codes AU-E3...AU-E9):
//       AU-E3  the World allows dynamic insertion;
//       AU-E4  a mark, an entry, Runs or OneEndOnly names nothing in the registry;
//       AU-E5  the tick is written by a System, hook or observer the time Module
//              does not list (TickWriters);
//       AU-E6  (Z6) a System of a resimulated schedule, not skipped while
//              resimulating, writes decision data that neither rolls back nor
//              is Derived; a hook or observer writes
//              decision data that does not roll back, in a World with a step
//              schedule. The tick is the exception; messages are left out;
//       AU-E7  (Z8) a decision input registered as rollback state;
//       AU-E8  (D-E15) an entry in a resimulated schedule not skipped while
//              resimulating;
//       AU-E9  a schedule a Runs System in a resimulated schedule runs is not
//              registered in the rollback Module as nested.
//   ConsistencyFingerprint  what both ends must have in common (authority
//       declaration 5.5), numbered in authority registration order.
//
// It counts no writers and checks no reads or writes of decision data (P13,
// P14). What a startup check sees: typed parameters, ExclusiveWorld<Writes<...>>
// annotations, hooks and observers, and the components a sync point fills
// through Registrar::InitialValue; untyped writes and unannotated exclusive
// writes are not seen (L3, L7).

#include "engine/authority/AuthorityStrategy.hpp"

namespace GYO::Engine::Authority {

class ServerAuthorityStrategy : public AuthorityStrategy {
public:
    ServerAuthorityStrategy() = default;

    [[nodiscard]] std::string_view Name() const override { return "ServerAuthorityStrategy"; }
    [[nodiscard]] std::uint32_t Version() const override { return 1; }
    void OnRegister(RegisterContext& context) override;
    void Check(CheckContext& context) override;
    [[nodiscard]] Kernel::Fingerprint ConsistencyFingerprint(FingerprintContext& context) override;
};

} // namespace GYO::Engine::Authority
