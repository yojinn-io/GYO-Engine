#pragma once

#include "kernel_spike/Assert.hpp"
#include "kernel_spike/Descriptor.hpp"
#include "kernel_spike/PageGuard.hpp"
#include "kernel_spike/Registry.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

// The World stores bytes under ids: one type-erased column per component
// (iterated by row index), one slot per resource, one channel per event, and
// the deferred queue. It never sees a component, resource or event type
// except through TypeOps and the typed convenience templates.
//
// Access checks (KERNEL_SPIKE_ACCESS_CHECKS, on by default unless NDEBUG):
//  1. Every World access made while a System runs is compared with that
//     System's descriptor; an undeclared id fails KERNEL_SPIKE_ASSERT.
//  2. Around each System run the World compares storage bytes with a
//     snapshot: after the run, every storage the System did not declare as
//     written must be unchanged; before the run, every storage must be
//     unchanged since the last snapshot unless the World API wrote it.
//     This is what sees writes that never call the World (const_cast, raw
//     pointers, references kept from an earlier run).
// Release builds (KERNEL_SPIKE_ACCESS_CHECKS 0) do neither.
//
// Page guard (WorldOptions::pageGuard, a run-time option in every build;
// PageGuard.hpp): a third layer that catches the write itself. Off by
// default; off means operator new storage and no protection call, exactly as
// without the option. On: Exclusive columns get whole pages; a Run (System.hpp)
// makes them read-only, lifts the protection of the columns a (c) System
// declared as written for that System's call (all of them for a (b) System),
// and makes them writable again when it ends. Faults are returned as Err
// (Violation), alongside the second layer's differences.

#if !defined(KERNEL_SPIKE_ACCESS_CHECKS)
#if defined(NDEBUG)
#define KERNEL_SPIKE_ACCESS_CHECKS 0
#else
#define KERNEL_SPIKE_ACCESS_CHECKS 1
#endif
#endif

namespace KernelSpike {

namespace Detail {

inline std::byte* AllocateBytes(std::size_t bytes, std::size_t align) {
    return static_cast<std::byte*>(::operator new(bytes, std::align_val_t{align}));
}

inline void DeallocateBytes(std::byte* data, std::size_t align) noexcept {
    ::operator delete(data, std::align_val_t{align});
}

// A dense type-erased array. A page-guarded column (World option pageGuard,
// Exclusive columns only) keeps its rows in whole pages of its own,
// registered with the page guard's range table under (world, id); any other
// column uses operator new.
class Column final {
public:
    Column(const TypeOps& ops, const void* world, ComponentId id, bool pageGuarded) noexcept
        : ops_(ops), world_(world), id_(id), pageGuarded_(pageGuarded) {}
    Column(Column&& other) noexcept
        : ops_(other.ops_), world_(other.world_), id_(other.id_), pageGuarded_(other.pageGuarded_),
          data_(std::exchange(other.data_, nullptr)), count_(std::exchange(other.count_, 0)),
          capacity_(std::exchange(other.capacity_, 0)), pageBytes_(std::exchange(other.pageBytes_, 0)) {}
    Column(const Column&) = delete;
    Column& operator=(const Column&) = delete;
    Column& operator=(Column&&) = delete;
    ~Column() {
        for (std::size_t row = 0; row < count_; ++row) {
            ops_.destroy(data_ + row * ops_.size);
        }
        Release(data_, pageBytes_);
    }

    // Moves the object at source into a new last row.
    void PushMove(void* source) {
        Reserve(count_ + 1);
        ops_.moveConstruct(data_ + count_ * ops_.size, source);
        ++count_;
    }

    [[nodiscard]] std::span<std::byte> Bytes() noexcept { return {data_, count_ * ops_.size}; }
    [[nodiscard]] std::span<const std::byte> Bytes() const noexcept { return {data_, count_ * ops_.size}; }

