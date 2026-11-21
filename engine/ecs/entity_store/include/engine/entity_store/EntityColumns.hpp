#pragma once

#include "engine/base/Assert.hpp"
#include "engine/entity_store/EntityId.hpp"
#include "engine/entity_store/EntityKey.hpp"

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace Engine::EntityStore {

// A column is a std::vector of one element type, owned by the caller and kept
// in strictly ascending key order with no duplicates (docs/architecture/
// entity-store.md, Design Invariants 2 and 3): iteration order depends on keys
// alone, never on the history of inserts and erases. Membership changes only
// through InsertEntity and EraseEntity; a lookup is a binary search on the
// column itself. Every function requires the column to be strictly ascending
// and does not re-check it (StrictlyAscending is the caller's check).
//
// Invalidation: InsertEntity and EraseEntity invalidate every pointer,
// reference, iterator and span into that column. A pointer from FindEntity
// stays valid until the next InsertEntity or EraseEntity on the same column.
//
// Keys are read only through EntityKey (EntityKey.hpp); every call below is
// qualified, so no function of the element's namespace takes part.

namespace detail {
// On the container's own iterators: a std::span iterator is a different type
// from std::vector's on libstdc++ and MSVC, so it cannot meet vector::end().
template <typename Iterator>
[[nodiscard]] Iterator EntityLowerBound(Iterator first, Iterator last, EntityId id) noexcept {
    return std::lower_bound(first, last, id,
                            [](const auto& value, EntityId key) { return EntityStore::EntityOf(value) < key; });
}

// The element with this key, or last.
template <typename Iterator>
[[nodiscard]] Iterator EntityPosition(Iterator first, Iterator last, EntityId id) noexcept {
    const auto position = detail::EntityLowerBound(first, last, id);
    return position != last && EntityStore::EntityOf(*position) == id ? position : last;
}
} // namespace detail

// True when every key is greater than the one before it.
template <HasEntityKey T>
[[nodiscard]] bool StrictlyAscending(std::span<const T> column) noexcept {
    for (std::size_t i = 1; i < column.size(); ++i)
        if (EntityStore::EntityOf(column[i - 1]) >= EntityStore::EntityOf(column[i])) return false;
    return true;
}

// The element with this key, or nullptr. No side effect.
template <HasEntityKey T>
[[nodiscard]] const T* FindEntity(std::span<const T> column, EntityId id) noexcept {
    const auto position = detail::EntityPosition(column.begin(), column.end(), id);
    return position != column.end() ? &*position : nullptr;
}
template <HasEntityKey T>
[[nodiscard]] const T* FindEntity(const std::vector<T>& column, EntityId id) noexcept {
    return EntityStore::FindEntity(std::span<const T>(column), id);
}
template <HasEntityKey T>
[[nodiscard]] T* FindEntity(std::vector<T>& column, EntityId id) noexcept {
    const auto position = detail::EntityPosition(column.begin(), column.end(), id);
    return position != column.end() ? &*position : nullptr;
}

// Inserts at the lower_bound position, never by appending, even when the key
// is greater than every key present. Inserting a key already present is a
// Programmer Error. Returns the inserted element.
template <HasEntityKey T>
T& InsertEntity(std::vector<T>& column, T value) {
    const auto id = EntityStore::EntityOf(value);
    const auto position = detail::EntityLowerBound(column.begin(), column.end(), id);
    GYO_ASSERT(position == column.end() || EntityStore::EntityOf(*position) != id);
    return *column.insert(position, std::move(value));
}

// Removes the element with this key and keeps the order of the rest (no
// swap-and-pop, no tombstone). Returns whether it was present: an absent key
// is a result, not an error.
template <HasEntityKey T>
bool EraseEntity(std::vector<T>& column, EntityId id) {
    const auto position = detail::EntityPosition(column.begin(), column.end(), id);
    if (position == column.end()) return false;
    column.erase(position);
    return true;
}

} // namespace Engine::EntityStore
