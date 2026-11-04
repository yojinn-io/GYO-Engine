# Event sounds (gyo.event_sounds)

Owner: the framework (`framework/`): the structure, the rules and the binding in the core
(`gyo/framework/EventSounds.hpp`, `GYO::ToolFramework`), the text in the JSON binding
(`gyo/framework/json/EventSoundsText.hpp`, `GYO::ToolFrameworkJson`).
Plan and record: [event-sounds plan](../architecture/plans/event-sounds/README.md), card
`v8-arch-event-sounds-contract` (user, 2026-11-04: a composition format owned by the framework; Engine's
`gyo.anim_events` and `GYO::Audio` stay as they are; one event, one sound).

**Status (2026-11-04): version 1 is specified; the code lands with `v8-arch-event-sounds-impl`.** The data is
written by hand (the manual stage, AGENTS.md section 5); the model viewer (`v8-tool-model-viewer`) is planned to
preview and later edit the same files.

## What it is

Which sound plays at a named moment on a model's clips, and which point of that sound lands on the moment:
"when `magazine_in` happens, play `game.audio.magazine_in` so that its 0.33 s lands on the moment, at gain 0.8".
It joins two Engine assets: the moments of a `gyo.anim_events` file
([contract](../architecture/animation-events.md)) and sound clips (`GYO::Audio`). It does not describe either
asset, and it does not say what a moment means to a product.

```json
{
  "schema": "gyo.event_sounds",
  "version": 1,
  "animation_events_asset_id": "game.anim_events.rifle.viewmodel",
  "events": {
    "magazine_out": { "sound_asset_id": "game.audio.magazine_out", "sync_seconds": 0.05, "gain": 0.8 },
    "magazine_in":  { "sound_asset_id": "game.audio.magazine_in",  "sync_seconds": 0.33, "gain": 0.8 },
    "bolt_pull":    { "sound_asset_id": "game.audio.bolt_pull",    "sync_seconds": 0.10, "gain": 0.8 }
  }
}
```

- **Asset.** A UTF-8 JSON file in a product's asset root, registered in that product's catalog with type `text`
  (no new asset type); the `schema` field tells a tool what the file is. A product refers to it by asset ID from its
  own definitions (for example next to the `animation_events_asset_id` of a weapon presentation).
- **One file per anim_events file.** A file names one `gyo.anim_events` file, so the same event name may sound
  different on two models. It is not one table for a whole product.
- **One event, one sound.** Each action that sounds is its own event with its own clip (`magazine_out`,
  `magazine_in`, `bolt_pull`), so stretching an action moves every sound with its own moment. One stitched clip on
  one event is the special case of a single entry. An event with no entry is silent: the mapping goes from sounds to
  events, and an event may exist for something else (an effect, a gameplay hint).
- **Sound time.** `sync_seconds` is the sound clip's own timeline, from its first sample: the point that lands on
  the event's moment. A sound may start before its moment (the sync point is later than 0) and so before the
  action. Playback speed and stretching of the animation, and Engine time, belong to the consumer.
- **Gain.** `gain` is a `GYO::Audio` gain of the clip as played, in [0, `Engine::Audio::kMaxGain`]. Spatial
  attenuation, panning and a product's own volume scales belong to the consumer and are applied on top.

### Validation (version 1)

`ParseEventSounds(text)` (the JSON binding) checks the text and then calls the core's `CheckEventSoundSet`, which
checks everything that needs no other asset. `BindEventSounds(sounds, events, soundSeconds)` (the core) checks the
references. A violation of rules 1 to 6 is an `EventSoundsErrorCode::InvalidEventSounds` Runtime Error, of rules 7
and 8 an `UnboundEventSounds`; the detail is the path of the offending value (for example
`events."magazine_in".sync_seconds`). Nothing is ignored, clamped or repaired.

1. The document is one JSON object with exactly the fields `schema`, `version`, `animation_events_asset_id` and
   `events`; no key repeats in any object (a repeated key is an error, not "the last one wins").
