# GYO time base and wait primitive

Owner: Engine (`engine/time` → `GYO::Time`).
Plan and record: [time-threads-trace](plans/time-threads-trace/README.md), batch TT-1 (D29); this document was written
in [os-subsystem](plans/os-subsystem/README.md) batch time-threads-invariants. The platform wait mechanism is
engine/os's `WakeSignal` (v8-arch-gyo2-os-wait, [declaration](target/v2/os-wait-declaration.zh-Hant.md)); the Waiter
keeps what a wake means and calls no OS API itself.

## What it is

```text
GYO::Base ◄── GYO::Time ◄── GYO::Threads, GYO::Engine and products
                  │
                  └──(private)──→ GYO::Os (WakeSignal)
```

- **The time base** (`Time::MonotonicClock`, `Now`, `NowNs`): every Engine module measures elapsed time, waits and
  stamps events on this clock. It is `std::chrono::steady_clock`: monotonic and never adjusted. Sources: macOS
  libc++ reads `CLOCK_MONOTONIC_RAW` (it counts system sleep), Windows QueryPerformanceCounter, Linux libstdc++
  `CLOCK_MONOTONIC`. SDL's ticks are another domain and are converted at the platform boundary by sampling both
  clocks together (input-and-present IP-3), never by a fixed offset. File times are wall-clock values and stay off
  this base.
- **`Time::Waiter`**: one thread waits until a deadline or until another thread notifies, whichever comes first.
- **`Time::LateWakeStats`**: a fixed-bin histogram of how late deadline wakes were, with max and count.

## Contracts

- `WaitUntil(deadline)` returns `{reason, planned, woke, late}`: `reason` is `Deadline` or `Notified`; `planned` is the
  deadline asked for (`TimePoint::max()` for `Wait()`); `woke` is the time base read after waking; `late` is
  `woke - planned` for a deadline wake, never negative, and zero for a notified one. A sentinel deadline saturates
  `late` instead of overflowing.
- One thread waits; any thread may `Notify`. A notification is latched: one sent before the wait starts is not lost,
  and several before one wake coalesce into that wake.
- `Wait()` has no deadline and returns only when notified.
- `Backend()` names the platform mechanism engine/os uses (os.md, Wait): `Kqueue` on macOS (an `EVFILT_TIMER` with `NOTE_CRITICAL`, which avoids
  the timer-coalescing leeway std and SDL sleeps get, and `EVFILT_USER` for notifications);
  `HighResolutionWaitableTimer` on Windows, or `WaitableTimer` when the high-resolution timer is not available;
  `ConditionVariable` elsewhere. The names are part of the consumer's log lines (`wait_backend=`).
- `Create()` fails with `WaitPrimitiveUnavailable` when the OS refuses the primitive; the message is engine/os's and
  the detail carries engine/os's error (`Base::CauseDetail`, for example `IoFailed: errno=24`; ★ in
  v8-arch-gyo2-os-wait, where it replaced `strerror` text and `GetLastError=<n>`). A moved-from Waiter ignores
  `Notify` (it is `noexcept` and cannot assert); waiting on it is a Programmer Error (`GYO_ASSERT`).

## Baselines

Wake precision is recorded, never a pass condition. `tests/common/time/WakeRecord.cpp` builds
`gyo_time_wake_record` (no window) and `gyo_time_wake_record_window` (an Engine SDL window pumped between waits);
neither is a CTest. Each waits on 60 Hz absolute deadlines with the Waiter, a condition variable and `sleep_until`
for 30 s, and measures notify to wake for the Waiter and a condition variable. `tests/common/time/wake_baseline.py`
runs the declared procedure (os-subsystem D68): a 5 s sleeper before and after each mode, and `meta.txt` with the
commit, the program hashes, the OS, the CPU and the load. Evidence lives under `build/target/_build/test/logs/` of
the main checkout.

| Platform | Recorded | Waiter deadline P50 / P99, no window; window | Evidence |
|---|---|---|---|
| macOS Intel x86_64 (Kqueue) | 2026-10-27, `f6997194` | 0.077 / 0.145 ms; 0.077 / 0.134 ms | `os-wake-baseline-20261027-macos-intel` |
| macOS Intel x86_64 (Kqueue), TT-1 L2 | 2026-10-09, `a8f9535` | 0.025 / 0.10 ms; 0.077 / 0.133 ms (idle host) | `tt1-l2-20261009-idle` |
| Windows x64 (HighResolutionWaitableTimer), os-windows-run W4 | 2026-10-28, programs built from `4ffc7c65` (the Waiter before v8-arch-gyo2-os-wait) | 0.617 / 1.000 ms; 0.450 / 0.928 ms | `wake-baseline/` on the Windows machine ([record](../dev_logs/2026_10_28_engine_os_windows_run.zh-Hant.md)) |
| Linux x64 | cancelled (os-subsystem D78) | | |
| macOS arm64 | not run: no machine (CI runs L1 only) | | |

