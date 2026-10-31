# Animation events (gyo.anim_events)

Owner: Engine, the model and animation owner (`engine/model` → `GYO::Model`; `engine/model/AnimationEvents.hpp`).
Plan and record: [tools v8 asset toolchain](../tools/plans/v8/assets.md), batch v8-tool-assets-anim-events
(decision v8-tool-assets-anim-events-format: a generic format whose contract belongs to the animation owner, data
in each product, tools that know only this format).

**Status (2026-10-31): version 1 landed with v8-tool-assets-anim-events.** The data is written by hand (the manual
stage, AGENTS.md section 5); the model viewer (v8-tool-model-viewer) is planned to edit the same files.

## What it is

Named moments on the clips of one model: "at 2.35 s of `Armature|Reload`, `magazine_in`". A product decides what a
name means and what it triggers (a sound, an effect, a gameplay hint); the format and `GYO::Model` only carry it.

```json
{
  "schema": "gyo.anim_events",
  "version": 1,
  "model_asset_id": "object_fps_pvp.model.nailer_v2",
  "clips": {
    "Armature|Reload": [
      { "seconds": 2.35, "event": "magazine_in" }
    ]
  }
}
```

- **Asset.** A UTF-8 JSON file in a product's asset root, registered in that product's catalog with type `text`
  (no new asset type); the `schema` field tells a tool what the file is. A product refers to it by asset ID from
  its own definitions (PvP: `animation_events_asset_id` in a weapon presentation).
- **Clip time.** `seconds` is the clip's own timeline, the same seconds as `AnimationClip::durationSeconds` of the
  loaded model, from 0 to the clip's duration. It does not change when a product plays the clip faster, slower or
  stretched over an action; mapping clip seconds to playback and to Engine time is the consumer's.
- **Clips by name.** A clip is named exactly as the model names it (for FBX, `Armature|Reload`), not by a product's
  semantic name (an animation set's `Reload`), which belongs to the product.

### Validation (version 1)

`ParseAnimationEvents(text)` checks everything that does not need the model; `BindAnimationEvents(set, model)`
checks the rest. Any violation is a `ModelErrorCode::InvalidAnimationEvents` Runtime Error whose detail is the
path of the offending value (for example `clips."Armature|Reload"[0].seconds`). Nothing is ignored or repaired.

1. The document is one JSON object with exactly the fields `schema`, `version`, `model_asset_id` and `clips`; no
   key repeats in any object (a repeated key is an error, not "the last one wins").
2. `schema` is `"gyo.anim_events"`; `version` is the integer 1.
3. `model_asset_id` is a nonempty string.
4. `clips` is a nonempty object; each key is a nonempty clip name and each value a nonempty array of events.
5. An event is an object with exactly `seconds` and `event`. `seconds` is a finite number, not negative. `event`
   is 1 to 64 characters of `a`–`z`, `0`–`9`, `_` and `.`.
6. In one clip, events are in non-decreasing `seconds`; the same name twice at the same seconds is an error.
   Different names may share a moment, and a name may recur at other moments.
7. Binding: every clip name exists in the model, and every `seconds` is at most that clip's duration. Binding fails
   as a whole; no event is dropped.

The module does not know asset IDs: the consumer checks that the model it binds to is the one `model_asset_id`
names (PvP rejects a file written for another model).

### Using it

```text
product definition ──animation_events_asset_id──► text asset ──ParseAnimationEvents──► AnimationEventSet
                                                                     │
                                         loaded ModelAsset ──BindAnimationEvents──► BoundAnimationEvents
                                                                     │                  (per clip index)
product: clip seconds → its playback (speed, stretch) → Engine time ─┘
```

`BoundAnimationEvents::ForClip(clipIndex)` lists a clip's events in order. A consumer that plays a clip with
`AnimationInstance` can test each event with `PlaybackInterval::Crossed`; a consumer that knows an action's whole
timeline ahead (PvP's reload, stretched over the authority's reload time) converts every moment at the start.

## Contracts

- **Versions.** A change of a field's meaning, a new required field or a new rule is a new version; a reader rejects
  a version it does not know. Version 1 has no optional fields.
- **Producers.** Any file that passes validation is valid, whoever wrote it: by hand, by a script or by a tool.
  A tool keeps its own copy of the rules only together with a test that aligns it with this parser.
- **Cost.** Parsing and binding happen when content loads; the bound events are plain vectors. Nothing here runs per
  frame except what a consumer does with them.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **Neutral.** `GYO::Model` knows an event as a clip, a moment and an opaque name only: no sounds, effects,
   weapons, characters or product names, and no API named after a product meaning. Its links are exactly
   `GYO::Engine`, `GYO::Math` and, privately, the JSON parser: no audio, render or product library. Guard: the
   configure-time check `gyo_expect_model_links` (`tests/common/model/CMakeLists.txt`; mutant `ae-model-links-audio`)
   and review for names.
2. **Clip time.** Moments are in the clip's own seconds. Playback speed, stretching and Engine time belong to the
   consumer and never enter the file or the module. Guard: binding checks moments against the clip's own duration
   ("binding fails as a whole for a missing clip or an event beyond its clip"); review.
3. **One validator.** Whether a file is valid is decided by `ParseAnimationEvents` and `BindAnimationEvents` alone.
   The runtime needs no tool, and a file is not more or less valid for where it came from. Guard: the rule tests in
   `tests/common/model/AnimationEventsTests.cpp` (one test case per rule group, mutants `ae-*` in
   `docs/tools/plans/v8/scripts/anim_events_mutations.json`); a tool that validates on its own adds an alignment test.
4. **Strict.** An unknown schema, version or field, a repeated key, an unsorted or repeated event and a value out of
   range are errors, never ignored, clamped or repaired. Guard: the same rule tests and mutants.
5. **Bound to one model.** A file names one model and its clips by exact name. A missing clip or a moment beyond its
   clip fails binding as a whole. Guard: "binding fails as a whole for a missing clip or an event beyond its clip";
   the PvP presentation test rejects a file of another model.

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. Events need content beyond a name at a moment (parameters, durations or ranges, per-event data), so "a moment
   and a name" is no longer the format's premise.
2. Events must attach to something other than one clip of one model (blend trees, state machines, clips transferred
   between models), so a model and a clip name no longer identify the timeline.
3. A tool has to re-verify the format's rules on its own each time it gains a feature, or its rules and this parser
   keep diverging.
