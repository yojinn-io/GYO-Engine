#pragma once

// A range allotter for the tests of the kernel and the orchestration layer (world-gen declaration
// docs/architecture/target/v2/world-gen-declaration.zh-Hant.md, section 5.10 and section 10). The kernel ships no
// allotter type (DI-KS 4) and these tests do not link the entity_ids Module, so they declare this test type: the
// same semantics as EntityIds::RangeAllotter (one range, monotonic, exhaustion reported, no wrap), synthetic test
// data (AGENTS §7). It lets the kernel's allot machinery be exercised; the properties of the default strategy
// itself are tested in tests/common/entity_ids/. Header-only; a consumer links the kernel.

#include "engine/entity_store/EntityId.hpp"
#include "engine/kernel/Hash.hpp"

#include <optional>

namespace GYO::Test {

struct TestRangeAllotter {
    using EntityId = ::GYO::Engine::EntityStore::EntityId;

    EntityId first = 0;
    EntityId last = 0;
    EntityId next = 0;
    bool exhausted = false;

    [[nodiscard]] static TestRangeAllotter Of(EntityId first, EntityId last) noexcept {
        return TestRangeAllotter{first, last, first, first > last};
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

inline void HashValue(::GYO::Kernel::Hasher& h, const TestRangeAllotter& value) {
    HashValue(h, value.first);
    HashValue(h, value.last);
    HashValue(h, value.next);
    HashValue(h, value.exhausted);
}

} // namespace GYO::Test
