# GYO SDL platform backend

Owner: Engine (`engine/platform/sdl` → `GYO::PlatformSDL`, `engine/input/backend/sdl` → `GYO::InputBackendSDL`,
`engine/render/backend/sdl` → `GYO::RenderBackendSDL`, `engine/render/backend/sdl_gpu` → `GYO::RenderBackendSDLGPU`;
`engine/audio/backend/sdl` → `GYO::AudioBackendSDL` follows the same rules without a `*Native` target, see
[audio.md](audio.md)).
Plan and record: [input-and-present](plans/input-and-present/README.md), batches IP-3 and IP-4.

**Status (2026-10-21): approved design (input-and-present D31), being implemented.** Milestones ip3b and ip3c have
landed: the neutral/native split (Design Invariant 1) and event timestamps (Design Invariants 3 and 7). ip3d added
text input and the clipboard, ip3e the entry point, base directory and log bridge (Design Invariants 5 and 8). IP-3 is
complete. IP-4 is complete: ip4a (queries, operations, test injection), ip4b (event watches, capture), ip4c (PRIVATE
linking) and ip4d (full mutation run, L3). Each contract and invariant below names the milestone that
makes it true and the guard that keeps it true; until that milestone lands, the current code is the reference.
The input frame the backend fills is specified in [input.md](input.md).

## What it is

```text
Application / Editor
        │  neutral API: no SDL type, no SDL header
        ▼
GYO::PlatformSDL ─── GYO::InputBackendSDL ─── GYO::RenderBackendSDL / GYO::RenderBackendSDLGPU
        │  PRIVATE                                (each links SDL3 PRIVATE from IP-4 on)
        ▼
      SDL3
GYO::*Native (INTERFACE): the only targets that carry SDL3 to a user
```

- **Neutral layer.** The ordinary backend headers (`SdlPlatform.hpp`, `SdlInput.hpp`, `SdlRenderer.hpp`,
  `SdlGpuRenderDevice.hpp`) neither include SDL directly nor indirectly and contain no `SDL_` symbol. Events reach
  observers as `PlatformEvent` (valid only during the call, not copyable), carrying an Engine-time `Timestamp()`.
- **Native layer.** Every API that carries an SDL type lives in a `*Native.hpp` under a separate include root
  (`engine/<backend>/native/include`), exposed by one INTERFACE target per backend: `GYO::PlatformSDLNative`,
  `GYO::InputBackendSDLNative`, `GYO::RenderBackendSDLNative` (only when the SDL renderer component is enabled).
  Linking a `*Native` target is the declaration "this code uses SDL directly". A backend reaches its own native
  header through `target_include_directories(PRIVATE native/include)` and never links its own Native target, so no
  cycle forms; the input and render backends link `GYO::PlatformSDLNative` PRIVATE.
- **Process services** in `GYO::PlatformSDL`: `BaseDirectory()` (SDL's base path semantics, UTF-8 to a `char8_t`
  path; main thread or before other threads start), `PlatformName()` (diagnostics only, never for branching),
  `ClipboardText()`, the opt-in `SdlLogBridge`, and the entry point.
- **Entry point.** `GYO::PlatformSDLMain` is an OBJECT library that includes `<SDL3/SDL_main.h>`, defines `main`
  and calls the product's `int GyoMain(int argc, char* argv[])`. argv is UTF-8 on every platform. A product links
  it explicitly in its own CMake; the shared app project helper does not change.
- **Test support (IP-4).** Window and display queries that read SDL/OS every time; `SetWindowSize`,
  `SetWindowPosition`, `Raise`, `Sync`; `SdlInputInjector` (input backend) that pushes key, mouse and focus events
  through SDL's event queue via the platform's `PushInjectedEvent`; `AddEventWatch` returning an RAII `EventWatch`
  that hands out this window's events with Engine timestamps and an `injected` flag; a capture file writer in the
  GPU render backend.

Ownership: the platform owns the SDL window, the clock pair, the clipboard, the log bridge and the injection flag.
The input backend owns collecting input and text into the frame. Window state and text input state are owned by
SDL/the OS. Capture policy, text editing policy and every product rule stay with the consumer.

## Contracts

### Event timestamps (IP-3, milestone ip3c)

