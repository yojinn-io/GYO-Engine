# GYO Voxel Map Editor

A design tool that edits a map as a grid of shaped, coloured blocks and bakes it into Engine's `gyo.voxel_mesh`
([contract](../architecture/voxel-mesh.md)), the only file a game reads. Plan, batches and judgments:
[voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md); record:
[voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md).

Owner: `voxel_map_editor` (`tools/voxel_map_editor/`).

**Status (2026-11-05): `v8-tool-voxel-map-editor-core`.** The core library and the command line (the `cli`
variant); the window (`-window`) follows.

## The grid: `gyo.voxel_grid` version 1

The tool's own format, read only by this tool (Design Invariant 6). A product never reads it.

```json
{
  "schema": "gyo.voxel_grid",
  "version": 1,
  "origin": [-1, 0, -1],
  "cell_size": 1.0,
  "shapes": [
    { "name": "cube", "boxes": [{ "min": [0, 0, 0], "max": [16, 16, 16] }] },
    { "name": "slab", "boxes": [{ "min": [0, 0, 0], "max": [16, 8, 16] }] },
    { "name": "stair", "boxes": [{ "min": [0, 0, 0], "max": [16, 8, 16] }, { "min": [0, 8, 8], "max": [16, 16, 16] }] }
  ],
  "blocks": [{ "name": "wall", "shape": 0, "color": [51, 79, 99, 255] }],
  "chunks": [{ "at": [0, 0, 0], "runs": [[1, 0, 17], [0, 0, 4079]] }]
}
```

- `origin` is in cells; its y is 0, because the floor is the implicit y = 0 plane of the product's arena.
- `cell_size` is within [2^-16, 2^16], the range of `gyo.voxel_mesh`. The writer gives the shortest decimal of the
  float's double (`0.3` is written `0.30000001192092896`), which reads back as the same float.
- A shape is one or more boxes in 1/16 of a cell, within [0, 16] on every axis. A block is a name, a shape and an
  sRGB colour. Names are 1 to 64 of `a`–`z`, `0`–`9`, `_` and `.`, unique among shapes and among blocks.
