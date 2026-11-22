#pragma once

#include <functional>

// Where the store runs its jobs (declaration 5.6, X-1, X-4, X-5). A job reads
// one file and decodes it; it never touches a World. asset does not depend on
// threads: whoever owns the threads (the exchange point, a runner, a tool, a
// test) implements this, and world_asset connects the one it is given.
//
// Contract of an implementation:
// - Submit runs the job once, later or now, on any thread, or drops it once
//   the executor has stopped (then Submit is a safe no-op).
// - A job may call Submit (a finished job submits the jobs of its
//   dependencies). An executor that runs jobs at once must queue a Submit made
//   from inside a job and run it after, never recursively.
// - The store calls Submit with no lock of its own held.
//
// The Engine has no ready-made adapter: a product puts threads' WorkerPool
// behind this interface itself (a few lines: WorkerPool::Submit queues a
// Submit made from inside a job and takes nothing after Stop, as this contract
// asks; tests/common/world_asset/ScenarioTests.cpp has one). The
// store's Shutdown has two modes: drain (wait for every queued job) or discard
// (count what did not finish as discarded); both wait for jobs already in a
// loader. A store given no executor fails every request that needs a job with
// NoExecutor (E2); it never runs a job on the caller's thread.

namespace GYO::Engine::Asset {

class AssetExecutor {
public:
    using Job = std::function<void()>;

    virtual ~AssetExecutor() = default;

    virtual void Submit(Job job) = 0;
};

} // namespace GYO::Engine::Asset
