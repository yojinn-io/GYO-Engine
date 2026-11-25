#pragma once

// The visibility Module (big-map declaration section 13, docs/architecture/target/v2/big-map-declaration.zh-Hant.md;
// after bevy_camera's check_visibility_cpu_culling, visibility/mod.rs:748, and check_visibility_ranges, range.rs:230).
// In the source World it mounts CheckVisibility: for every active, valid 3D camera, the entities with an Aabb or a
// VisibilityRange (render's Drawables.hpp, registered by world_render) that the camera does not see go into the
// camera's CulledEntities, in ascending EntityId, written only when the list changes. Queue3d leaves those out. It is
// the one place that decides which entities are culled at all: entities with neither component never are, and it
// decides nothing about RenderLayers.
//
// The source World needs transform, camera and world_render. Not carried by default (config/modules.csv): a World
// without it has no lists, so its cameras cull nothing.

#include <string>
#include <string_view>

#include "engine/orchestration/Module.hpp"
#include "engine/orchestration/ScheduleSkeleton.hpp"
#include "engine/orchestration/Setup.hpp"

namespace GYO::Engine::Visibility {

// Where CheckVisibility runs (WorldDescription::options; none given: the default skeleton). It orders itself after
// the transform Module's propagation and the camera Module's update, which run there by default.
struct VisibilityOptions final {
    std::string schedule = std::string(Orchestration::DefaultSchedules::frame);
    std::string stage = std::string(Orchestration::DefaultSchedules::postUpdate);
};

// The public set of the check (Bevy's VisibilitySystems::CheckVisibility), in VisibilityOptions::schedule.
inline constexpr std::string_view kCheckSet = "CheckVisibility";

class VisibilityModule final : public Orchestration::Module {
public:
    VisibilityModule() = default;

    void Build(Orchestration::WorldSetup& setup) override;
};

} // namespace GYO::Engine::Visibility
