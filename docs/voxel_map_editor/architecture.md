# GYO Voxel Map Editor

A design tool that edits a map as a grid of shaped, coloured blocks and bakes it into Engine's formats
([contract](../architecture/voxel-mesh.md)), the only files a game reads: the chunked map (`gyo.voxel_map` with
`gyo.voxel_region`) and, until v8-arch-big-map-product-arena-v3 retires it, the single file `gyo.voxel_mesh`. Plan, batches and judgments:
[voxel-map-editor.md](../tools/plans/v8/voxel-map-editor.md); record:
[voxel-map-editor-HANDOFF.md](../tools/plans/v8/voxel-map-editor-HANDOFF.md).

Owner: `voxel_map_editor` (`tools/voxel_map_editor/`).

**Status (2026-11-06): `v8-arch-big-map-tool-chunked` landed** ([declaration](../tools/plans/v8/voxel-map-tool-chunked.md)):
the chunked bake, its commands and markers, on top of the core library and the command line. The window
(`v8-tool-voxel-map-editor-window`, the `gui` variant, the default; the `cli` variant has no window) is being
declared again on top of it, with Goxel's functions and interaction (`v8-tool-voxel-window-goxel`); until then it
previews and saves the single file.

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

## The chunked map

The build of `gyo.voxel_map` and its regions, one chunk at a time (`BakeMap`, `RebakeChunks`; header
`ChunkedMap.hpp`). The grid's chunks (16³ cells from origin) are Engine's chunks; the map's origin and cell size
are the grid's.

1. Per chunk, the blocks' boxes and their faces as in steps 1 and 2 of the single file, in units from the chunk's
   minimum corner (0 to 256). A face on the chunk's side reads the neighbouring cell across it, so culling does not
   depend on where chunks end.
2. Greedy merge, inside the chunk only: the faces left are grouped by direction, plane and palette entry, the
   group's edges are the only coordinates, and a fixed scan (rows along v, then along u) grows each rectangle along
   u as far as it can, then along v while the whole row is free. The union of the quads is the union of the faces,
   and no two quads overlap. No merge crosses a chunk's side.
3. Collision: `MergeBoxes` (step 4 of the single file) on each chunk's boxes. A cell's boxes never leave its cell,
   so no box crosses a chunk's side.
4. Palette: every block of the grid, in grid order (the single file lists only the blocks in use, by name). So only
   a palette edit changes every chunk; `RebakeChunks` then bakes everything.
5. Every chunk is checked with Engine's `ValidateVoxelChunk`; the index is read back with `ParseVoxelMap`.
6. Rebake: `ChunksTouchedBy` gives the chunks a change of some cells can change (each cell's chunk, and the chunk
   across any side the cell lies on); `RebakeChunks` bakes only those and rebuilds the index when the bounds or the
   regions change. The result is the same as baking everything (Design Invariant 3).
