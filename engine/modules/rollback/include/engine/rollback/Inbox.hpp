#pragma once

// The rollback Module's way in for authoritative values (authority-rollback
// declaration 5.5, the shape the user approved again on 2026-11-20): one entry
// for every strategy, a World Resource. The network Module, or the author,
// writes into it:
//   - Value<T>(entity, tick, value): component T of entity had value at tick;
//   - Removed<T>(entity, tick): entity certainly had no T at tick;
//   - ConfirmedThrough(tick): every snapshot up to tick has come (the
//     confirmed point C; the reliable event stream does not count for it).
// A value of a tick later than C stays and is compared when C reaches it; a
// late value of a tick before C counts as "the last value at or before C" (MC-1:
// an event is never dropped silently).
//
// This batch makes the behaviour thin (declaration 5.5): a whole snapshot, the
// values of one tick and the confirmed point; what is not said is a spawn or a
// despawn of the game. The four entries below return NotImplemented
// (RollbackFailure, an E2 the caller decides on): part of an entity's
// components, "this snapshot does not carry it" (interest management), an
// explicit entry or exit (a loaded or unloaded segment), and a value from an
// event written into the history (MC-2).

#include "engine/base/Result.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"

#include <cstdint>
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

    // Not this batch's behaviour (declaration 5.5): each returns NotImplemented.
    template <class T>
    Base::Result<void, RollbackFailure> PartialValue(Kernel::EntityId, std::uint64_t, T) {
        return NotImplemented("a value of part of an entity's components");
    }
    Base::Result<void, RollbackFailure> NotSent(Kernel::EntityId, std::uint64_t) {
        return NotImplemented("\"this snapshot does not carry it\" (interest management)");
    }
    Base::Result<void, RollbackFailure> Entered(Kernel::EntityId, std::uint64_t) {
        return NotImplemented("an explicit entry of an entity (a loaded segment)");
    }
    Base::Result<void, RollbackFailure> Exited(Kernel::EntityId, std::uint64_t) {
        return NotImplemented("an explicit exit of an entity (an unloaded segment)");
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
    // Drops what is older than oldest (the strategy, after a decision).
    void Prune(std::uint64_t oldest) {
        for (auto& [key, column] : columns_) column->Prune(oldest);
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
    static Base::Result<void, RollbackFailure> NotImplemented(std::string what) {
        return Base::Err(RollbackFailure{RollbackFailureCode::NotImplemented,
                                                   "RollbackInbox: " + std::move(what) + " is not implemented yet"});
    }

    std::vector<std::pair<Kernel::TypeKey, std::unique_ptr<InboxColumnBase>>> columns_;
    std::optional<std::uint64_t> confirmed_;
};

// The inbox is a buffer of what came, not simulation state: nothing is hashed.
inline void HashValue(Kernel::Hasher&, const RollbackInbox&) {}

} // namespace GYO::Engine::Rollback
