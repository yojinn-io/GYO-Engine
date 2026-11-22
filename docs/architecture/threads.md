# GYO threads

Owner: Engine (`engine/core/threads` → `GYO::Threads`; core, not a Module).
Plan and record: [time-threads-trace](plans/time-threads-trace/README.md), batch TT-1 (D27, D29); this document was
written in [os-subsystem](plans/os-subsystem/README.md) batch time-threads-invariants; the thread naming and priority
calls moved into engine/os in batch os-threads-backend (D69). Re-reviewed for GYO2 in
[threads-io-review](target/v2/threads-io-review-declaration.zh-Hant.md) (`v8-arch-gyo2-threads-io-review`, 2026-11-22):
`RoleThread` kept as it is, `WorkerPool` added.

## What it is

```text
GYO::Time ◄── GYO::Threads ◄── engine/modules/logging, products (runners, pools for World-external work)
GYO::OS   ◄──┘ (privately: the thread name and priority calls)
```

- **`Threads::RoleThread`**: one long-lived, named thread running one role's loop, with its own `Time::Waiter`. A role
  is a thread with a name and a fixed duty ([vocabulary](target/v2/rebuild-vocabulary.zh-Hant.md), Role); it is
  the dedicated-thread placement of an executor (rebuild-concept C8).
- **`Threads::RoleContext`**: what the loop sees: its stop request and its Waiter.
- **`Threads::WorkerPool`**: a fixed number of role threads taking jobs first in, first out; the worker-pool placement
  of C8. Whoever creates it owns it.

Users: logging's file writer thread is a role. A runner may hand a whole World to a role it creates and joins
(orchestration O-3, O-13; the entry's shape comes with the first product that does it); the World is not bound to
the thread. A product that wants World-external work off the World's threads (asset reads and decodes: the asset
declaration, branch `claude/gyo2-asset-585196`, X-1) creates a `WorkerPool`, chooses its thread count (X-4 there),
and hands it on through `ProductDescription::options`;
the Engine starts no pool by default.

Ownership: Engine owns the thread's lifetime, its name and priority hint per platform, and its wake. engine/threads
owns the role and its lifetime and the pool's queue; the OS calls that name and prioritize the thread are engine/os's
(`OS::Thread`, os-threads-backend, D69). The consumer owns the loop, its tick policy and thresholds, the order in
which roles stop and join, and what a job does.

## Contracts

- `Start(options, body)` returns once the new thread has applied its name and priority and is about to run the body.
  An empty name or an empty body is a Programmer Error. `StartFailed` reports a thread or Waiter the OS refused.
- `RequestStop()` makes the body's `StopRequested()` true and wakes its wait; `Notify()` wakes it without stopping.
  Both may come from any thread, and do nothing on a moved-from RoleThread.
- `Join()` waits for the body to return; it is idempotent and not called from the role's own thread. The destructor
  requests a stop and joins.
- The body must return once `StopRequested()` is true and must not let an exception escape (a thread entry point
  handles its own Runtime Errors). It is a `std::function`, so it is copyable; move-only state goes in a
  `shared_ptr`.
- The name is for debuggers, profilers and trace; Linux keeps its first 15 bytes; Windows takes it as UTF-8 converted
  to UTF-16, and a name that is not UTF-8 is not applied (D69 ★; Windows widened each byte before). `NameApplied()`
  tells whether the platform took it.
- The priority (`Background`, `Normal`, `Interactive`) is a scheduling hint: macOS QoS classes, Windows thread
  priorities, Linux only `Background` (nice 10, which fails if the process already runs nicer). It is not a timing
  guarantee: QoS alone does not remove macOS timer-coalescing leeway, which `Time::Waiter` avoids instead.
  `PriorityApplied()` tells whether the platform took it. `Normal` is the default class or priority: macOS and
  Windows set it explicitly, Linux and other platforms make no call, and it counts as applied there.
- `WaitBackend()` reports the backend of the role's own Waiter.
- `WorkerPool::Start(options)` starts `threadCount` (at least one) role threads named `"<name> (<index>)"` with the
  given priority; a thread the OS refuses is `StartFailed` and the pool's other threads are stopped and joined.
  `Submit(job)` queues the job and returns true, or, once `Stop` has begun, takes nothing and returns false; jobs
  start in the order they were submitted. `Stop(Drain)` runs the jobs already queued, `Stop(Discard)` drops the
  jobs not started; both return once no job is running and every thread has returned, and a second `Stop` does
  nothing. The destructor is `Stop(Discard)`. A job must not let an exception escape. `Stop` and the destructor are
  not called from one of the pool's threads, nor from two threads at once. A moved-from pool takes no job. No guard
  checks that the destructor drops the queued jobs (a test cannot tell when the destructor has begun), that the
  priority reaches the threads (the pool reads none back), or that a failed `Start` stops the threads it started
  (no failure can be injected); review keeps them. Linux keeps 15 bytes of a name, so a long pool name can lose its
  `(<index>)`. Dropped jobs are destroyed on the thread that called `Stop`.
