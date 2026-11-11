#pragma once

#include "kernel_spike/Assert.hpp"
#include "kernel_spike/Descriptor.hpp"
#include "kernel_spike/Registry.hpp"
#include "kernel_spike/TypeList.hpp"
#include "kernel_spike/World.hpp"

#include <cstddef>
#include <functional>
#include <span>
#include <utility>
#include <vector>

// The seven System parameter kinds. Each one is the access declaration and
// the access itself: ParamAccess<P> says what P reads and writes, and P is
// the only handle a System gets for that storage.
//
// Handles are constructed only by ParamFactory, for one System run, and are
// neither copyable nor movable: a System receives them by value (guaranteed
// copy elision from ParamFactory::Make), cannot store one past its run, and
// cannot construct one for an access it did not declare. Read and Res hand
// out const references only.

namespace KernelSpike {

// Not a System parameter unless specialized below.
template <class P>
struct ParamAccess {
    static constexpr bool valid = false;
    using Reads = TypeList<>;
    using Writes = TypeList<>;
};

struct ParamFactory {
    template <class P>
    static P Make(World& world) {
        return P(world);
    }
};

// ---- Components

template <class T>
class Read final {
public:
    Read(const Read&) = delete;
    Read& operator=(const Read&) = delete;

    [[nodiscard]] std::size_t Size() const noexcept { return rows_; }
    [[nodiscard]] const T& operator[](std::size_t row) const {
        KERNEL_SPIKE_ASSERT(row < rows_, "Read: row out of range");
        return *ObjectAt<T>(data_ + row * sizeof(T));
    }

private:
    friend struct ParamFactory;
    explicit Read(World& world) {
        const std::span<const std::byte> bytes = world.ReadColumn(world.Types().ComponentIdOf<T>());
        data_ = bytes.data();
        rows_ = bytes.size() / sizeof(T);
    }

    const std::byte* data_ = nullptr;
    std::size_t rows_ = 0;
};

template <class T>
struct ParamAccess<Read<T>> {
    static constexpr bool valid = true;
    using Reads = TypeList<Access<AccessKind::Component, T>>;
    using Writes = TypeList<>;
};

template <class T>
class Write final {
public:
    Write(const Write&) = delete;
    Write& operator=(const Write&) = delete;

    [[nodiscard]] std::size_t Size() const noexcept { return rows_; }
    [[nodiscard]] T& operator[](std::size_t row) const {
        KERNEL_SPIKE_ASSERT(row < rows_, "Write: row out of range");
        return *ObjectAt<T>(data_ + row * sizeof(T));
    }

private:
    friend struct ParamFactory;
    explicit Write(World& world) {
        const std::span<std::byte> bytes = world.WriteColumn(world.Types().ComponentIdOf<T>());
        data_ = bytes.data();
        rows_ = bytes.size() / sizeof(T);
    }

    std::byte* data_ = nullptr;
    std::size_t rows_ = 0;
};

template <class T>
struct ParamAccess<Write<T>> {
    static constexpr bool valid = true;
    using Reads = TypeList<>;
    using Writes = TypeList<Access<AccessKind::Component, T>>;
};

// ---- Resources

template <class T>
class Res final {
public:
    Res(const Res&) = delete;
    Res& operator=(const Res&) = delete;

    [[nodiscard]] const T& operator*() const noexcept { return *object_; }
    [[nodiscard]] const T* operator->() const noexcept { return object_; }

private:
    friend struct ParamFactory;
    explicit Res(World& world) : object_(ObjectAt<T>(world.ReadResource(world.Types().ResourceIdOf<T>()))) {}

    const T* object_;
};

template <class T>
struct ParamAccess<Res<T>> {
    static constexpr bool valid = true;
    using Reads = TypeList<Access<AccessKind::Resource, T>>;
    using Writes = TypeList<>;
};

template <class T>
class ResMut final {
public:
    ResMut(const ResMut&) = delete;
    ResMut& operator=(const ResMut&) = delete;

    [[nodiscard]] T& operator*() const noexcept { return *object_; }
    [[nodiscard]] T* operator->() const noexcept { return object_; }

private:
    friend struct ParamFactory;
    explicit ResMut(World& world) : object_(ObjectAt<T>(world.WriteResource(world.Types().ResourceIdOf<T>()))) {}

    T* object_;
};

template <class T>
struct ParamAccess<ResMut<T>> {
    static constexpr bool valid = true;
    using Reads = TypeList<>;
    using Writes = TypeList<Access<AccessKind::Resource, T>>;
};

// ---- Events

template <class T>
class EventReader final {
public:
    EventReader(const EventReader&) = delete;
    EventReader& operator=(const EventReader&) = delete;

    // Every event in the channel since the last ClearEvents, in send order.
    [[nodiscard]] std::span<const T> Events() const noexcept { return *channel_; }

private:
    friend struct ParamFactory;
    explicit EventReader(World& world)
        : channel_(ObjectAt<std::vector<T>>(world.ReadEvents(world.Types().EventIdOf<T>()))) {}

    const std::vector<T>* channel_;
};

template <class T>
struct ParamAccess<EventReader<T>> {
    static constexpr bool valid = true;
    using Reads = TypeList<Access<AccessKind::Event, T>>;
    using Writes = TypeList<>;
};

template <class T>
class EventWriter final {
public:
    EventWriter(const EventWriter&) = delete;
    EventWriter& operator=(const EventWriter&) = delete;

    void Send(T event) const { channel_->push_back(std::move(event)); }

private:
    friend struct ParamFactory;
    explicit EventWriter(World& world)
        : channel_(ObjectAt<std::vector<T>>(world.WriteEvents(world.Types().EventIdOf<T>()))) {}

    std::vector<T>* channel_;
};

template <class T>
struct ParamAccess<EventWriter<T>> {
    static constexpr bool valid = true;
    using Reads = TypeList<>;
    using Writes = TypeList<Access<AccessKind::Event, T>>;
};

// ---- Deferred queue

class Deferred final {
public:
    Deferred(const Deferred&) = delete;
    Deferred& operator=(const Deferred&) = delete;

    // The command runs at the next World::ApplyDeferred, with exclusive access.
    void Push(World::Command command) const { queue_->push_back(std::move(command)); }

private:
    friend struct ParamFactory;
    explicit Deferred(World& world) : queue_(&world.DeferredQueue()) {}

    std::vector<World::Command>* queue_;
};

template <>
struct ParamAccess<Deferred> {
    static constexpr bool valid = true;
    using Reads = TypeList<>;
    using Writes = TypeList<Access<AccessKind::Deferred, void>>;
};

} // namespace KernelSpike
