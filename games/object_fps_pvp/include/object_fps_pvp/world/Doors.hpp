#pragma once

// The doors set (interact-door declaration section 3): the first kind of interaction target. A door is a scene object
// (world/Scene.hpp; its data in the scene file, not the map): both ends put it before the first frame with its shape and
// its first state. Only its state is decision data (DoorState: the Match changes it, a Client is sent it and predicts
// with it); its pose (Transform) follows from the state every step (PoseDoors), so the walls a body moves against and a
// shot meets are the door as it stands at that step, and the core's Systems need no change.
//
//   FixedUpdate/First      PoseDoors      both ends: each door's Transform from its DoorState (none: as it was)
//   FixedUpdate/PostUpdate OperateDoors   the authority, after JudgeInteractions: a door Interacted with turns, unless
//                                         its new pose would meet a living player's body

#include "object_fps_pvp/world/GameplaySets.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/transform/Transform.hpp"

#include <cstdint>

namespace object_fps_pvp::world {

// Open (1) or shut (0). Decision data, replicated (Codec 1).
struct DoorState final {
    std::uint8_t open = 0;
    friend bool operator==(const DoorState&, const DoorState&) = default;
};
void HashValue(K::Hasher&, const DoorState&);

// What the scene says of a door: its leaf, standing on the floor at the hinge when shut, runs `width` from the hinge to
// the right of a body facing `yaw` (PlayerBody's yaw), `height` up and `thickness` across (centred on that line);
// open, it is turned `swing` radians about the hinge's vertical (the sign says which way). Not decision data: both ends
// read it from the same scene (the scene digest).
struct DoorShape final {
    GYO::Engine::Math::Vec3 hinge{};
    float yaw = 0.0F;
    float width = 1.0F;
    float height = 2.0F;
    float thickness = 0.1F;
    float swing = 0.0F;
    GYO::Engine::Math::Vec3 color{};
};
void HashValue(K::Hasher&, const DoorShape&);

// The leaf's turn about the hinge's vertical when it is `turned` of the way open (0: shut, 1: open; a picture swinging
// between them is in between).
[[nodiscard]] GYO::Engine::Math::Quaternion DoorTurn(const DoorShape& shape, float turned);

// The leaf's pose for a state: its centre and its turn about +Y (DoorTurn of 0 or 1).
[[nodiscard]] GYO::Engine::Transform::Transform DoorPose(const DoorShape& shape, bool open);

inline constexpr const char* kDoorsSet = "doors";
[[nodiscard]] GameplaySet DoorsSet();

} // namespace object_fps_pvp::world
