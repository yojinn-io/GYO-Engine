#pragma once

// Internal to the kernel (namespace Detail): the World's storage
// (target/world.zh-Hant.md section 6, layout 2 "Cell<T>"; Delta-3 of
// docs/architecture/plans/kernel/migration-plan.zh-Hant.md).
//
// A column is a std::vector<Cell<T>> kept by engine/entity_store's algorithms
// (InsertEntity, EraseEntity, FindEntity, StrictlyAscending): strictly
// ascending EntityId, no duplicates (DI-KS 3). The cell is the kernel's type:
// its key, its two change stamps and the value. entity_store sees the cell as
// an opaque element with a member `id`; it never reads or decides on a stamp
// (target/world section 6). Every stamp read and write is in engine/kernel.
//
// Resources, event buffers and diagnostics buffers are typed slots behind a
// small virtual interface, made by factories the Registrar records when a
// type is registered (Registry.hpp). Nothing here allocates pages or protects
// memory: columns use the ordinary allocator; the place where an
// independent-page allocation would plug in is the column factory (U1 of the
// migration plan: the page guard is decided after G2a).

#include "engine/base/Assert.hpp"
#include "engine/entity_store/EntityColumns.hpp"
#include "engine/entity_store/EntityId.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace GYO::Kernel {

// The key of an entity (target/world section 1): a uint64 in Bevy's layout,
// low 32 bits index, high 32 bits generation. The kernel compares only the
// whole 64-bit value and never reads the two halves.
using EntityId = Engine::EntityStore::EntityId;

// Spec A.1: a World-wide counter advanced at every Plan step, in Plan order.
using ChangeStamp = std::uint64_t;

namespace Detail {

template <class T>
struct Cell {
    EntityId id;
    ChangeStamp added;
    ChangeStamp changed;
    T value;
};

// The storage contract of a component type (spec W-8, plus what the Cell
// layout needs from std::vector: insert and erase move-assign the cells after
// the position).
template <class T>
inline constexpr bool kStorable = std::is_object_v<T> && !std::is_const_v<T> &&
                                  std::is_nothrow_move_constructible_v<T> && std::is_nothrow_destructible_v<T> &&
                                  std::is_nothrow_move_assignable_v<T>;

class ColumnBase {
public:
    virtual ~ColumnBase() = default;
    ColumnBase(const ColumnBase&) = delete;
    ColumnBase& operator=(const ColumnBase&) = delete;

    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;
    [[nodiscard]] virtual EntityId KeyAt(std::size_t index) const noexcept = 0;
    [[nodiscard]] virtual ChangeStamp AddedAt(std::size_t index) const noexcept = 0;
    [[nodiscard]] virtual ChangeStamp ChangedAt(std::size_t index) const noexcept = 0;
    // The value's object bytes only (no key, no stamps).
    [[nodiscard]] virtual std::span<const std::byte> ValueBytesAt(std::size_t index) const noexcept = 0;
    // Every cell's object bytes: what the boundary comparison snapshots (spec A.6 layer 2).
    [[nodiscard]] virtual std::span<const std::byte> StorageBytes() const noexcept = 0;
    [[nodiscard]] virtual bool Contains(EntityId id) const noexcept = 0;
    // Destroys the cell of id; false when absent.
    virtual bool Erase(EntityId id) = 0;
    [[nodiscard]] virtual bool StrictlyAscending() const noexcept = 0;

    // +1 at every membership change (spec A.1 epoch; kept when verification is off).
    std::uint64_t epoch = 0;

protected:
    ColumnBase() = default;
};

template <class T>
class Column final : public ColumnBase {
public:
    Column() = default;
    // Spec W-7: the cells are destroyed in ascending EntityId order.
    ~Column() override {
        std::reverse(cells.begin(), cells.end());
        while (!cells.empty()) cells.pop_back();
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return cells.size(); }
    [[nodiscard]] EntityId KeyAt(std::size_t index) const noexcept override { return cells[index].id; }
    [[nodiscard]] ChangeStamp AddedAt(std::size_t index) const noexcept override { return cells[index].added; }
    [[nodiscard]] ChangeStamp ChangedAt(std::size_t index) const noexcept override { return cells[index].changed; }
    [[nodiscard]] std::span<const std::byte> ValueBytesAt(std::size_t index) const noexcept override {
        return {reinterpret_cast<const std::byte*>(&cells[index].value), sizeof(T)};
    }
    [[nodiscard]] std::span<const std::byte> StorageBytes() const noexcept override {
        return {reinterpret_cast<const std::byte*>(cells.data()), cells.size() * sizeof(Cell<T>)};
    }
    [[nodiscard]] bool Contains(EntityId id) const noexcept override {
        return Engine::EntityStore::FindEntity(cells, id) != nullptr;
    }
    bool Erase(EntityId id) override {
        if (!Engine::EntityStore::EraseEntity(cells, id)) return false;
        ++epoch;
        return true;
    }
    [[nodiscard]] bool StrictlyAscending() const noexcept override {
        return Engine::EntityStore::StrictlyAscending(std::span<const Cell<T>>(cells));
    }

