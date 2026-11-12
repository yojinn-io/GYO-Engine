#pragma once
#include "engine/collision/Collision.hpp"
#include "engine/collision/StaticAabbGrid.hpp"
#include "engine/math/linear/Vec3.hpp"
#include <span>
namespace fps::pvp {
inline namespace sim {
// What one MoveCharacterBody call met, for the optional movement diagnostics
// (v8-product-pm-detect-diag). Filling it never changes the result.
struct CharacterMoveDiagnostics final {
    // Penetration resolution: how many pushes ran, and the deepest contact the
    // first pass found (the embedding the body started in).
    int penetrationPushes{};
    Engine::Math::Vec3 deepestNormal{};
    float deepestDepth{};
    // Sweep and slide: how many passes hit something, and the last hit's
    // contact (its raw fraction, before the 0.0001 backoff the move applies).
    int sweepHits{};
    Engine::Math::Vec3 sweepNormal{};
    float sweepFraction{};
};

// Pure product-local kinematic policy; Engine owns shape queries.
// The walls are a StaticAabbGrid (the arena's, Arena::WallGrid): every function
// returns, bit for bit, what the former linear scan of the walls in index order
// returned (v8-product-pa-broadphase).
// constrainToFloor preserves the body's initial feet height and resolves contact
// in the horizontal plane; player movement keeps the default full 3D response.
[[nodiscard]] Engine::Math::Vec3 MoveCharacterBody(Engine::Collision::VerticalCapsule body, Engine::Math::Vec3 displacement,
                                       const Engine::Collision::StaticAabbGrid &walls,
                                       std::span<const Engine::Collision::VerticalCapsule> actors,
                                       bool constrainToFloor = false,
                                       CharacterMoveDiagnostics* diagnostics = nullptr);
[[nodiscard]] bool
CanPlaceCharacterBody(const Engine::Collision::VerticalCapsule &body,
                      const Engine::Collision::StaticAabbGrid &walls,
                      std::span<const Engine::Collision::VerticalCapsule> actors);
// Whether a body stands on something: the implicit floor (feet at y = 0) or the
// top of a wall just below its feet. Movement's grounding and the arena's spawn
// rule both ask this one function, so a spawn is valid exactly where a body
// would stand.
[[nodiscard]] bool IsCharacterSupported(const Engine::Collision::VerticalCapsule &body,
                                        const Engine::Collision::StaticAabbGrid &walls);
} // namespace sim
} // namespace fps::pvp
