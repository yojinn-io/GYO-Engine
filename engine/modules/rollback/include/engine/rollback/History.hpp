#pragma once

// The official strategy's history (rollback declaration 5.5 points 1 and 2;
// authority-rollback declaration 5.2): a World Resource, so a new World has no
// old history (R-5). For each type that may roll back, per entity, the entries
// (tick, value or "none") a record left: a record keeps only the cells changed
// since the last record (their changed stamp above the stamp kept then; the
// kernel's primitive 1 and Stamp, declaration 2.4 G5) and the cells gone since;
// the alive set is kept whole per tick. What is older than the depth is
// dropped, except, per entity, the newest entry before it (the base the window
// starts from). The history is a cache to rebuild, not simulation state: its
// hash function hashes nothing (authority-rollback declaration 3.3).

#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/rollback/Inbox.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace GYO::Engine::Rollback {

using Exclusive = Kernel::ExclusiveWorld<>;

class ColumnHistoryBase {
public:
    virtual ~ColumnHistoryBase() = default;
    ColumnHistoryBase(const ColumnHistoryBase&) = delete;
    ColumnHistoryBase& operator=(const ColumnHistoryBase&) = delete;

    [[nodiscard]] virtual Kernel::TypeKey Key() const noexcept = 0;
    // Records, at tick, the cells changed since stamp since and the cells gone since the last record. Entries of
    // tick or later (left by the run a resimulation replaces) are dropped first.
    virtual void Record(Exclusive& world, Kernel::ChangeStamp since, std::uint64_t tick) = 0;
    // Drops what is older than oldest, keeping per entity the newest entry at or before oldest.
    virtual void Prune(std::uint64_t oldest) = 0;
    // Puts the state at tick into set: for each entity of alive, the authoritative value at or before tick when the
    // inbox has one, else the history's.
    virtual void AddTo(Kernel::RestoreSet& set, std::uint64_t tick, std::span<const Kernel::EntityId> alive,
                       const InboxColumnBase* authoritative) const = 0;
    // Whether an authoritative value at or before tick differs from the history at tick. Only the entities the
    // inbox has a value for and that were alive at tick are compared: whether an entity is alive follows the
    // game's spawns and despawns (declaration 5.5: what is not said is a spawn or a despawn of the game), so a
    // value for an entity this World does not have cannot be put back and is not a reason to go back.
    [[nodiscard]] virtual bool Differs(std::uint64_t tick, const InboxColumnBase& authoritative,
                                       std::span<const Kernel::EntityId> alive) const = 0;
    // The history at tick takes the authoritative values of the entities alive at tick (it agrees with the
    // authority afterwards).
    virtual void Adopt(std::uint64_t tick, const InboxColumnBase& authoritative, std::span<const Kernel::EntityId> alive) = 0;

protected:
    ColumnHistoryBase() = default;
};

template <class T>
class ComponentHistory final : public ColumnHistoryBase {
public:
    using Equal = bool (*)(const T&, const T&);
    explicit ComponentHistory(Equal equal) : equal_(equal) {}

    [[nodiscard]] Kernel::TypeKey Key() const noexcept override { return Kernel::KeyOf<T>(); }

    void Record(Exclusive& world, Kernel::ChangeStamp since, std::uint64_t tick) override {
        DropFrom(tick);
        std::vector<Kernel::EntityId> present;
        world.template ReadColumn<T>([&](Kernel::EntityId entity, Kernel::ChangeStamp, Kernel::ChangeStamp changed, const T& value) {
            present.push_back(entity);
            if (changed > since || !Has(entity)) entries_[entity].push_back({tick, value});
        });
        for (auto& [entity, list] : entries_) {
            if (list.empty() || !list.back().value) continue;
            if (!std::binary_search(present.begin(), present.end(), entity)) list.push_back({tick, std::nullopt});
        }
    }

