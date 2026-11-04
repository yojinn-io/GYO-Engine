#pragma once

#include <cstdint>

namespace fps::pvp {

// One id space for every entity of a Match (docs/object_fps_pvp/entity-model
// E-1): players, AI, bosses, projectiles, placed items. Kept 64-bit inside
// the process (pv8-seq-cpp-types (b)); the wire carries uint32.
using EntityId = std::uint64_t;
// The role "entity with a connection": its value is the player_id the
// Gateway allotted (E-2). An alias, not a second key.
using PlayerId = EntityId;
// No entity: a rejected Join, a missed shot's target, the Gateway's own stream.
inline constexpr EntityId NoEntity = 0;

} // namespace fps::pvp
