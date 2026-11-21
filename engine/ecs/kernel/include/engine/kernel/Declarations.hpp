#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// The explicit half of a declaration (spec B.2): what an owner states when it
// registers a component, resource, event, effect, schedule or System. The other
// half, a System's data access, is derived from its parameter types
// (Params.hpp); nothing here can declare access by hand (DI-KS 1).

namespace GYO::Kernel {

// Ids are handed out in registration order (DI-KS 4, spec R-1): the n-th
// component registered gets ComponentId n. They are in-process only.
using ComponentId = std::uint32_t;
using ResourceId = std::uint32_t;
using MessageTypeId = std::uint32_t;
using EventTypeId = std::uint32_t;
using EffectId = std::uint32_t;
using SetId = std::uint32_t;
using RegistrationSeq = std::uint32_t;

struct ComponentOptions {
    std::string name;
};

struct ResourceOptions {
    std::string name;
    // Bound to the thread that puts the value in (DI-KS 14; Bevy's NonSend): a
    // touch from any other thread is a GYO_ASSERT, destroying the World with a
    // value included; removing the value forgets the thread. Which thread runs
    // what is the orchestration layer's (v8-arch-gyo2-thread-binding-by-runner).
    bool threadBound = false;
};

// A retained message type (event cursor declaration 3.2, 5.1; DI-KS 10).
struct MessageOptions {
    std::string name;
    // How many records of this type the World keeps: a write to a full buffer
    // drops the oldest one and the World records it (DroppedMessages). A default,
    // not a measured value; at least 1 (GYO_ASSERT).
    std::uint32_t capacity = 1024;
};

// A pushed event type (event cursor declaration 3.3, 5.2; DI-KS 10): kept
// nowhere; a Trigger runs its observers (Registrar::Observer). The type's name
// is in the past tense (naming DI 2).
struct EventOptions {
    std::string name;
};

// An observer (event cursor declaration 3.3, 5.2): a System that runs where its
// event is triggered, not a Plan step. Names are unique among observers: the
// observers of one trigger run in name order (deterministic and independent of
// registration order; not promised to authors).
struct ObserverOptions {
    std::string name;
};

// A named group of a schedule's Systems (spec C.1 sets): order edges and run
// conditions given once for every member.
struct SetOptions {
    std::string name;
    std::string schedule;
    std::vector<std::string> before;
    std::vector<std::string> after;
};

enum class ExecutorKind : std::uint8_t {
    Serial,
    // Not implemented in v1: any Parallel schedule is D-E17 (LIM-2).
    Parallel,
};

// A sync point declared inside a stage (spec D.1 syncPoints). Systems refer to
// it by name in before / after; every stage also ends with one.
struct SyncPointOptions {
    std::string name;
    std::string stage;
};

// Given by the Host (spec D.1); the kernel builds in no schedule or stage name
// (DI-KS 15) and keeps no clock: a schedule belongs to no time domain and its
// Run advances no tick (the tick is the time Module's, engine/world_time;
// v8-arch-gyo2-clock-out-of-kernel).
struct ScheduleOptions {
    std::string name;
    // The only schedule kind where Extract parameters are legal (spec H.6).
    bool extract = false;
    std::vector<std::string> stages;
    std::vector<SyncPointOptions> syncPoints;
    ExecutorKind executor = ExecutorKind::Serial;
    // The most hook rounds after the last sync point of a Run (spec F-12). Empty:
    // the World's KernelSettings::hookRoundLimit. 0 is D-E20. In the Plan
    // fingerprint.
    std::optional<std::uint32_t> hookRoundLimit;
};

} // namespace GYO::Kernel