2. `schema` is `"gyo.event_sounds"`; `version` is the integer 1.
3. `animation_events_asset_id` is a nonempty string.
4. `events` is a nonempty object; each key is a nonempty event name and each value an object with exactly
   `sound_asset_id`, `sync_seconds` and `gain`. One name, one entry (rule 1 refuses a second).
5. `sound_asset_id` is a nonempty string.
6. `sync_seconds` is a finite number, not negative. `gain` is a number that `Engine::Audio::IsValidGain` accepts
   as a float.
7. Binding, events: every event name is the name of at least one event in the `gyo.anim_events` set the file is
   bound to. Since Engine validated those names, a bound name also has Engine's form (1 to 64 characters of
   `a`–`z`, `0`–`9`, `_` and `.`); the framework does not copy that rule (Design Invariant 3).
8. Binding, sounds: for every entry, `soundSeconds(sound_asset_id)` answers the clip's length in seconds, finite and
   not negative, and `sync_seconds` is at most that length. No answer (`std::nullopt`) or another length is an
   error. Binding fails as a whole; no entry is dropped.

The framework does not know asset IDs: the caller checks that the `gyo.anim_events` set it binds to is the one
`animation_events_asset_id` names, and answers `soundSeconds` from the clips it loaded through Engine (for example
from a clip's frame count over `Engine::Audio::kSampleRate`). `BindEventSounds` opens no file.

### Using it

```text
product definition ──event_sounds_asset_id──► text asset ──ParseEventSounds──► EventSoundSet
                                                                   │
          AnimationEventSet (Engine) + the caller's soundSeconds ──BindEventSounds──► BoundEventSounds
                                                                   │
   for each event of a clip (Engine): Find(event name) → sound, sync point, gain
   EventSoundOffsetSeconds(event seconds, clip seconds, action seconds, sync point) → when it starts
```

- `BoundEventSounds::Find(name)` gives the entry of an event name, or nothing for a silent event. A consumer
  iterates the events Engine bound to a clip and looks each one up.
- `EventSoundOffsetSeconds(eventSeconds, clipSeconds, actionSeconds, syncSeconds)` is when an event's sound starts,
  in seconds after the action starts, for a clip of `clipSeconds` played stretched over an action of
  `actionSeconds`: `eventSeconds × actionSeconds / clipSeconds − syncSeconds`. It may be negative (the sound leads
  the action; a consumer that starts now plays it late or skips its start, its own choice). It gives nothing for a
  clip or an action without a positive finite length, or a non-finite moment or sync point. Every consumer that
  previews or plays event sounds uses this one function, so a preview lands where the game does.

## Contracts

- **Versions.** A change of a field's meaning, a new required field or a new rule is a new version; a reader
  rejects a version it does not know. Version 1 has no optional fields.
- **Producers.** Any file that passes validation is valid, whoever wrote it: by hand, by a script or by a tool.
  A tool keeps its own copy of the rules only together with a test that aligns it with this parser. The framework
  writes no file (its Design Invariant 4): version 1 has no writer; a tool that edits the file writes its text
  through `engine/io`.
- **Cost.** Parsing and binding happen when content loads; the bound set is a plain vector looked up by name.
  Nothing here runs per frame except what a consumer does with it.

## Layers checks

The checks of `docs/architecture.md` ("Layers: admission and format ownership") for this format.

- **Check 2, two products and the Engine capabilities.** The products: the `object_fps_pvp` Client (its reload
  sounds today, from its own `sfx.json`) and the model viewer (`v8-tool-model-viewer`, preview and editing). The
  model viewer is planned and not built: today one product reads the mapping, so the second reader is predicted
  pressure, accepted by the user on 2026-11-04 when the framework was chosen as the owner (the request of the tool
  line, `docs/tools/plans/v8/anim-event-sound-request.md` on `claude/v8-tool-audio-editor-edit`, shows that the
  viewer cannot preview sounds without it and must not read a product's format, AGENTS.md section 9). The Engine
  capabilities it joins: `GYO::Model`'s event set (`AnimationEventSet`, the names and moments of `gyo.anim_events`),
  `GYO::Audio`'s clips and gains (`IsValidGain`, a clip's length) and the asset ids of the content contract.
  `EventSoundOffsetSeconds` is the composition itself, the mapping of a clip moment and a sound's sync point to
  action time; it wraps no single Engine call.
- **Check 3, which kind of format.** It describes how assets combine: a moment of one asset (an anim_events file)
  with another asset (a sound clip) and a point in it. It describes neither asset, so it is not Engine's, and it
  carries no product meaning, so it is not a product's. One answer, one document.
- **Check 4, the fields.**

  | Field | Kind of reference |
  |---|---|
  | `schema`, `version` | the format's own identity |
  | `animation_events_asset_id` | an Engine asset id (a `text` asset holding `gyo.anim_events`) |
  | key of `events` | an event name of that `gyo.anim_events` file: Engine's opaque name, matched exactly |
  | `sound_asset_id` | an Engine asset id (a `sound` asset) |
  | `sync_seconds` | an Engine value: seconds on the sound clip's own timeline |
  | `gain` | an Engine value: a `GYO::Audio` gain |

  No field carries a product name, and no field decides a product's behaviour by what a name means. The facts the
  document references are checked through Engine: the event names against the `AnimationEventSet` Engine parsed,
  the gain by `Engine::Audio::IsValidGain`, the sync point against the length of the clip the caller loaded through
  Engine.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **Neutral.** The format and its code know an event as an opaque name, a sound as an Engine asset id, and
   seconds and gains as Engine values: no weapons, actions, cues, characters or product names, and no API named
   after a product meaning. Every reference is an Engine asset id or an Engine value. Guard: the framework
   dependency guard (no registered product or tool id in framework code); the rule tests use synthetic data only;
   review for names.
2. **One validator.** Whether a file is valid is decided by `ParseEventSounds`, `CheckEventSoundSet` and
   `BindEventSounds` alone. A runtime needs no tool, and a file is not more or less valid for where it came from.
   Guard: the rule tests in `tests/framework/EventSoundsTests.cpp` (one test case per rule group, ES0 to ES9) and
   the `es-*` mutants in `docs/architecture/plans/event-sounds/scripts/event_sounds_mutations.json`; a tool that
   validates on its own adds an alignment test.
3. **Strict.** An unknown schema, version or field, a repeated key, a missing field and a value out of range are
   errors, never ignored, clamped or repaired. The rules Engine owns are applied by calling Engine
   (`IsValidGain`) or through what Engine validated (event names), never copied. Guard: the same rule tests and
   mutants.
4. **Bound to one anim_events file.** A file names one `gyo.anim_events` file and its events by exact name, and
   every entry's sound by its asset id. A name in no clip of that set, a sound without a length or a sync point
   beyond its sound fails binding as a whole. Guard: ES7 and ES8 ("binding fails as a whole"), mutants
   `es-bind-*`.
5. **Writes nothing.** The core and the binding open no file and write none: parsing takes text, binding takes
   the parsed sets and the caller's answer for a sound's length, and the results are structures. Guard: the API
   has no file or path parameter and no writer (review); the framework's Design Invariant 4.
6. **One schedule.** When an event's sound starts is `EventSoundOffsetSeconds` alone: a product and a tool that
   play or preview event sounds call it and keep no copy. Guard: ES9 and mutants `es-offset-*`; review of the
   consumers' migration batches.

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. An event needs more than one sound, or an action other than a sound (an effect, a variation picked at random, a
   stop or a fade), so "one event, one sound" is no longer the format's premise.
2. The format has to reference something other than an Engine asset id or an Engine value (a product's cue, a
   bus, a mixer group, a product's volume scale).
3. A product asks for a field of its own, or for a rule that holds for it alone.
4. Validating the format needs an Engine rule that Engine does not expose, so the framework would copy it.