    [[nodiscard]] Cell<T>* Find(EntityId id) noexcept { return Engine::EntityStore::FindEntity(cells, id); }
    [[nodiscard]] const Cell<T>* Find(EntityId id) const noexcept { return Engine::EntityStore::FindEntity(cells, id); }

    // Inserts a new cell (added = changed = stamp) or overwrites an existing one
    // (changed = stamp). Returns true when the cell is new.
    bool Put(EntityId id, T&& value, ChangeStamp stamp) {
        if (Cell<T>* cell = Find(id)) {
            cell->value = std::move(value);
            cell->changed = stamp;
            return false;
        }
        Engine::EntityStore::InsertEntity(cells, Cell<T>{id, stamp, stamp, std::move(value)});
        ++epoch;
        return true;
    }

    std::vector<Cell<T>> cells;
};

class ResourceBase {
public:
    virtual ~ResourceBase() = default;
    ResourceBase(const ResourceBase&) = delete;
    ResourceBase& operator=(const ResourceBase&) = delete;

    [[nodiscard]] virtual bool Present() const noexcept = 0;
    [[nodiscard]] virtual std::span<const std::byte> StorageBytes() const noexcept = 0;
    virtual void Reset() noexcept = 0;

    ChangeStamp changed = 0;

protected:
    ResourceBase() = default;
};

template <class T>
class ResourceSlot final : public ResourceBase {
public:
    [[nodiscard]] bool Present() const noexcept override { return value.has_value(); }
    [[nodiscard]] std::span<const std::byte> StorageBytes() const noexcept override {
        return value ? std::span<const std::byte>(reinterpret_cast<const std::byte*>(&*value), sizeof(T))
                     : std::span<const std::byte>();
    }
    void Reset() noexcept override { value.reset(); }

    std::optional<T> value;
};

class EventBufferBase {
public:
    virtual ~EventBufferBase() = default;
    EventBufferBase(const EventBufferBase&) = delete;
    EventBufferBase& operator=(const EventBufferBase&) = delete;

    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;
    [[nodiscard]] virtual std::span<const std::byte> StorageBytes() const noexcept = 0;
    [[nodiscard]] virtual std::span<const std::byte> PayloadBytesAt(std::size_t index) const noexcept = 0;
    // Spec F.3 trim: drops every record whose tick is before keepFrom.
    virtual void Trim(std::uint64_t keepFrom) = 0;

protected:
    EventBufferBase() = default;
};

template <class E>
class EventBuffer final : public EventBufferBase {
public:
    struct Record {
        std::uint64_t tick;
        std::uint64_t seq;
        std::uint32_t writerPos;
        E payload;
    };

    [[nodiscard]] std::size_t Size() const noexcept override { return records.size(); }
    [[nodiscard]] std::span<const std::byte> StorageBytes() const noexcept override {
        return {reinterpret_cast<const std::byte*>(records.data()), records.size() * sizeof(Record)};
    }
    [[nodiscard]] std::span<const std::byte> PayloadBytesAt(std::size_t index) const noexcept override {
        return {reinterpret_cast<const std::byte*>(&records[index].payload), sizeof(E)};
    }
    void Trim(std::uint64_t keepFrom) override {
        records.erase(std::remove_if(records.begin(), records.end(),
                                     [keepFrom](const Record& r) { return r.tick < keepFrom; }),
                      records.end());
    }

