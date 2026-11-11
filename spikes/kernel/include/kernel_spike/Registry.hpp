#pragma once

#include "kernel_spike/Assert.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

// Type registration. Ids follow registration order: the n-th component (or
// resource, or event) registered gets id n. They are not derived from a type
// hash and not handed out on first use, so the same registration code gives
// the same ids on every compiler and every run.

namespace KernelSpike {

using ComponentId = std::uint32_t;
using ResourceId = std::uint32_t;
using EventId = std::uint32_t;

// The only things the World knows about a stored type.
struct TypeOps {
    const char* name;  // diagnostics only; given at registration, not from RTTI
    std::size_t size;
    std::size_t align;
    void (*moveConstruct)(void* destination, void* source) noexcept;
    void (*destroy)(void* object) noexcept;
};

template <class T>
[[nodiscard]] T* ObjectAt(void* storage) noexcept {
    return std::launder(static_cast<T*>(storage));
}

template <class T>
[[nodiscard]] const T* ObjectAt(const void* storage) noexcept {
    return std::launder(static_cast<const T*>(storage));
}

template <class T>
[[nodiscard]] TypeOps OpsOf(const char* name) noexcept {
    static_assert(std::is_object_v<T> && !std::is_const_v<T>, "stored types are non-const object types");
    static_assert(std::is_nothrow_move_constructible_v<T>, "stored types must be nothrow move constructible");
    static_assert(std::is_nothrow_destructible_v<T>, "stored types must be nothrow destructible");
    return TypeOps{
        name,
        sizeof(T),
        alignof(T),
        [](void* destination, void* source) noexcept { ::new (destination) T(std::move(*ObjectAt<T>(source))); },
        [](void* object) noexcept { ObjectAt<T>(object)->~T(); },
    };
}

namespace Detail {

// One object per type; its address identifies the type for registry lookup.
// The address is a lookup key only, never an id. Deliberately not const: a
// linker that folds identical read-only data (MSVC /OPT:ICF, lld --icf=all)
// could otherwise give two types the same address.
template <class T>
struct TypeTokenOf {
    static inline char tag = 0;
};

} // namespace Detail

using TypeToken = const void*;

template <class T>
[[nodiscard]] constexpr TypeToken TokenOf() noexcept {
    return &Detail::TypeTokenOf<T>::tag;
}

class Registry {
public:
    struct Entry {
        TypeToken token;
        // For an event T the stored object is the channel, std::vector<T>.
        TypeOps ops;
        // Events only.
        void (*constructChannel)(void* at) noexcept = nullptr;
        void (*clearChannel)(void* channel) noexcept = nullptr;
        std::span<const std::byte> (*channelBytes)(const void* channel) noexcept = nullptr;
    };

    template <class T>
    ComponentId RegisterComponent(const char* name) {
        return Add(components_, Entry{TokenOf<T>(), OpsOf<T>(name)}, "component type registered twice");
    }

    template <class T>
    ResourceId RegisterResource(const char* name) {
        return Add(resources_, Entry{TokenOf<T>(), OpsOf<T>(name)}, "resource type registered twice");
    }

    template <class T>
    EventId RegisterEvent(const char* name) {
        Entry entry{TokenOf<T>(), OpsOf<std::vector<T>>(name)};
        entry.constructChannel = [](void* at) noexcept { ::new (at) std::vector<T>(); };
        entry.clearChannel = [](void* channel) noexcept { ObjectAt<std::vector<T>>(channel)->clear(); };
        entry.channelBytes = [](const void* channel) noexcept {
            const std::vector<T>& events = *ObjectAt<std::vector<T>>(channel);
            return std::span<const std::byte>(reinterpret_cast<const std::byte*>(events.data()),
                                              events.size() * sizeof(T));
        };
        return Add(events_, entry, "event type registered twice");
    }

    // Looking up a type that was never registered is a Programmer Error: the
    // id is not created here.
    template <class T>
    [[nodiscard]] ComponentId ComponentIdOf() const {
        return Find(components_, TokenOf<T>(), "component type is not registered");
    }
    template <class T>
    [[nodiscard]] ResourceId ResourceIdOf() const {
        return Find(resources_, TokenOf<T>(), "resource type is not registered");
    }
    template <class T>
    [[nodiscard]] EventId EventIdOf() const {
        return Find(events_, TokenOf<T>(), "event type is not registered");
    }

    [[nodiscard]] std::span<const Entry> Components() const noexcept { return components_; }
    [[nodiscard]] std::span<const Entry> Resources() const noexcept { return resources_; }
    [[nodiscard]] std::span<const Entry> Events() const noexcept { return events_; }

private:
    static std::uint32_t Add(std::vector<Entry>& table, const Entry& entry, const char* duplicate) {
        for (const Entry& existing : table) {
            KERNEL_SPIKE_ASSERT(existing.token != entry.token, duplicate);
        }
        const auto id = static_cast<std::uint32_t>(table.size());
        table.push_back(entry);
        return id;
    }

    static std::uint32_t Find(const std::vector<Entry>& table, TypeToken token, const char* missing) {
        for (std::size_t index = 0; index < table.size(); ++index) {
            if (table[index].token == token) {
                return static_cast<std::uint32_t>(index);
            }
        }
        KERNEL_SPIKE_FAIL(missing);
    }

    std::vector<Entry> components_;
    std::vector<Entry> resources_;
    std::vector<Entry> events_;
};

} // namespace KernelSpike