    // The whole pages of a page-guarded column; empty for any other column
    // and for a page-guarded column with no storage yet.
    [[nodiscard]] std::span<std::byte> Pages() const noexcept {
        return {pageBytes_ != 0 ? data_ : nullptr, pageBytes_};
    }

private:
    void Reserve(std::size_t rows) {
        if (rows <= capacity_) {
            return;
        }
        const std::size_t capacity = std::max({rows, capacity_ * 2, std::size_t{4}});
        std::byte* data = nullptr;
        std::size_t pageBytes = 0;
        if (pageGuarded_) {
            KERNEL_SPIKE_ASSERT(ops_.align <= PageGuard::PageSize(), "page guard: alignment larger than a page");
            pageBytes = PageGuard::RoundUpToPages(capacity * ops_.size);
            data = PageGuard::AllocatePages(pageBytes);
            PageGuard::AddRange(world_, id_, data, pageBytes);
        } else {
            data = AllocateBytes(capacity * ops_.size, ops_.align);
        }
        for (std::size_t row = 0; row < count_; ++row) {
            ops_.moveConstruct(data + row * ops_.size, data_ + row * ops_.size);
            ops_.destroy(data_ + row * ops_.size);
        }
        Release(data_, pageBytes_);
        data_ = data;
        capacity_ = capacity;
        pageBytes_ = pageBytes;
    }

    void Release(std::byte* data, std::size_t pageBytes) noexcept {
        if (data == nullptr) {
            return;
        }
        if (pageBytes != 0) {
            PageGuard::RemoveRange(data);
            PageGuard::FreePages(data, pageBytes);
            return;
        }
        DeallocateBytes(data, ops_.align);
    }

    TypeOps ops_;
    const void* world_;
    ComponentId id_;
    bool pageGuarded_;
    std::byte* data_ = nullptr;
    std::size_t count_ = 0;
    std::size_t capacity_ = 0;
    std::size_t pageBytes_ = 0;  // 0 unless the storage is page-guarded
};

// Storage for at most one object (a resource, or an event channel).
class Slot final {
public:
    explicit Slot(const TypeOps& ops) noexcept : ops_(ops) {}
    Slot(Slot&& other) noexcept
        : ops_(other.ops_), data_(std::exchange(other.data_, nullptr)), present_(std::exchange(other.present_, false)) {}
    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;
    Slot& operator=(Slot&&) = delete;
    ~Slot() {
        if (present_) {
            ops_.destroy(data_);
        }
        if (data_ != nullptr) {
            DeallocateBytes(data_, ops_.align);
        }
    }

    void* Storage() {
        if (data_ == nullptr) {
            data_ = AllocateBytes(ops_.size, ops_.align);
        }
        return data_;
    }
    void MarkConstructed() noexcept { present_ = true; }

    // Replaces any current object with one moved from source.
    void EmplaceMove(void* source) {
        void* storage = Storage();
        if (present_) {
            ops_.destroy(storage);
            present_ = false;
        }
        ops_.moveConstruct(storage, source);
        present_ = true;
    }

    [[nodiscard]] bool HasValue() const noexcept { return present_; }
    [[nodiscard]] void* Object() const noexcept { return data_; }
    // The object's own bytes. For a type that owns heap memory this is the
    // handle (pointer, size), not what it points to.
    [[nodiscard]] std::span<const std::byte> InlineBytes() const noexcept {
        return present_ ? std::span<const std::byte>(data_, ops_.size) : std::span<const std::byte>();
    }

private:
    TypeOps ops_;
    std::byte* data_ = nullptr;
    bool present_ = false;
};

} // namespace Detail

class System;
class Executor;
struct PageGuardProbe;  // test access to the page guard's steps (cost measurement)

// The spike's stand-in for the spec's KernelSettings (a construction argument
// instead of a Resource written from start-up parameters).
struct WorldOptions {
    bool pageGuard = false;
};

// One entry of a Run's Err: the spike's stand-in for spec B-9's Siblings
// list (a plain list).
struct Violation {
    enum class Source : std::uint8_t {
        // Page guard: a (c) System wrote a guarded column it did not declare
        // as written (the fault address is in address).
        PageGuard,
        // Page guard: more faults in one System call than RunContext has
        // slots; dropped holds how many were not recorded.
        PageGuardDropped,
        // Second layer: storage the System did not declare as written differs
        // after its call.
        AfterSystem,
        // Second layer: storage changed since the last comparison without the
        // World API. Attributed to the previous segment, not to the next
        // System, which is not called (spec E.2 step 0); system is empty.
        BeforeSystem,
    };

