#include "RetroFPS/Collision/CharacterCollision.hpp"
#include "engine/math/scalar/Scalar.hpp"
#include <array>
#include <cmath>
#include <stdexcept>
namespace fps {
namespace {
// A ground-bound enemy cannot use a vertical separation or slide component.
// Resolve rare initial penetration along a horizontal direction using the same
// exact Engine query, rather than approximating the raised actor as a 2D disk.
template <class Overlap>
Engine::Collision::Contact PlanarPenetration(Engine::Collision::Contact contact,
                                             Engine::Collision::VerticalCapsule body,
                                             Overlap overlap) {
    std::array<Engine::Math::Vec3, 4> directions{{{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}}};
    const float horizontal =
        Engine::Math::Length(Engine::Math::Vec3{contact.normal.x, 0, contact.normal.z});
    std::size_t count = directions.size();
    if (horizontal > 0.000001F) {
        directions[0] = Engine::Math::Vec3{contact.normal.x, 0, contact.normal.z} / horizontal;
        count = 1;
    }
    std::optional<Engine::Collision::Contact> best;
    for (std::size_t index = 0; index < count; ++index) {
        const auto direction = directions[index];
        const auto penetrates = [&](const float distance) {
            auto candidate = body;
            candidate.feet.x += direction.x * distance;
            candidate.feet.z += direction.z * distance;
            const auto hit = overlap(candidate);
            return hit && hit->penetrationDepth > 0.0000001F;
        };
        float low = 0, high = Engine::Math::Max(body.radius, contact.penetrationDepth);
        int attempts = 0;
        while (penetrates(high) && attempts++ < 64)
            high *= 2;
        if (!std::isfinite(high) || attempts >= 64)
            throw std::runtime_error(
                "Cannot resolve planar character penetration within finite bounds");
        for (int iteration = 0; iteration < 24; ++iteration) {
            const float middle = (low + high) * 0.5F;
            if (penetrates(middle))
                low = middle;
            else
                high = middle;
        }
        if (!best || high < best->penetrationDepth) {
            contact.normal = direction;
            contact.penetrationDepth = high;
            best = contact;
        }
    }
    return *best;
}
} // namespace
bool CanPlaceCharacterBody(const Engine::Collision::VerticalCapsule &body,
                           std::span<const Engine::Math::Aabb> walls,
                           std::span<const Engine::Collision::VerticalCapsule> actors) {
    for (const auto &wall : walls)
        if (auto c = Engine::Collision::OverlapVerticalCapsuleAabb(body, wall);
            c && c->penetrationDepth > 0.00001F)
            return false;
    for (const auto &actor : actors)
        if (auto c = Engine::Collision::OverlapVerticalCapsules(body, actor);
            c && c->penetrationDepth > 0.00001F)
            return false;
    return true;
}
Engine::Math::Vec3 MoveCharacterBody(Engine::Collision::VerticalCapsule body, Engine::Math::Vec3 displacement,
                         std::span<const Engine::Math::Aabb> walls,
                         std::span<const Engine::Collision::VerticalCapsule> actors,
                         const bool constrainToFloor) {
    if (constrainToFloor)
        displacement.y = 0;
    // Resolve existing penetration (e.g. landing beside an actor), then sweep and slide.
    for (int iteration = 0; iteration < 8; ++iteration) {
        std::optional<Engine::Collision::Contact> deepest;
        const auto consider = [&](auto c) {
            if (c && c->penetrationDepth > 0.00001F &&
                (!deepest || c->penetrationDepth > deepest->penetrationDepth))
                deepest = c;
        };
        for (const auto &wall : walls) {
            auto contact = Engine::Collision::OverlapVerticalCapsuleAabb(body, wall);
            if (constrainToFloor && contact && contact->penetrationDepth > 0.00001F)
                contact = PlanarPenetration(*contact, body, [&](const auto &candidate) {
                    return Engine::Collision::OverlapVerticalCapsuleAabb(candidate, wall);
                });
            consider(contact);
        }
        for (const auto &actor : actors) {
            auto contact = Engine::Collision::OverlapVerticalCapsules(body, actor);
            if (constrainToFloor && contact && contact->penetrationDepth > 0.00001F)
                contact = PlanarPenetration(*contact, body, [&](const auto &candidate) {
                    return Engine::Collision::OverlapVerticalCapsules(candidate, actor);
                });
            consider(contact);
        }
        if (!deepest)
            break;
        body.feet = body.feet + deepest->normal * (deepest->penetrationDepth + 0.0001F);
    }
    for (int iteration = 0; iteration < 5; ++iteration) {
        std::optional<Engine::Collision::Contact> nearest;
        const auto consider = [&](auto c) {
            if (!c)
                return;
            if (constrainToFloor) {
                const float horizontal =
                    Engine::Math::Length(Engine::Math::Vec3{c->normal.x, 0, c->normal.z});
                if (horizontal <= 0.000001F)
                    return;
                c->normal = Engine::Math::Vec3{c->normal.x, 0, c->normal.z} / horizontal;
            }
            if (!nearest || c->fraction < nearest->fraction)
                nearest = c;
        };
        const Engine::Math::Vec3 d = displacement;
        for (const auto &wall : walls)
            consider(Engine::Collision::SweepVerticalCapsuleAgainstAabb(body, d, wall));
        for (const auto &actor : actors)
            consider(Engine::Collision::SweepVerticalCapsuleAgainstCapsule(body, d, actor));
        const float length = Engine::Math::Length(d);
        if (length < 0.000001F)
            break;
        const float fraction =
            nearest ? Engine::Math::Max(0.0F, nearest->fraction - 0.0001F / length) : 1.0F;
        body.feet = body.feet + d * fraction;
        if (!nearest)
            break;
        displacement = d * (1 - fraction);
        const auto n = nearest->normal;
        const float into = Engine::Math::Min(0.0F, Engine::Math::Dot(displacement, n));
        displacement = displacement - n * into;
    }
    return {body.feet.x, Engine::Math::Max(0.0F, body.feet.y), body.feet.z};
}
} // namespace fps