- On entering a pump the platform samples `e0 = Time::Now()`, `s = SDL_GetTicksNS()`, `e1 = Time::Now()` and uses
  the pair `engine = mid(e0, e1)`, `bracket = e1 - e0`. Only `SdlPlatform` holds the pair. The three reads and their
  order are fixed in one function that takes the readers as parameters, so tests check the order and the midpoint
  exactly with fake readers.
- An event with SDL time `t` gets `min(engine + (int64)(t - s), Now() at delivery)`; `t == 0` (events added without a
  timestamp) gets the delivery time.
- Guaranteed: (1) no timestamp is later than its delivery; (2) within one pump, events whose SDL times are nonzero
  and do not decrease keep non-decreasing timestamps (an event with `t == 0` gets its delivery time, which may be
  later than the next event's converted time); (4) `t == 0` maps to the delivery time.
- Not guaranteed: (3) increasing order along `events`, or order across pumps. SDL mixes OS event times with push
  times, and the pair has an error of about ±bracket/2.
- The watch path samples one pair per callback, keeping "when the event was queued" for observation.
- Known limits, documented and not corrected: sleep between queueing and pumping shifts a macOS result by the sleep
  length; on Linux the Engine clock (`CLOCK_MONOTONIC`) and SDL's (`MONOTONIC_RAW`) may differ in rate by about
  500 ppm, growing with the event's age relative to the pair; Windows event times have millisecond resolution
  (`GetMessageTime`), and macOS derives its offset from the first event's delay.

### Log bridge (IP-3, milestone ip3e)

- Effective only while a consumer holds an `SdlLogBridge`; one at a time. By default it tees: Base receives every SDL
  line and SDL's previous output still runs (`keepSdlOutput = true`); with `false` only Base receives it.
- Levels: trace/verbose/debug → Debug, info → Info, warn → Warning, error/critical → Error. Categories come from a
  static table as `sdl.<category>`; custom categories are `sdl.custom`. The path is `noexcept` and allocates nothing
  that can throw.
- Re-entry on the same thread sends the line only to SDL's previous output.
- Precondition: while a bridge exists, the Base log handler does not call SDL's log functions (two threads could
  otherwise wait on each other's lock; SDL's log lock is recursive, so a thread-local guard cannot prevent it).
