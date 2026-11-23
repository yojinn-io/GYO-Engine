#pragma once

// The rollback Module's way in for authoritative values (authority-rollback
// declaration 5.5, the shape the user approved again on 2026-11-20): one entry
// for every strategy, a World Resource. The network Module, or the author,
// writes into it:
//   - Value<T>(entity, tick, value): component T of entity had value at tick;
//   - Removed<T>(entity, tick): entity certainly had no T at tick;
//   - ConfirmedThrough(tick): every snapshot up to tick has come (the
//     confirmed point C; the reliable event stream does not count for it);
//   - Entered(entity, tick), Exited(entity, tick): the caller says when an
//     entity exists (R-8, gate 1 A of the net Module's PR-2): at C it is alive
//     when it entered at or before C and did not exit at or before C, whatever
//     the World's own spawns and despawns did; one not said follows them;
//   - ResourceValue<T>(tick, value): resource T had value at tick (the rules
//     of a component's value, without an entity; gate 1 F).
// A value of a tick later than C stays and is compared when C reaches it; a
// late value of a tick before C counts as "the last value at or before C" (MC-1:
// an event is never dropped silently).
//
// Not made yet (RollbackFailure NotImplemented, an E2 the caller decides on):
// part of an entity's components and a value from an event written into the
// history (MC-2; the net Module's declaration section 13: not used), and "this
// snapshot does not carry it" (interest management; the net Module's step 5).

#include "engine/base/Result.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace GYO::Engine::Rollback {

enum class RollbackFailureCode : std::uint8_t {
    // The entry is part of the approved shape, not of this batch's behaviour.
    NotImplemented = 1,
};

[[nodiscard]] inline std::string_view ToString(RollbackFailureCode code) noexcept {
    switch (code) {
    case RollbackFailureCode::NotImplemented: return "NotImplemented";
    }
    return "Unknown";
}

// What an entry of the rollback Module cannot do (an E2: returned, the caller decides).
struct RollbackFailure {
    RollbackFailureCode code;
    std::string message;
    std::string detail;

    RollbackFailure(RollbackFailureCode failureCode, std::string failureMessage, std::string failureDetail = {})
        : code(failureCode), message(std::move(failureMessage)), detail(std::move(failureDetail)) {}
};

// The authoritative values of one component type.
class InboxColumnBase {
public:
    virtual ~InboxColumnBase() = default;
    InboxColumnBase(const InboxColumnBase&) = delete;
    InboxColumnBase& operator=(const InboxColumnBase&) = delete;
    // The entities with a value or a removal at or before tick, ascending.
    [[nodiscard]] virtual std::vector<Kernel::EntityId> EntitiesThrough(std::uint64_t tick) const = 0;
    // Drops what is older than oldest, keeping per entity the newest entry at or before oldest.
    virtual void Prune(std::uint64_t oldest) = 0;

protected:
    InboxColumnBase() = default;
};

template <class T>
class InboxColumn final : public InboxColumnBase {
public:
    void Put(Kernel::EntityId entity, std::uint64_t tick, std::optional<T> value) {
        entries_[{entity, tick}] = std::move(value);
    }
    // The last value of entity at or before tick: nothing when there is none;
    // a nullopt inside when it was removed.
    [[nodiscard]] std::optional<std::optional<T>> At(Kernel::EntityId entity, std::uint64_t tick) const {
        auto it = entries_.upper_bound({entity, tick});
        if (it == entries_.begin()) return std::nullopt;
        --it;
        if (it->first.first != entity) return std::nullopt;
        return it->second;
    }
    [[nodiscard]] std::vector<Kernel::EntityId> EntitiesThrough(std::uint64_t tick) const override {
        std::vector<Kernel::EntityId> out;
        for (const auto& [key, value] : entries_) {
            if (key.second <= tick && (out.empty() || out.back() != key.first)) out.push_back(key.first);
        }
        return out;
    }
    void Prune(std::uint64_t oldest) override {
        for (auto it = entries_.begin(); it != entries_.end();) {
            auto next = std::next(it);
            const bool laterOfSameEntity = next != entries_.end() && next->first.first == it->first.first;
            if (it->first.second < oldest && laterOfSameEntity && next->first.second <= oldest) {
                it = entries_.erase(it);
            } else {
                it = next;
            }
        }
    }

private:
    std::map<std::pair<Kernel::EntityId, std::uint64_t>, std::optional<T>> entries_;
};

class RollbackInbox {
public:
    RollbackInbox() = default;
    RollbackInbox(RollbackInbox&&) noexcept = default;
    RollbackInbox& operator=(RollbackInbox&&) noexcept = default;

    template <class T>
    void Value(Kernel::EntityId entity, std::uint64_t tick, T value) {
        Column<T>(true)->Put(entity, tick, std::move(value));
    }
    template <class T>
    void Removed(Kernel::EntityId entity, std::uint64_t tick) {
        Column<T>(true)->Put(entity, tick, std::nullopt);
    }
    // Every snapshot up to tick has come (never moves back).
    void ConfirmedThrough(std::uint64_t tick) {
        if (!confirmed_ || tick > *confirmed_) confirmed_ = tick;
    }
    [[nodiscard]] std::optional<std::uint64_t> Confirmed() const noexcept { return confirmed_; }

