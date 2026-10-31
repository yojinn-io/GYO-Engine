# GYO UI v1 and the standalone UI editor

GYO UI is a closed, data-driven runtime standard, not an app-specific menu
framework. Its dependency direction is fixed:

```text
apps/<game> policy + assets
        |
        v
GYO::Ui --------------------> JSON, layout, bindings, focus, hit-test, actions
GYO::UiRenderer ------------> AssetManager, text rasterizer, RenderQueue Overlay

tools/ui_editor/gyo_ui_editor --> GYO::Ui + optional read-only content snapshot
        X
        +-------------------- no app dependency, catalog writes, or publishing
```

`GYO::Ui` never knows what an action such as `object_fps.start_game` means.
It emits an opaque, declared, typed action. The game adapter maps that id to a
game command, and the game flow remains responsible for legality and effects.
Likewise, the game owns wording, hierarchy, binding values and the consequence
of changing a display setting. GYO owns font assets, text rasterization/cache,
layout and render submission.

Ordinary game builds have testing, package acceptance and design tools disabled. The editor is explicitly selected for local authoring; `engine/config/tools.csv` currently selects its GUI variant for every release platform, even when the game registry is empty. Engine libraries are statically linked into the editor; game archives do not include it.

Tests live under `tests/ui_editor` and common engine validation under `tests/common`. The standalone editor entry forwards to the root build graph. See [project management](architecture.md#build-project-management).

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's
agreement, however small (AGENTS.md section 3). So is changing this section.

### GYO::Ui

1. Ui does not know what an action means. It emits only opaque, typed actions
   the document declares; the game decides their meaning, legality and effects,
   and owns wording, hierarchy and binding values.
2. Ui does not depend on `GYO::Input`. Products convert input into Ui's own
   neutral types (`UiInputFrame`); a future text field follows the same rule.
   Guard: `ui.dependency_guard` (`tests/common/ui/ui_dependency_guard.py`).
3. The element set is closed. Adding or removing an element, or changing the
   layout rule, is a contract change with a new version and documentation.
   Readers reject an unknown element or version explicitly; they never skip it.
4. Ui holds no game state. Which page belongs to a game phase, and every
   binding value, are the product's. Ui holds only state it can rebuild, such
   as focus and the pressed control, and, while a text field is being edited,
   that edit (its text, cursor and selection). A submit only emits the field's
   action with the edited text; whether it is accepted, and the binding value,
   stay the product's. Cancelling, losing focus or a change of the binding
   discards the edit; Ui keeps no copy of an earlier value. Guard: `A text
   field discards its edit when the binding changes or the pointer leaves`.
5. Which characters a text field accepts is the product's. Ui hands committed
   text (typed or pasted) to a check function the product supplies and keeps
   only what it returns. Ui knows no product character rule, and a gyo.ui
   document declares no character set, only limits without product meaning
   such as `max_length`. Guard: `A text field keeps only what the product's
   check function returns`.
6. An input method's composition is shown only. Ui draws it (for example
   underlined) but never puts it into the edit or a submitted value; only
   committed text passes the check of invariant 5 into the edit. Guard: `A
   text field shows a composition but never submits it` (Enter, losing focus
   and cancel during a composition).
7. Text events reach Ui as its own neutral types, converted by the product,
   like `UiInputFrame` (invariant 2); clipboard text is also handed in by the
   product, so Ui never reaches the platform. Guard: `ui.dependency_guard`.

### ui_editor

1. The editor links no game, no `GYO::Input` and no SDL_GPU. Guard:
   `gyo_ui_editor.dependency_guard` (`tests/ui_editor/ui_editor_dependency_guard.py`),
   a positive list of includes and targets. The window, ImGui's SDL backends
   and the system dialogs come through the tools' shared layer, whose own
   guard keeps the same rule ([framework](framework/architecture.md), Design
   Invariant 2).
2. Documents enter and leave only through `UiDocumentBridge` and the `GYO::Ui`
   codec: canonical JSON written by atomic replace, no side files (`.meta`,
   autosave, `imgui.ini`) and no native path in the output. The framework's
   host keeps ImGui's own side files out ([framework](framework/architecture.md),
   Design Invariant 4).
3. A mounted asset catalog is read-only; nothing is written inside its root.
   The mount is the framework's `ReadOnlyAssetCatalog`, which has no write API
   ([framework](framework/architecture.md), Design Invariant 4).
