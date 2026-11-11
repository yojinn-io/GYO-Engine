#pragma once

#include <algorithm>
#include <compare>
#include <cstdint>
#include <string>
#include <vector>

// What a System declares, at compile time (Access<Kind, T>) and at run time
// (AccessKey: kind plus the id the Registry gave the type).

namespace KernelSpike {

enum class AccessKind : std::uint8_t {
    Component,
    Resource,
    Event,
    // The deferred queue: structural changes a System asks for, applied at
    // the next sync point (World::ApplyDeferred).
    Deferred,
};

enum class AccessMode : std::uint8_t {
    Read,
    Write,
};

// Compile-time access entry. For Deferred, T is void.
template <AccessKind K, class T>
struct Access {
    static constexpr AccessKind kind = K;
    using Type = T;
};

struct AccessKey {
    AccessKind kind;
    std::uint32_t id;

    friend constexpr bool operator==(const AccessKey&, const AccessKey&) = default;
    friend constexpr auto operator<=>(const AccessKey&, const AccessKey&) = default;
};

struct SystemDescriptor {
    std::string name;
    // Sorted and unique; reads and writes are disjoint (MakeSystem rejects a
    // signature that reads and writes the same key, or writes it twice).
    std::vector<AccessKey> reads;
    std::vector<AccessKey> writes;

    [[nodiscard]] bool DeclaresRead(AccessKey key) const {
        return std::binary_search(reads.begin(), reads.end(), key);
    }
    [[nodiscard]] bool DeclaresWrite(AccessKey key) const {
        return std::binary_search(writes.begin(), writes.end(), key);
    }
};

class World;

namespace Detail {

// The System running on this thread and the World it runs on. Maintained in
// every build (System::Run); only the access checks read the descriptor.
inline thread_local const SystemDescriptor* runningSystem = nullptr;
inline thread_local const World* runningWorld = nullptr;

class RunningScope final {
public:
    RunningScope(const SystemDescriptor& system, const World& world) noexcept {
        runningSystem = &system;
        runningWorld = &world;
    }
    // Also runs when a test handler throws out of the System body.
    ~RunningScope() {
        runningSystem = nullptr;
        runningWorld = nullptr;
    }
    RunningScope(const RunningScope&) = delete;
    RunningScope& operator=(const RunningScope&) = delete;
};

} // namespace Detail

} // namespace KernelSpike
