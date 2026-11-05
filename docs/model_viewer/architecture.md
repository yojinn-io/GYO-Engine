# GYO Model Viewer

A design tool that shows a model of a mounted product (its nodes, meshes, materials and clips) and edits the
moments of its `gyo.anim_events` file ([contract](../architecture/animation-events.md)). It is one tool of the
asset toolchain ([plan](../tools/plans/v8/assets.md); batches and judgments in
[model-viewer.md](../tools/plans/v8/model-viewer.md)): the tool edits the events, the catalog registers them, and
the runtime only loads them.

Owner: `model_viewer` (`tools/model_viewer/`).

**Status (2026-11-04): `v8-tool-model-viewer-event-sounds`.** The core library, the command line and the window with
the 3D preview, playback, event editing and the event sounds; the trial (`-trial`) follows.

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's agreement, however small
(AGENTS.md section 3). So is changing this section. (User, 2026-11-04, `v8-tool-model-viewer-declaration`.)

1. **Dependencies.** The viewer links no game and names no product. It may use any Engine module; what two or more
   products compose in the same way comes through the framework (for `gyo.event_sounds`, the framework's
   `ParseEventSounds`, `BindEventSounds` and `EventSoundOffsetSeconds`). It includes no `SDL3/` header, calls no
   `SDL_` function, names no SDL or `*Native` target and includes no `imgui_internal.h`. Guard: the viewer's
   dependency guard (`tests/model_viewer/model_viewer_dependency_guard.py`), a positive list of include prefixes and
   of targets (every `Name::Name`, and every library a `target_link_libraries` call names, so a game's plain target
   name fails too), and its self-test.
2. **One validator.** Whether a file is valid is decided by Engine (`ParseAnimationEvents` and
   `BindAnimationEvents`) and, for event sounds, the framework (`ParseEventSounds` and `BindEventSounds`) alone. The
   viewer keeps no copy of their rules: before every write it reads the text it is about to write back through them,
   bound to the opened model, and writes nothing when any step fails. Guard: the core tests ("Engine decides what is
   refused", "saving writes only the opened file") and their mutants.
3. **Lossless.** An opened file saved without an edit reads back as the same events, every seconds value bit for
   bit (the shortest decimal that reads back as the same double). An edit changes only the event it edits. The
   layout may change (clips in name order, the writer's indentation). Guard: the round-trip tests.
4. **Writes only what it edits.** The opened events file (and, after `-event-sounds`, the opened event sounds
   file), by atomic replace through `engine/io`; a file that changed since it was opened is not overwritten. A new
   events file is written only where no file is, and registered in the one catalog the user names, with type
   `text`, through the framework's `RegisterAsset` (`v8-tool-model-viewer-new-file`); a product's own definitions
   that refer to it are the user's to edit. Model files, textures, product definitions and every other catalog
   entry are left as they are. Guard: the save and create tests and their mutants.
5. **The preview is Engine's.** A pose is `SamplePose` at the clip's own seconds; skinning and drawing are Engine's
   `ModelRenderer` and `Renderer` (`v8-tool-model-viewer-render`, (B′)). The viewer has no sampler, interpolation
   or shading of its own. A playback speed only maps wall time to clip seconds and never enters a file. When an
   event sound starts is `EventSoundOffsetSeconds` only. Guard: the pose-equality test of `-preview` and its mutants.
6. **Neutral.** An event name is an opaque string to the viewer: it does not know what an event means and does not
   branch on a product. Guard: review, and the dependency guard: no file of the viewer names an id registered in
   `engine/config/tools.csv` or `engine/config/projects.csv` other than its own.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to repair or rebuild the viewer.

1. The preview is good enough only when the viewer draws itself (its own projection, sorting or shading), or
   Engine's offscreen drawing with a synchronous readback and upload misses the frame-time judgment (P1 in
   [model-viewer.md](../tools/plans/v8/model-viewer.md)). Propose the next step (an asynchronous readback, a smaller
   preview, moving the ImGui binding to SDL_GPU); do not patch it in the viewer.
2. Editing reaches beyond these two formats (hit volumes, animation sets, material bindings), turning the viewer
   into a general model editor.
3. A preview of another model's clips is needed (retargeting, blending): `gyo.anim_events` Replacement Criterion 2.
4. The viewer has to rewrite a format rule of its own, or re-verify the formats' rules each time it gains a feature.

## Command line

`gyo_model_viewer` (variant `cli`) reads a mounted content root (`--asset-root`, with `content.json`) or one
catalog (`--asset-catalog`, its entries under `--asset-root`). It writes nothing.

- `models`: the model assets, id and path.
- `info --model <id>`: Engine's view of the model (`DescribeModel`: node tree, meshes, materials, clips) and the
  text assets that are its `gyo.anim_events` files, each parsed and bound to the model by Engine. A text that mentions
  the schema but Engine refuses (to parse, or to bind to this model) is listed with Engine's reason; whether a file is
  valid is still Engine's.
- `roundtrip --events <id>`: opens the events file against the model it names and writes its text in memory as a
  save would (`CheckedText`), reporting whether the bytes are the file's own. The judgment P2 runs it on a
  product's data.

Exit status: 0 done, 2 usage, 3 a file could not be read, 4 content that cannot be mounted or that Engine or the
catalog refuses.

## Window

`gyo_model_viewer window [--asset-root <dir> [--asset-catalog <catalog.json>]] [--gpu-driver <driver>]` (variant `gui`,
the default) runs on the framework's ImGui shell: the content (from the command line or the OS folder dialog), the
model list, Engine's description of the model (`DescribeModel`), clip playback and the 3D preview.

- `ModelSession`: the opened model, its clip and its `PlaybackClock`, or the reason the last open failed; a failed open
  shows nothing of the model before it.
- `PlaybackClock` and `PreviewPose`: wall time becomes clip seconds through the speed; the pose is `SamplePose` at
  those seconds, or `MakeDefaultPose` for a model without clips (Design Invariant 5).
- `PreviewRenderer` (`gyo_model_viewer_preview`): Engine's `ModelRenderer` submitting into a frame of the framework's
  offscreen preview (`OffscreenPreview`: the offscreen GPU device, Engine's `Renderer` and the readback,
  [contract](../framework/preview.md), `v8-framework-preview-component`), at the size the panel wants; every
  material's base colour is its tint (no textures: which texture a material gets is the product's); the bones as thin
  boxes on the `WorldOverlay` layer. The framework's `OrbitCamera` and the viewer's `FrameModel` place the camera;
  drawing is Engine's.
