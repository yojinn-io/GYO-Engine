#pragma once

// The default entity allotter Module (world-gen declaration
// docs/architecture/target/v2/world-gen-declaration.zh-Hant.md, section 5.10; Design Invariants EI-1 to EI-3 in
// docs/architecture/target/v2/entity-ids.zh-Hant.md; v8-arch-gyo2-default-allotter-module). Carried by default
// (config/modules.csv), so an author can Spawn without declaring an allotter: in every World it joins it
// registers RangeAllotter as the World's allotter and, before the first Run, puts the range of EntityIdsOptions
// unless a value is already there (one carried over; O-7). A product that wants another strategy leaves this
// Module out of that World and declares its own type; declaring a second one with it is the kernel's RE2.
//
// Its duties (EI-1): register the allotter, put the default range. Not: recycling ids, rollback registration
// (an author's, rollback R-2 premise 6), network id mapping, saves. The kernel and the orchestration layer know
// nothing of it.

#include "engine/entity_ids/RangeAllotter.hpp"
#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::EntityIds {

// The range of one World (WorldDescription::options; none given: these defaults). 0 is left to authors as
// "no entity"; the last id keeps the generation half of the key 0 (DI-KS 4: low 32 bits index).
struct EntityIdsOptions final {
    EntityId first = 1;
    EntityId last = 0xFFFF'FFFFULL;
};

class EntityIdsModule final : public Orchestration::Module {
public:
    EntityIdsModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::EntityIds
