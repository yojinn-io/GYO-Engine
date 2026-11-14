#pragma once

#include "engine/base/Assert.hpp"
#include "engine/base/Result.hpp"
#include "engine/kernel/Access.hpp"
#include "engine/kernel/TypeList.hpp"
#include "engine/kernel/detail/Context.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// The System parameter types (spec B.4 point 4) and their ParamTraits.
//
// A System's parameter list is its only access declaration (DI-KS 1): the
// access set is the union of the parameters' Contributions. Only types with a
// ParamTraits specialization are parameters; World&, T&, raw pointers and int
// are not (spec S-2).
//
// Handles are small values that a System receives by value. Each one is
// neither copyable nor movable and has a private constructor (spec B.4 point
// 8): a System cannot keep one past its call or make one it did not declare.
// Fetch (ParamTraits<P>::Fetch) makes the handle for one call: it only
// resolves ids and copies the call's serial numbers, with no side effect and no
// dependence on the order arguments are evaluated in (spec B.4 point 7). A
// handle keeps the World and the call's serial and sync epoch, never the call
// itself; with verification on, using a handle outside its call or after a sync
// point is a violation (spec W-4). Extract parameters run with
// rollback-extract.

namespace GYO::Kernel {

// ---- Query terms (spec B.4 point 4): Query<Q..., F...>

template <class T> struct Read;
template <class T> struct Write;
template <class T> struct With;
template <class T> struct Without;
template <class T> struct Changed;
template <class T> struct Added;

template <class Term>
struct QueryTerm {
    static constexpr bool valid = false;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<>;
};
template <class T>
struct QueryTerm<Read<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::ComponentRead<T>>;
};
template <class T>
struct QueryTerm<Write<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = false;
    using Contribution = TypeList<Access::ComponentWrite<T>>;
};
template <class T>
struct QueryTerm<With<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::Membership<T>>;
};
template <class T>
struct QueryTerm<Without<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::Membership<T>>;
};
template <class T>
struct QueryTerm<Changed<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::ChangeFilter<T>>;
};
template <class T>
struct QueryTerm<Added<T>> {
    static constexpr bool valid = true;
    static constexpr bool readOnly = true;
    using Contribution = TypeList<Access::ChangeFilter<T>>;
};

// ---- Run-time pieces of the query terms

enum class TermKind : std::uint8_t { Read, Write, With, Without, Changed, Added };

template <class Term> struct TermOf;
template <class T> struct TermOf<Read<T>> { using Type = T; static constexpr TermKind kind = TermKind::Read; };
template <class T> struct TermOf<Write<T>> { using Type = T; static constexpr TermKind kind = TermKind::Write; };
template <class T> struct TermOf<With<T>> { using Type = T; static constexpr TermKind kind = TermKind::With; };
template <class T> struct TermOf<Without<T>> { using Type = T; static constexpr TermKind kind = TermKind::Without; };
template <class T> struct TermOf<Changed<T>> { using Type = T; static constexpr TermKind kind = TermKind::Changed; };
template <class T> struct TermOf<Added<T>> { using Type = T; static constexpr TermKind kind = TermKind::Added; };

// Mutable access to one cell (spec A.6 "change stamp"): taking the value for
// writing (Get, non-const ->, *) sets the cell's changed stamp to the step's
// stamp (W-11); reading through Read() does not. Empty when the cell is absent.
template <class T>
class Mut {
public:
    Mut() = default;
    Mut(Detail::Cell<T>* cell, ChangeStamp stamp) noexcept : cell_(cell), stamp_(stamp) {}

    explicit operator bool() const noexcept { return cell_ != nullptr; }
    [[nodiscard]] EntityId Entity() const { GYO_ASSERT(cell_ != nullptr); return cell_->id; }
    [[nodiscard]] const T& Read() const { GYO_ASSERT(cell_ != nullptr); return cell_->value; }
    [[nodiscard]] T& Get() {
        GYO_ASSERT(cell_ != nullptr);
        cell_->changed = stamp_;
        return cell_->value;
    }
    T& operator*() { return Get(); }
    T* operator->() { return &Get(); }
    const T* operator->() const { return &Read(); }

private:
    Detail::Cell<T>* cell_ = nullptr;
    ChangeStamp stamp_ = 0;
};

