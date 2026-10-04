#pragma once
#include "engine/collision/Collision.hpp"
#include "engine/math/linear/Vec3.hpp"
#include <span>
namespace fps {
// Pure product-local kinematic policy; Engine owns shape queries.
// constrainToFloor preserves the body's initial feet height and resolves contact
// in the horizontal plane; player movement keeps the default full 3D response.
[[nodiscard]] Engine::Math::Vec3 MoveCharacterBody(Engine::Collision::VerticalCapsule body, Engine::Math::Vec3 displacement,
                                       std::span<const Engine::Math::Aabb> walls,
                                       std::span<const Engine::Collision::VerticalCapsule> actors,
                                       bool constrainToFloor = false);
[[nodiscard]] bool
CanPlaceCharacterBody(const Engine::Collision::VerticalCapsule &body,
                      std::span<const Engine::Math::Aabb> walls,
                      std::span<const Engine::Collision::VerticalCapsule> actors);
} // namespace fps
