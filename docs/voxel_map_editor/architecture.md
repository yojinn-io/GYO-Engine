# GYO Voxel Map Editor

A design tool that edits a map as a grid of shaped, coloured blocks and bakes it into Engine's `gyo.voxel_mesh`
([contract](../architecture/voxel-mesh.md)), the only file a game reads. Plan, batches and judgments:
[voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md); record:
[voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md).

Owner: `voxel_map_editor` (`tools/voxel_map_editor/`).

**Status (2026-11-06): `v8-tool-voxel-map-editor-window`.** The core library, the command line and the window
(the `gui` variant, the default; the `cli` variant has no window). The preview's frame time on large maps is with the
architecture session (`v8-tool-voxel-window-p1-to-architect`).

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
4. Collision: the union of every box, merged slab by slab along y. Inside a slab every column x is cut into its
   longest runs along z (strips), and a strip goes on along x only while the next column has exactly the same strip;
   a rectangle that goes on unchanged into the next slab extends its box upwards. So a wall along z keeps its
   corners and a wall along x stops at them (as hand-written levels of axis walls list them;
   v8-product-arena-v2-merge). The boxes are disjoint and their union is the input's.
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

| `edit <grid> <mesh> [--gpu-driver <driver>]` | opens the window (gui variant): a new grid when `<grid>` does not exist; every save writes `<grid>`, then `<mesh>` | 0 when the window closes; 2 in the cli variant |
| `measure-preview (--grid <grid> \| --synthetic) --csv <file> [--frames <n>] [--gpu-driver <driver>]` | the preview's frame time with one cell edit per frame (P1 of the trial batch) | 0; 3; 4 a frame failed; 2 in the cli variant |

A wrong command line exits with 2.

## The window

- **Editing model** (`Editing.hpp`, in the core, so the CPU tests cover it): place (beside the face the pointer is
  on, or on the edit plane), erase, paint (another block, the orientation kept), pick a cell's block and orientation,
  fill or clear a box drawn on the edit plane with a height; the edit plane is one cell layer y, moved with
  PgUp/PgDn. Undo and redo: a drag is one step; a block or shape edit is one step. A block or shape in use cannot be
  removed; every block or shape edit must read back through `ParseGrid`.
- **Picking**: the ray through the pointer's pixel, in Engine's camera convention (`ViewProjection` composes the
  camera as Engine's `Renderer` does), walked through the cells (DDA); a cell of any shape counts as whole. With no
  cell hit, the ray meets the edit plane.
- **Saving**: the grid text, then its build (Engine reads it back); only when both succeed, the grid and then the
  mesh by atomic replace. A file that changed on disk since it was opened or saved is not overwritten
  (`FileChanged`); a mesh that could not be written after the grid was is reported, and `verify` finds the pair
  different.
- **Preview**: after every change the whole grid is built and Engine's `ParseVoxelMesh` and `ExpandVoxelMesh` give
  what is drawn, on the framework's offscreen preview ([preview.md](../framework/preview.md)), with Engine's unlit
  shader and the palette texture, as the game draws it. The pointer's cell and the edit plane's grid are lines of
  ImGui's draw list, projected by the same `ViewProjection`.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section. (User, 2026-11-05, `v8-tool-voxel-map-editor-core` declaration.)

1. **Dependencies.** The editor links no game and names no product. It has two parts.
   - The **core** (every file outside `src/window/`; the libraries `gyo_voxel_map_editor_core` and
     `gyo_voxel_map_editor`) uses Engine (`GYO::VoxelMesh`, engine/io, `GYO::Base`, `GYO::Math`), the framework's
     headless shell and its own JSON parser. It includes no SDL or ImGui header (nor the framework's ImGui binding),
     no `engine/render/` or `engine/platform/` header and none of the framework's preview or window-shell headers,
     and links no SDL, ImGui, render, platform or `*Native` target; of the window it includes only the entry header
     `window/EditorWindow.hpp`, and only the executable links the window's library (gui variant), which brings the
     window's targets with it.
   - The **window** (`src/window/`, the library `gyo_voxel_map_editor_window`, gui variant only) may also use the
     framework's ImGui shell and offscreen preview (`GYO::ToolFrameworkImGui`, `GYO::ToolFrameworkPreview`), Engine's
     `GYO::Render`, `GYO::RenderBackendSDL`, `GYO::PlatformSDL`, and `imgui.h` (`imgui::core`). It still includes no
     `SDL3/` header, uses no `SDL_` name, no `*Native` header or target, no `imgui_internal.h` or ImGui backend; a
     window-only target is named only in the window library's links and the gui variant's existence check.
   Links are made only by `target_link_libraries` with literal names, and includes are plain forward-slash paths
   without `.` or `..` steps, so the lists cannot be gone round. Guard: the dependency guard
   (`tests/voxel_map_editor/voxel_map_editor_dependency_guard.py`, a positive list per part) and its self-test. (`v8-tool-voxel-map-editor-window`, user, 2026-11-06.)
2. **One validator.** Whether a baked file is valid is decided by Engine's `ParseVoxelMesh` alone. The editor keeps
   no copy of its rules: it reads every text it bakes back through it and returns nothing Engine refuses. Guard:
   "build: what Engine refuses is not returned", the command-line test `build_refused`, and their mutants.
3. **Deterministic.** One grid bakes to the same bytes on every platform; a grid the editor wrote reads back and
   writes again byte for byte. Guard: the golden files in `tests/voxel_map_editor/fixtures/` (kept byte for byte
   by their `.gitattributes`), the build-twice test and the round-trip test.
4. **Lossless.** The collision boxes are disjoint and their union is the union of every block's boxes; a face is
   dropped only when a neighbour covers that whole side. Guard: the 200 random merge sets, and the cullface test,
   which compares whole quad lists (a cube, two cubes, a cube beside a slab) and a stair's two sides.
5. **Writes only what it makes.** The baked file or the grid a command names; the window, the grid and the mesh its
   command line names. Each by atomic replace through engine/io, and the window never overwrites a file that changed
   on disk since it was opened or saved; `measure-preview`, the CSV its command line names. No catalog, product file
   or arena. Guard: the command-line test (refused
   inputs write nothing) and the save tests (W5).
6. **The grid is the tool's.** `gyo.voxel_grid` has one reader, this tool; Engine knows no blocks, shapes or
   orientations (`voxel-mesh.md`, Design Invariant 1). Guard: review.
7. **What is shown is what the game reads.** The preview draws the geometry Engine's `ParseVoxelMesh` and
   `ExpandVoxelMesh` give for the text `BuildMesh` returns; the editor makes no triangles of its own from the grid.
   Guard: W6 ("preview: what is drawn is Engine's expansion of the baked text") and its mutant. (User, 2026-11-06.)

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to repair or rebuild.

1. The grid gains a second reader (for example a scene editor): it belongs in Engine then.
2. A shape needs geometry other than boxes (slopes).
3. Build time or the size of the baked file is measured to need render merging, chunked output or incremental
   builds.
4. The editor has to re-verify Engine's rules on its own each time it gains a feature.
