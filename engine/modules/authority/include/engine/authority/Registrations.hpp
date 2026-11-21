#pragma once

// The authority Module's registrations (authority declaration
// docs/architecture/target/v2/authority-module-declaration.zh-Hant.md, 4.3;
// authority-rollback declaration 4 and 6): what decides the game.
//
//   Component<T>, Resource<T>, Message<M>  the author's decision data: recorded,
//       and registered in the kernel with the author's Registrar (the owner stays
//       the author; the kernel sees an ordinary registration, DI-A 2);
//   Mark<T>(), MarkResource(name)  a type another Module registered (the time
//       Module's Tick by type, a state machine's value by name): only recorded;
//   Ingress<&F>(options)  an entry System of outside input: added on the
//       author's behalf, skipped while resimulating (with the server authority);
//   Runs(system, schedules)  an exclusive System that runs other schedules nested;
//   OneEndOnly(system, reason)  a System that writes decision data at one end only.
//
// With a strategy chosen (AuthorityOptions), each call is handed to the
// strategy's OnRegister at the call (DI-A 11: before the Plans are built), which
// registers on the author's behalf (the kernel, the rollback Module, the run
// condition of an entry). Without one, the entry still puts the type into the
// kernel itself and adds the entry System (authority-rollback declaration 4.2,
// check S10), and the Module's Check reports AU-E2 (authority declaration 4.7).
//
// The records are appended to the table in the ModuleBuffer while the World is
// built and put into the World as the Resource AuthorityRegistry when it is
// prepared (read at run time by the digest; a failed replacement leaves the
// old World's as they were).

#include "engine/base/Assert.hpp"
#include "engine/kernel/Hash.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/TypeKey.hpp"
#include "engine/kernel/World.hpp"
#include "engine/orchestration/Setup.hpp"
#include "engine/rollback/Registrations.hpp"

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <type_traits>
#include <vector>

namespace GYO::Engine::Authority {

class AuthorityStrategy;

enum class DataKind : std::uint8_t { Component, Resource, Message };

// What the strategy registers in the rollback Module for a decision type (only a
// relay: the facts live in the rollback Module, L24).
enum class RollbackChoice : std::uint8_t {
    // May roll back (the default).
    Rollback,
    // Does not roll back.
    NoRollback,
    // Recomputed each step before every reader (components only).
    Derived,
    // Nothing on the author's behalf: the author registers it in the rollback Module.
    Self,
};

// The options of Component<T> and Resource<T> (authority declaration 4.3).
struct DataOptions {
    // An outside input's buffer (a decision input): never registered as rollback
    // state (Z8; rollback.Input<T> keeps its history instead).
    bool input = false;
    // Left out of the digest (authority declaration 5.6).
    bool digestExclude = false;
    RollbackChoice rollback = RollbackChoice::Rollback;
};

using IdLookup = std::optional<std::uint32_t> (*)(const Kernel::FrozenRegistry&);

// The type-erased operations of one decision type, made where the type is known.
struct DataOps {
    IdLookup idIn = nullptr;
    // Puts the type into the kernel (name; a message's capacity).
    void (*registerInKernel)(Kernel::Registrar& registrar, const std::string& name, std::uint32_t capacity) = nullptr;
    // The rollback registration of the choice, on the author's behalf (nullptr for a message).
    void (*registerInRollback)(Rollback::RollbackEntry& rollback, RollbackChoice choice) = nullptr;
    // The digest's sum over the type's values in world, each hashed with index (nullptr for a message).
    std::uint64_t (*digest)(const Kernel::World& world, std::uint64_t index) = nullptr;
};

// One decision type. seq: its place in the authority registration order (data and marks counted together; the
// consistency fingerprint's numbering, authority declaration 5.5).
struct DataRecord {
    Kernel::TypeKey key;
    DataKind kind = DataKind::Component;
    std::string name;
    DataOptions options;
    std::uint32_t capacity = 0;
    const DataOps* ops = nullptr;
    std::uint32_t seq = 0;
};

// A type another Module registered, marked as decision data: by type (key and ops set) or by name (byName).
struct MarkRecord {
    std::optional<Kernel::TypeKey> key;
    std::string name;
    DataKind kind = DataKind::Resource;
    const DataOps* ops = nullptr;
    std::uint32_t seq = 0;
};

struct IngressRecord {
    std::string system;
};

struct RunsRecord {
    std::string system;
    std::vector<std::string> schedules;
};

struct OneEndRecord {
    std::string system;
    std::string reason;
};

// The table in the ModuleBuffer: every registration appended while the World is built. Appending after Build is a
// GYO_ASSERT (authority-rollback declaration 6 point 1).
class AuthorityRegistrations {
public:
    explicit AuthorityRegistrations(const Orchestration::ModuleBuffer& buffer) : buffer_(&buffer) {}

