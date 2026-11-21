#pragma once

// What the rollback Module puts into a World that chose a strategy (rollback
// declaration 5.3, 5.7): the strategy to call and the step schedule; the
// rollbacks given up, sent as RollbackAbandoned (authority-rollback declaration
// 10.3: an E3 told as a message, as the time Module's TimeDiscarded; a product
// that wants it in the log forwards it with WorldLogging::Forward).

#include "engine/kernel/Hash.hpp"
#include "engine/rollback/RollbackStrategy.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace GYO::Engine::Rollback {

// The strategy is shared with the World description that chose it (as the authority Module's AuthorityRegistry
// holds its strategy): the World does not depend on the description outliving it.
struct RollbackRuntime {
    std::shared_ptr<RollbackStrategy> strategy;
    std::string stepSchedule;
};
// Not simulation state: nothing is hashed.
inline void HashValue(Kernel::Hasher&, const RollbackRuntime&) {}

// A retained message: a rollback to step requested was given up at step tick (further back than depth).
struct RollbackAbandoned {
    std::uint64_t requested = 0;
    std::uint64_t tick = 0;
    std::uint64_t depth = 0;
};
inline void HashValue(Kernel::Hasher& h, const RollbackAbandoned& v) {
    HashValue(h, v.requested);
    HashValue(h, v.tick);
    HashValue(h, v.depth);
}

// The notes the exclusive rollback System leaves for the System that sends them (an ExclusiveWorld writes no message).
struct RollbackPending {
    std::vector<RollbackAbandoned> abandoned;
};
inline void HashValue(Kernel::Hasher& h, const RollbackPending& v) {
    h.U64(v.abandoned.size());
    for (const RollbackAbandoned& note : v.abandoned) HashValue(h, note);
}

// The Systems of the rollback Module (RollbackModule.cpp).
::Engine::Base::Result<void, Kernel::KernelError> RecordStep(Exclusive world);
::Engine::Base::Result<void, Kernel::KernelError> RunRollback(Exclusive world);
::Engine::Base::Result<void, Kernel::KernelError> CorrectFrame(Exclusive world);
::Engine::Base::Result<void, Kernel::KernelError> SendRollbackNotes(Kernel::ResMut<RollbackPending> pending,
                                                                  Kernel::MessageWriter<RollbackAbandoned> notes);

} // namespace GYO::Engine::Rollback