namespace Detail {

template <class T, class... Terms>
constexpr std::size_t TermIndex() {
    constexpr bool matches[] = {std::is_same_v<typename TermOf<Terms>::Type, T> &&
                                (TermOf<Terms>::kind == TermKind::Read || TermOf<Terms>::kind == TermKind::Write)...};
    for (std::size_t i = 0; i < sizeof...(Terms); ++i) {
        if (matches[i]) return i;
    }
    return sizeof...(Terms);
}

template <class Term, class... Terms>
constexpr std::size_t IndexOfTerm() {
    constexpr bool matches[] = {std::is_same_v<Terms, Term>...};
    for (std::size_t i = 0; i < sizeof...(Terms); ++i) {
        if (matches[i]) return i;
    }
    return sizeof...(Terms);
}

template <class T, class... Terms>
constexpr bool WritesTerm() {
    return ((std::is_same_v<typename TermOf<Terms>::Type, T> && TermOf<Terms>::kind == TermKind::Write) || ...);
}

template <class T, class... List>
inline constexpr bool OneOf = (std::is_same_v<T, List> || ...);

} // namespace Detail

// ---- Bundles and typed command lists

template <class... Components> struct Bundle;
template <class... Components> struct Inserts;
template <class... Components> struct Removes;

// Not a parameter unless specialized below.
template <class P>
struct ParamTraits {
    static constexpr bool valid = false;
};

namespace Detail {

// Base of every handle: no copy, no move, constructed only by its ParamTraits.
class Handle {
protected:
    explicit Handle(SystemContext& call) noexcept
        : world_(call.world), callSerial_(call.callSerial), syncEpoch_(call.syncEpoch) {}
    ~Handle() = default;

    // Spec W-4 (verification on): the handle is used inside the call that fetched it.
    void Check(std::string_view what) const { CheckHandle(*world_, callSerial_, syncEpoch_, what); }

    World* world_;
    std::uint64_t callSerial_;
    std::uint64_t syncEpoch_;

public:
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

template <class P, class Entries, bool ReadOnly>
struct HandleTraits {
    static constexpr bool valid = true;
    static constexpr bool readOnly = ReadOnly;
    // ExclusiveWorld must be the only parameter (spec S-10).
    static constexpr bool sole = false;
    // Extract parameters are legal only in an extract schedule (spec H.6).
    static constexpr bool extractOnly = false;
    using Contribution = Entries;

    // A prvalue of a non-movable type: initializes the System's by-value parameter directly.
    static P Fetch(SystemContext& call) { return P(call); }
};

} // namespace Detail

#define GYO_KERNEL_HANDLE(Name)                                \
    template <class>                                           \
    friend struct ::GYO::Kernel::ParamTraits;                  \
    template <class, class, bool>                              \
    friend struct ::GYO::Kernel::Detail::HandleTraits;         \
    explicit Name(Detail::SystemContext& call)

template <class... Terms>
class Query final : Detail::Handle {
    static_assert(sizeof...(Terms) > 0, "a Query names at least one term");
    static_assert((QueryTerm<Terms>::valid && ...),
                  "Query terms are Read<T>, Write<T>, With<T>, Without<T>, Changed<T> and Added<T>");
    static constexpr std::size_t kTerms = sizeof...(Terms);
    static constexpr TermKind kKinds[kTerms] = {TermOf<Terms>::kind...};

    GYO_KERNEL_HANDLE(Query)
        : Handle(call), ids_{Detail::ComponentIdOf(*call.world, KeyOf<typename TermOf<Terms>::Type>())...},
          stamp_(call.stamp), lastRun_(call.lastRun) {}

public:
    // The matching entities in ascending EntityId order (spec A.3, W-2): the
    // smallest required column drives, every other term is a lookup.
    [[nodiscard]] std::vector<EntityId> Entities() const {
        Check("Query");
        std::vector<EntityId> out;
        const Detail::ColumnBase* driver = nullptr;
        for (std::size_t i = 0; i < kTerms; ++i) {
            if (kKinds[i] == TermKind::Without) continue;
            const Detail::ColumnBase& column = Detail::ColumnAt(*world_, ids_[i]);
            if (driver == nullptr || column.Size() < driver->Size()) driver = &column;
        }
        if (driver == nullptr) {
            for (EntityId id : Detail::AliveEntities(*world_)) {
                if (Matches(id)) out.push_back(id);
            }
            return out;
        }
        for (std::size_t i = 0; i < driver->Size(); ++i) {
            const EntityId id = driver->KeyAt(i);
            if (Matches(id)) out.push_back(id);
        }
        return out;
    }

