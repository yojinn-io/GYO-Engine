#pragma once

// Relationships (declaration v8-arch-gyo2-kernel-relationship, DI-KS 29 to 33;
// the shape of Bevy's bevy_ecs relationship: a source component on the source
// entity names one target entity, and the target component on the target lists
// the sources, kept by the kernel).
//
// The kernel ships one pair, ChildOf (source) and Children (target, linked
// despawn), registered by the kernel itself when a Registry is made, so every
// World has them (ComponentId 0 and 1). The source cannot be written in place,
// only inserted (a new target) or removed; the target is read only, and only
// ExclusiveWorld::Restore puts one back (RestoreSet::Column). Both are refused
// at compile time everywhere else (the RelationshipTraits checks below are
// used by every entry that writes a component).
//
// The general entry (Registrar::Relationship<R>, one-to-one, allowSelf, custom
// collections, reordering) is a shape only: kernel RC 27.

#include "engine/base/Assert.hpp"
#include "engine/entity_store/EntityId.hpp"
#include "engine/kernel/Hash.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

namespace GYO::Kernel {

class Children;

// The parent of the entity that holds it. Insert another ChildOf to give the
// entity a new parent (it is appended to the new parent's Children); remove it
// to detach. A parent that is not alive, or the entity itself, makes the
// relationship fail at the sync point (RejectedRelationship, DI-KS 31).
struct ChildOf {
    Engine::EntityStore::EntityId parent = 0;

    friend bool operator==(const ChildOf&, const ChildOf&) = default;
};

inline void HashValue(Hasher& h, const ChildOf& value) { HashValue(h, value.parent); }

namespace Detail {
struct RelationshipStore;
}

// The children of the entity that holds it, in the order they were attached
// (DI-KS 30, the user's Q1 (A')): a child attached later comes after; children
// attached in one application come in the order of the records that attached
// them, (planPos, recSeq). Present exactly when at least one alive entity has a
// ChildOf naming this entity. Read only: kept by the kernel.
//
// Its size is the same on every 64-bit platform and build type (it is in every
// Registry fingerprint): a pointer and two 32-bit counts, not a std::vector.
class Children {
public:
    using EntityId = Engine::EntityStore::EntityId;

    Children() noexcept = default;
    Children(const Children& other) : Children() { CopyFrom(other); }
    Children& operator=(const Children& other) {
        if (this != &other) {
            Children copy(other);
            *this = std::move(copy);
        }
        return *this;
    }
    Children(Children&& other) noexcept
        : data_(std::move(other.data_)), size_(other.size_), capacity_(other.capacity_) {
        other.size_ = 0;
        other.capacity_ = 0;
    }
    Children& operator=(Children&& other) noexcept {
        if (this == &other) return *this;
        data_ = std::move(other.data_);
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.size_ = 0;
        other.capacity_ = 0;
        return *this;
    }
    ~Children() = default;

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] const EntityId* begin() const noexcept { return data_.get(); }
    [[nodiscard]] const EntityId* end() const noexcept { return data_.get() + size_; }
    [[nodiscard]] EntityId operator[](std::size_t index) const noexcept { return data_[index]; }
    [[nodiscard]] bool Contains(EntityId child) const noexcept { return std::find(begin(), end(), child) != end(); }

    friend bool operator==(const Children& a, const Children& b) noexcept {
        return a.size_ == b.size_ && std::equal(a.begin(), a.end(), b.begin());
    }

private:
    friend struct Detail::RelationshipStore;

    void CopyFrom(const Children& other) {
        Reserve(other.size_);
        std::copy(other.begin(), other.end(), data_.get());
        size_ = other.size_;
    }
    void Reserve(std::uint32_t capacity) {
        if (capacity <= capacity_) return;
        auto grown = std::make_unique<EntityId[]>(capacity);
        std::copy(begin(), end(), grown.get());
        data_ = std::move(grown);
        capacity_ = capacity;
    }
    void Append(EntityId child) {
        if (size_ == capacity_) {
            GYO_ASSERT(capacity_ < 0x80000000U);  // 2^31 children: the count would overflow
            Reserve(capacity_ == 0 ? 4U : capacity_ * 2U);
        }
        data_[size_++] = child;
    }
    // Keeps the order of the others; false when child is not in it.
    bool Erase(EntityId child) noexcept {
        EntityId* at = std::find(data_.get(), data_.get() + size_, child);
        if (at == data_.get() + size_) return false;
        std::copy(at + 1, data_.get() + size_, at);
        --size_;
        return true;
    }

    std::unique_ptr<EntityId[]> data_;
    std::uint32_t size_ = 0;
    std::uint32_t capacity_ = 0;
};

// The count, then each child in order (the order is part of the World summary).
inline void HashValue(Hasher& h, const Children& value) {
    h.U64(value.size());
    for (const auto child : value) HashValue(h, child);
}

// What the entries that write a component check at compile time (DI-KS 29).
template <class T>
struct RelationshipTraits {
    // ChildOf and Children: not written in place (Write<T>, World::GetMut, ExclusiveWorld::Modify).
    static constexpr bool noInPlaceWrite = false;
    // Children: not inserted, spawned or removed, and not an initial value target.
    static constexpr bool keptByKernel = false;
};
template <>
struct RelationshipTraits<ChildOf> {
    static constexpr bool noInPlaceWrite = true;
    static constexpr bool keptByKernel = false;
};
template <>
struct RelationshipTraits<Children> {
    static constexpr bool noInPlaceWrite = true;
    static constexpr bool keptByKernel = true;
};

template <class T>
inline constexpr bool kNoInPlaceWrite = RelationshipTraits<std::remove_cv_t<T>>::noInPlaceWrite;
template <class T>
inline constexpr bool kKeptByKernel = RelationshipTraits<std::remove_cv_t<T>>::keptByKernel;

// Why a relationship failed at an application (DI-KS 31).
enum class RelationshipRejectReason : std::uint8_t {
    TargetNotAlive,
    SelfReference,
};

// Which application the relationship failed in.
enum class RelationshipPath : std::uint8_t {
    SyncPoint,
    ImmediateChange,
    Restore,
    Host,
};

// DI-KS 31: one relationship that did not hold, a diagnostics record of the
// kernel's own like RejectedSpawn: read with World::Diagnostics<RejectedRelationship>()
// and taken with TakeDiagnostics between two Runs. A product System may not write this type.
struct RejectedRelationship {
    Engine::EntityStore::EntityId entity = 0;
    Engine::EntityStore::EntityId target = 0;
    RelationshipRejectReason reason = RelationshipRejectReason::TargetNotAlive;
    // The relationship component's name ("ChildOf").
    std::string relationship;
    // The System or hook that recorded it; "Host" for the Host's World::Insert.
    std::string emitter;
    bool emitterIsHook = false;
    // The schedule and step of the record (empty and 0 for the Host).
    std::string schedule;
    std::uint32_t step = 0;
    // The sync point that applied it (as RejectedSpawn::appliedAt; 0 for the Host).
    std::uint32_t appliedAt = 0;
    std::uint64_t syncEpoch = 0;
    RelationshipPath path = RelationshipPath::SyncPoint;
};

} // namespace GYO::Kernel