- The framework's `UploadFrame`: the frame into a texture of the shell's SDL renderer, shown with the framework's
  `ImGuiTextureId`; it runs in `Draw` before `ImGui::Image`, so the texture ImGui draws is the one just uploaded.
- Engine's builtin shaders are copied next to the executable (`shaders/builtin`) on every build.
- Events: one of the model's `gyo.anim_events` files (`OpenEvents`) or a new one (`CreateEvents`, registered in the
  catalog the mount reads), edited through `EventHistory`: each edit one undo step, the state the file holds tracked
  for "unsaved". A dragged marker stops at the clip's ends (`DragSeconds`); a typed moment is not clamped, and Engine
  refuses it on save. A marker lights when playback passes it (`CrossedEvents`, Engine's `PlaybackInterval::Crossed`).
  Changing the model, the content, closing the file or closing the window (the shell's `CloseRequested`,
  `v8-framework-close-request`) asks before unsaved edits go; a field still being typed in counts as an edit.

- Event sounds: the `gyo.event_sounds` files of the opened events file (`EventSoundsFor`, `OpenEventSounds`), or a new
  one (`CreateEventSounds`); every sound's sound id, sync point and gain edited and saved (`SaveEventSounds`), always
  through the framework's `ParseEventSounds` and `BindEventSounds` (`CheckedEventSoundsText`, Design Invariant 2), and
  only while the events file is saved (the sounds are checked against it). The preview plays them on Engine's mixer
  and SDL audio output (`PreviewSounds`), at the times `ScheduleSounds` gives (the framework's
  `EventSoundOffsetSeconds` only) and `SoundPlayback` sends round by round.

The guard's positive list has the window's targets since `v8-tool-model-viewer-preview`
([judgment change](../tools/plans/v8/model-viewer-guard-judgment-change.md), user, 2026-11-04).

## Core library

`gyo_model_viewer_core` (`tools/model_viewer/include/gyo/model_viewer/`):

- `ModelContent`: the framework's read-only catalog mount; a model through Engine's `UfbxModelLoader` from the
  file's bytes (read through `engine/io`, at most 256 MB, the loader's own limit); a text asset; the events files of
  a model.
- `EventDocument`: the events being edited. `Add`, `Move`, `Rename` and `Remove` keep a clip's events in
  non-decreasing seconds (a moved or added event goes after the events at the same moment) and drop a clip that
  loses its last event. They check nothing else: `CheckedText` does, through Engine.
- `WriteAnimationEvents` and `CheckedText`: the text of version 1 in the layout a hand-written file uses, and that
  text once Engine reads it back as the same events, bound to the model.
- `OpenEvents`, `SaveEvents`, `CreateEvents`: a file with its stamp (`engine/io`'s `FileStamp`), saved only while
  the file is still that stamp, and the stamp after a save checked against the bytes written; a new file only for
  the opened model, written first and the catalog last, so a failed catalog write leaves an unregistered file that
  the error names. The probe and the write are two steps (a change between them is seen by the next save), and the
  catalog is not stamped: the user names a catalog nothing else edits meanwhile.