    Source source;
    std::string system;
    AccessKey key;
    std::uintptr_t address = 0;
    std::size_t dropped = 0;
};

// Page-guard diagnostics of one World.
struct PageGuardStats {
    // Protection changes the World asked for (World::SetColumnWritable), one
    // per column per change, including columns with no storage yet.
    std::uint64_t protectCalls = 0;
};

class World final {
public:
    using Command = std::function<void(World&)>;

    explicit World(Registry registry, WorldOptions options = {})
        : registry_(std::move(registry)), pageGuard_(options.pageGuard) {
        if (pageGuard_) {
            PageGuard::InstallFaultHandler();
        }
        const std::span<const Registry::Entry> components = registry_.Components();
        for (std::size_t id = 0; id < components.size(); ++id) {
            const bool guarded = components[id].writePolicy == WritePolicy::Exclusive;
            guarded_.push_back(guarded);
            columns_.emplace_back(components[id].ops, this, static_cast<ComponentId>(id), pageGuard_ && guarded);
        }
        for (const Registry::Entry& entry : registry_.Resources()) {
            resources_.emplace_back(entry.ops);
        }
        for (const Registry::Entry& entry : registry_.Events()) {
            Detail::Slot& channel = channels_.emplace_back(entry.ops);
            entry.constructChannel(channel.Storage());
            channel.MarkConstructed();
        }
        const std::size_t storages = columns_.size() + resources_.size() + channels_.size();
        baseline_.resize(storages);
        dirty_.assign(storages, true);
    }
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    [[nodiscard]] const Registry& Types() const noexcept { return registry_; }

    // ---- Page-guard diagnostics.
    [[nodiscard]] bool PageGuardEnabled() const noexcept { return pageGuard_; }
    [[nodiscard]] const PageGuardStats& GuardStats() const noexcept { return guardStats_; }
    // The pages of a page-guarded column; empty when the column is not
    // page-guarded (Shared, or pageGuard off) or has no storage yet.
    [[nodiscard]] std::span<const std::byte> GuardedPages(ComponentId id) const noexcept {
        return columns_[id].Pages();
    }

    // ---- Exclusive API: host and setup code, and deferred commands. A
    // structural change while a System runs is a Programmer Error in every
    // build: handles hold pointers into the columns.

    template <class T>
    void Push(T value) {
        KERNEL_SPIKE_ASSERT(Detail::runningSystem == nullptr,
                            "structural change during a System run; queue it through Deferred");
        const ComponentId id = registry_.ComponentIdOf<T>();
        MarkWritten(AccessKey{AccessKind::Component, id});
        columns_[id].PushMove(&value);
    }

    template <class T>
    void InsertResource(T value) {
        KERNEL_SPIKE_ASSERT(Detail::runningSystem == nullptr,
                            "structural change during a System run; queue it through Deferred");
        const ResourceId id = registry_.ResourceIdOf<T>();
        MarkWritten(AccessKey{AccessKind::Resource, id});
        resources_[id].EmplaceMove(&value);
    }

    // Sync point: runs the queued commands in the order they were queued.
    void ApplyDeferred() {
        KERNEL_SPIKE_ASSERT(Detail::runningSystem == nullptr, "ApplyDeferred during a System run");
        std::vector<Command> commands = std::exchange(deferred_, {});
        for (Command& command : commands) {
            command(*this);
        }
    }

    // Sync point: empties every event channel.
    void ClearEvents() {
        KERNEL_SPIKE_ASSERT(Detail::runningSystem == nullptr, "ClearEvents during a System run");
        const std::span<const Registry::Entry> events = registry_.Events();
        for (std::size_t id = 0; id < events.size(); ++id) {
            MarkWritten(AccessKey{AccessKind::Event, static_cast<EventId>(id)});
            events[id].clearChannel(channels_[id].Object());
        }
    }

    // ---- Checked access by id. Handles call these with the access their
    // System declared. Any other caller during a System run (a captured
    // World&, a global pointer) is checked against the running System too.
    // Read access returns const storage; mutable storage takes Write access.

