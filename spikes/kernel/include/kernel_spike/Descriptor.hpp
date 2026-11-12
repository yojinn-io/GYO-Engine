#pragma once

#include <algorithm>
#include <atomic>
#include <compare>
#include <cstddef>
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

// The determinism zone a System runs in (spec G). The spike uses it only for
// the page guard (World.hpp): writes during a (c) System are guarded, writes
// during a (b) System are checked by the two access-check layers only.
enum class Zone : std::uint8_t {
    C,
    B,
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
    // Spike: defaults to C so the existing MakeSystem calls stay unchanged;
    // the spec's System zone has no default (S-6).
    Zone zone = Zone::C;

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

// One page-guard fault: a write that hit a guarded page (PageGuard.hpp).
struct PageFault {
    const void* world;
    std::uint32_t column;
    std::uintptr_t address;
};

// The context of one System call: what the page-guard fault handler records
// for it (spec A.6: violations go to this call's context, the descriptor is
// immutable). The handler writes it from a POSIX signal handler or a Windows
// vectored exception handler, so it is fixed size and never allocates; a
// column is recorded once per call (its protection is lifted on the first
// fault).
struct RunContext {
    static constexpr std::size_t kFaultSlots = 16;
    PageFault faults[kFaultSlots] = {};
    std::atomic<std::size_t> faultCount{0};
    // Faults past kFaultSlots: counted, not recorded.
    std::atomic<std::size_t> faultsDropped{0};
};
static_assert(std::atomic<std::size_t>::is_always_lock_free, "the fault handler needs lock-free counters");

// The spike's tlCurrent for the page guard: the context of the System call
// running on this thread, or nullptr. constinit so the fault handler reads it
// without a thread_local initialization call.
inline constinit thread_local RunContext* runningContext = nullptr;

class RunningScope final {
public:
    RunningScope(const SystemDescriptor& system, const World& world, RunContext& context) noexcept {
        runningSystem = &system;
        runningWorld = &world;
        runningContext = &context;
    }
    // Also runs when a test handler throws out of the System body.
    ~RunningScope() {
        runningSystem = nullptr;
        runningWorld = nullptr;
        runningContext = nullptr;
    }
    RunningScope(const RunningScope&) = delete;
    RunningScope& operator=(const RunningScope&) = delete;
};

} // namespace Detail

} // namespace KernelSpike
