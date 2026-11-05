# Voxel mesh (gyo.voxel_map, gyo.voxel_region; gyo.voxel_mesh, deprecated)

Owner: Engine (`engine/voxel_mesh` → `GYO::VoxelMesh`; `engine/voxel_mesh/VoxelMesh.hpp`).
Plan and record: [voxel map editor](../tools/plans/v8/voxel-map-editor.md), batch v8-engine-voxel-mesh-contract
(decisions v8-tool-voxel-offline-build, v8-tool-voxel-layers, v8-tool-voxel-units and
v8-tool-voxel-artifact-encoding: a tool bakes a grid-built map offline; the game reads only the baked file, whose
format and only validator belong to Engine).

**Status (2026-11-06): the chunked map (`gyo.voxel_map` version 1 with `gyo.voxel_region` version 1) landed with
v8-arch-big-map-engine-format ([plan](plans/big-map/README.md)); no tool or product uses it yet.** The single-file
`gyo.voxel_mesh` version 1 (2026-11-05, v8-engine-voxel-mesh-contract) is **deprecated**: the voxel map editor
(`v8-tool-voxel-map-editor-core`) still writes it and object_fps_pvp loads it as the geometry of its arena files
(`v8-product-arena-v2`). It stays readable until v8-arch-big-map-product-arena-v3 has moved both maps to the chunked
format, and is removed in that batch with its tests and mutants (decision `em-v1-retire`, 2026-11-06). Nothing new
is added to it.

## The chunked map (version 1)

A map is an index and the files of its regions. A **chunk** (16^3 cells) is the unit of validation and expansion; a
**region** (8 × 8 chunk columns, every height) is the unit of files; how a consumer draws, loads or caches them is
its own. Every iteration order follows the total order of keys: chunk keys `(x, y, z)` and region keys `(x, z)`,
lexicographic.

```text
<map directory>/
├── map.json            gyo.voxel_map v1     the index (at most 1 MiB)
└── regions/…json       gyo.voxel_region v1  one region (at most 16 MiB)
```

The module parses text; it opens no file and knows no directory. A region's `file` is an opaque relative name that
the consumer resolves.

```json
{
  "schema": "gyo.voxel_map", "version": 1,
  "source_sha256": "<64 lowercase hex digits>", "build_version": "voxel_map_editor 2",
  "origin": [0, 0, 0], "cell_size": 0.5,
  "chunk_size": 16, "region_size": 8,
  "bounds": { "min": [-2, 0, -2], "max": [5, 3, 5] },
  "palette": [ { "name": "a", "faces": [[r, g, b, a], "… six colours"] } ],
  "regions": [ { "at": [-1, -1], "file": "regions/r_-1_-1.json", "sha256": "<64 lowercase hex digits>" } ]
}
```

```json
{
  "schema": "gyo.voxel_region", "version": 1,
  "at": [-1, -1],
  "chunks": [ { "at": [-2, 0, -2], "quads": [[0, 0, 0, 2, 16, 16, 0]], "colliders": [{ "min": [0, 0, 0], "max": [16, 16, 16] }] } ]
}
```