    DataRecord& Append(DataRecord record);
    MarkRecord& Append(MarkRecord record);
    void Append(IngressRecord record);
    void Append(RunsRecord record);
    void Append(OneEndRecord record);

    [[nodiscard]] const std::vector<DataRecord>& Data() const noexcept { return data_; }
    [[nodiscard]] const std::vector<MarkRecord>& Marks() const noexcept { return marks_; }
    [[nodiscard]] const std::vector<IngressRecord>& Ingress() const noexcept { return ingress_; }
    [[nodiscard]] const std::vector<RunsRecord>& Runs() const noexcept { return runs_; }
    [[nodiscard]] const std::vector<OneEndRecord>& OneEnd() const noexcept { return oneEnd_; }
    // Whether any registration entry was called (a World with none needs no strategy, authority declaration 4.7).
    [[nodiscard]] bool Any() const noexcept {
        return !data_.empty() || !marks_.empty() || !ingress_.empty() || !runs_.empty() || !oneEnd_.empty();
    }

private:
    void Open() const;
    const Orchestration::ModuleBuffer* buffer_;
    std::vector<DataRecord> data_;
    std::vector<MarkRecord> marks_;
    std::vector<IngressRecord> ingress_;
    std::vector<RunsRecord> runs_;
    std::vector<OneEndRecord> oneEnd_;
    std::uint32_t next_ = 0;
};

// The registrations of one World, put into it when it is prepared (the digest reads them at run time). Its hash
// function hashes names and flags only, never a type key or a function pointer (declaration 3.3).
struct AuthorityRegistry {
    std::vector<DataRecord> data;
    std::vector<MarkRecord> marks;
    std::vector<IngressRecord> ingress;
    std::vector<RunsRecord> runs;
    std::vector<OneEndRecord> oneEnd;
    // The World's strategy (it holds no per-World state).
    std::shared_ptr<AuthorityStrategy> strategy;
};

void HashValue(Kernel::Hasher& h, const AuthorityRegistry& registry);

namespace Detail {

std::uint64_t CellDigest(std::uint64_t index, const Kernel::Hasher& value);

template <class T>
std::optional<std::uint32_t> ComponentIdIn(const Kernel::FrozenRegistry& registry) {
    return registry.ComponentIdOf<T>();
}
template <class T>
std::optional<std::uint32_t> ResourceIdIn(const Kernel::FrozenRegistry& registry) {
    return registry.ResourceIdOf<T>();
}
template <class T>
std::optional<std::uint32_t> MessageIdIn(const Kernel::FrozenRegistry& registry) {
    return registry.MessageIdOf<T>();
}

template <class T>
void RegisterComponent(Kernel::Registrar& r, const std::string& name, std::uint32_t) {
    r.Component<T>({.name = name});
}
template <class T>
void RegisterResource(Kernel::Registrar& r, const std::string& name, std::uint32_t) {
    r.Resource<T>({.name = name});
}
template <class T>
void RegisterMessage(Kernel::Registrar& r, const std::string& name, std::uint32_t capacity) {
    r.Message<T>({.name = name, .capacity = capacity});
}

// A type that rolls back is compared with == on behalf; a type without one is registered by the author.
template <class T>
void ComponentInRollback(Rollback::RollbackEntry& rollback, RollbackChoice choice) {
    if (choice == RollbackChoice::Derived) rollback.Derived<T>();
    if (choice != RollbackChoice::Rollback) return;
    if constexpr (std::equality_comparable<T> && std::is_copy_constructible_v<T>) {
        rollback.Component<T>();
    } else {
        GYO_ASSERT(false && "authority.Component<T>: T has no == (or is not copyable), so it cannot roll back on the "
                            "author's behalf: choose RollbackChoice::Self and register rollback.Component<T>({.equal = ...})");
    }
}
template <class T>
void ResourceInRollback(Rollback::RollbackEntry& rollback, RollbackChoice choice) {
    GYO_ASSERT(choice != RollbackChoice::Derived && "authority.Resource<T>: Derived is for components only");
    if (choice != RollbackChoice::Rollback) return;
    if constexpr (std::equality_comparable<T> && std::is_copy_constructible_v<T>) {
        rollback.Resource<T>();
    } else {
        GYO_ASSERT(false && "authority.Resource<T>: T has no == (or is not copyable), so it cannot roll back on the "
                            "author's behalf: choose RollbackChoice::Self and register rollback.Resource<T>({.equal = ...})");
    }
}

// Each value of T: a hash of (index, value), summed (mod 2^64; order free, and two equal cells do not cancel as
// with XOR; authority-rollback declaration 4.4). No entity id (4.4: not decided in this batch).
template <class T>
std::uint64_t ComponentDigest(const Kernel::World& world, std::uint64_t index) {
    if (!world.Types().ComponentIdOf<T>()) return 0;
    std::uint64_t sum = 0;
    for (const Kernel::EntityId entity : world.Keys<T>()) {
        Kernel::Hasher h;
        HashValue(h, *world.Get<T>(entity));
        sum += CellDigest(index, h);
    }
    return sum;
}
template <class T>
std::uint64_t ResourceDigest(const Kernel::World& world, std::uint64_t index) {
    if (!world.Types().ResourceIdOf<T>()) return 0;
    const T* value = world.Resource<T>();
    if (value == nullptr) return 0;
    Kernel::Hasher h;
    HashValue(h, *value);
    return CellDigest(index, h);
}

template <class T>
inline constexpr DataOps kComponentOps{&ComponentIdIn<T>, &RegisterComponent<T>, &ComponentInRollback<T>, &ComponentDigest<T>};
template <class T>
inline constexpr DataOps kResourceOps{&ResourceIdIn<T>, &RegisterResource<T>, &ResourceInRollback<T>, &ResourceDigest<T>};
template <class T>
inline constexpr DataOps kMessageOps{&MessageIdIn<T>, &RegisterMessage<T>, nullptr, nullptr};
// A marked type by type: found and hashed, never registered (another Module did).
template <class T>
inline constexpr DataOps kMarkedResourceOps{&ResourceIdIn<T>, nullptr, nullptr, &ResourceDigest<T>};
template <class T>
inline constexpr DataOps kMarkedComponentOps{&ComponentIdIn<T>, nullptr, nullptr, &ComponentDigest<T>};

template <auto Fn>
Kernel::RegistrationSeq AddSystemOf(Kernel::Registrar& r, Kernel::SystemOptions options, std::source_location source) {
    return r.AddSystem<Fn>(std::move(options), source);
}

} // namespace Detail

// What a strategy's OnRegister gets: the registration just made, and the ways to register on the author's behalf
// (authority declaration 4.4). Exactly one of Data(), Mark(), Ingress(), Runs(), OneEnd() is set.
class RegisterContext {
public:
    [[nodiscard]] const DataRecord* Data() const noexcept { return data_; }
    [[nodiscard]] const MarkRecord* Mark() const noexcept { return mark_; }
    [[nodiscard]] const IngressRecord* Ingress() const noexcept { return ingress_; }
    [[nodiscard]] const RunsRecord* Runs() const noexcept { return runs_; }
    [[nodiscard]] const OneEndRecord* OneEnd() const noexcept { return oneEnd_; }

