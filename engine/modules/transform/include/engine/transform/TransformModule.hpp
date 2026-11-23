#pragma once

// The transform Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 4.1; Design
// Invariants TF-1 to TF-4 in docs/architecture/target/v2/transform.zh-Hant.md). Carried by default
// (config/modules.csv). In every World it joins it registers Transform and GlobalTransform and keeps them equal for
// entities without a parent (the parent hierarchy is batch X2):
// - the initial value of GlobalTransform from Transform: right at the sync point Transform appears (Bevy's
//   require(GlobalTransform), with the right value instead of the identity);
// - an OnAdd hook of Transform that puts GlobalTransform on an entity that has none, for the path the initial value
//   does not take (ExclusiveWorld::Restore derives no initial value, DI-KS 12);
// - SyncTransforms (Bevy's sync_simple_transforms) after Transform is written, in the set kPropagateSet.
//
// Its duties (TF-1): these and nothing else. Not: parents (the kernel's relationship, batch KR), registering
// Transform for rollback, authority or replication (an author's), interpolation, drawing.

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Transform {

// Where SyncTransforms runs (WorldDescription::options; none given: the default skeleton). A World whose skeleton
// is off names its own; a missing schedule or stage is the kernel's D-E1.
struct TransformOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::postUpdate);
};

// The public set of the propagation (Bevy's TransformSystems::Propagate), in TransformOptions::schedule. A System
// that reads GlobalTransform in the same stage orders itself after it.
inline constexpr std::string_view kPropagateSet = "TransformPropagate";

class TransformModule final : public Orchestration::Module {
public:
    TransformModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::Transform