7. Output (`WriteMap`): `map.json` and one file per region, `regions/r_<x>_<z>_<h>.json` (`h`, the first 16
   hexadecimal digits of the region text's SHA-256), in Engine's key order; `build_version` is
   `voxel_map_editor 2`. Every text is read back with `ParseVoxelMap` and `ParseVoxelRegion` and compared with the
   baked map before it is returned.
8. Saving (`SaveMap`): region files first (a file already holding the same bytes is left alone), then `map.json` by
   atomic replace, then the region files the previous `map.json` listed and the new one does not (names of the
   region file form under `regions/` only). A save that stops before `map.json` leaves the previous map readable.
   engine/io cannot list a directory, so a region file that no index lists (left by such a stopped save) stays.

A grid without filled cells has no chunked map (`EmptyMap`, the tool's own check before Engine's rule M8). Only
cell edits are rebaked chunk by chunk: when the palette, the shape library, a block's shape, the origin or the cell
size changed, `RebakeChunks` bakes everything.

Range: a grid accepts a filled cell within [-2^16, 2^16 - 1] after adding origin, but a chunk covers whole 16-cell
steps from origin, so when origin's x or z is not a multiple of 16 a chunk at the edge of that range ends past
Engine's ±2^20 units (rule M6) and `build-map` refuses the grid (`BuildRefused`), while `build` still bakes it.

## Markers

The editor places and names markers of a map (`gyo.map_markers`) and offers the kinds a product lists
(`gyo.marker_kinds`), both Engine formats ([map markers](../architecture/map-markers.md)); it interprets no kind
and no tag. Header `MarkersFile.hpp`:

- `ReadMarkers` and `ReadMarkerKinds` are Engine's `ParseMapMarkers` and `ParseMarkerKinds` (`InvalidMarkers`
  with Engine's message and path).
- `WriteMarkers` writes one text (markers and tags in the document's order, yaw as the shortest decimal of the
  float's double), reads it back with `ParseMapMarkers` and compares it with the document; what Engine refuses
  or reads back different is not returned.
- Which markers have a kind the list does not hold is Engine's `UnknownMarkerKinds`; the editor keeps no copy of
  that check.

Where the kinds list comes from is the command line's (`--marker-kinds`): the tool names no product. Placing
markers in the window is the window batch's.

## Command line (`gyo_voxel_map_editor`)

| Command | Does | Exit status |
|---|---|---|
| `build <grid> <mesh>` | bakes and writes the mesh in place (atomic replace) | 0; 1 an input the formats refuse; 3 a file could not be read or written |
| `verify <grid> <mesh>` | bakes again and compares the bytes | 0 the same; 4 different; 1 or 3 as above |
| `inspect <file>` | summarizes a grid or a baked mesh, chosen by the file's `schema` field | 0; 1; 3 |
| `import-boxes <json> <pointer> <grid>` | a grid of cubes from boxes in whole cells at a JSON pointer | 0; 1; 3 |
| `build-map <grid> <map-dir>` | bakes the chunked map and saves it in the folder (`SaveMap`) | 0; 1; 3 |
| `verify-map <grid> <map-dir>` | bakes again and compares `map.json` and every region file it lists | 0 the same; 4 different; 1 or 3 |
| `inspect-map <map-dir>` | summarizes a chunked map, read with Engine's parsers | 0; 1; 3 |
| `check-markers <markers> --marker-kinds <kinds>` | reads both files with Engine's parsers and lists every marker whose kind is not in the list (`UnknownKinds`) | 0; 1; 3 |
| `measure-map <out-dir> [<grid>...]` | the declared measurement of `v8-arch-big-map-tool-chunked`: writes a synthetic building-like grid and a per-chunk CSV, prints a summary per map | 0; 1; 3 |

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
     `gyo_voxel_map_editor`) uses Engine (`GYO::VoxelMesh`, `GYO::MapMarkers`, engine/io, `GYO::Base`, `GYO::Math`), the framework's
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
   (`tests/voxel_map_editor/voxel_map_editor_dependency_guard.py`, a positive list per part) and its self-test. (`v8-tool-voxel-map-editor-window`, user, 2026-11-06; `GYO::MapMarkers`: user, 2026-11-06, `v8-tool-chunked-declare`.)
2. **One validator.** Whether baked output is valid is decided by Engine alone: `ValidateVoxelChunk` for every chunk
   the editor bakes, `ParseVoxelMap` and `ParseVoxelRegion` for the map text it writes, `ParseVoxelMesh` for the
   deprecated single file, `ParseMapMarkers` for markers, `ParseMarkerKinds` for a kinds list and
   `UnknownMarkerKinds` for whether a marker's kind is listed. The editor keeps no copy of these rules: it reads every text it writes back, compares
   what it read with what it baked, and returns nothing Engine refuses. Guard: "build: what Engine refuses is not
   returned", "map: a chunk Engine refuses is not kept", "map: an index Engine refuses is not returned", "map: a
   region Engine refuses is not returned", "markers: a document Engine refuses is not written", the command-line
   tests `build_refused`, `build_map_refused` and `check_markers_unknown`, and their mutants. (User, 2026-11-06,
   `v8-tool-chunked-declare`.)
3. **Deterministic.** One grid bakes to the same bytes on every platform, in either format; a grid the editor wrote
   reads back and writes again byte for byte; rebaking the chunks a change touched gives the bytes of baking
   everything. Guard: the golden files in `tests/voxel_map_editor/fixtures/` (`sample.voxel_mesh.json`,
   `sample_map/`; kept byte for byte by their `.gitattributes`), the build-twice tests, the round-trip test and
   "map: a rebake of the touched chunks equals a whole bake" (200 edit sequences).
4. **Lossless.** The collision boxes are disjoint and their union is the union of every block's boxes (in the
   chunked map, per chunk); a face is dropped only when a neighbour covers that whole side. Greedy merging keeps the
   covered area: for every direction, plane and palette entry, the union of the merged quads is the union of the
   faces left after culling, and no two quads overlap. Guard: the 200 random merge sets, the cullface test, which
   compares whole quad lists (a cube, two cubes, a cube beside a slab) and a stair's two sides, "map: greedy keeps
   every face square once" and "map: each chunk's boxes are disjoint and fill its blocks" (200 random grids each,
   against the single file's faces), and "map: cullface and greedy across a chunk border".
5. **Writes only what it makes.** The baked file or the grid a command names, by atomic replace through engine/io;
   in a map folder a command names, `map.json` and region files, and it removes only the region files the previous
   `map.json` listed and the new one does not (names of the region file form under `regions/`); `measure-map`, the
   synthetic grid and the CSV in the folder its command line names; the window, the grid and the mesh its command
   line names, never overwriting a file that changed on disk since it was opened or saved; `measure-preview`, the
   CSV its command line names. No catalog, product file or arena. Guard: the command-line test (refused inputs write
   nothing), "map: a save stopped before the index leaves the previous map readable" and the save tests (W5).
6. **The grid is the tool's.** `gyo.voxel_grid` has one reader, this tool; Engine knows no blocks, shapes or
   orientations (`voxel-mesh.md`, Design Invariant 1). Guard: review.
7. **What is shown is what the game reads.** The preview draws the geometry Engine's `ParseVoxelMesh` and
   `ExpandVoxelMesh` give for the text `BuildMesh` returns; the editor makes no triangles of its own from the grid.
   Guard: W6 ("preview: what is drawn is Engine's expansion of the baked text") and its mutant. (User, 2026-11-06.)

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to repair or rebuild.

1. The grid gains a second reader (for example a scene editor): it belongs in Engine then.
2. A shape needs geometry other than boxes (slopes).
3. (Met and handled, 2026-11-06.) Build time or the size of the baked file was measured to need render merging,
   chunked output or incremental builds (big-map survey and explore-p1): the chunked map, greedy merging and
   `RebakeChunks` are this tool's answer (`v8-arch-big-map-tool-chunked`).
4. The editor has to re-verify Engine's rules on its own each time it gains a feature.
5. Rebaking one chunk takes longer than `v8-arch-big-map-tool-chunked` declared (judgment T5: the 95th percentile
   of every chunk's median over a building-like map at most 16 ms), or a change has to rebake more than its
   chunks and the chunks across their sides.
6. Greedy merging leaves visible cracks (T-junctions) that need merging across chunks or a change in Render.
