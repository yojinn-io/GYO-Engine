#pragma once

#include "engine/entity_store/EntityId.hpp"

#include <concepts>

namespace Engine::EntityStore {

// How the store reads an element's key (Design Invariant 10): only through
// EntityKey<T>, never by naming another field or by name lookup in the
// element's namespace.
//
// The primary template reads the member `id`. The owner of an element type
// whose key lives elsewhere specializes EntityKey for it (a partial
// specialization constrained by a concept is allowed) with
//     static constexpr EntityId Of(const T& value) noexcept;
// A specialization always wins over the primary, also for a type that has an
// `id` member too: it is chosen by template specialization, not by overload
// resolution, so the two never compete.
//
// Visibility: the specialization must be declared before the first use of
// EntityKey<T> (EntityOf, a column function, HasEntityKey) in every
// translation unit that uses it; otherwise the program is ill-formed, no
// diagnostic required. Declare it next to the element type, or in a header
// every column user includes first.
template <typename T>
struct EntityKey {
    [[nodiscard]] static constexpr EntityId Of(const T& value) noexcept
        requires requires(const T& element) { { element.id } -> std::convertible_to<EntityId>; }
    {
        return value.id;
    }
};

// A type whose key the store can read: it has a member `id` convertible to
// EntityId, or an EntityKey specialization. A type with neither fails this
// concept, so a column of it does not compile.
template <typename T>
concept HasEntityKey = requires(const T& value) {
    { EntityKey<T>::Of(value) } -> std::same_as<EntityId>;
};

// The key of one element.
template <HasEntityKey T>
[[nodiscard]] constexpr EntityId EntityOf(const T& value) noexcept(noexcept(EntityKey<T>::Of(value))) {
    return EntityKey<T>::Of(value);
}

} // namespace Engine::EntityStore