    [[nodiscard]] std::size_t Count() const { return Entities().size(); }
    [[nodiscard]] bool Contains(EntityId id) const {
        Check("Query");
        return Matches(id);
    }

    // The value of a Read<T> or Write<T> term for one matching entity, or nullptr.
    template <class T>
    [[nodiscard]] const T* Get(EntityId id) const {
        constexpr std::size_t index = Detail::TermIndex<T, Terms...>();
        static_assert(index < kTerms, "Get<T>: T is not a Read<T> or Write<T> term of this Query");
        Check("Query");
        if (!Matches(id)) return nullptr;
        const auto* cell = static_cast<Detail::Column<T>&>(Detail::ColumnAt(*world_, ids_[index])).Find(id);
        return cell != nullptr ? &cell->value : nullptr;
    }

    // Mutable access through a Write<T> term (marks the changed stamp when taken).
    template <class T>
    [[nodiscard]] Mut<T> GetMut(EntityId id) {
        static_assert(Detail::WritesTerm<T, Terms...>(), "GetMut<T>: T is not a Write<T> term of this Query");
        constexpr std::size_t index = Detail::TermIndex<T, Terms...>();
        Check("Query");
        if (!Matches(id)) return {};
        return Mut<T>(static_cast<Detail::Column<T>&>(Detail::ColumnAt(*world_, ids_[index])).Find(id), stamp_);
    }

    // Calls fn(EntityId, values...) for every matching entity in ascending order;
    // values are const T& for Read<T> and Mut<T> for Write<T>, in term order.
    template <class F>
    void ForEach(F&& fn) {
        for (EntityId id : Entities()) {
            std::apply(fn, std::tuple_cat(std::tuple<EntityId>(id), ValueOf<Terms>(id)...));
        }
    }

private:
    [[nodiscard]] bool Matches(EntityId id) const {
        for (std::size_t i = 0; i < kTerms; ++i) {
            const Detail::ColumnBase& column = Detail::ColumnAt(*world_, ids_[i]);
            switch (kKinds[i]) {
            case TermKind::Without:
                if (column.Contains(id)) return false;
                break;
            case TermKind::Changed:
            case TermKind::Added:
                if (!Fresh(column, id, kKinds[i] == TermKind::Added)) return false;
                break;
            default:
                if (!column.Contains(id)) return false;
                break;
            }
        }
        return true;
    }

    // Spec B-12: changed (added) after the mount's last run.
    [[nodiscard]] bool Fresh(const Detail::ColumnBase& column, EntityId id, bool added) const {
        std::size_t low = 0;
        std::size_t high = column.Size();
        while (low < high) {
            const std::size_t mid = low + (high - low) / 2;
            if (column.KeyAt(mid) < id) low = mid + 1; else high = mid;
        }
        if (low == column.Size() || column.KeyAt(low) != id) return false;
        return (added ? column.AddedAt(low) : column.ChangedAt(low)) > lastRun_;
    }

    template <class Term>
    auto ValueOf(EntityId id) {
        using T = typename TermOf<Term>::Type;
        constexpr std::size_t index = Detail::IndexOfTerm<Term, Terms...>();
        auto& column = static_cast<Detail::Column<T>&>(Detail::ColumnAt(*world_, ids_[index]));
        if constexpr (TermOf<Term>::kind == TermKind::Read) {
            return std::tuple<const T&>(column.Find(id)->value);
        } else if constexpr (TermOf<Term>::kind == TermKind::Write) {
            return std::tuple<Mut<T>>(Mut<T>(column.Find(id), stamp_));
        } else {
            return std::tuple<>();
        }
    }