    [[nodiscard]] std::span<const std::byte> ReadColumn(ComponentId id) {
        Check(AccessKey{AccessKind::Component, id}, AccessMode::Read);
        return std::as_const(columns_[id]).Bytes();
    }
    [[nodiscard]] std::span<std::byte> WriteColumn(ComponentId id) {
        Check(AccessKey{AccessKind::Component, id}, AccessMode::Write);
        return columns_[id].Bytes();
    }
    [[nodiscard]] const void* ReadResource(ResourceId id) {
        Check(AccessKey{AccessKind::Resource, id}, AccessMode::Read);
        KERNEL_SPIKE_ASSERT(resources_[id].HasValue(), "resource was registered but never inserted");
        return resources_[id].Object();
    }
    [[nodiscard]] void* WriteResource(ResourceId id) {
        Check(AccessKey{AccessKind::Resource, id}, AccessMode::Write);
        KERNEL_SPIKE_ASSERT(resources_[id].HasValue(), "resource was registered but never inserted");
        return resources_[id].Object();
    }
    [[nodiscard]] const void* ReadEvents(EventId id) {
        Check(AccessKey{AccessKind::Event, id}, AccessMode::Read);
        return channels_[id].Object();
    }
    [[nodiscard]] void* WriteEvents(EventId id) {
        Check(AccessKey{AccessKind::Event, id}, AccessMode::Write);
        return channels_[id].Object();
    }
    [[nodiscard]] std::vector<Command>& DeferredQueue() {
        Check(AccessKey{AccessKind::Deferred, 0}, AccessMode::Write);
        return deferred_;
    }

    // ---- Typed convenience over the checked access (same checks).

    template <class T>
    [[nodiscard]] std::size_t Rows() {
        return ReadColumn(registry_.ComponentIdOf<T>()).size() / sizeof(T);
    }
    template <class T>
    [[nodiscard]] const T& Get(std::size_t row) {
        const std::span<const std::byte> bytes = ReadColumn(registry_.ComponentIdOf<T>());
        KERNEL_SPIKE_ASSERT(row < bytes.size() / sizeof(T), "World::Get: row out of range");
        return *ObjectAt<T>(bytes.data() + row * sizeof(T));
    }
    template <class T>
    [[nodiscard]] T& GetMut(std::size_t row) {
        const std::span<std::byte> bytes = WriteColumn(registry_.ComponentIdOf<T>());
        KERNEL_SPIKE_ASSERT(row < bytes.size() / sizeof(T), "World::GetMut: row out of range");
        return *ObjectAt<T>(bytes.data() + row * sizeof(T));
    }
    template <class T>
    [[nodiscard]] const T& Resource() {
        return *ObjectAt<T>(ReadResource(registry_.ResourceIdOf<T>()));
    }
    template <class T>
    [[nodiscard]] T& ResourceMut() {
        return *ObjectAt<T>(WriteResource(registry_.ResourceIdOf<T>()));
    }
    template <class T>
    [[nodiscard]] const std::vector<T>& Events() {
        return *ObjectAt<std::vector<T>>(ReadEvents(registry_.EventIdOf<T>()));
    }

private:
    friend class System;
    friend class Executor;
    friend struct PageGuardProbe;

    [[nodiscard]] std::size_t StorageIndex(AccessKey key) const noexcept {
        switch (key.kind) {
        case AccessKind::Component:
            return key.id;
        case AccessKind::Resource:
            return columns_.size() + key.id;
        case AccessKind::Event:
            return columns_.size() + resources_.size() + key.id;
        case AccessKind::Deferred:
            break;
        }
        return baseline_.size();  // the deferred queue is not snapshotted
    }

    [[nodiscard]] AccessKey KeyOfStorage(std::size_t index) const noexcept {
        if (index < columns_.size()) {
            return AccessKey{AccessKind::Component, static_cast<std::uint32_t>(index)};
        }
        index -= columns_.size();
        if (index < resources_.size()) {
            return AccessKey{AccessKind::Resource, static_cast<std::uint32_t>(index)};
        }
        return AccessKey{AccessKind::Event, static_cast<std::uint32_t>(index - resources_.size())};
    }

    [[nodiscard]] std::span<const std::byte> StorageBytes(std::size_t index) const noexcept {
        const AccessKey key = KeyOfStorage(index);
        switch (key.kind) {
        case AccessKind::Component:
            return columns_[key.id].Bytes();
        case AccessKind::Resource:
            return resources_[key.id].InlineBytes();
        case AccessKind::Event:
            // Events are compared by their elements, not the vector's handle.
            return registry_.Events()[key.id].channelBytes(channels_[key.id].Object());
        case AccessKind::Deferred:
            break;
        }
        return {};
    }