- Destruction checks that the bridge is still SDL's output function before restoring the previous one, and must
  happen before `SDL_Quit` (which destroys SDL's log lock).

### Pump, observers and watches

- `PumpEvents` may be nested (an observer may pump again, as IP-2 guarantees for live frames). Each pump, nested
  or not, samples its own clock pair; when a nested pump returns, the outer pump's pair is restored, so the outer
  pump's remaining events keep its pair.
- Queries (`QueryWindow`, `QueryDisplay`, `VideoDriverName`, `X11WindowNumber`) read SDL's cached state; they run on
  the main thread and may be called inside an observer and a live frame (a live frame reads the size SDL has
  already applied).
- Operations (`SetWindowSize`, `SetWindowPosition`, `Raise`, `Sync`) must not be called inside an observer, a live
  frame or a watch callback. `AddEventWatch`, `EventWatch` destruction and injection must not be called inside a
  live frame or a watch callback (SDL marks its watcher list as dispatching with a flag, not a depth). A
  thread-local dispatch depth checks this with `GYO_ASSERT`.
- Watch callbacks run on whichever thread pushes the event (on Windows in relative mode, SDL's raw input thread),
  inside SDL's global watcher lock (the live frame runs inside the same lock). A callback must not wait for
  anything that may be held during a platform call (pumping, injection, window operations, `EventWatch`
  destruction); a short critical section under a lock never held across a platform call is fine.
- An `EventWatch` keeps only the window ID, never the platform. Destroying it waits for an in-flight callback on
  another thread. If the platform is destroyed first, the watch receives no more events of that window (apart from
  the last window events SDL sends while destroying it), and destroying the watch afterwards is safe.
- Injection uses only SDL's event queue or real window operations. An injected event does not update SDL's internal
  mouse, keyboard or modifier state and produces no text input; motion is accumulated only while focused.

### Linking (IP-4, milestone ip4c)

- The four backends and `GYO::PlatformSDLMain` link SDL3 PRIVATE: SDL3 may appear in their
  `INTERFACE_LINK_LIBRARIES` only as `$<LINK_ONLY:>`. Only the three `*Native` targets carry SDL3 as INTERFACE.
  A `*Native` target in those interfaces may appear only as `$<LINK_ONLY:>` too, and no `native/include` directory
  is public. G3 checks this once the whole project is configured (`tests/common/platform_sdl/CMakeLists.txt`,
  `gyo_expect_private_sdl`), failing the configure step with a message naming the target.
- A target that uses SDL directly links a `*Native` target (or `SDL3::SDL3`) itself; nothing reaches SDL through a
  backend's ordinary target.
- Engine never calls the global `SDL_Init`/`SDL_Quit`; each subsystem is initialized and quit by its owner with
  SDL's reference counting (`SdlPlatform` owns VIDEO).

## Design Invariants

1. The non-native public headers of the SDL backends include no SDL header, directly or indirectly, and contain no
   `SDL_` symbol. Guards: G1 (a hygiene translation unit that fails if `SDL_stdinc_h_`, `SDL_h_` or
   `SDL_platform_defines_h_` is defined) and G2 (a ratchet scan of `engine/**/include` outside `native/`, with an
   allowlist that only shrinks and is empty after IP-3). From ip3b.
2. SDL types are reachable only through the `*Native` targets; linking one declares direct SDL use. From IP-4 the
   backends link SDL3 PRIVATE. Guard: G3 (configure-time link check). From ip4c.
3. Every time the platform hands out is on the Engine time base and satisfies the timestamp contract above.
   Conversion always samples both clocks together and never uses a fixed offset; the pump path samples once per
   pump, the watch path once per callback; only `SdlPlatform` holds the pair. Guards: mutants m1–m4 and m-zero
   (pump path, from ip3c); m20, m21, m22, m-watch-sdl, m-watch-engine, m-watch-mid, m-watch-first, m-watch-late and
   m-watch-e1 (watch path, from ip4b, D38).
4. Test injection goes only through SDL's event queue or real window operations; there is no back door that writes
   a frame. The `injected` flag is owned by the platform. Guards: mutants m14, m15. From ip4a.
5. The log bridge is effective only while a consumer holds it; without it SDL's output is unchanged. While it
   exists, the Base log handler does not call SDL's log functions. The bridge does not throw or recurse, restores
   only after checking it is still installed, and is destroyed before `SDL_Quit`. Guards: mutants m10, m11. From
   ip3e.
6. Window, display and text input state are read from SDL/the OS every time; no copy of them is kept for decisions.
7. Queries may run inside an observer and a live frame; operations may not run inside an observer, a live frame or
   a watch callback, and `AddEventWatch`, `EventWatch` destruction and injection may not run inside a live frame or
   a watch callback (thread-local dispatch depth, `GYO_ASSERT`). An `EventWatch` is destroyed on the main thread only
   (`GYO_ASSERT`, guard: mutant m-main-thread; SDL tears its watcher list down without its lock, D38). An `EventWatch` never refers to the
   platform and may outlive it. A nested `PumpEvents` samples its own clock pair and restores the outer pump's pair when it returns (guard:
   mutant m-nest); window operations are not called inside an observer or a live frame; watch
   callbacks do not wait for anything held during a platform call. Checked with `GYO_ASSERT`.
8. Engine does not call the global `SDL_Init`/`SDL_Quit`; subsystems are reference-counted by their owners. No
   exception crosses SDL's or the OS's C stack (live frames and the entry point catch and report).
9. The backend carries no capture policy, no text editing policy and no product knowledge.

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. A second, non-SDL backend is needed: propose a backend-neutral platform interface instead.
2. Apart from the ui_editor ImGui integration listed when IP-3 ends, an active product needs a `*Native` target for
   a new capability: the neutral API is cut wrongly.
3. `PlatformEvent` has to expose fields to users other than `SdlInput` and the native layer.
4. Each new event kind or window feature requires re-verifying the timestamp conversion or the log bridge.
5. Keeping invariant 3 on the pump path requires per-event sampling or sleep detection.
6. The pump has to leave the main thread and `SdlPlatform` cannot meet SDL's main-thread rules.