    ComponentId ids_[kTerms];
    ChangeStamp stamp_;
    ChangeStamp lastRun_;
};
template <class... Terms>
struct ParamTraits<Query<Terms...>>
    : Detail::HandleTraits<Query<Terms...>, Concat<typename QueryTerm<Terms>::Contribution...>,
                           (QueryTerm<Terms>::readOnly && ...)> {};

template <class T>
class Res final : Detail::Handle {
    GYO_KERNEL_HANDLE(Res) : Handle(call), id_(Detail::ResourceIdOf(*call.world, KeyOf<T>())) {}

public:
    [[nodiscard]] bool Present() const {
        Check("Res");
        return Slot().value.has_value();
    }
    [[nodiscard]] const T& operator*() const {
        Check("Res");
        GYO_ASSERT(Slot().value.has_value());
        return *Slot().value;
    }
    const T* operator->() const { return &**this; }

private:
    [[nodiscard]] Detail::ResourceSlot<T>& Slot() const {
        return static_cast<Detail::ResourceSlot<T>&>(Detail::ResourceAt(*world_, id_));
    }
    ResourceId id_;
};
template <class T>
struct ParamTraits<Res<T>> : Detail::HandleTraits<Res<T>, TypeList<Access::ResourceRead<T>>, true> {};

template <class T>
class ResMut final : Detail::Handle {
    GYO_KERNEL_HANDLE(ResMut) : Handle(call), id_(Detail::ResourceIdOf(*call.world, KeyOf<T>())), stamp_(call.stamp) {}

public:
    [[nodiscard]] bool Present() const {
        Check("ResMut");
        return Slot().value.has_value();
    }
    [[nodiscard]] const T& Read() const {
        Check("ResMut");
        GYO_ASSERT(Slot().value.has_value());
        return *Slot().value;
    }
    // Taking the value for writing marks the resource's changed stamp (W-11).
    [[nodiscard]] T& Get() {
        Check("ResMut");
        Detail::ResourceSlot<T>& slot = Slot();
        GYO_ASSERT(slot.value.has_value());
        slot.changed = stamp_;
        return *slot.value;
    }
    T& operator*() { return Get(); }
    T* operator->() { return &Get(); }

private:
    [[nodiscard]] Detail::ResourceSlot<T>& Slot() const {
        return static_cast<Detail::ResourceSlot<T>&>(Detail::ResourceAt(*world_, id_));
    }
    ResourceId id_;
    ChangeStamp stamp_;
};
template <class T>
struct ParamTraits<ResMut<T>> : Detail::HandleTraits<ResMut<T>, TypeList<Access::ResourceWrite<T>>, false> {};

template <class E>
class EventReader final : Detail::Handle {
    GYO_KERNEL_HANDLE(EventReader)
        : Handle(call), id_(Detail::EventIdOf(*call.world, KeyOf<E>())), position_(call.planPos),
          tick_(Detail::DomainTick(*call.world, Detail::EventDomain(*call.world, id_))) {}

public:
    // Spec F.3, no cursor: the records of tick t-1 written after this reader's
    // Plan position and of tick t written before it, in (tick, writerPos, seq) order.
    [[nodiscard]] std::vector<const E*> Read() const {
        Check("EventReader");
        const auto& buffer = static_cast<const Detail::EventBuffer<E>&>(Detail::EventsAt(*world_, id_));
        std::vector<const typename Detail::EventBuffer<E>::Record*> picked;
        for (const auto& record : buffer.records) {
            const bool previous = tick_ > 0 && record.tick == tick_ - 1 && record.writerPos > position_;
            const bool current = record.tick == tick_ && record.writerPos < position_;
            if (previous || current) picked.push_back(&record);
        }
        std::sort(picked.begin(), picked.end(), [](const auto* a, const auto* b) {
            if (a->tick != b->tick) return a->tick < b->tick;
            if (a->writerPos != b->writerPos) return a->writerPos < b->writerPos;
            return a->seq < b->seq;
        });
        std::vector<const E*> out;
        out.reserve(picked.size());
        for (const auto* record : picked) out.push_back(&record->payload);
        return out;
    }

private:
    EventTypeId id_;
    std::uint32_t position_;
    std::uint64_t tick_;
};
template <class E>
struct ParamTraits<EventReader<E>> : Detail::HandleTraits<EventReader<E>, TypeList<Access::EventRead<E>>, true> {};

template <class E>
class EventWriter final : Detail::Handle {
    GYO_KERNEL_HANDLE(EventWriter)
        : Handle(call), id_(Detail::EventIdOf(*call.world, KeyOf<E>())), position_(call.planPos),
          tick_(Detail::DomainTick(*call.world, Detail::EventDomain(*call.world, id_))) {}

public:
    // Appends one record: tick = the event domain's tick, writerPos = this
    // call's Plan position (a hook's: its sync point's), seq = the buffer's count.
    void Send(E event) {
        Check("EventWriter");
        auto& buffer = static_cast<Detail::EventBuffer<E>&>(Detail::EventsAt(*world_, id_));
        buffer.records.push_back(typename Detail::EventBuffer<E>::Record{tick_, buffer.nextSeq++, position_, std::move(event)});
    }

private:
    EventTypeId id_;
    std::uint32_t position_;
    std::uint64_t tick_;
};
template <class E>
struct ParamTraits<EventWriter<E>> : Detail::HandleTraits<EventWriter<E>, TypeList<Access::EventWrite<E>>, false> {};

// Untyped deferred insert / remove of zone B, Shared columns only (spec F.1);
// checked when recorded, at run time (F-10). No access, no conflict edge.
class Commands final : Detail::Handle {
    GYO_KERNEL_HANDLE(Commands) : Handle(call) {}

public:
    // Deferred insert into a zone B, Shared column; any other column is Err
    // (spec F.1, F-10: a Programmer Error returned as Err, in every build).
    template <class T>
    [[nodiscard]] Engine::Base::Result<void, KernelError> Insert(EntityId entity, T value) {
        Check("Commands");
        Detail::SystemContext& call = Detail::RequireCall(*world_);
        const ComponentId component = Detail::ComponentIdOf(*world_, KeyOf<T>());
        if (auto error = Detail::CheckUntypedCommand(call, component, entity, true)) return Engine::Base::Err(std::move(*error));
        Detail::Record record = Detail::Record::Of(Detail::RecordKind::Insert, entity, component);
        record.value = std::make_unique<Detail::TypedBox<T>>(std::move(value));
        Detail::Push(call, std::move(record));
        return {};
    }