    // The author's Registrar (the owner stays the author).
    [[nodiscard]] Kernel::Registrar& Registrar() noexcept { return setup_->Registrar(); }
    // The rollback Module's public registrations, on the author's behalf (Source::OnBehalf).
    [[nodiscard]] Rollback::RollbackEntry& Rollback() noexcept { return rollback_; }

    // Data: puts the type into the kernel with the author's Registrar.
    void RegisterInKernel();
    // Data (a component or a resource, not an input): the rollback registration its options choose, on the
    // author's behalf. Nothing for a message, an input or RollbackChoice::Self.
    void RegisterInRollback();
    // Ingress: the options the entry is added with (to change before AddIngress), and the AddSystem.
    [[nodiscard]] Kernel::SystemOptions& IngressOptions() noexcept { return *ingressOptions_; }
    void AddIngress();

private:
    friend class AuthorityEntry;
    explicit RegisterContext(Orchestration::WorldSetup& setup)
        : setup_(&setup), rollback_(setup, Rollback::Source::OnBehalf) {}

    Orchestration::WorldSetup* setup_;
    Rollback::RollbackEntry rollback_;
    const DataRecord* data_ = nullptr;
    const MarkRecord* mark_ = nullptr;
    const IngressRecord* ingress_ = nullptr;
    const RunsRecord* runs_ = nullptr;
    const OneEndRecord* oneEnd_ = nullptr;
    Kernel::SystemOptions* ingressOptions_ = nullptr;
    Kernel::RegistrationSeq (*addIngress_)(Kernel::Registrar&, Kernel::SystemOptions, std::source_location) = nullptr;
    std::source_location ingressSource_;
    bool ingressAdded_ = false;
};

// The author's way in (authority declaration 4.3): made from the WorldSetup of any Build of a World the authority
// Module is built into (the product's own included); without the Module it is a GYO_ASSERT.
class AuthorityEntry {
public:
    explicit AuthorityEntry(Orchestration::WorldSetup& setup);

