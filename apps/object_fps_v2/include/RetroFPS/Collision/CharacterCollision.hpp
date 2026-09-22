#pragma once
#include "RetroFPS/Math/Vector.hpp"
#include "RetroFPS/World/GridMap.hpp"
#include "RetroFPS/World/WorldSettings.hpp"
#include "engine/collision/Collision.hpp"
#include <span>
#include <vector>
namespace fps {
// Product-local world composition and kinematic policy; Engine owns shape queries.
[[nodiscard]] std::vector<Engine::Collision::Aabb>
BuildWorldCollisionBoxes(const GridMap &map, const WorldSettings &settings);
// constrainToFloor preserves the body's initial feet height and resolves contact
// in the horizontal plane; player movement keeps the default full 3D response.
[[nodiscard]] Float3 MoveCharacterBody(Engine::Collision::VerticalCapsule body, Float3 displacement,
                                       std::span<const Engine::Collision::Aabb> walls,
                                       std::span<const Engine::Collision::VerticalCapsule> actors,
                                       bool constrainToFloor = false);
[[nodiscard]] bool
CanPlaceCharacterBody(const Engine::Collision::VerticalCapsule &body,
                      std::span<const Engine::Collision::Aabb> walls,
                      std::span<const Engine::Collision::VerticalCapsule> actors);
} // namespace fps