    // The entity exists from tick on / no longer exists from tick on (R-8 A).
    void Entered(Kernel::EntityId entity, std::uint64_t tick) { lifecycle_[entity][tick] = true; }
    void Exited(Kernel::EntityId entity, std::uint64_t tick) { lifecycle_[entity][tick] = false; }
    // Whether entity is alive at tick as the caller said (its last entry or exit at or before tick); nothing when
    // the caller said nothing of it up to tick.
    [[nodiscard]] std::optional<bool> AliveAt(Kernel::EntityId entity, std::uint64_t tick) const {
        const auto found = lifecycle_.find(entity);
        if (found == lifecycle_.end()) return std::nullopt;
        auto it = found->second.upper_bound(tick);
        if (it == found->second.begin()) return std::nullopt;
        return std::prev(it)->second;
    }
    // The alive set at tick: alive (ascending; the World's own) with what the caller said applied.
    [[nodiscard]] std::vector<Kernel::EntityId> AliveWith(std::vector<Kernel::EntityId> alive, std::uint64_t tick) const {
        for (const auto& [entity, entries] : lifecycle_) {
            const std::optional<bool> said = AliveAt(entity, tick);
            if (!said) continue;
            const auto at = std::lower_bound(alive.begin(), alive.end(), entity);
            const bool there = at != alive.end() && *at == entity;
            if (*said && !there) alive.insert(at, entity);
            if (!*said && there) alive.erase(at);
        }
        return alive;
    }

    // Resource T had value at tick (R-8 F).
    template <class T>
    void ResourceValue(std::uint64_t tick, T value) {
        ResourceColumn<T>(true)->Put(kResourceEntity, tick, std::move(value));
    }
    // The authoritative values of resource T (under kResourceEntity), or nullptr when none came.
    template <class T>
    [[nodiscard]] const InboxColumn<T>* ResourceValues() const {
        return const_cast<RollbackInbox*>(this)->template ResourceColumn<T>(false);
    }
    [[nodiscard]] const InboxColumnBase* ResourceValuesOf(Kernel::TypeKey key) const noexcept {
        for (const auto& [entry, column] : resources_) {
            if (entry == key) return column.get();
        }
        return nullptr;
    }
    // The entity a resource's values are kept under.
    static constexpr Kernel::EntityId kResourceEntity = 0;

    // Not made (rollback.zh-Hant.md section 2): each returns NotImplemented.
    template <class T>
    Base::Result<void, RollbackFailure> PartialValue(Kernel::EntityId, std::uint64_t, T) {
        return NotImplemented("a value of part of an entity's components");
    }
    Base::Result<void, RollbackFailure> NotSent(Kernel::EntityId, std::uint64_t) {
        return NotImplemented("\"this snapshot does not carry it\" (interest management)");
    }
    template <class T>
    Base::Result<void, RollbackFailure> FromEvent(Kernel::EntityId, std::uint64_t, T) {
        return NotImplemented("a value from an event written into the history (MC-2)");
    }

    // The authoritative values of T, or nullptr when none came.
    template <class T>
    [[nodiscard]] const InboxColumn<T>* Values() const {
        return const_cast<RollbackInbox*>(this)->template Column<T>(false);
    }
    [[nodiscard]] const InboxColumnBase* ValuesOf(Kernel::TypeKey key) const noexcept {
        for (const auto& [entry, column] : columns_) {
            if (entry == key) return column.get();
        }
        return nullptr;
    }
    // Drops what is older than oldest (the strategy, after a decision), keeping per entity the newest entry at or
    // before oldest (values, resources' values, entries and exits alike).
    void Prune(std::uint64_t oldest) {
        for (auto& [key, column] : columns_) column->Prune(oldest);
        for (auto& [key, column] : resources_) column->Prune(oldest);
        for (auto& [entity, entries] : lifecycle_) {
            while (entries.size() >= 2 && std::next(entries.begin())->first <= oldest) entries.erase(entries.begin());
        }
    }

private:
    template <class T>
    InboxColumn<T>* Column(bool make) {
        for (auto& [key, column] : columns_) {
            if (key == Kernel::KeyOf<T>()) return static_cast<InboxColumn<T>*>(column.get());
        }
        if (!make) return nullptr;
        columns_.emplace_back(Kernel::KeyOf<T>(), std::make_unique<InboxColumn<T>>());
        return static_cast<InboxColumn<T>*>(columns_.back().second.get());
    }
    template <class T>
    InboxColumn<T>* ResourceColumn(bool make) {
        for (auto& [key, column] : resources_) {
            if (key == Kernel::KeyOf<T>()) return static_cast<InboxColumn<T>*>(column.get());
        }
        if (!make) return nullptr;
        resources_.emplace_back(Kernel::KeyOf<T>(), std::make_unique<InboxColumn<T>>());
        return static_cast<InboxColumn<T>*>(resources_.back().second.get());
    }
    static Base::Result<void, RollbackFailure> NotImplemented(std::string what) {
        return Base::Err(RollbackFailure{RollbackFailureCode::NotImplemented,
                                                   "RollbackInbox: " + std::move(what) + " is not implemented yet"});
    }

    std::vector<std::pair<Kernel::TypeKey, std::unique_ptr<InboxColumnBase>>> columns_;
    std::vector<std::pair<Kernel::TypeKey, std::unique_ptr<InboxColumnBase>>> resources_;
    std::map<Kernel::EntityId, std::map<std::uint64_t, bool>> lifecycle_;
    std::optional<std::uint64_t> confirmed_;
};

// The inbox is a buffer of what came, not simulation state: nothing is hashed.
inline void HashValue(Kernel::Hasher&, const RollbackInbox&) {}

} // namespace GYO::Engine::Rollback
