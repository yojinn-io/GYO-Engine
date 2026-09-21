# GYO UI v1 and the standalone UI editor

GYO UI is a closed, data-driven runtime standard, not an app-specific menu
framework. Its dependency direction is fixed:

```text
apps/<game> policy + assets
        |
        v
GYO::Ui --------------------> JSON, layout, bindings, focus, hit-test, actions
GYO::UiRenderer ------------> AssetManager, text rasterizer, RenderQueue Overlay

tools/ui_editor/gyo_ui_editor --> GYO::Ui + optional read-only app AssetCatalog
        X
        +-------------------- no app dependency, catalog writes, or publishing
```

`GYO::Ui` never knows what an action such as `object_fps.start_game` means.
It emits an opaque, declared, typed action. The game adapter maps that id to a
game command, and the game flow remains responsible for legality and effects.
Likewise, the game owns wording, hierarchy, binding values and the consequence
of changing a display setting. GYO owns font assets, text rasterization/cache,
layout and render submission.

Ordinary game builds have testing, package acceptance and design tools disabled. The editor is explicitly selected for local authoring and is a fixed GUI toolchain product in every release platform, even when the game registry is empty. Engine libraries are statically linked into the editor; game archives do not include it.

Tests live under `tests/ui_editor` and common engine validation under `tests/common`. The standalone editor entry forwards to the root build graph. See [project management](architecture.md#build-project-management).

## JSON v1

A document has `schema: "gyo.ui"`, `version: 1`, one reference canvas, font
aliases, named sRGB colors, typed actions and bindings with preview values, and
an ordered set of canvases. Runtime colors use linear RGB with straight alpha;
JSON colors use `#RRGGBBAA`. Loading performs the exact sRGB-to-linear transfer
on RGB while leaving alpha unchanged.

The v1 element set is deliberately closed:

- `container`, `panel`, `image`, `text`, `button`
- `horizontal_slider`, `fixed_step_list`

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
cmake --preset dev -DGYO_BUILD_UI_EDITOR=ON -DGYO_APPS=
cmake --build --preset dev --target gyo_ui_editor
```

The assembled executable is under `build/target/toolchain/bin`. The optional
`cmake -S tools/ui_editor -B build/target/_build/ui-editor` entry forwards to the
same repository graph. `-DGYO_UI_EDITOR_BUILD_GUI=OFF` provides local command-line
validation only; release toolchain archives always build the GUI.

The standalone target is `gyo_ui_editor` and does not link ObjectFPS, Input,
or SDL_GPU. ImGui supplies editor chrome. Canvas rectangles, clipping,
ordering and hit testing come from the shared GYO UI evaluator.

```text
gyo_ui_editor [--open <working.json>] [--output <export.json>]
              [--asset-catalog <catalog.json>] [--asset-root <dir>]
gyo_ui_editor --validate <json>
              [--asset-catalog <catalog.json>] [--asset-root <dir>]
```

The editor writes one canonical UTF-8 JSON file (two-space indentation, LF,
one trailing newline) using an atomic replace. It does not write `.meta`, a
project file, autosave, preview cache, or `imgui.ini`. A mounted app catalog is
read-only and pickers save only `AssetId`; native paths are never serialized.
Without a catalog, unresolved asset ids are warnings. With a catalog, missing
or wrong-typed assets block export.

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