    template <class T>
    [[nodiscard]] Engine::Base::Result<void, KernelError> Remove(EntityId entity) {
        Check("Commands");
        Detail::SystemContext& call = Detail::RequireCall(*world_);
        const ComponentId component = Detail::ComponentIdOf(*world_, KeyOf<T>());
        if (auto error = Detail::CheckUntypedCommand(call, component, entity, false)) return Engine::Base::Err(std::move(*error));
        Detail::Push(call, Detail::Record::Of(Detail::RecordKind::Remove, entity, component));
        return {};
    }
};
template <>
struct ParamTraits<Commands> : Detail::HandleTraits<Commands, TypeList<Access::UntypedCommands>, false> {};

template <class InsertList, class RemoveList>
class CommandsFor;
template <class... I, class... R>
class CommandsFor<Inserts<I...>, Removes<R...>> final : Detail::Handle {
    GYO_KERNEL_HANDLE(CommandsFor) : Handle(call) {}

public:
    template <class T>
    void Insert(EntityId entity, T value) {
        static_assert(Detail::OneOf<T, I...>, "CommandsFor: T is not in Inserts<...>");
        Check("CommandsFor");
        Detail::SystemContext& call = Detail::RequireCall(*world_);
        Detail::Record record = Detail::Record::Of(Detail::RecordKind::Insert, entity, Detail::ComponentIdOf(*world_, KeyOf<T>()));
        record.value = std::make_unique<Detail::TypedBox<T>>(std::move(value));
        Detail::Push(call, std::move(record));
    }

