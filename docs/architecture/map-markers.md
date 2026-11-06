# Map markers (gyo.map_markers)

Owner: Engine (`engine/map_markers` → `GYO::MapMarkers`; `engine/map_markers/MapMarkers.hpp`).
Plan and record: [big map](plans/big-map/README.md), batch v8-arch-big-map-engine-format (decision
v8-arch-big-map-markers-owner: Engine owns the format, a product owns the meaning, a tool edits it).

**Status (2026-11-06): version 1 landed with v8-arch-big-map-engine-format.** No product or tool reads it yet; the
voxel map editor (v8-arch-big-map-tool-chunked) is planned to edit it and object_fps_pvp
(v8-arch-big-map-product-arena-v3) to read it.

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
- **Kinds.** A product lists the kinds it knows (its own data, for example a `marker_kinds.json`) and rejects others
  when it loads the file; an editor reads that list as a palette without interpreting it.

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

## Contracts

- **Versions.** A change of a field's meaning, a new required field, a new shape or a new rule is a new version; a
  reader rejects a version it does not know. Version 1 has no optional fields.
- **Producers.** Any file that passes validation is valid, whoever wrote it (a tool, a script or a hand). A
  producer writes tags in key order and reads what it wrote back with `ParseMapMarkers` before it keeps it.
- **Cost.** Parsing happens when content loads; nothing here runs per frame. The document is built with nlohmann's
  `ordered_json`, which keeps keys in text order (rule K6 needs it) and inserts each key with a linear search; so
  before building it, a scan rejects any object with more than 32 keys, more than any valid object has, at the path
  the rules would report for that object. The rules decide the same documents either way; the scan only keeps a
  malformed file from costing quadratic time.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **Neutral.** `GYO::MapMarkers` knows ids, kinds, tags, points, boxes and paths; names are opaque. The module,
   its tests and this contract name no product and no gameplay concept. Guard: `gyo_map_markers.neutral_names`
   (`tests/common/map_markers/neutral_names_guard.py`, which lists the words it rejects and also scans
   `GYO::VoxelMesh`; mutant `mm-guard-gameplay-word`) and review.
2. **Leaf.** Its links are exactly `GYO::Base`, `GYO::Math` and, privately, the JSON parser: no geometry format,
   collision, render, asset or product library. Guard: the configure-time check `gyo_expect_map_markers_links`
   (`tests/common/map_markers/CMakeLists.txt`; mutant `mm-links-voxel-mesh`).
3. **One validator.** Whether a file is valid is decided by `ParseMapMarkers` alone. Guard: the rule tests in
   `tests/common/map_markers/MapMarkersTests.cpp` (one test case per rule, mutants `mm-*` in
   `docs/architecture/plans/big-map/scripts/engine_format_mutations.json`).
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
