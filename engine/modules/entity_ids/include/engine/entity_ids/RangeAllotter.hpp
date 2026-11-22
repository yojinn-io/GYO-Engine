#pragma once

// The default entity allotter (world-gen declaration
// docs/architecture/target/v2/world-gen-declaration.zh-Hant.md, section 5.10; Design Invariant EI-2 in
// docs/architecture/target/v2/entity-ids.zh-Hant.md): one range [first, last], monotonic, never reuses an id,
// reports exhaustion and never wraps; no reset. It is one strategy behind the kernel's EntityAllotter concept
// (Registrar::Allotter<T>); the kernel ships none (DI-KS 4). A product may declare its own type instead.

#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Registry.hpp"

#include <optional>

namespace GYO::Engine::EntityIds {

using EntityId = Kernel::EntityId;

struct RangeAllotter {
    EntityId first = 0;
    EntityId last = 0;
    EntityId next = 0;
    bool exhausted = false;

    [[nodiscard]] static RangeAllotter Of(EntityId first, EntityId last) noexcept {
        return RangeAllotter{first, last, first, first > last};
    }

    [[nodiscard]] std::optional<EntityId> Allot() noexcept {
        if (exhausted) return std::nullopt;
        const EntityId id = next;
        if (id == last) {
            exhausted = true;
        } else {
            ++next;
        }
        return id;
    }

    [[nodiscard]] bool Owns(EntityId id) const noexcept { return first <= id && id <= last; }
};

// Field by field: the padding after exhausted is not hashed (World::Summary, DI-KS 27).
inline void HashValue(Kernel::Hasher& h, const RangeAllotter& value) {
    HashValue(h, value.first);
    HashValue(h, value.last);
    HashValue(h, value.next);
    HashValue(h, value.exhausted);
}

} // namespace GYO::Engine::EntityIds
