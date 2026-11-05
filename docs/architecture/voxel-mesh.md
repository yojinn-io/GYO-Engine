# Voxel mesh (gyo.voxel_mesh)

Owner: Engine (`engine/voxel_mesh` → `GYO::VoxelMesh`; `engine/voxel_mesh/VoxelMesh.hpp`).
Plan and record: [voxel map editor](../tools/plans/v8/voxel-map-editor.md), batch v8-engine-voxel-mesh-contract
(decisions v8-tool-voxel-offline-build, v8-tool-voxel-layers, v8-tool-voxel-units and
v8-tool-voxel-artifact-encoding: a tool bakes a grid-built map offline; the game reads only the baked file, whose
format and only validator belong to Engine).

**Status (2026-11-05): version 1 landed with v8-engine-voxel-mesh-contract.** The voxel map editor
(`v8-tool-voxel-map-editor-core`) writes these files; object_fps_pvp loads them as the geometry of its arena files
(`v8-product-arena-v2`).

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

### Validation (version 1)

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

### Using it

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
- **Cost.** Parsing and expansion happen when content loads; nothing here runs per frame.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section.

1. **Neutral.** `GYO::VoxelMesh` knows faces, colours, boxes and opaque names only: no blocks, shapes, orientations,
   spawn points or product names. Its links are exactly `GYO::Base`, `GYO::Math` and, privately, the JSON parser: no
   render, collision, asset or product library. Guard: the configure-time check `gyo_expect_voxel_mesh_links`
   (`tests/common/voxel_mesh/CMakeLists.txt`; mutant `vm-links-collision`) and review for names.
2. **No build.** The module validates, expands and converts. It does not cull, merge, shade or choose anything the
   producer decided; its entry points are `ParseVoxelMesh` and `ExpandVoxelMesh` (besides naming its error codes).
   Guard: review.
3. **One validator.** Whether a file is valid is decided by `ParseVoxelMesh` alone, wherever the file came from.
   Guard: the rule tests in `tests/common/voxel_mesh/VoxelMeshTests.cpp` (one test case per rule, mutants `vm-*` in
   `docs/tools/plans/v8/scripts/voxel_mesh_mutations.json`).
4. **Strict.** An unknown schema, version or field, a repeated key, a non-canonical order and a value out of range
   are errors, never ignored, clamped or repaired. Guard: the same rule tests and mutants.
5. **Deterministic.** One document expands to the same float bits on every platform. Guard: the goldens "golden
   vertices, indices and uv" and "golden world coordinates", which compare bits, some against literal values, at
   both ends of the ±2^20 range; the `cell_size` range of rule 4 keeps every result a normal float.

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. Collision needs a shape other than an axis-aligned box (a slope).
2. Faces need to leave the axes, or vertices need their own attributes (normals, ambient occlusion).
3. Loading or collision queries are measured to need the geometry split into chunks or a spatial index.
4. A producer has to re-verify these rules on its own each time it gains a feature.
