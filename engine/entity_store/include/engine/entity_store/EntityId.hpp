#pragma once

#include <cstdint>

namespace Engine::EntityStore {

// The key of every column element (docs/architecture/entity-store.md, Design
// Invariant 1). The caller gives every value: the store never allots, recycles
// or reserves one, carries no generation bits and knows no range. A consumer
// may give 0 or any other value a meaning; the store does not.
using EntityId = std::uint64_t;

} // namespace Engine::EntityStore