    template <class T>
    void Remove(EntityId entity) {
        static_assert(Detail::OneOf<T, R...>, "CommandsFor: T is not in Removes<...>");
        Check("CommandsFor");
        Detail::Push(Detail::RequireCall(*world_),
                     Detail::Record::Of(Detail::RecordKind::Remove, entity, Detail::ComponentIdOf(*world_, KeyOf<T>())));
    }
};
template <class... I, class... R>
struct ParamTraits<CommandsFor<Inserts<I...>, Removes<R...>>>
    : Detail::HandleTraits<CommandsFor<Inserts<I...>, Removes<R...>>,
                           TypeList<Access::DeferredWrite<I>..., Access::DeferredWrite<R>...>, false> {};

// Spawns with an id from the World's allotter: writes the allotter and defers
// a write of every bundle component (spec A.1, B3).
template <class B>
class Spawn;
template <class... C>
class Spawn<Bundle<C...>> final : Detail::Handle {
    static_assert(sizeof...(C) > 0, "a Bundle names at least one component");
    GYO_KERNEL_HANDLE(Spawn) : Handle(call) {}

public:
    // Allots the id now (spec A.1: at record time, so the sync point can order
    // by id) and records the spawn; the entity appears after the sync point (F-1).
    // An exhausted allotter is Err, never a wrapped id (W-9).
    [[nodiscard]] Engine::Base::Result<EntityId, KernelError> New(C... values) {
        Check("Spawn");
        Detail::SystemContext& call = Detail::RequireCall(*world_);
        const std::optional<EntityId> id = Detail::Allot(*world_);
        if (!id) {
            return Engine::Base::Err(Detail::CallError(call, KernelErrorCode::AllotterExhausted, "W-9",
                                                       "the entity allotter's range is used up"));
        }
        Detail::Record record = Detail::Record::Of(Detail::RecordKind::Spawn, *id);
        (record.bundle.push_back(Detail::BundleValue{Detail::ComponentIdOf(*world_, KeyOf<C>()),
                                                     std::make_unique<Detail::TypedBox<C>>(std::move(values))}),
         ...);
        Detail::Push(call, std::move(record));
        return *id;
    }
};
template <class... C>
struct ParamTraits<Spawn<Bundle<C...>>>
    : Detail::HandleTraits<Spawn<Bundle<C...>>, TypeList<Access::Spawn, Access::DeferredWrite<C>...>, false> {};

// Spawns with a given id: the bundle's deferred writes, no allotter (spec A.1).
template <class B>
class SpawnWithId;
template <class... C>
class SpawnWithId<Bundle<C...>> final : Detail::Handle {
    static_assert(sizeof...(C) > 0, "a Bundle names at least one component");
    GYO_KERNEL_HANDLE(SpawnWithId) : Handle(call) {}

public:
    // Spawns with an id allotted elsewhere (spec A.1). The id must not be alive
    // at the sync point unless this window despawns it, and must not be in this
    // World's allotter range (GYO_ASSERT at the sync point). A duplicate id that
    // comes from outside data is the caller's to reject first (spec A.1).
    void New(EntityId id, C... values) {
        Check("SpawnWithId");
        Detail::SystemContext& call = Detail::RequireCall(*world_);
        Detail::Record record = Detail::Record::Of(Detail::RecordKind::Spawn, id);
        record.withId = true;
        (record.bundle.push_back(Detail::BundleValue{Detail::ComponentIdOf(*world_, KeyOf<C>()),
                                                     std::make_unique<Detail::TypedBox<C>>(std::move(values))}),
         ...);
        Detail::Push(call, std::move(record));
    }
};
template <class... C>
struct ParamTraits<SpawnWithId<Bundle<C...>>>
    : Detail::HandleTraits<SpawnWithId<Bundle<C...>>, TypeList<Access::DeferredWrite<C>...>, false> {};

// Not a writer, no conflict edge (spec B.3, B.9 trade-off 1); Z5 at run time.
class Despawn final : Detail::Handle {
    GYO_KERNEL_HANDLE(Despawn) : Handle(call) {}

public:
    // Deferred despawn (spec F.2 step 2). A zone B System despawning an entity
    // that has a zone C column is Err when recorded (spec Z5).
    [[nodiscard]] Engine::Base::Result<void, KernelError> Entity(EntityId entity) {
        Check("Despawn");
        Detail::SystemContext& call = Detail::RequireCall(*world_);
        if (auto error = Detail::CheckDespawn(call, entity)) return Engine::Base::Err(std::move(*error));
        Detail::Push(call, Detail::Record::Of(Detail::RecordKind::Despawn, entity));
        return {};
    }
};
template <>
struct ParamTraits<Despawn> : Detail::HandleTraits<Despawn, TypeList<Access::Despawn>, false> {};

// Per-mount state outside the World (spec B.1 runtime, B-12). Forbidden in
// zone C (S-7).
template <class T>
class Local final : Detail::Handle {
    GYO_KERNEL_HANDLE(Local) : Handle(call), value_(Find(call)) {}

public:
    T& operator*() {
        Check("Local");
        return *value_;
    }
    T* operator->() { return &**this; }

private:
    // One value per mount and type (spec B-12); made on first use. Two Local<T>
    // parameters of one T share it.
    static T* Find(Detail::SystemContext& call) {
        GYO_ASSERT(call.locals != nullptr);
        for (auto& [key, value] : *call.locals) {
            if (key == KeyOf<T>()) return static_cast<T*>(value.get());
        }
        auto made = std::make_shared<T>();
        T* pointer = made.get();
        call.locals->emplace_back(KeyOf<T>(), std::move(made));
        return pointer;
    }
    T* value_;
};
template <class T>
struct ParamTraits<Local<T>> : Detail::HandleTraits<Local<T>, TypeList<Access::LocalState<T>>, false> {};

// The domain and tick of the running mount's schedule (spec B.4, B-1).
class CurrentTick final : Detail::Handle {
    GYO_KERNEL_HANDLE(CurrentTick) : Handle(call), domain_(call.domain), tick_(call.tick) {}

public:
    [[nodiscard]] Domain Clock() const { Check("CurrentTick"); return domain_; }
    [[nodiscard]] std::uint64_t Tick() const { Check("CurrentTick"); return tick_; }

private:
    Domain domain_;
    std::uint64_t tick_;
};
template <>
struct ParamTraits<CurrentTick> : Detail::HandleTraits<CurrentTick, TypeList<Access::DomainTickRead>, true> {};

// Write-only diagnostics sink (spec S-8's only effect exception): effect
// Diagnostics, no access entry, no conflict edge.
template <class Record>
class DiagnosticsWriter final : Detail::Handle {
    GYO_KERNEL_HANDLE(DiagnosticsWriter) : Handle(call), tick_(call.tick) {}

public:
    // Appends to the World-side buffer (B-13), which an Interrupted World keeps.
    void Write(Record record) {
        Check("DiagnosticsWriter");
        Detail::DiagnosticsBase* found = Detail::FindDiagnostics(*world_, KeyOf<Record>());
        if (found == nullptr) {
            auto made = std::make_unique<Detail::DiagnosticsBuffer<Record>>();
            found = made.get();
            Detail::AddDiagnostics(*world_, KeyOf<Record>(), std::move(made));
        }
        static_cast<Detail::DiagnosticsBuffer<Record>*>(found)->entries.push_back(
            typename Detail::DiagnosticsBuffer<Record>::Entry{tick_, false, std::move(record)});
    }

private:
    std::uint64_t tick_;
};
template <class Record>
struct ParamTraits<DiagnosticsWriter<Record>>
    : Detail::HandleTraits<DiagnosticsWriter<Record>, TypeList<Access::Diagnostics<Record>>, true> {};

// Reads every column of a named component set (spec B.4 point 4, S13).
template <class SetTag>
class SetRead final : Detail::Handle {
    GYO_KERNEL_HANDLE(SetRead) : Handle(call), members_(Detail::SetMembersOf(*call.world, KeyOf<SetTag>())) {}

public:
    // One member column, read as bytes with its keys and stamps (spec B.4 point 4).
    class ColumnView {
    public:
        [[nodiscard]] ComponentId Id() const noexcept { return id_; }
        [[nodiscard]] std::size_t Size() const noexcept { return column_->Size(); }
        [[nodiscard]] EntityId KeyAt(std::size_t i) const noexcept { return column_->KeyAt(i); }
        [[nodiscard]] ChangeStamp ChangedAt(std::size_t i) const noexcept { return column_->ChangedAt(i); }
        [[nodiscard]] std::span<const std::byte> ValueBytesAt(std::size_t i) const noexcept {
            return column_->ValueBytesAt(i);
        }

