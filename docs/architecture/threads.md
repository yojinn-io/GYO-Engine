# GYO role threads

Owner: Engine (`engine/threads` → `GYO::Threads`).
Plan and record: [time-threads-trace](plans/time-threads-trace/README.md), batch TT-1 (D27, D29); this document was
written in [os-subsystem](plans/os-subsystem/README.md) batch time-threads-invariants; the thread naming and priority
calls moved into engine/os in batch os-threads-backend (D69).

## What it is

```text
GYO::Time ◄── GYO::Threads ◄── GYO::Trace, products
GYO::Os   ◄──┘ (privately: the thread name and priority calls)
```

- **`Threads::RoleThread`**: one long-lived, named thread running one role's loop, with its own `Time::Waiter`.
- **`Threads::RoleContext`**: what the loop sees: its stop request and its Waiter.

Ownership: Engine owns the thread's lifetime, its name and priority hint per platform, and its wake. engine/threads
owns the role and its lifetime; the OS calls that name and prioritize the thread are engine/os's (`Os::Thread`,
os-threads-backend, D69). The consumer owns the loop, its tick policy and thresholds, and the order in which roles
stop and join.

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
   macOS and Windows); `tests/common/os/ThreadTests.cpp` "Os thread applies the priority hints as the platform reads
   them back" reads the class, priority or niceness back; mutants `th-x4` (macOS), `th-x5` (Windows), `th-x6`
   (Linux).
6. **The name is reported, and its truncation stays.** A non-empty name is required; `NameApplied()` tells whether the
   platform took it; Linux keeps 15 bytes (os.md Invariant 9 keeps that truncation when the backend moves). Guards:
   "a role needs a name and a body", the name case of "a role reports its name…" (applied on macOS and Linux, also
   for a name longer than 15 bytes, which is reported in full); `tests/common/os/ThreadTests.cpp` "Os thread names
   the calling thread as the platform reads it back" (the name the OS keeps, Linux's 15 bytes, a Windows name
   beyond ASCII); mutants `ti-x4-linux-name-not-cut` (Linux), `th-x7` (macOS), `th-x3` (Windows).
7. **Each role waits on its own Waiter.** Wakes reach only that role, and `WaitBackend()` answers that Waiter's
   backend. Guard: "a role reports the backend of its own Waiter"; mutant `tt2-role-backend-fixed`.
8. **The body's obligations are the consumer's.** It returns once a stop is requested and lets no exception escape;
   RoleThread does not catch for it. No guard; a body that breaks this hangs the join or ends the process.
9. **`GYO::Threads` links exactly `GYO::Time`, `Threads::Threads` and, privately, `GYO::Os`** (checked at configure
   time by `tests/common/time/CMakeLists.txt`, which also refuses `GYO::Os` in its interface): no SDL, no
   `GYO::Engine`, and what links `GYO::Threads` gets no engine/os headers from it (os-threads-backend, D69). Guards:
   mutants `ti-x2-threads-links-more`, `th-x1-threads-os-public`.

### Replacement Criteria

Stop adding features and propose redesigning instead of patching when any of these holds:

1. A consumer needs a thread pool, a scheduler, a work queue or channels: plan a component of its own instead of
   growing RoleThread.
2. A platform needs priority or affinity with timing semantics (real-time scheduling, CPU pinning) rather than a hint.
3. The order in which roles stop has to be enforced by the library, or the destructor's stop-and-join is not enough
   for a consumer's shutdown.