    void Prune(std::uint64_t oldest) override {
        for (auto it = entries_.begin(); it != entries_.end();) {
            std::deque<Entry>& list = it->second;
            while (list.size() >= 2 && list[1].tick <= oldest) list.pop_front();
            if (list.empty() || (list.size() == 1 && list.front().tick <= oldest && !list.front().value)) {
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void AddTo(Kernel::RestoreSet& set, std::uint64_t tick, std::span<const Kernel::EntityId> alive,
               const InboxColumnBase* authoritative) const override {
        const auto* inbox = static_cast<const InboxColumn<T>*>(authoritative);
        std::vector<std::pair<Kernel::EntityId, T>> cells;
        for (Kernel::EntityId entity : alive) {
            if (inbox != nullptr) {
                if (const auto value = inbox->At(entity, tick)) {
                    if (*value) cells.emplace_back(entity, **value);
                    continue;
                }
            }
            if (const std::optional<T> value = At(entity, tick)) cells.emplace_back(entity, *value);
        }
        set.Column<T>(std::move(cells), [equal = equal_](const T& a, const T& b) { return Same(equal, a, b); });
    }

    [[nodiscard]] bool Differs(std::uint64_t tick, const InboxColumnBase& authoritative,
                               std::span<const Kernel::EntityId> alive) const override {
        const auto& inbox = static_cast<const InboxColumn<T>&>(authoritative);
        for (Kernel::EntityId entity : inbox.EntitiesThrough(tick)) {
            if (!std::binary_search(alive.begin(), alive.end(), entity)) continue;
            const std::optional<T> wanted = *inbox.At(entity, tick);
            const std::optional<T> had = At(entity, tick);
            if (wanted.has_value() != had.has_value()) return true;
            if (wanted && !Same(equal_, *wanted, *had)) return true;
        }
        return false;
    }

    void Adopt(std::uint64_t tick, const InboxColumnBase& authoritative, std::span<const Kernel::EntityId> alive) override {
        const auto& inbox = static_cast<const InboxColumn<T>&>(authoritative);
        for (Kernel::EntityId entity : inbox.EntitiesThrough(tick)) {
            if (!std::binary_search(alive.begin(), alive.end(), entity)) continue;
            std::deque<Entry>& list = entries_[entity];
            while (!list.empty() && list.back().tick >= tick) list.pop_back();
            list.push_back({tick, *inbox.At(entity, tick)});
        }
    }

    // The value of entity at tick: the last entry at or before it.
    [[nodiscard]] std::optional<T> At(Kernel::EntityId entity, std::uint64_t tick) const {
        const auto it = entries_.find(entity);
        if (it == entries_.end()) return std::nullopt;
        std::optional<T> value;
        for (const Entry& entry : it->second) {
            if (entry.tick > tick) break;
            value = entry.value;
        }
        return value;
    }

private:
    struct Entry {
        std::uint64_t tick;
        std::optional<T> value;
    };
    static bool Same(Equal equal, const T& a, const T& b) {
        if (equal != nullptr) return equal(a, b);
        if constexpr (std::equality_comparable<T>) {
            return a == b;
        } else {
            return false;
        }
    }
    [[nodiscard]] bool Has(Kernel::EntityId entity) const {
        const auto it = entries_.find(entity);
        return it != entries_.end() && !it->second.empty() && it->second.back().value.has_value();
    }
    void DropFrom(std::uint64_t tick) {
        for (auto& [entity, list] : entries_) {
            while (!list.empty() && list.back().tick >= tick) list.pop_back();
        }
    }

    Equal equal_;
    std::map<Kernel::EntityId, std::deque<Entry>> entries_;
};

template <class T>
class ResourceHistory final : public ColumnHistoryBase {
public:
    using Equal = bool (*)(const T&, const T&);
    explicit ResourceHistory(Equal equal) : equal_(equal) {}

    [[nodiscard]] Kernel::TypeKey Key() const noexcept override { return Kernel::KeyOf<T>(); }

    void Record(Exclusive& world, Kernel::ChangeStamp since, std::uint64_t tick) override {
        while (!entries_.empty() && entries_.back().tick >= tick) entries_.pop_back();
        const std::optional<Kernel::ChangeStamp> stamp = world.template ResourceStamp<T>();
        if (!stamp) {
            if (!entries_.empty() && entries_.back().value) entries_.push_back({tick, std::nullopt});
            return;
        }
        if (*stamp > since || entries_.empty() || !entries_.back().value) {
            auto value = world.template Read<T>([](const T& v) { return v; });
            if (value && *value) entries_.push_back({tick, **value});
        }
    }
    void Prune(std::uint64_t oldest) override {
        while (entries_.size() >= 2 && entries_[1].tick <= oldest) entries_.pop_front();
    }
    void AddTo(Kernel::RestoreSet& set, std::uint64_t tick, std::span<const Kernel::EntityId>,
               const InboxColumnBase*) const override {
        std::optional<T> value;
        for (const Entry& entry : entries_) {
            if (entry.tick > tick) break;
            value = entry.value;
        }
        set.Resource<T>(std::move(value), [equal = equal_](const T& a, const T& b) {
            if (equal != nullptr) return equal(a, b);
            if constexpr (std::equality_comparable<T>) {
                return a == b;
            } else {
                return false;
            }
        });
    }
    // Resources take no authoritative values in this batch.
    [[nodiscard]] bool Differs(std::uint64_t, const InboxColumnBase&, std::span<const Kernel::EntityId>) const override {
        return false;
    }
    void Adopt(std::uint64_t, const InboxColumnBase&, std::span<const Kernel::EntityId>) override {}

private:
    struct Entry {
        std::uint64_t tick;
        std::optional<T> value;
    };
    Equal equal_;
    std::deque<Entry> entries_;
};

// The history of one input type: its value at each tick, at most capacity ticks.
class InputHistoryBase {
public:
    virtual ~InputHistoryBase() = default;
    InputHistoryBase(const InputHistoryBase&) = delete;
    InputHistoryBase& operator=(const InputHistoryBase&) = delete;
    // Copies the input's value now into tick.
    virtual void Record(Exclusive& world, std::uint64_t tick) = 0;
    // Writes the value of tick back into the input (nothing when tick is not kept: the official strategy's Check
    // makes the capacity at least the depth, RB-E9).
    virtual void WriteBack(Exclusive& world, std::uint64_t tick) const = 0;
    // Keeps the input's value now, and puts it back: a resimulation writes the inputs of the steps it runs again
    // into the input, and afterwards the input holds what its entry wrote this frame again, whether the entry ran
    // before the rollback or after it.
    virtual void Keep(Exclusive& world) = 0;
    virtual void PutBack(Exclusive& world) = 0;

protected:
    InputHistoryBase() = default;
};

template <class T>
class InputHistory final : public InputHistoryBase {
public:
    explicit InputHistory(std::uint32_t capacity) : capacity_(capacity) {}
    // The value of tick is replaced, the later ones are kept: while resimulating, the inputs of the steps still to
    // run again are the ones they were given the first time (an input is not resimulated).
    void Record(Exclusive& world, std::uint64_t tick) override {
        auto value = world.template Read<T>([](const T& v) { return v; });
        if (!value || !*value) return;
        auto at = std::find_if(ring_.begin(), ring_.end(), [tick](const auto& entry) { return entry.first >= tick; });
        if (at != ring_.end() && at->first == tick) {
            at->second = **value;
        } else {
            ring_.insert(at, {tick, **value});
        }
        while (ring_.size() > capacity_) ring_.pop_front();
    }
    void WriteBack(Exclusive& world, std::uint64_t tick) const override {
        for (const auto& [kept, value] : ring_) {
            if (kept != tick) continue;
            static_cast<void>(world.template Modify<T>([&value](T& input) { input = value; }));
            return;
        }
    }
    void Keep(Exclusive& world) override {
        auto value = world.template Read<T>([](const T& v) { return v; });
        now_ = value && *value ? std::optional<T>(**value) : std::nullopt;
    }
    void PutBack(Exclusive& world) override {
        if (!now_) return;
        static_cast<void>(world.template Modify<T>([this](T& input) { input = *now_; }));
        now_.reset();
    }

private:
    std::uint32_t capacity_;
    std::deque<std::pair<std::uint64_t, T>> ring_;
    std::optional<T> now_;
};

// A recorded input that is a component (net Module declaration D-11, section 5: the input is a component on the
// player's entity at both ends): every cell of T by tick, one value per entity, written back cell by cell. The
// shape follows lightyear's InputBuffer (crates/inputs/inputs/src/input_buffer.rs: one buffer of inputs by tick per
// entity), kept here as one ring of the whole column per tick. An entity without T at that tick gets nothing back.
template <class T>
class ComponentInputHistory final : public InputHistoryBase {
public:
    explicit ComponentInputHistory(std::uint32_t capacity) : capacity_(capacity) {}
    // The cells of tick are replaced, the later ticks are kept (as InputHistory<T>).
    void Record(Exclusive& world, std::uint64_t tick) override {
        Cells cells = Read(world);
        auto at = std::find_if(ring_.begin(), ring_.end(), [tick](const auto& entry) { return entry.first >= tick; });
        if (at != ring_.end() && at->first == tick) {
            at->second = std::move(cells);
        } else {
            ring_.insert(at, {tick, std::move(cells)});
        }
        while (ring_.size() > capacity_) ring_.pop_front();
    }
    void WriteBack(Exclusive& world, std::uint64_t tick) const override {
        for (const auto& [kept, cells] : ring_) {
            if (kept != tick) continue;
            Write(world, cells);
            return;
        }
    }
    void Keep(Exclusive& world) override { now_ = Read(world); }
    void PutBack(Exclusive& world) override {
        if (!now_) return;
        Write(world, *now_);
        now_.reset();
    }

private:
    using Cells = std::vector<std::pair<Kernel::EntityId, T>>;
    static Cells Read(Exclusive& world) {
        Cells cells;
        world.template ReadColumn<T>(
            [&cells](Kernel::EntityId id, Kernel::ChangeStamp, Kernel::ChangeStamp, const T& value) { cells.emplace_back(id, value); });
        return cells;
    }
    static void Write(Exclusive& world, const Cells& cells) {
        for (const auto& [id, value] : cells) {
            static_cast<void>(world.template Modify<T>(id, [&value](T& input) { input = value; }));
        }
    }
    std::uint32_t capacity_;
    std::deque<std::pair<std::uint64_t, Cells>> ring_;
    std::optional<Cells> now_;
};

// The World Resource the official strategy keeps its history in.
struct RollbackHistory {
    std::vector<std::unique_ptr<ColumnHistoryBase>> columns;
    std::vector<std::unique_ptr<InputHistoryBase>> inputs;
    // The alive set after each recorded tick.
    std::deque<std::pair<std::uint64_t, std::vector<Kernel::EntityId>>> alive;
    // The stamp the last record kept (G5), and its tick.
    Kernel::ChangeStamp lastStamp = 0;
    std::optional<std::uint64_t> lastTick;
    // The confirmed point the last decision looked at.
    std::optional<std::uint64_t> decided;

    RollbackHistory() = default;
    RollbackHistory(RollbackHistory&&) noexcept = default;
    RollbackHistory& operator=(RollbackHistory&&) noexcept = default;

    // The alive set at tick (the last kept at or before it), or nothing when tick is older than the history.
    [[nodiscard]] const std::vector<Kernel::EntityId>* AliveAt(std::uint64_t tick) const {
        const std::vector<Kernel::EntityId>* found = nullptr;
        for (const auto& [kept, ids] : alive) {
            if (kept > tick) break;
            found = &ids;
        }
        return found;
    }
};

inline void HashValue(Kernel::Hasher&, const RollbackHistory&) {}

} // namespace GYO::Engine::Rollback