## Design Invariants

1. **One time base.** Engine code measures elapsed time, waits and stamps events on `Time::MonotonicClock`, which is
   `std::chrono::steady_clock` (D29). Guard: `tests/common/time/WaiterTests.cpp` "Time base is the steady clock and
   NowNs follows it". No guard keeps another clock out of the rest of `engine/`.
2. **A deadline wake is never early**: `woke >= planned`, and `late` is never negative. Guards: "Time Waiter never
   wakes before a deadline and reports how late it woke", "Time Waiter saturates how late a sentinel deadline is";
   mutant `tt1-waiter-early`.
3. **Notifications are latched and coalesce.** One sent before the wait is not lost; several before one wake give one
   wake; concurrent notifiers lose none. Guards: "latches a notification sent before the wait", "coalesces several
   notifications into one wake", "loses no notification under concurrent producers", "wakes on a notification from
   another thread before a far deadline"; mutants `tt1-notify-dropped`, `tt1-latch-cleared-on-entry`,
   `tt1-latch-never-consumed`.
4. **A passed deadline is reported first.** When the deadline has already passed, the wait reports `Deadline` and
   leaves a pending notification latched, so the next wait returns `Notified` at once. Guards: "returns at once for a
   deadline already passed", "reports a passed deadline first and keeps the notification latched"; mutant
   `tt1-latch-before-deadline`.
5. **Relative arming, then a re-check.** The Waiter hands `Os::WakeSignal::Block` a relative timeout every time and
   re-reads the time base after each wake; a spurious or early platform wake only waits again. No absolute time
   converted from the base is handed to the OS. Guard: mutant `tt1-no-recheck`.
6. **`Wait()` has no timer**: it returns only when notified, so a caller that must never poll (trace.md Invariant 2)
   can rely on it. Guard: "waits without a deadline until notified".
7. **The backend is reported, and its names stay.** `Backend()` answers which mechanism this Waiter uses, on every
   platform (the Windows fallback included); the `ToString` names do not change, since consumer logs carry them
   (os.md Invariant 9). The names are Time's `WaitBackend`; `Os::WaitMechanism` maps to it value for value
   (`static_assert` in `Waiter.cpp`). Guards: `tests/common/os/WakeSignalTests.cpp` "Os wake signal uses the platform
   mechanism" for the selection (mutants ow-x8, ow-x14, ow-x15), "Time Waiter reports the os mechanism" for the
   mapping (mutant ow-x7), "Time Waiter backend names stay as consumers log them" for the names; mutant
   `ti-x3-backend-name-changed`.
8. **The latch is Time's state** (AGENTS.md §3, State Authority): no other component keeps a copy of it or decides on
   it. engine/os never reads the latch; the wake primitive keeps its own pending signal (os.md Invariant 8). Guard:
   structural: `WakeSignal`'s API takes no latch, and engine/os cannot reach engine/time (os.md Invariant 3, R5 and
   the link check).
9. **Use after move is refused**: waiting on a moved-from Waiter is a Programmer Error, and `Notify` on it does
   nothing. Guard: "rejects use after move and ignores notifications on a moved-from waiter".
10. **`GYO::Time` links exactly `GYO::Base` and, privately, `GYO::Os`** (checked at configure time by
    `tests/common/time/CMakeLists.txt`, which also refuses `GYO::Os` in its interface): no SDL, no `GYO::Engine`, no
    other engine unit. The thread library comes through engine/os where the fallback needs it. Guards: mutants
    `ti-x1-time-links-more`, `ow-x2-time-os-public`.
11. **`LateWakeStats` has fixed bins, allocates nothing and runs no thread.** Guard: "Time LateWakeStats bins late
    wakes and bounds quantiles" for the bins and quantiles. No guard for the allocation.

### Replacement Criteria

Stop adding features and propose redesigning instead of patching when any of these holds:

1. A consumer needs more than one waiter on one Waiter, or a cancellation token instead of a notification and its
   own flag: the single-waiter latch would have to become something else.
2. A backend cannot keep Invariants 2 and 5 with a relative timeout and a re-check (a platform without relative
   timers, or a wake source that cannot be re-checked).
3. (Removed in v8-arch-gyo2-os-wait.) It said: moving the backends into engine/os changes a wake measurably in the
   Wait timing L2. It fired at W3 (the Windows window-mode deadline P50, os-windows-run) and ended in the D78
   freeze without the cause found; the move was then accepted by CI L1, the guards and mutants instead of a timing
   L2, by the user's decision (declaration 5.4). Timing changes of the backends are os.md Replacement Criterion 2.
4. Engine needs a second time base for scheduling (for example wall-clock deadlines): decide it explicitly instead
   of adding a clock.
