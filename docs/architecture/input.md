# GYO physical input frame

Owner: Engine (`engine/input` → `GYO::Input`, `engine/input/include/engine/input/PhysicalInputFrame.hpp`).
Filled by a backend, today only the SDL one ([platform-sdl.md](platform-sdl.md)).
Plan and record: [input-and-present](plans/input-and-present/README.md), batches IP-1 (done) and IP-3 (the parts
marked IP-3 below: event timestamps landed in ip3c, text input in ip3d).

## What it is

`PhysicalInputFrame` is one frame of physical input of one window, built between `BeginFrame` and `EndFrame` of a
backend:

- `keys` and `mouseButtons`: `ButtonState{held, pressed, released}` per physical key (scancode position,
  independent of the layout) and per mouse button.
- `pointer`: position, accumulated delta, relative mode.
- `windowFocused`.
- `events`: this frame's input and window events of this window in arrival order (`InputEvent`).
- IP-3 adds `InputEvent::timestamp` on the Engine time base and `text`: text and key presses in arrival order while
  text input is on (`TextEvent{kind, text, key, layoutKey, modifiers, repeat}`).

Ownership: `GYO::Input` owns the frame's shape and meaning. A backend owns filling it. Capture policy (when to
grab or release the pointer) and text editing policy (what Ctrl+A means) belong to the consumer.

## Contracts

- `held` is the state at the end of the frame; `pressed` and `released` describe the last transition of the frame,
  so a press and a release in one frame leave `pressed` false; `events` keeps every transition in order.
- `events` holds only this window's events. Key repeats are not events; neither are the releases a focus loss
  applies to held buttons.
- `Key` names and values stay stable: the first fourteen keys keep their original names and values, and new keys
  are appended.
- IP-3, `InputEvent::timestamp`: the type is `std::chrono::steady_clock::time_point`, asserted equal to
  `Time::TimePoint` in the backend, so `GYO::Input` gains no dependency on `GYO::Time`. Its meaning follows the
  backend's timestamp contract (for SDL, [platform-sdl.md](platform-sdl.md)).
- IP-3, `text`: filled only while text input is on (the backend asks SDL at arrival, it keeps no copy of that
  state); `Kind::Text` carries UTF-8 text; `Kind::Key` records presses only, repeats included, never releases;
  `key` is the physical position, `layoutKey` the key the current layout produces at that position (`Key::Count` if
  none); text and key entries keep their arrival order inside `text`; `BeginFrame` clears it. No ordering is
  promised between `text` and `events`.
- `SdlInput::SetTextDiagnostics` (v8-tool-ui-ime-observe): off by default; while on, each text input switch,
  composition (SDL's TEXT_EDITING and its candidates), typed text and text key is written to `Engine::Base::Log`
  (category `input.text`) with whether the frame took it. Diagnostics only: the frame is the same either way, and
  compositions are not part of the frame.
- Every focus loss, key press and mouse button event the backend receives for this window is an event in `events`,
  even when the state does not change (a focus loss while unfocused, a second non-repeat press); only key repeats are
  dropped. A product's capture policy may rely on it (sdl-isolation J1).
- Mouse button events: `insideWindow` compares the click position with the window's size when the backend handles
  the event, never with a size seen earlier; a product's capture policy may rely on it (sdl-isolation J1).

## Design Invariants

1. The frame is extended only by addition: no existing field, `InputEventKind` value or `Key` value changes its
   meaning, and `pressed`/`released`/`held` keep the semantics above. Guards: the IP-1 tests in
   `tests/common/input` keep their judgments; mutants m7 and m8 (IP-3).
2. `events` contains only this window's events in arrival order; key repeats and focus-loss releases are not
   events. Guard: the IP-1 repeat and focus tests; mutant m8 (a repeat entering `events`).
3. Text appears in `text` only while text input is on, as presses only, in arrival order. Guards: mutants m7, m9,
   m-up (IP-3).
4. The frame carries no capture policy, no text editing policy and no product knowledge.

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. A new backend or consumer needs a field whose meaning depends on the backend (the frame would stop being
   physical and neutral).
2. A new event kind cannot be added without changing an existing kind or the `pressed`/`released`/`held` semantics.
3. Consumers need a total order across `events` and `text`: the two lists should become one ordered stream.
