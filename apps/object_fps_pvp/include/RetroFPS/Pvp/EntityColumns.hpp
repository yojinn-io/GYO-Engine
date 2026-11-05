#pragma once

#include "RetroFPS/Pvp/EntityId.hpp"
#include "engine/entity_store/EntityColumns.hpp"
#include "engine/entity_store/EntityKey.hpp"

#include <concepts>

// The product's adapter to the Engine entity store
// (docs/architecture/entity-store.md; entity-model E-5). The column mechanism
// (strictly ascending EntityId order, lower_bound insert, order-preserving
// erase, binary-search lookup) is the Engine's and is guarded by its common
// tests. This header keeps what is the product's: how its transition-shape
// components give their key, and the names its callers use.

namespace fps::pvp {

// The transition shape: PlayerState, CombatState and PlayerAnimation keep
// their playerId field as the column key; new components carry id, which the
// Engine's primary EntityKey reads.
template <typename T>
concept HasPlayerId = requires(const T& value) {
    { value.playerId } -> std::convertible_to<EntityId>;
};

} // namespace fps::pvp

namespace Engine::EntityStore {
// Keys a playerId type by that field, whatever namespace the type lives in
// (no name lookup involved). Declared here, before any column use: every
// column user includes this header (EntityKey.hpp, visibility rule).
//
// Only for the transition shape: a type with both playerId and id (say a
// projectile with its owner's playerId) must not be keyed silently by the
// owner, so using such a type as a column fails to compile here; its key is
// id, and its owner gives it its own EntityKey specialization if it needs
// another. No product type has both (checked 2026-11-05: the id-keyed types
// are PvpMatch's AnimationTrack, ActionLedger, Connection and EntityShapes,
// none of which has playerId).
template <::fps::pvp::HasPlayerId T>
struct EntityKey<T> {
    static_assert(!requires(const T& v) { v.id; },
                  "fps::pvp EntityKey: a type with both playerId and id is not a transition-shape component; "
                  "key it by id (no playerId field) or give it its own EntityKey specialization "
                  "(entity-model E-5, docs/architecture/entity-store.md Invariant 10)");
    [[nodiscard]] static constexpr EntityId Of(const T& value) noexcept { return value.playerId; }
};
} // namespace Engine::EntityStore

namespace fps::pvp {

// The column functions under the names the Match already uses; a
// using-declaration, so qualified calls (::fps::pvp::EraseEntity) find them
// next to the product's own EraseEntity overloads.
using Engine::EntityStore::EntityOf;
using Engine::EntityStore::EraseEntity;
using Engine::EntityStore::FindEntity;
using Engine::EntityStore::InsertEntity;
using Engine::EntityStore::StrictlyAscending;

} // namespace fps::pvp