    std::vector<Record> records;
    std::uint64_t nextSeq = 0;
};

// The World end of DiagnosticsWriter<T> (spec B.4 point 4, B-13): kept when
// the World is Interrupted; the Host takes it at its exchange point.
class DiagnosticsBase {
public:
    virtual ~DiagnosticsBase() = default;
    DiagnosticsBase(const DiagnosticsBase&) = delete;
    DiagnosticsBase& operator=(const DiagnosticsBase&) = delete;
    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;

protected:
    DiagnosticsBase() = default;
};

template <class R>
class DiagnosticsBuffer final : public DiagnosticsBase {
public:
    struct Entry {
        std::uint64_t tick;
        // True while a Replay runs (spec H-8); rollback-extract sets it. Always false here.
        bool rollbackActive;
        R record;
    };
    [[nodiscard]] std::size_t Size() const noexcept override { return entries.size(); }
    std::vector<Entry> entries;
};

// A value recorded by a deferred command, moved into the World at the sync point.
class ValueBox {
public:
    virtual ~ValueBox() = default;
    ValueBox(const ValueBox&) = delete;
    ValueBox& operator=(const ValueBox&) = delete;
    // Moves the value into column (insert or overwrite); true when the cell is new.
    virtual bool PutInto(ColumnBase& column, EntityId id, ChangeStamp stamp) = 0;
    // Moves the value into resource.
    virtual void PutInto(ResourceBase& resource) = 0;

protected:
    ValueBox() = default;
};

template <class T>
class TypedBox final : public ValueBox {
public:
    explicit TypedBox(T&& v) : value(std::move(v)) {}
    bool PutInto(ColumnBase& column, EntityId id, ChangeStamp stamp) override {
        return static_cast<Column<T>&>(column).Put(id, std::move(value), stamp);
    }
    void PutInto(ResourceBase& resource) override { static_cast<ResourceSlot<T>&>(resource).value = std::move(value); }
    T value;
};

// One storage's bytes at the last boundary comparison (spec A.6 layer 2).
// apiWritten: written through the World API since (the Host in Ready, a
// declared write); the next comparison takes it again instead of comparing.
struct Snapshot {
    std::vector<std::byte> bytes;
    bool valid = false;
    bool apiWritten = false;
};

using MakeColumnFn = std::unique_ptr<ColumnBase> (*)();
using MakeResourceFn = std::unique_ptr<ResourceBase> (*)();
using MakeEventsFn = std::unique_ptr<EventBufferBase> (*)();

template <class T>
std::unique_ptr<ColumnBase> MakeColumn() {
    return std::make_unique<Column<T>>();
}
template <class T>
std::unique_ptr<ResourceBase> MakeResource() {
    return std::make_unique<ResourceSlot<T>>();
}
template <class E>
std::unique_ptr<EventBufferBase> MakeEvents() {
    return std::make_unique<EventBuffer<E>>();
}

// The allotter resource's operations (spec A.1: the World does not allot; the
// declared allotter resource does). Recorded by Registrar::Allotter<T>.
struct AllotterOps {
    std::optional<EntityId> (*allot)(ResourceBase&) = nullptr;
    bool (*owns)(const ResourceBase&, EntityId) = nullptr;
};

} // namespace Detail

// What Registrar::Allotter<T> requires of an allotter resource (spec A.1, W-9):
// Allot gives the next id or nothing when the range is used up (an error is
// reported, never a wrap); Owns says whether an id is in this World's range
// (SpawnWithId must not use one: spec A.1, a GYO_ASSERT at the sync point).
template <class A>
concept EntityAllotter = requires(A allotter, const A& constant, EntityId id) {
    { allotter.Allot() } -> std::same_as<std::optional<EntityId>>;
    { constant.Owns(id) } -> std::same_as<bool>;
};

// The kernel's range allotter (spec A.1, W-9): one range [first, last],
// monotonic, never reuses an id, reports exhaustion and never wraps; no reset.
// The policy "monotonic, generation 0" of target/world section 1 is this
// allotter with a product's range. A product may declare its own type instead.
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

namespace Detail {

template <class A>
std::optional<EntityId> AllotIn(ResourceBase& resource) {
    auto& slot = static_cast<ResourceSlot<A>&>(resource);
    if (!slot.value) return std::nullopt;
    return slot.value->Allot();
}

template <class A>
bool OwnsIn(const ResourceBase& resource, EntityId id) {
    const auto& slot = static_cast<const ResourceSlot<A>&>(resource);
    return slot.value && slot.value->Owns(id);
}

} // namespace Detail

} // namespace GYO::Kernel