4. Canvas rectangles, clipping, draw order and hit testing come from the
   `GYO::Ui` evaluator. The editor may keep its own copy of the layout rule
   only to solve values backwards (the gizmo's `position` and `size_delta`);
   the evaluator must then place every solved element where the gizmo
   intended. Guard: `TestGizmoAgreesWithUiEvaluator` in `gyo_ui_editor.core`.
5. The preview records actions and never executes them.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to
repair or rebuild the component.

- ui_editor: after `v8-tool-ui-editor-element-schema`, adding an element still
  needs edits in more than one panel or file; the preview needs layout or hit
  testing that the `GYO::Ui` evaluator does not provide; or a required
  interaction (typing on the canvas, input-method composition) is beyond ImGui.
- GYO::Ui: a product needs to branch on an element's internal state, or needs
  scripts to express a menu; or text-editing policy placed in Ui starts to
  need product-specific rules; or a product needs editing beyond the common
  set (Backspace, Delete, Left/Right, Home/End, select all, paste, Enter to
  submit, Escape to cancel), such as its own shortcuts, several lines, rich
  text or completion. Ui does not grow to cover these; the user decides
  whether text editing is redesigned.

## JSON v1

A document has `schema: "gyo.ui"`, `version: 1`, one reference canvas, font
aliases, named sRGB colors, typed actions and bindings with preview values, and
an ordered set of canvases. Runtime colors use linear RGB with straight alpha;
JSON colors use `#RRGGBBAA`. Loading performs the exact sRGB-to-linear transfer
on RGB while leaving alpha unchanged.

The v1 element set is deliberately closed:

- `container`, `panel`, `image`, `text`, `button`
- `horizontal_slider`, `fixed_step_list`

Numbers must be finite. Geometry is held as 32-bit floats, so a
`fixed_step_list` `item_step` must also be finite after conversion to float: a
finite JSON number beyond the float range (for example `1e39`) is rejected with
the error located at that element's `item_step`, as are values made non-finite
in memory before saving. Rejecting such out-of-range steps does not change the
schema version.

Every element uses `anchor_min`, `anchor_max`, `pivot`, `position`, and
`size_delta`. For a parent rectangle, layout is evaluated as:

```text
size      = parentSize * (anchorMax - anchorMin) + sizeDelta
anchorRef = parentMin + parentSize * lerp(anchorMin, anchorMax, pivot)
rectMin   = anchorRef + position - pivot * size
```

Actions carry only `none` or `number`. Bindings are `string`, `integer`,
`number`, `boolean`, `enum`, or `list<object>` with a declared non-nested
record shape. Text is limited to literal/value, typed named-placeholder
compose, boolean/enum select and fixed-list item fields. V1 has
no scripts, custom widgets, plugin ABI, Flex/Grid, animation, rotation, scroll,
rich text, shaping or localization framework.

## JSON v2

Version 2 (v8-tool-ui-text-field) adds one element and one payload type; every
version 1 document reads as before and is written back as version 1. A reader
accepts versions 1 and 2 and rejects any other explicitly. `text_field` and the
`string` payload in a version 1 document are version errors.

- Action payload `string`: the action carries text (a text field's submit).
- `text_field`: `binding` (a `string` binding: the value shown and edited),
  `action` (`string` payload: emitted on submit with the edited text),
  `max_length` (code points, at least 1), optional `placeholder` with its
  `placeholder_color` (shown while the value is empty and the field is not
  being edited), `background` (`normal`, `focused`, and `pressed` while
  editing), and the text style keys (`font`, `point_size`, `text_color`,
  `horizontal_align`, `vertical_align`, centered vertically by default). It is
  interactive and belongs in `focus_order`; it is forbidden in list templates.
  One line, no scrolling: the text is clipped to the field, so size the field
  for `max_length`.

Editing, by `UiRuntime` (Design Invariants 4-7):

- Activate on the focused field, or a press on it, starts editing a copy of the
  binding value with the cursor at its end. `UiInteractionState::editingElement`
  names the field while it is edited; the product turns the platform's text
  input on for it.
- `UiInputFrame::text` carries the edit, converted by the product:
  `Insert` (committed or pasted text), `Composition` (shown at the cursor,
  never edited), `Backspace`, `Delete`, `MoveLeft`, `MoveRight`, `MoveHome`,
  `MoveEnd`, `SelectAll`. Inserted text passes the product's
  `UiRuntime::SetTextFilter` (all of it without one), then becomes one line of
  whole code points and is cut to `max_length`.
- Activate submits: the field's action with the edited text as its payload.
  Cancel discards the edit (it is not the canvas cancel action then). Moving
  the focus, a press elsewhere, or a change of the binding value discards it
  too; moving the pointer does not.
- The renderer draws the selection, the composition's underline and the caret
  at the measured width of the text before them (`UiTextDraw::caret`).

## Scene display adjustment

Render colors, tints and clear colors are linear RGB with straight alpha.
Meshes and `CompositeLayer::Scene` sprites render to a private
`R16G16B16A16_FLOAT` target. The post pass applies exposure and the relative
gamma adjustment before an `SDR_LINEAR` sRGB swapchain performs the standard
linear-to-sRGB conversion. `CompositeLayer::Overlay` is submitted afterwards,
so HUD, menu text and fades are not altered by player display settings.

Object_FPS stores Exposure and Gamma in its app runtime client. They persist
across pause, main-menu and new-campaign transitions in one process, but are
not part of gameplay snapshots and are not serialized.

## Editor workflow

Select the editor independently of the CSV-selected games:

```sh
cmake --preset dev -DGYO_TOOLS=ui_editor -DGYO_APPS=
cmake --build --preset dev --target gyo_ui_editor
```

The assembled executable is under `build/target/toolchain/bin`. The optional
`cmake -S tools/ui_editor -B build/target/_build/ui-editor` entry forwards to the
same repository graph. `-DGYO_TOOLS=ui_editor:cli` provides local command-line
validation only, under `build/target/_tools/ui_editor/bin`; it is not packageable.
The GUI default and both variants' component requirements live in the tool's
`project.json`, not central CMake conditions. See [registering tools](tool_projects.md).

The standalone target is `gyo_ui_editor` and does not link ObjectFPS, Input,
or SDL_GPU. ImGui supplies editor chrome. Canvas rectangles, clipping,
ordering and hit testing come from the shared GYO UI evaluator.

```text
gyo_ui_editor [--open <working.json>] [--output <export.json>]
              [--asset-catalog <catalog.json>] [--asset-root <dir>]
gyo_ui_editor --validate <json>
              [--asset-catalog <catalog.json>] [--asset-root <dir>]
```

`--asset-root <dir>` loads that root's `content.json` and every catalog it
declares. Missing or invalid manifests fail the mount; the editor does not
guess `asset_catalog.json`. The merged catalog uses the same engine loader as
the game, including duplicate-ID and path validation. It is parsed once per
mount: the asset list, document validation and preview consume the same
snapshot. A failed replacement preserves the previous complete mount.

Explicit `--asset-catalog <file>` selects a single catalog as a partial view.
Its root defaults to the catalog's parent; an explicit `--asset-root` overrides
that directory. This mode does not load or merge `content.json`, even when one
exists. The GUI's **Mount Read-Only Content** dialog offers the same root and
single-catalog modes. **Open Asset Folder...** mounts a complete root chosen in
the operating system's folder dialog (SDL3); Open, Export and the mount fields
also use the system dialogs. The dialog only returns a path: the editor reads
and writes through the same engine file layer, and remembers no path between
runs. Where the platform cannot show a dialog (for example Linux without a
desktop portal or zenity), the editor falls back to typing the path. Use a complete root when a document references assets
from multiple catalogs.

The editor writes one canonical UTF-8 JSON file (two-space indentation, LF,
one trailing newline) using an atomic replace. It does not write `.meta`, a
project file, autosave, preview cache, or `imgui.ini`. A mounted app catalog is
read-only and pickers save only `AssetId`; native paths are never serialized.
Without a catalog, unresolved asset ids are warnings. With a catalog, missing
or wrong-typed assets block export. Changes to catalogs or preview assets on
disk take effect after an explicit remount; there is no implicit reload.

Preview textures remain alive for the complete ImGui frame. Mount and unmount
requests are applied before the next frame starts; text-cache eviction also
runs at that boundary, after the previous frame has been presented. A frame
with more than 256 unique text runs may temporarily exceed the cache budget.
Cancelling a frame discards its unsubmitted draw commands before retiring
preview resources. Windowless SDL software-renderer tests cover this lifecycle
with the real ImGui renderer backend and a deterministic text rasterizer.

Export into the mounted app asset root is rejected. The release workflow is
intentionally explicit:

```text
Editor export to an external working location
        -> author manually copies JSON into apps' asset root
        -> author manually adds the app catalog entry
        -> local/CI build automatically assembles content.json into the product
```

Future tools remain separate executables. A map editor may use a different
SDL_GPU viewport, but it must not depend on the UI editor or turn it into a
generic plugin host.
