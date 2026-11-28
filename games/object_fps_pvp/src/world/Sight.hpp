#pragma once

// What the shots and the interactions share in the Match's step (interact-door declaration 2.3): both cast the eye's
// view at the colliders, stopped by the floor plane, from the input a player has by the step.

#include "object_fps_pvp/world/Components.hpp"

#include "engine/kernel/Kernel.hpp"
#include "engine/math/geometry/Ray.hpp"
#include "engine/physics_3d/Collider.hpp"
#include "engine/transform/Transform.hpp"

#include <cstdint>
#include <optional>

namespace object_fps_pvp::world {

// The colliders a body and a ray meet: every collider not under a parent (the map's walls, a scene's doors).
using Walls = K::Query<K::Read<GYO::Engine::Physics3d::Collider>, K::Read<GYO::Engine::Transform::Transform>, K::Without<K::ChildOf>>;

// The ray from a body's eye along its view.
[[nodiscard]] Math::Ray ShotRay(const GameRules& rules, const PlayerBody& body);

// Where the ray meets the arena's floor plane (a solid half-space below it): 0 from inside it, nothing when the arena
// has none or the ray does not go down to it.
[[nodiscard]] std::optional<float> FloorAlong(const GameRules& rules, const Math::Ray& ray);

// Before the start the players wait: their input is none (they neither move, turn, fire nor interact). By the step, so
// a Client simulating the steps around the start again gives each the Match's answer.
[[nodiscard]] const PlayerInput& Playable(const PlayerInput& input, const MatchInfo& info, std::uint64_t tick);

} // namespace object_fps_pvp::world
