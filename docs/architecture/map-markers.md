# Map markers (gyo.map_markers)

Owner: Engine (`engine/map_markers` → `GYO::MapMarkers`; `engine/map_markers/include/engine/map_markers/MapMarkers.hpp`).
Plan and record: [big map](plans/big-map/README.md), batch v8-arch-big-map-engine-format (decision
v8-arch-big-map-markers-owner: Engine owns the format, a product owns the meaning, a tool edits it).

**Status (2026-11-06): version 1 landed with v8-arch-big-map-engine-format.** The voxel map editor reads it and its
library writes it (v8-arch-big-map-tool-chunked; placing markers in its window is the window batch's); object_fps_pvp
(v8-arch-big-map-product-arena-v3) is planned to read it.

## What it is

Named places on one map: points with a heading, axis-aligned boxes and paths. Each marker has an id, a kind and
string tags. Which kinds exist, what a kind or a tag means, and whether a marker makes sense where it stands belong
to the product that reads the file; this module checks form only. The precedent is
[gyo.anim_events](animation-events.md): Engine parses, names stay opaque, a product gives them meaning.

```json
{
  "schema": "gyo.map_markers",
  "version": 1,
  "markers": [
    { "id": "m1", "kind": "a", "tags": {},                       "point": [0, 16, 0], "yaw": 1.5 },
    { "id": "m2", "kind": "b", "tags": { "k": "v" },             "box": { "min": [0, 0, 0], "max": [32, 32, 32] } },
    { "id": "m3", "kind": "c", "tags": { "x": "1", "y": "2" },   "path": [[0, 0, 0], [16, 0, 0]] }
  ]
}
```

- **Asset.** A UTF-8 JSON file beside the map it belongs to. Its catalog type, how a product finds it and which map
  it belongs to are the product's.
- **Units.** Coordinates are integers in 1/16 of a cell from the map's origin, the frame of the map's own geometry.
  The module returns them as integers: the cell size and the origin are facts of the map, kept in one place, so a
  consumer converts them with the map (for a `gyo.voxel_map`, `Engine::VoxelMesh::MapUnitsToWorld`).
- **Yaw.** An angle in radians, a finite float; its zero, direction and range are the product's.
- **Kinds.** A product lists the kinds it knows in a `gyo.marker_kinds` file (below) and rejects a marker whose kind
  is not listed when it loads the map; an editor reads the same file as a palette without interpreting it.

### Validation (version 1)

`ParseMapMarkers(text)` decides every rule. Any violation is a `MapMarkersErrorCode::InvalidMapMarkers` Runtime Error
whose detail is the path of the offending value (for example `markers[2].tags`); a repeated key or a text that is
not JSON reports an empty path. Nothing is ignored or repaired.

1. (K1) The document is one JSON object with exactly the fields `schema`, `version` and `markers`; no key repeats in
   any object.
2. (K2) `schema` is `"gyo.map_markers"`; `version` is the integer 1.
3. (K3) `markers` lists 0 to 65,536 markers in strictly increasing byte order of `id` (so ids are unique).
4. (K4) A marker has exactly `id`, `kind`, `tags` and one shape: `point` with `yaw`, `box`, or `path`.
5. (K5) `id` and `kind` are 1 to 64 characters of `a`–`z`, `0`–`9`, `_`, `.` and `-`.
6. (K6) `tags` is an object of 0 to 32 entries; each key is 1 to 64 characters of `a`–`z`, `0`–`9`, `_` and `.`,
   keys appear in strictly increasing byte order in the text, and each value is a string of 0 to 256 printable
   ASCII characters.
7. (K7) `point` is three integers; `yaw` is a number that is finite as a float.
8. (K8) `box` has exactly `min` and `max`, three integers each, `min < max` on every axis.
9. (K9) `path` lists 2 to 4,096 points of three integers.
10. (K10) Every coordinate lies within ±2^20 units; the text is at most 4 MiB.

A JSON number with a fraction or an exponent is not an integer. Repeated path points and overlapping shapes are
valid: what they mean is the product's.

## Marker kinds (gyo.marker_kinds)

The list of kinds a product knows: written by the product, read by the product and by any editor that places
markers for it (decision `v8-arch-big-map-marker-kinds-format`, 2026-11-06). It plays the role a Source `.fgd`
file plays for Hammer: the game tells the editor which names exist, the editor offers them and interprets none.
The format describes one asset's vocabulary in Engine vocabulary (opaque names) and carries no meaning, so the
layers table ([architecture](../architecture.md), "Layers") gives the format to Engine and the content to the
product; the two readers then share one validator instead of each writing its own.

```json
{ "schema": "gyo.marker_kinds", "version": 1, "kinds": ["a", "b", "c"] }
```

**Status (2026-11-06): `ParseMarkerKinds` and `UnknownMarkerKinds` landed with v8-arch-big-map-tool-chunked (its
`-markers` step)**, with one rule test per rule (Q1 to Q5) in `tests/common/map_markers/MapMarkersTests.cpp` and
their mutants (`mm-kinds-*`, `mm-unknown-kinds-none`) in `docs/architecture/plans/big-map/scripts/engine_format_mutations.json`.
The voxel map editor reads kinds through them (`check-markers`).

### Validation (version 1)

`ParseMarkerKinds(text)` decides every rule and returns the kinds in text order. Any violation is a
`MapMarkersErrorCode::InvalidMarkerKinds` Runtime Error whose detail is the path of the offending value (for example
`kinds[3]`); a repeated key or a text that is not JSON reports an empty path. Nothing is ignored or repaired.

1. (Q1) The document is one JSON object with exactly the fields `schema`, `version` and `kinds`; no key repeats.
2. (Q2) `schema` is `"gyo.marker_kinds"`; `version` is the integer 1.
3. (Q3) `kinds` lists 1 to 256 strings in strictly increasing byte order (so kinds are unique).
4. (Q4) Each kind satisfies rule K5 (1 to 64 characters of `a`–`z`, `0`–`9`, `_`, `.` and `-`).
5. (Q5) The text is at most 64 KiB.

### Relation to a markers file

A markers file is valid on its own: `ParseMapMarkers` does not take a kinds list, and which list applies to a map
is the product's fact. A marker whose `kind` is not in that list is an error for the product that loads the map and
for an editor that places markers for it, and both find it the same way: `UnknownMarkerKinds(markers, kinds)`
returns the paths of the offending markers' kinds (`markers[i].kind`) in markers order, empty when there are none. What a kind means, which
shape it should have and which tags it needs stay the product's (Invariant 1); a version 2 may add per-kind form
facts (for example the shapes a kind allows) when an editor or a product shows the need, not before.

## Contracts

- **Versions.** A change of a field's meaning, a new required field, a new shape or a new rule is a new version; a
  reader rejects a version it does not know. Version 1 has no optional fields.
- **Producers.** Any file that passes validation is valid, whoever wrote it (a tool, a script or a hand). A
  producer writes tags in key order and reads what it wrote back with `ParseMapMarkers` before it keeps it.
- **Cost.** Parsing happens when content loads; nothing here runs per frame. The document is built with nlohmann's
  `ordered_json`, which keeps keys in text order (rule K6 needs it) and inserts each key with a linear search; so
  before building it, a scan rejects any object with more than 32 keys, more than any valid object has, at the path
  the rules would report for that object. The rules decide the same documents either way; the scan only keeps a
  malformed file from costing quadratic time. The scan itself keeps, per open container, only a count and the last
  key, and builds a path only to report an error, so its memory is linear in the text however deep the nesting
  (test `gyo_map_markers_alloc_tests`). `ParseMarkerKinds` has no such scan: its 64 KiB limit bounds the worst
  malformed file (one object of about 8,000 keys) to about 130 ms (measured by the v8-arch-big-map-tool-chunked
  reviewer at -O2), and only when content loads.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **Neutral.** `GYO::MapMarkers` knows ids, kinds, tags, points, boxes and paths; names are opaque. The module,
   its tests and this contract name no product and no gameplay concept. Guard: the test
   `gyo_map_markers.neutral_names`, which runs the shared guard `tests/common/scripts/neutral_names_guard.py` (it
   lists the words it rejects) on this module, its tests and this contract (mutant `mm-guard-gameplay-word`), and
   review.
2. **Leaf.** Its links are exactly `GYO::Base`, `GYO::Math` and, privately, the JSON parser: no geometry format,
   collision, render, asset or product library. Guard: the configure-time check `gyo_expect_map_markers_links`
   (`tests/common/map_markers/CMakeLists.txt`; mutant `mm-links-voxel-mesh`).
3. **One validator.** Whether a markers file is valid is decided by `ParseMapMarkers` alone, whether a kinds file
   is valid by `ParseMarkerKinds` alone, and whether a marker's kind is listed by `UnknownMarkerKinds` alone; no
   tool or product re-implements a rule. Guard: the rule tests in `tests/common/map_markers/MapMarkersTests.cpp`
   (one test case per rule, mutants `mm-*` in `docs/architecture/plans/big-map/scripts/engine_format_mutations.json`;
   the kinds rules are tested in the same file, mutants mm-kinds-* and mm-unknown-kinds-none in the same catalog), and review.
4. **Strict.** An unknown schema, version or field, a repeated key, a non-canonical order and a value out of range
   are errors, never ignored, clamped or repaired. Guard: the same rule tests and mutants.
5. **Integers.** Positions stay integers in the map's units; the module converts nothing to world floats, so it
   needs no fact of the map. Guard: review (the public header has no float position).

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. Markers need checks of meaning (where a marker may stand, which kinds go together): those are the product's;
   adding them here would break Invariant 1.
2. A product needs a shape that points, boxes and paths cannot express (a polygon, a sphere): that is a new
   version, decided with the user.
3. A marker needs to refer to another asset by id: that is a composition of assets, and its owner is decided again
   by the layers table ([architecture](../architecture.md), "Layers").
