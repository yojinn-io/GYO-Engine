#pragma once

#include "RetroFPS/Pvp/EntityId.hpp"
#include "engine/base/Assert.hpp"

#include <algorithm>
#include <concepts>
#include <span>
#include <utility>
#include <vector>

namespace fps::pvp {

// A column is a std::vector of one component type kept in strictly ascending
// EntityId order (entity-model E-5): iteration order depends on ids alone,
// never on the history of joins and leaves. These three free functions are
// the only way a column gains or loses a member; a lookup is a binary search.
// No registry, no template class: the structure is the contract.

template <typename T>
concept HasPlayerId = requires(const T& value) {
    { value.playerId } -> std::convertible_to<EntityId>;
};
template <typename T>
concept HasEntityId = requires(const T& value) {
    { value.id } -> std::convertible_to<EntityId>;
};

// The column key of one element: PlayerState, CombatState and PlayerAnimation
// keep their playerId field through the transition; new components carry id.
template <HasPlayerId T>
[[nodiscard]] constexpr EntityId EntityOf(const T& value) noexcept { return value.playerId; }
template <HasEntityId T>
    requires(!HasPlayerId<T>)
[[nodiscard]] constexpr EntityId EntityOf(const T& value) noexcept { return value.id; }

namespace detail {
template <typename T>
[[nodiscard]] auto EntityLowerBound(std::span<T> column, EntityId id) noexcept {
    return std::lower_bound(column.begin(), column.end(), id,
                            [](const T& value, EntityId key) { return EntityOf(value) < key; });
}
} // namespace detail

// True when every id is greater than the one before it.
template <typename T>
[[nodiscard]] bool StrictlyAscending(std::span<const T> column) noexcept {
    for (std::size_t i = 1; i < column.size(); ++i)
        if (EntityOf(column[i - 1]) >= EntityOf(column[i])) return false;
    return true;
}

// The element with this id, or nullptr.
template <typename T>
[[nodiscard]] const T* FindEntity(std::span<const T> column, EntityId id) noexcept {
    const auto position = detail::EntityLowerBound(column, id);
    return position != column.end() && EntityOf(*position) == id ? &*position : nullptr;
}
template <typename T>
[[nodiscard]] const T* FindEntity(const std::vector<T>& column, EntityId id) noexcept {
    return FindEntity(std::span<const T>(column), id);
}
template <typename T>
[[nodiscard]] T* FindEntity(std::vector<T>& column, EntityId id) noexcept {
    const auto position = detail::EntityLowerBound(std::span<T>(column), id);
    return position != column.end() && EntityOf(*position) == id ? &*position : nullptr;
}

// Inserts at the ordered position (never an append: a player id the Gateway
// allots may be smaller than every id present). Inserting an id already in
// the column is a Programmer Error.
template <typename T>
T& InsertEntity(std::vector<T>& column, T value) {
    const auto id = EntityOf(value);
    const auto position = detail::EntityLowerBound(std::span<T>(column), id);
    GYO_ASSERT(position == column.end() || EntityOf(*position) != id);
    return *column.insert(position, std::move(value));
}

// Removes the element with this id, keeping the order of the rest (no
// swap-and-pop). Returns whether it was present.
template <typename T>
bool EraseEntity(std::vector<T>& column, EntityId id) {
    const auto position = detail::EntityLowerBound(std::span<T>(column), id);
    if (position == column.end() || EntityOf(*position) != id) return false;
    column.erase(position);
    return true;
}

} // namespace fps::pvp
