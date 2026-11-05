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
// Whether a body stands on something: the implicit floor (feet at y = 0) or the
// top of a wall just below its feet. Movement's grounding and the arena's spawn
// rule both ask this one function, so a spawn is valid exactly where a body
// would stand.
[[nodiscard]] bool IsCharacterSupported(const Engine::Collision::VerticalCapsule &body,
                                        std::span<const Engine::Math::Aabb> walls);
} // namespace fps
