#pragma once

// The physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, section 4.5; Design
// Invariants P3-1 to P3-6, section 8). Not carried by default (config/modules.csv). In every World it joins it
// registers Collider ("Physics3d.Collider") and nothing else: this batch has no simulation step, so no System.
//
// Its duties (P3-1): shapes, the pure queries, BoxSet, Collider and the spatial query functions. Not: a simulation
// step, rollback or authority registrations (an author's), character movement rules, drawing.

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Physics3d {

class Physics3dModule final : public Orchestration::Module {
public:
    Physics3dModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::Physics3d