- Each pool thread is a `RoleThread` (one place for the name, the priority and the start handshake, as a Bevy pool
  is a set of named threads joined on drop). Its body waits on the pool, not on its `RoleContext`: the pool, not
  the role's stop request, ends it, an exception to Invariant 8's wording that the pool keeps by stopping before
  any of its roles is destroyed. Each thread also makes a Waiter it never uses (a kqueue on macOS, an event and a
  timer on Windows), and a Waiter the OS refuses fails `Start`.

## Design Invariants

1. **`Start` hands over a running role.** When it returns, the new thread has applied its name and priority and is
   about to run the body, on a thread of its own. Guards: `tests/common/threads/RoleThreadTests.cpp` "the body runs
   on its own thread after Start returns", "a role reports its name and whether the platform applied it and the
   priority". No guard checks that the name and priority are applied before `Start` returns rather than after.
2. **Stopping is cooperative and always wakes the role.** `RequestStop` sets the flag and wakes the wait; `Notify`
   wakes without stopping. Guards: "a stop request wakes a role blocked in its wait", "Notify wakes a role without
   stopping it"; mutant `tt1-stop-without-notify`.
3. **Destruction stops and joins.** A RoleThread never outlives its owner's scope with its thread still running.
   Guard: "destroying a role stops and joins it". No guard for `Join` from the role's own thread.
4. **The owner orders roles.** RoleThread never decides which role stops or joins first. No guard; the API has no
   such parameter.
5. **Priority is a hint with an explicit result.** A refused hint makes `PriorityApplied()` false and is never an
   error; `Normal` is applied on every platform, by the call on macOS and Windows and with no call elsewhere (os.md
   Invariant 10). No timing behaviour depends on it.
   Guard: the priority case of "a role reports its name…" (`Normal` applied everywhere; every priority applied on
   macOS and Windows); `tests/common/os/ThreadTests.cpp` "OS thread applies the priority hints as the platform reads
   them back" reads the class, priority or niceness back; mutants `th-x4` (macOS), `th-x5` (Windows), `th-x6`
   (Linux).
6. **The name is reported, and its truncation stays.** A non-empty name is required; `NameApplied()` tells whether the
   platform took it; Linux keeps 15 bytes (os.md Invariant 9 keeps that truncation when the backend moves). Guards:
   "a role needs a name and a body", the name case of "a role reports its name…" (applied on macOS and Linux, also
   for a name longer than 15 bytes, which is reported in full); `tests/common/os/ThreadTests.cpp` "OS thread names
   the calling thread as the platform reads it back" (the name the OS keeps, Linux's 15 bytes, a Windows name
   beyond ASCII); mutants `ti-x4-linux-name-not-cut` (Linux), `th-x7` (macOS), `th-x3` (Windows).
7. **Each role waits on its own Waiter.** Wakes reach only that role, and `WaitBackend()` answers that Waiter's
   backend. Guard: "a role reports the backend of its own Waiter"; mutant `tt2-role-backend-fixed`.
8. **The body's obligations are the consumer's.** It returns once a stop is requested and lets no exception escape;
   RoleThread does not catch for it. No guard; a body that breaks this hangs the join or ends the process.
9. **`GYO::Threads` links exactly `GYO::Time`, `Threads::Threads` and, privately, `GYO::OS`** (checked at configure
   time by `tests/common/time/CMakeLists.txt`, which also refuses `GYO::OS` in its interface): no SDL, no
   `GYO::Engine`, and what links `GYO::Threads` gets no engine/os headers from it (os-threads-backend, D69). Guards:
   mutants `ti-x2-threads-links-more`, `th-x1-threads-os-public`.

10. **A stopped pool takes no job.** After `Stop` has begun, `Submit` returns false and the job neither runs nor
    stays. Guard: `tests/common/threads/WorkerPoolTests.cpp` "WorkerPool takes no job after Stop"; mutant
    `wp-submit-after-stop`.
11. **Stopping a pool ends its work.** `Stop` returns once no job is running and every thread has returned; `Drain`
    runs the jobs already queued first, `Discard` drops them. Guards: "WorkerPool Stop with Drain runs the jobs
    already queued", "WorkerPool Stop with Discard drops the queued jobs and waits for the running one"; mutants
    `wp-drain-drops-queued`, `wp-discard-keeps-queued`, `wp-discard-returns-early`. The order of jobs (Contracts):
    "WorkerPool starts jobs in the order they were submitted", mutant `wp-last-in-first-out`.
12. **A pool is not global.** There is no shared or singleton pool: whoever creates one owns it and passes it on
    (orchestration O-15: one process may hold several Products). No guard; the API has no such accessor.

### Replacement Criteria

Stop adding features and propose redesigning instead of patching when any of these holds:

1. A consumer needs a scheduler, dependencies between jobs, job priorities, scopes or channels: plan a component of
   its own, or replace `WorkerPool`, instead of growing RoleThread or the pool.
2. A second consumer needs a pool (after asset): propose one Module that creates the Product's pool into the shared
   place (orchestration O-15) instead of each consumer asking the product for its own.
3. The pool's unused Waiters cost too many handles, or fail its start: split the thread start (name, priority,
   handshake) out of `RoleThread` and start pool threads without a Waiter.
4. A platform needs priority or affinity with timing semantics (real-time scheduling, CPU pinning) rather than a hint.
5. The order in which roles stop has to be enforced by the library, or the destructor's stop-and-join is not enough
   for a consumer's shutdown.