    [[nodiscard]] const char* NameOf(AccessKey key) const noexcept {
        switch (key.kind) {
        case AccessKind::Component:
            return registry_.Components()[key.id].ops.name;
        case AccessKind::Resource:
            return registry_.Resources()[key.id].ops.name;
        case AccessKind::Event:
            return registry_.Events()[key.id].ops.name;
        case AccessKind::Deferred:
            break;
        }
        return "the deferred queue";
    }

    static const char* KindName(AccessKind kind) noexcept {
        switch (kind) {
        case AccessKind::Component:
            return "component";
        case AccessKind::Resource:
            return "resource";
        case AccessKind::Event:
            return "event";
        case AccessKind::Deferred:
            break;
        }
        return "deferred";
    }

    // Host code writing through the World API: the next snapshot comparison
    // takes the new bytes as the baseline instead of reporting them.
    void MarkWritten(AccessKey key) noexcept {
        const std::size_t index = StorageIndex(key);
        if (index < dirty_.size()) {
            dirty_[index] = true;
        }
    }

    void Check([[maybe_unused]] AccessKey key, [[maybe_unused]] AccessMode mode) {
#if KERNEL_SPIKE_ACCESS_CHECKS
        const SystemDescriptor* system = Detail::runningSystem;
        if (system == nullptr) {
            if (mode == AccessMode::Write) {
                MarkWritten(key);
            }
            return;
        }
        KERNEL_SPIKE_ASSERT(Detail::runningWorld == this, "a System accessed a World it is not running on");
        const bool declared = mode == AccessMode::Read ? system->DeclaresRead(key) || system->DeclaresWrite(key)
                                                       : system->DeclaresWrite(key);
        if (!declared) {
            std::fprintf(stderr, "kernel spike access check: System '%s' %s %s '%s' without declaring it\n",
                         system->name.c_str(), mode == AccessMode::Read ? "read" : "wrote", KindName(key.kind),
                         NameOf(key));
            std::fflush(stderr);
        }
        KERNEL_SPIKE_ASSERT(declared, mode == AccessMode::Read ? "undeclared read of World storage"
                                                               : "undeclared write of World storage");
#endif
    }

    void Capture(std::size_t index) {
        const std::span<const std::byte> bytes = StorageBytes(index);
        baseline_[index].assign(bytes.begin(), bytes.end());
        dirty_[index] = false;
    }

    [[nodiscard]] bool Unchanged(std::size_t index) const noexcept {
        return std::ranges::equal(baseline_[index], StorageBytes(index));
    }

    // Before a System runs: nothing may have changed since the last snapshot
    // except through the World API. Without sink (System::Run outside a Run)
    // a change fails KERNEL_SPIKE_ASSERT; with sink (inside a Run) it is
    // appended and the new bytes become the baseline. False when a change was
    // appended.
    bool VerifyBetweenSystems(std::vector<Violation>* sink = nullptr) {
        bool unchanged = true;
        for (std::size_t index = 0; index < baseline_.size(); ++index) {
            if (dirty_[index]) {
                Capture(index);
                continue;
            }
            if (!Unchanged(index)) {
                const AccessKey key = KeyOfStorage(index);
                if (sink != nullptr) {
                    sink->push_back(Violation{Violation::Source::BeforeSystem, std::string(), key});
                    Capture(index);
                    unchanged = false;
                    continue;
                }
                std::fprintf(stderr,
                             "kernel spike access check: %s '%s' changed outside any System without the World API\n",
                             KindName(key.kind), NameOf(key));
                std::fflush(stderr);
                KERNEL_SPIKE_FAIL("World storage changed outside any System without the World API");
            }
        }
        return unchanged;
    }