- A chunk is 16³ cells at chunk coordinate `at` (cells relative to origin, divided by 16). `runs` cover its 4096
  cells, x fastest, then z, then y, as `[block, orientation, count]`: block 0 is empty (orientation 0), block n is
  `blocks[n - 1]`; no run repeats the one before it. Chunks are in increasing order of `at`; no chunk is all empty.
  Every filled cell, after adding origin, lies within [-2^16, 2^16 - 1] (so every corner of the baked file stays
  within Engine's ±2^20 units); `at` itself only has to stay within ±2^16.
- Strict: an unknown or repeated field, a value out of range and a non-canonical order (of chunks, or a run that
  repeats the one before it) are errors. Whitespace and key order are free.
- One text: what `WriteGrid` writes, `ParseGrid` reads back to the same document, and writes again byte for byte.

## Orientations

24 rotations about the cell's centre (8, 8, 8): `up = (+Y, -Y, +X, -X, +Z, -Z)[o / 4]`, then `o % 4` quarter turns
about that up axis (with up +Y, orientation 1 takes +Z to +X). A rotated box is still an axis-aligned box, so a
turned stair still collides as boxes. The number is stored in grid files, so this table is part of the format
(rows of the matrix that takes a cell-local direction to its turned direction):

| o | up / turn | matrix |
|---|---|---|
| 0 | +Y / 0° | `[1 0 0] [0 1 0] [0 0 1]` |
| 1 | +Y / 90° | `[0 0 1] [0 1 0] [-1 0 0]` |
| 2 | +Y / 180° | `[-1 0 0] [0 1 0] [0 0 -1]` |
| 3 | +Y / 270° | `[0 0 -1] [0 1 0] [1 0 0]` |
| 4 | -Y / 0° | `[1 0 0] [0 -1 0] [0 0 -1]` |
| 5 | -Y / 90° | `[0 0 1] [0 -1 0] [1 0 0]` |
| 6 | -Y / 180° | `[-1 0 0] [0 -1 0] [0 0 1]` |
| 7 | -Y / 270° | `[0 0 -1] [0 -1 0] [-1 0 0]` |
| 8 | +X / 0° | `[0 1 0] [-1 0 0] [0 0 1]` |
| 9 | +X / 90° | `[0 1 0] [0 0 -1] [-1 0 0]` |
| 10 | +X / 180° | `[0 1 0] [1 0 0] [0 0 -1]` |
| 11 | +X / 270° | `[0 1 0] [0 0 1] [1 0 0]` |
| 12 | -X / 0° | `[0 -1 0] [1 0 0] [0 0 1]` |
| 13 | -X / 90° | `[0 -1 0] [0 0 1] [-1 0 0]` |
| 14 | -X / 180° | `[0 -1 0] [-1 0 0] [0 0 -1]` |
| 15 | -X / 270° | `[0 -1 0] [0 0 -1] [1 0 0]` |
| 16 | +Z / 0° | `[1 0 0] [0 0 -1] [0 1 0]` |
| 17 | +Z / 90° | `[0 0 1] [1 0 0] [0 1 0]` |
| 18 | +Z / 180° | `[-1 0 0] [0 0 1] [0 1 0]` |
| 19 | +Z / 270° | `[0 0 -1] [-1 0 0] [0 1 0]` |
| 20 | -Z / 0° | `[1 0 0] [0 0 1] [0 -1 0]` |
| 21 | -Z / 90° | `[0 0 1] [-1 0 0] [0 -1 0]` |
| 22 | -Z / 180° | `[-1 0 0] [0 0 -1] [0 -1 0]` |
| 23 | -Z / 270° | `[0 0 -1] [1 0 0] [0 -1 0]` |


## The build

1. Every block's boxes, turned to its orientation, placed in units from origin.
2. Faces: each box face that lies on the cell's side is dropped when the neighbouring cell covers that whole side
   (Minecraft's cullface); every other face stays. No render merging, and faces between boxes of one cell stay.
3. Colours: one palette entry per block in use, ordered by name; each face direction is the block's colour times a
   brightness of the tool's own (+X/−X 80 %, +Y 100 %, −Y 50 %, +Z/−Z 65 %), rounded to the nearest integer
   (halves up: 150 × 65 % = 97.5 → 98); alpha unchanged.
4. Collision: the union of every box, merged slab by slab along y (greedy along x, then z; a rectangle that goes on
   unchanged into the next slab extends its box upwards). The boxes are disjoint and their union is the input's.
   How many boxes a product accepts is the product's rule, not the tool's.
5. Output: Engine's canonical order, written in one layout; `source_sha256` is the SHA-256 of the grid file's bytes
   and `build_version` is `voxel_map_editor 1`. The text is read back with Engine's `ParseVoxelMesh` before it is
   returned; a text Engine refuses is never written.

## Command line (`gyo_voxel_map_editor`)

| Command | Does | Exit status |
|---|---|---|
| `build <grid> <mesh>` | bakes and writes the mesh in place (atomic replace) | 0; 1 an input the formats refuse; 3 a file could not be read or written |
| `verify <grid> <mesh>` | bakes again and compares the bytes | 0 the same; 4 different; 1 or 3 as above |
| `inspect <file>` | summarizes a grid or a baked mesh, chosen by the file's `schema` field | 0; 1; 3 |
| `import-boxes <json> <pointer> <grid>` | a grid of cubes from boxes in whole cells at a JSON pointer | 0; 1; 3 |

`import-boxes` takes every item with `min` and `max` (other fields of an item are ignored, overlapping boxes are
fine), places the grid's origin at the lowest x and z, refuses a box below the floor (y < 0) or not in whole cells,
and reads the grid it made back with `ParseGrid` before it writes it.

A wrong command line exits with 2.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section. (User, 2026-11-05, `v8-tool-voxel-map-editor-core` declaration.)

1. **Dependencies.** The editor links no game and names no product. It uses Engine (`GYO::VoxelMesh`, engine/io,
   `GYO::Base`), the framework's headless shell, and its own JSON parser. It includes no SDL or ImGui header and
   names no SDL, ImGui, render, platform or `*Native` target. Guard: the dependency guard
   (`tests/voxel_map_editor/voxel_map_editor_dependency_guard.py`, a positive list) and its self-test.
2. **One validator.** Whether a baked file is valid is decided by Engine's `ParseVoxelMesh` alone. The editor keeps
   no copy of its rules: it reads every text it bakes back through it and returns nothing Engine refuses. Guard:
   "build: what Engine refuses is not returned", the command-line test `build_refused`, and their mutants.
3. **Deterministic.** One grid bakes to the same bytes on every platform; a grid the editor wrote reads back and
   writes again byte for byte. Guard: the golden files in `tests/voxel_map_editor/fixtures/` (kept byte for byte
   by their `.gitattributes`), the build-twice test and the round-trip test.
4. **Lossless.** The collision boxes are disjoint and their union is the union of every block's boxes; a face is
   dropped only when a neighbour covers that whole side. Guard: the 200 random merge sets, and the cullface test,
   which compares whole quad lists (a cube, two cubes, a cube beside a slab) and a stair's two sides.
5. **Writes only what it makes.** The baked file or the grid a command names, by atomic replace through engine/io.
   No catalog, product file or arena. Guard: the command-line test (refused inputs write nothing).
6. **The grid is the tool's.** `gyo.voxel_grid` has one reader, this tool; Engine knows no blocks, shapes or
   orientations (`voxel-mesh.md`, Design Invariant 1). Guard: review.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to repair or rebuild.

1. The grid gains a second reader (for example a scene editor): it belongs in Engine then.
2. A shape needs geometry other than boxes (slopes).
3. Build time or the size of the baked file is measured to need render merging, chunked output or incremental
   builds.
4. The editor has to re-verify Engine's rules on its own each time it gains a feature.