    private:
        friend class SetRead;
        ColumnView(ComponentId id, const Detail::ColumnBase& column) : id_(id), column_(&column) {}
        ComponentId id_;
        const Detail::ColumnBase* column_;
    };

    [[nodiscard]] std::size_t Columns() const { Check("SetRead"); return members_.size(); }
    [[nodiscard]] ColumnView Column(std::size_t index) const {
        Check("SetRead");
        return ColumnView(members_[index], Detail::ColumnAt(*world_, members_[index]));
    }

private:
    std::vector<ComponentId> members_;
};
template <class SetTag>
struct ParamTraits<SetRead<SetTag>> : Detail::HandleTraits<SetRead<SetTag>, TypeList<Access::SetReadOf<SetTag>>, true> {};

// Untyped access to the whole World; zone B only (S-13); the only parameter
// (S-10). Conflicts with every System of its stage.
class ExclusiveWorld final : Detail::Handle {
    GYO_KERNEL_HANDLE(ExclusiveWorld) : Handle(call), stamp_(call.stamp) {}

public:
    // Reads any column (spec S-13: ExclusiveWorld reads everything).
    template <class T>
    [[nodiscard]] const T* Get(EntityId entity) const {
        Check("ExclusiveWorld");
        const auto* cell =
            static_cast<Detail::Column<T>&>(Detail::ColumnAt(*world_, Detail::ComponentIdOf(*world_, KeyOf<T>()))).Find(entity);
        return cell != nullptr ? &cell->value : nullptr;
    }