    // After a System ran: only what it declared as written may differ. Sink:
    // as VerifyBetweenSystems.
    void VerifyAfterSystem(const SystemDescriptor& system, std::vector<Violation>* sink = nullptr) {
        for (std::size_t index = 0; index < baseline_.size(); ++index) {
            const AccessKey key = KeyOfStorage(index);
            if (system.DeclaresWrite(key)) {
                Capture(index);
                continue;
            }
            if (!Unchanged(index)) {
                if (sink != nullptr) {
                    sink->push_back(Violation{Violation::Source::AfterSystem, system.name, key});
                    Capture(index);
                    continue;
                }
                std::fprintf(stderr, "kernel spike access check: System '%s' changed %s '%s' without declaring a write\n",
                             system.name.c_str(), KindName(key.kind), NameOf(key));
                std::fflush(stderr);
                KERNEL_SPIKE_FAIL("a System changed World storage it did not declare as written");
            }
        }
    }

    // ---- Page guard (WorldOptions::pageGuard). Executor::Run and
    // System::Step call Guard at the four points of a Run; every protection
    // change goes through SetColumnWritable.

    enum class GuardStep : std::uint8_t {
        EnterRun,      // every guarded column read-only
        BeforeSystem,  // (c): lift the columns the System declared as written; (b): lift all
        AfterSystem,   // protect again what BeforeSystem and the fault handler lifted
        ExitRun,       // every guarded column writable (host writes in Ready are not guarded)
    };

    void Guard(GuardStep step, const SystemDescriptor* system, const Detail::RunContext* context) {
        if (!pageGuard_) {
            return;
        }
        switch (step) {
        case GuardStep::EnterRun:
            guardActive_ = true;
            SetGuardedWritable(false);
            return;
        case GuardStep::BeforeSystem:
            if (!guardActive_) {
                return;  // System::Run outside a Run
            }
            if (system->zone == Zone::B) {
                // (b): its writes are checked by the two layers only.
                SetGuardedWritable(true);
                return;
            }
            SetDeclaredWritable(*system, true);
            return;
        case GuardStep::AfterSystem:
            if (!guardActive_) {
                return;
            }
            if (system->zone == Zone::B) {
                SetGuardedWritable(false);
                return;
            }
            SetDeclaredWritable(*system, false);
            ProtectFaulted(*context);
            return;
        case GuardStep::ExitRun:
            guardActive_ = false;
            SetGuardedWritable(true);
            return;
        }
    }

    // Every guarded column.
    void SetGuardedWritable(bool writable) {
        for (std::size_t id = 0; id < guarded_.size(); ++id) {
            if (guarded_[id]) {
                SetColumnWritable(static_cast<ComponentId>(id), writable);
            }
        }
    }

    // The guarded columns a System declared as written.
    void SetDeclaredWritable(const SystemDescriptor& system, bool writable) {
        for (const AccessKey& key : system.writes) {
            if (key.kind == AccessKind::Component && guarded_[key.id]) {
                SetColumnWritable(key.id, writable);
            }
        }
    }

    // The columns of this World the fault handler made writable during the
    // call; every guarded column when it ran out of slots.
    void ProtectFaulted(const Detail::RunContext& context) {
        if (context.faultsDropped.load(std::memory_order_relaxed) != 0) {
            SetGuardedWritable(false);
            return;
        }
        const std::size_t count = context.faultCount.load(std::memory_order_acquire);
        for (std::size_t index = 0; index < count; ++index) {
            if (context.faults[index].world == this) {
                SetColumnWritable(context.faults[index].column, false);
            }
        }
    }

    void SetColumnWritable(ComponentId id, bool writable) {
        ++guardStats_.protectCalls;
        const std::span<std::byte> pages = columns_[id].Pages();
        if (pages.empty()) {
            return;  // no page-guarded storage (no rows yet, or the page guard is off)
        }
        const bool changed = PageGuard::SetWritable(pages.data(), pages.size(), writable);
        KERNEL_SPIKE_ASSERT(changed, "page guard: the OS refused a protection change");
    }

    Registry registry_;
    bool pageGuard_;
    bool guardActive_ = false;
    // Per component: true for Exclusive (page-guarded when pageGuard_ is on).
    std::vector<bool> guarded_;
    PageGuardStats guardStats_;
    std::vector<Detail::Column> columns_;
    std::vector<Detail::Slot> resources_;
    std::vector<Detail::Slot> channels_;
    std::vector<Command> deferred_;
    // Access-check snapshots, one per column, resource and event channel.
    std::vector<std::vector<std::byte>> baseline_;
    std::vector<bool> dirty_;
};

} // namespace KernelSpike
