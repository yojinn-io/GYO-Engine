#pragma once

// The two Resources of one state (state machine declaration docs/architecture/target/v2/state-machine-declaration.zh-Hant.md,
// section 5.2 rows 3 and 4; Design Invariants 1 and 4 in docs/architecture/target/v2/state-machine.zh-Hant.md).
//
// StateMachine<S> holds the current value. Once the state is entered, an author's code changes it only through a
// request (StateMachineRequest<S>), applied by the state's apply System: outside the Module's Detail, the type has
// no constructor from a value of S, no public default constructor and no member that sets a value, apart from
// SetInitial before the first apply (DI 1; the user's "use and patch" of Bevy's State, which a public State::new lets
// anyone overwrite). Copy, move and == are public: rollback and CarryOver put back a value saved before; an author
// can put back a saved value but cannot make one that was never saved.

#include "engine/base/Assert.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/state_machine/Traits.hpp"

#include <cstdint>
#include <optional>

namespace GYO::Engine::StateMachine {

// Where a state is: not entered yet (before the first apply), absent (a child whose parent is not at its value),
// or at a value.
enum class StateStatus : std::uint8_t {
    NotEntered = 1,
    Absent,
    Active,
};

namespace Detail {
template <class S>
struct Access;
}

template <State S>
class StateMachine final {
public:
    StateMachine(const StateMachine&) = default;
    StateMachine& operator=(const StateMachine&) = default;
    StateMachine(StateMachine&&) noexcept = default;
    StateMachine& operator=(StateMachine&&) noexcept = default;

    [[nodiscard]] StateStatus Status() const noexcept { return status_; }
    // The current value; nullopt when not entered or absent.
    [[nodiscard]] std::optional<S> Current() const noexcept {
        if (status_ != StateStatus::Active) return std::nullopt;
        return value_;
    }

    // Before the first apply only (GYO_ASSERT otherwise): the value the first apply enters instead of the traits'
    // initial one. For a child, the value it takes when it appears in the first apply; used once, and dropped when
    // the parent's first value is not the child's parent value (declaration section 2.2, option (i); section 6:
    // loading a save enters the saved value directly, like Bevy's insert_state).
    void SetInitial(S value) noexcept {
        GYO_ASSERT(!applied_ && "StateMachine::SetInitial after the first apply");
        initial_ = value;
    }

    friend bool operator==(const StateMachine&, const StateMachine&) = default;

    // Every field (World::Summary, the authority Module's digest).
    friend void HashValue(Kernel::Hasher& h, const StateMachine& v) {
        HashValue(h, v.status_);
        HashValue(h, v.value_);
        HashValue(h, v.initial_);
        HashValue(h, v.applied_);
    }

private:
    friend struct Detail::Access<S>;
    StateMachine() = default;

    StateStatus status_ = StateStatus::NotEntered;
    S value_ = StateMachineTraits<S>::initial;
    std::optional<S> initial_;
    bool applied_ = false;
};

// The pending request of one state: written by any System; taken by the apply System at the start of an apply.
// Two requests before one apply: the later one wins, the earlier one is counted (the user's
// v8-arch-gyo2-state-machine-overwritten-request-counted; a diagnostic, not reset while the World lives).
template <State S>
class StateMachineRequest final {
public:
    StateMachineRequest() = default;

    // Asks for value at the next apply: it runs the exits and enters (also for the current value).
    void Request(S value) noexcept {
        if (pending_) ++overwritten_;
        pending_ = value;
    }
    [[nodiscard]] std::optional<S> Pending() const noexcept { return pending_; }
    // Requests overwritten before an apply took them, since the World was built (rollback puts it back with the
    // rest of the value; declaration section 10).
    [[nodiscard]] std::uint64_t Overwritten() const noexcept { return overwritten_; }

    // Every field: a rollback puts the value back only when it differs (RestoreSet::Resource), so the count must
    // count. The hash leaves the count out: a diagnostic, not part of the game's state (declaration 5.2 row 4).
    friend bool operator==(const StateMachineRequest&, const StateMachineRequest&) = default;
    friend void HashValue(Kernel::Hasher& h, const StateMachineRequest& v) { HashValue(h, v.pending_); }

private:
    friend struct Detail::Access<S>;

    std::optional<S> pending_;
    std::uint64_t overwritten_ = 0;
};

namespace Detail {

// The Module's own access to the closed type (the registration's prepare function and the apply System).
template <class S>
struct Access {
    [[nodiscard]] static StateMachine<S> Root() { return StateMachine<S>{}; }
    [[nodiscard]] static StateMachine<S> Child() {
        StateMachine<S> made;
        made.status_ = StateStatus::Absent;
        return made;
    }
    [[nodiscard]] static bool Applied(const StateMachine<S>& m) noexcept { return m.applied_; }
    [[nodiscard]] static std::optional<S> Initial(const StateMachine<S>& m) noexcept { return m.initial_; }
    static void Enter(StateMachine<S>& m, S value) noexcept {
        m.status_ = StateStatus::Active;
        m.value_ = value;
    }
    // An absent child keeps nothing of where it was: the same whatever its history (World::Summary, the digest).
    static void Leave(StateMachine<S>& m) noexcept {
        m.status_ = StateStatus::Absent;
        m.value_ = StateMachineTraits<S>::initial;
    }
    // The first apply has used (or dropped) the initial value given with SetInitial.
    static void MarkApplied(StateMachine<S>& m) noexcept {
        m.applied_ = true;
        m.initial_.reset();
    }
    [[nodiscard]] static std::optional<S> Take(StateMachineRequest<S>& r) noexcept {
        std::optional<S> taken = r.pending_;
        r.pending_.reset();
        return taken;
    }
};

} // namespace Detail

} // namespace GYO::Engine::StateMachine