    // Writes only zone B, Shared columns; anything else is Err in every build (S-13).
    template <class T>
    [[nodiscard]] Engine::Base::Result<Mut<T>, KernelError> GetMut(EntityId entity) {
        Check("ExclusiveWorld");
        Detail::SystemContext& call = Detail::RequireCall(*world_);
        const ComponentId component = Detail::ComponentIdOf(*world_, KeyOf<T>());
        if (auto error = Detail::CheckExclusiveWrite(call, component, entity)) return Engine::Base::Err(std::move(*error));
        return Mut<T>(static_cast<Detail::Column<T>&>(Detail::ColumnAt(*world_, component)).Find(entity), stamp_);
    }

    // There is no untyped spawn (spec A.1): always Err, in every build (S-13, J-45).
    [[nodiscard]] Engine::Base::Result<EntityId, KernelError> Spawn() {
        Check("ExclusiveWorld");
        return Engine::Base::Err(Detail::CallError(Detail::RequireCall(*world_), KernelErrorCode::ExclusiveWorldWrite,
                                                   "S-13", "ExclusiveWorld cannot spawn: there is no untyped spawn"));
    }

private:
    ChangeStamp stamp_;
};
template <>
struct ParamTraits<ExclusiveWorld> : Detail::HandleTraits<ExclusiveWorld, TypeList<Access::WholeWorld>, false> {
    static constexpr bool sole = true;
};

// Read-only access to the source World of an extract schedule (spec H.6).
// Extract<Write<T>> has no ParamTraits: it does not compile (DI-KS 13).
template <class Term>
class Extract;
template <class T>
class Extract<Read<T>> final : Detail::Handle {
    // Extract runs with rollback-extract (spec H.6); a Plan never reaches one before that.
    GYO_KERNEL_HANDLE(Extract) : Handle(call) { GYO_ASSERT(call.world == nullptr && "Extract parameters run with rollback-extract"); }
};
template <class T>
struct ParamTraits<Extract<Read<T>>>
    : Detail::HandleTraits<Extract<Read<T>>, TypeList<Access::ExtractComponentRead<T>>, true> {
    static constexpr bool extractOnly = true;
};

template <class R>
class ExtractRes final : Detail::Handle {
    GYO_KERNEL_HANDLE(ExtractRes) : Handle(call) { GYO_ASSERT(call.world == nullptr && "Extract parameters run with rollback-extract"); }
};
template <class R>
struct ParamTraits<ExtractRes<R>> : Detail::HandleTraits<ExtractRes<R>, TypeList<Access::ExtractResourceRead<R>>, true> {
    static constexpr bool extractOnly = true;
};

// The entity and component a hook runs for (spec F.4). A hook parameter only:
// fetching it in a System is a Programmer Error (GYO_ASSERT).
class HookCtx final : Detail::Handle {
    GYO_KERNEL_HANDLE(HookCtx) : Handle(call), entity_(call.hookEntity), component_(call.hookComponent) {
        GYO_ASSERT(call.hook);
    }

public:
    [[nodiscard]] EntityId Entity() const noexcept { return entity_; }
    [[nodiscard]] ComponentId Component() const noexcept { return component_; }

private:
    EntityId entity_;
    ComponentId component_;
};
template <>
struct ParamTraits<HookCtx> : Detail::HandleTraits<HookCtx, TypeList<>, true> {};

#undef GYO_KERNEL_HANDLE

} // namespace GYO::Kernel
