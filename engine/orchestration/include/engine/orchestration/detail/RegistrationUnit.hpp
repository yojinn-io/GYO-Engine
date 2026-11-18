#pragma once

#include "engine/kernel/Registry.hpp"

namespace GYO::Engine::Orchestration {

class ProductSetup;
class WorldSetup;
class WorldFinish;
class WorldCheck;

namespace Detail {

// The base the two registration unit types share (declaration section 3.3).
// Authors derive from Module or Plugin, never from this. A unit has no Name():
// its name, file and whether it goes into a World come from its register row,
// which the generated table pairs with the instance (section 3.2).
//
// The lifecycle the orchestration layer drives (O-2): Configure once per
// Product before any World is built; for each World the unit joins, in unit
// order, Build, then the kernel's BuildPlans, then Finish, then (after every
// unit's Finish) Check. A World whose BuildPlans failed runs no Finish and no
// Check. Replacing a World runs Build, Finish and Check for it again (a unit
// keeps what it records per World by WorldName() and resets it in Build);
// Configure never runs again. A dynamic insert runs none of them (they belong
// to the unit's own lifecycle; v8-arch-gyo2-dynamic-insert-no-lifecycle-rerun).
class RegistrationUnit {
public:
    virtual ~RegistrationUnit();
    RegistrationUnit(const RegistrationUnit&) = delete;
    RegistrationUnit& operator=(const RegistrationUnit&) = delete;

    // Product level, once, before any World is built: insert into the game
    // initialization list, install the runner.
    virtual void Configure(ProductSetup& setup);
    // Once per World the unit joins: register through setup.Registrar(), read
    // its options, insert into the World's frame list, give prepare functions,
    // mark Resources to carry over.
    virtual void Build(WorldSetup& setup) = 0;
    // After the World's Plans are built (version 1): read the read-only
    // registry, report errors; cannot register.
    virtual void Finish(const Kernel::FrozenRegistry& registry, WorldFinish& context);
    // After every unit's Finish of the World: a strategy's checks.
    virtual void Check(const Kernel::FrozenRegistry& registry, WorldCheck& context);

protected:
    RegistrationUnit() = default;
};

} // namespace Detail
} // namespace GYO::Engine::Orchestration