- **Units.** A chunk's quads and boxes are in 1/16 of a cell from the chunk's minimum corner, 0 to 256 on every
  axis (256 is the chunk's far side, so a face may lie on it). A world coordinate is
  `float(local + 256 × key + 16 × origin) × cell_size / 16`: the version 1 formula with `units` replaced by the
  chunk's offset, so the same geometry expands to the same bits in either format.
- **Regions.** Region `(rx, rz)` holds the chunks with `floor(x / 8) = rx` and `floor(z / 8) = rz`, every `y`.
- **Palette.** One palette for the whole map (as version 1, at most 256 entries); `ExpandVoxelPalette` gives its
  image once, and every chunk's uv addresses it.

### Validation of the index (`ParseVoxelMap`, error `InvalidVoxelMap`)

1. (M1) One JSON object with exactly `schema`, `version`, `source_sha256`, `build_version`, `origin`, `cell_size`,
   `chunk_size`, `region_size`, `bounds`, `palette` and `regions`; no key repeats in any object.
2. (M2) `schema` is `"gyo.voxel_map"`; `version` is the integer 1.
3. (M3) `source_sha256` and `build_version` as version 1 rule 3.
4. (M4) `origin` and `cell_size` as version 1 rule 4.
5. (M5) `chunk_size` is the integer 16 and `region_size` the integer 8 (fixed in version 1; another value is a
   new version).
6. (M6) `bounds` has exactly `min` and `max`, three integers each, chunk keys, both ends included, `min ≤ max` on
   every axis; every corner of the whole range, `16 × origin + 256 × key` and `+ 256` on the far side, lies within
   ±2^20 units.
7. (M7) `palette` as version 1 rule 5.
8. (M8) `regions` lists one or more entries of exactly `at` (two integers, the region's `x` and `z`), `file` (1 to
   128 characters of `a`–`z`, `0`–`9`, `_`, `.`, `-` and `/`, not starting with `/`, without empty, `.` or `..`
   segments, unique in the list) and `sha256` (64 lowercase hexadecimal digits); every `at` lies in the regions of
   `bounds`; entries are in strictly increasing order of `at`.
9. (M9) Every integer lies within ±2^30.
10. (M10) The text is at most 1 MiB.

### Validation of a region (`ParseVoxelRegion(text, map)`, error `InvalidVoxelRegion`)

1. (R1) One JSON object with exactly `schema`, `version`, `at` and `chunks`; no key repeats.
2. (R2) `schema` is `"gyo.voxel_region"`; `version` is the integer 1.
3. (R3) `at` is two integers naming a region the map lists, and the text's SHA-256 is that entry's `sha256`.
4. (R4) `chunks` lists one or more objects of exactly `at`, `quads` and `colliders`; each `at` is three integers,
   in this region and in the map's bounds; chunks are in strictly increasing key order (empty chunks are left out).
5. (R5) Every integer lies within ±2^30; the text is at most 16 MiB.

And for every chunk, the chunk rules:

1. (C1) A chunk has at least one quad or collider, and lies in the map's bounds.
2. (C2) Quads as version 1 rule 6 (palette indices into the map's palette).
3. (C3) Colliders as version 1 rule 7.
4. (C4) Every corner of a quad or collider lies within [0, 256] on every axis.
5. (C5) Quads and colliders are each in strictly increasing order (version 1 rule 9).

`ValidateVoxelChunk(chunk, map)` checks C1 to C5 on a chunk that never was text (a tool's working copy), with the same
code that `ParseVoxelRegion` runs for each chunk (error `InvalidVoxelChunk`, paths relative to the chunk such as
`quads[3][4]`; `ParseVoxelRegion` reports `chunks[2].quads[3][4]`). A repeated key or a text that is not JSON reports
an empty path. Nothing is ignored or repaired.

### Using the chunked map

```text
tool ──writes──► map.json + regions ──ParseVoxelMap / ParseVoxelRegion──► documents (integers)
 (ValidateVoxelChunk while editing;                                           │
  reads what it wrote back with the parsers)                ExpandVoxelChunk, ExpandVoxelPalette
                                                                              │
                      VoxelChunkGeometry per chunk (vertices, indices, boxes) + one VoxelPaletteImage
```

`ExpandVoxelChunk` cannot fail; it takes only chunks that `ParseVoxelRegion` returned or `ValidateVoxelChunk`
accepted with the same map, and anything else is a programmer error that it asserts on. Vertex order, winding and uv
are those of version 1. `RegionOf(key)`, `ChunkWorldOrigin(key, map)` and `MapUnitsToWorld(units, map)` (integer
units from the map's origin, such as `gyo.map_markers` positions) use the same conversion. Parsing regions on several
threads is the consumer's (with `GYO::Threads`, putting results back in key order); the module stays single-threaded.

## The single-file format (version 1, deprecated)

## What it is

The geometry of one map, baked: axis-aligned faces with a colour per face direction, and axis-aligned collision
boxes. Everything is an integer in 1/16 of a cell, so the file has one canonical form and every platform expands it
to the same floats.

```json
{
  "schema": "gyo.voxel_mesh",
  "version": 1,
  "source_sha256": "<64 lowercase hex digits>",
  "build_version": "voxel_map_editor 1",
  "origin": [-1, 0, -1],
  "cell_size": 1.0,
  "palette": [
    { "name": "wall", "faces": [[r, g, b, a], "… six colours, in the order +X -X +Y -Y +Z -Z"] }
  ],
  "quads": [[0, 0, 0, 2, 16, 16, 0]],
  "colliders": [{ "min": [0, 0, 0], "max": [16, 48, 336] }]
}
```

- **Asset.** A UTF-8 JSON file. Its catalog type, and how a product finds it, belong to the product that loads it.
- **Provenance.** `source_sha256` is the SHA-256 of the source the file was built from, and `build_version` names the
  build that made it. The module checks only their form: the source lives with the tool (decision
  v8-tool-voxel-source-location), and Engine never reads it.
- **Units.** `quads` and `colliders` are in units of 1/16 of a cell, relative to `origin`, which is in cells. A world
  coordinate is `float(units + 16 × origin) × cell_size / 16`: the integer sum is exact, its conversion to float is
  exact within ±2^20, the product rounds once and the division by 16 is exact, because `cell_size` stays within
  [2^-16, 2^16] and no result leaves the normal floats. `cell_size` is the decimal's nearest double, then that
  double's nearest float; a producer writes a decimal that reads back as the float it means (the voxel map
  editor writes the shortest decimal of that float's double).
- **Quads.** `[x, y, z, dir, w, h, palette]`. `dir` is the direction the face looks at: 0 to 5 for +X, −X, +Y, −Y, +Z,
  −Z, which is also the face's row in the palette image. `(x, y, z)` is the face's minimum corner, on the face's
  plane; `w` and `h` run along its two in-plane axes: X faces (Z, Y), Y faces (X, Z), Z faces (X, Y).
- **Colours.** Each palette entry has six sRGB RGBA8 colours, one per face direction. How bright each direction is,
  and every other look decision, is the producer's.
- **Colliders.** Boxes with `min < max` on every axis; there may be none.

### Validation (gyo.voxel_mesh version 1)

`ParseVoxelMesh(text)` decides every rule. Any violation is a `VoxelMeshErrorCode::InvalidVoxelMesh` Runtime Error
whose detail is the path of the offending value (for example `quads[3][4]`); a corner computed from a quad or a box
reports the path of that quad or box, and a repeated key or a text that is not JSON reports an empty path. Nothing
is ignored or repaired.

1. The document is one JSON object with exactly the fields `schema`, `version`, `source_sha256`, `build_version`,
   `origin`, `cell_size`, `palette`, `quads` and `colliders`; no key repeats in any object.
2. `schema` is `"gyo.voxel_mesh"`; `version` is the integer 1.
3. `source_sha256` is 64 lowercase hexadecimal digits; `build_version` is 1 to 64 printable ASCII characters.
4. `origin` is three integers; `cell_size`, as a float, is within [2^-16, 2^16].
5. `palette` lists 1 to 256 entries, each with exactly `name` (1 to 64 characters of `a`–`z`, `0`–`9`, `_` and `.`,
   unique in the palette) and `faces` (exactly six colours of exactly four integers from 0 to 255).
6. `quads` lists one or more quads of exactly seven integers: `dir` 0 to 5, `w` and `h` at least 1, `palette` an
   index into the palette.
7. `colliders` is an array of objects with exactly `min` and `max`, three integers each, `min < max` on every axis.
8. Every integer lies within ±2^30 (so the arithmetic cannot overflow), and every corner of a quad or collider,
   after adding `16 × origin`, within ±2^20 units. `origin` alone has no other bound.
9. `quads` are in strictly increasing lexicographic order of their seven integers; `colliders` in strictly increasing
   order of `(min, max)`. So a file has no repeated quad or box, and one geometry has one order of quads and boxes.
10. The text is at most 16 MiB.

A JSON number with a fraction or an exponent is not an integer.

### Using the single file

```text
tool ──writes──► gyo.voxel_mesh text ──ParseVoxelMesh──► VoxelMeshDocument (integers)
 (reads it back with ParseVoxelMesh)                            │
                                                     ExpandVoxelMesh
                                                                │
                      VoxelMeshGeometry: vertices (position, uv), indices, palette image, colliders (Math::Aabb)
                         │                                        │
     a client: Render mesh and texture              a simulation: its collision against the boxes
```

`ExpandVoxelMesh` cannot fail; it takes only what `ParseVoxelMesh` returned, and any other document is a programmer
error that it asserts on. Each quad gives four vertices and six indices, in quad order. Seen from outside, the
corners run as the faces of Render's unit cube (`MakeUnitCube`, `engine/render/src/PrimitiveMesh.cpp`): clockwise,
Engine's front face (`RenderDeviceTypes.hpp`). Every vertex of a quad has the uv of the centre of its texel in the
palette image (one column per palette entry, one row per direction), because Render samples linearly and has no
mipmaps. The module returns its own vertex type and plain pixels: converting them to Render's `MeshView` and
`ImageView` is the consumer's, so the module does not link Render.

## Contracts

- **Versions.** A change of a field's meaning, a new required field or a new rule is a new version; a reader rejects
  a version it does not know. Version 1 has no optional fields.
- **Producers.** Any file that passes validation is valid, whoever wrote it. A producer reads what it wrote back with
  `ParseVoxelMesh` before it keeps it.
- **Cost.** Parsing and expansion happen when content loads; nothing here runs per frame. Parsing scans the text
  once for repeated keys and once to build the document (`engine/base/StrictJson.hpp`), so its time grows with the
  text, not with the number of objects in one array (survey section 5 (c) of the big-map plan: the 5.3 MB synthetic
  map took 1,580 ms before and 382 ms after, median of five, Intel Mac).

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **Neutral.** `GYO::VoxelMesh` knows faces, colours, boxes, chunks and opaque names only: no blocks, shapes,
   orientations, spawn points or product names. Its links are exactly `GYO::Base`, `GYO::Math` and, privately, the JSON parser: no
   render, collision, asset or product library. Guard: the configure-time check `gyo_expect_voxel_mesh_links`
   (`tests/common/voxel_mesh/CMakeLists.txt`; mutant `vm-links-collision`) and review for names.
2. **No build.** The module validates, expands and converts. It does not cull, merge, shade or choose anything the
   producer decided; its entry points are `ParseVoxelMap`, `ParseVoxelRegion`, `ValidateVoxelChunk`,
   `ExpandVoxelChunk`, `ExpandVoxelPalette`, `RegionOf`, `ChunkWorldOrigin`, `MapUnitsToWorld` and the deprecated
   `ParseVoxelMesh` and `ExpandVoxelMesh` (besides naming its error codes). Guard: review.
3. **One validator.** Whether a file or a chunk is valid is decided by this module alone, wherever it came from:
   `ParseVoxelMap` for an index, `ParseVoxelRegion` for a region, `ValidateVoxelChunk` for a chunk that is not text,
   and the last two run one implementation of the chunk rules. Guard: the rule tests in
   `tests/common/voxel_mesh/VoxelMeshTests.cpp` and `VoxelMapTests.cpp` (one test case per rule, named by its rule;
   mutants `vm-*` in `docs/tools/plans/v8/scripts/voxel_mesh_mutations.json`, `vmap-*`, `vreg-*` and `chunk-*`,
   among them `chunk-validate-skips-rules`, in `docs/architecture/plans/big-map/scripts/engine_format_mutations.json`).
4. **Strict.** An unknown schema, version or field, a repeated key, a non-canonical order and a value out of range
   are errors, never ignored, clamped or repaired. Guard: the same rule tests and mutants.
5. **Deterministic.** One document expands to the same float bits on every platform. Guard: the goldens "golden
   vertices, indices and uv", "golden world coordinates", "golden chunk vertices, uv and boxes" and "golden world
   coordinates at both ends of the range", which compare bits, some against literal values, at both ends of the
   ±2^20 range, and "a chunk expands to the same bits as the single-file format"; the `cell_size` range of rule 4
   keeps every result a normal float.
6. **Chunks and regions.** A chunk is the unit of validation and expansion, a region the unit of files; the module
   does not know how a consumer draws, loads or caches them. Guard: review (no entry point takes or returns more
   than one chunk's geometry).
7. **Key order.** Every iteration order is defined by the total order of chunk and region keys: regions in the
   index, chunks in a region, quads and boxes in a chunk. Guard: rules M8, R4 and C5 and their mutants.

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. Collision needs a shape other than an axis-aligned box (a slope).
2. Faces need to leave the axes, or vertices need their own attributes (normals, ambient occlusion).
3. (Met and handled, 2026-11-06.) Loading or collision queries were measured to need the geometry split into chunks
   or a spatial index (big-map survey, section 5): the chunked map is this module's answer, the spatial index is
   `GYO::Collision`'s `StaticAabbGrid` ([collision](collision.md)).
4. A producer has to re-verify these rules on its own each time it gains a feature.
5. Chunks need their own palette: the map's palette needs more than 256 entries, or palette indices are measured to
   be most of a region's text.
6. After the parse replacement, the largest map's load time still exceeds the value its batch declared: a binary
   cache (a second encoding) is then decided with the user.
7. Parsing must happen on several threads inside this module to be fast enough. Until then it is the consumer's
   (one `ParseVoxelRegion` per region, results put back in key order); if that is not enough, stop and let the user
   decide.