    // A decision component. T has a hash function (the digest; checked at compile time).
    template <class T>
    void Component(std::string name, DataOptions options = {}) {
        static_assert(Kernel::Hashable<T>, "authority.Component<T>: T needs a hash function (HashValue) for the digest");
        Data(DataRecord{Kernel::KeyOf<T>(), DataKind::Component, std::move(name), options, 0, &Detail::kComponentOps<T>, 0});
    }
    template <class T>
    void Resource(std::string name, DataOptions options = {}) {
        static_assert(Kernel::Hashable<T>, "authority.Resource<T>: T needs a hash function (HashValue) for the digest");
        Data(DataRecord{Kernel::KeyOf<T>(), DataKind::Resource, std::move(name), options, 0, &Detail::kResourceOps<T>, 0});
    }
    // A decision message (not in the digest; its capacity is in the consistency fingerprint).
    template <class M>
    void Message(std::string name, std::uint32_t capacity = 1024) {
        Data(DataRecord{Kernel::KeyOf<M>(), DataKind::Message, std::move(name), {}, capacity, &Detail::kMessageOps<M>, 0});
    }
    // A resource another Module registered (the time Module's Tick): only marked as decision data.
    template <class T>
    void Mark() {
        static_assert(Kernel::Hashable<T>, "authority.Mark<T>: T needs a hash function (HashValue) for the digest");
        MarkType(MarkRecord{Kernel::KeyOf<T>(), {}, DataKind::Resource, &Detail::kMarkedResourceOps<T>, 0});
    }
    // A component another Module registered.
    template <class T>
    void MarkComponent() {
        static_assert(Kernel::Hashable<T>, "authority.MarkComponent<T>: T needs a hash function (HashValue) for the digest");
        MarkType(MarkRecord{Kernel::KeyOf<T>(), {}, DataKind::Component, &Detail::kMarkedComponentOps<T>, 0});
    }
    // A resource another Module registered, by its kernel name (a state machine's value; the authority Module
    // does not depend on that Module). Not in the digest in this batch (no typed hash).
    void MarkResource(std::string name);
    // An entry System of outside input (added on the author's behalf).
    template <auto Fn>
    void Ingress(Kernel::SystemOptions options, std::source_location source = std::source_location::current()) {
        IngressOf(std::move(options), &Detail::AddSystemOf<Fn>, source);
    }
    // An exclusive System that runs the schedules nested.
    void Runs(std::string system, std::vector<std::string> schedules);
    // A System that writes decision data at one end only: not in the consistency fingerprint.
    void OneEndOnly(std::string system, std::string reason);

private:
    void Data(DataRecord record);
    void MarkType(MarkRecord record);
    void IngressOf(Kernel::SystemOptions options,
                   Kernel::RegistrationSeq (*add)(Kernel::Registrar&, Kernel::SystemOptions, std::source_location),
                   std::source_location source);

    Orchestration::WorldSetup* setup_;
    AuthorityRegistrations* table_;
    AuthorityStrategy* strategy_;
};

} // namespace GYO::Engine::Authority
