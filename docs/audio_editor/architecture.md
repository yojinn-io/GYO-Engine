# GYO Audio Editor

A design tool that turns a WAV from anywhere into a clip the engine can play,
writes it into a mounted product's asset root and registers it in that
product's catalog. It is one tool of the asset toolchain
([plan](../tools/plans/v8/assets.md)): the tool processes material, the
catalog registers the result, and the runtime only loads it.

Owner: `audio_editor` (`tools/audio_editor/`).

## Design Invariants

A change against any item below is an Architecture Delta that needs the user's
agreement, however small (AGENTS.md section 3). So is changing this section.

1. The editor links no game and names no other product. It may use any
   Engine module, as an editor depends on the engine; what a game and a tool,
   or two tools, both need comes through the framework first, even where
   Engine has it, and the framework calls Engine. Playback goes through
   `GYO::Audio` and its SDL backend; the editor itself includes no `SDL3/`
   header, calls no `SDL_` function and names no SDL or `*Native` target.
   Guard: the editor's dependency guard, a positive list of targets and
   include prefixes.
2. The output follows the format the engine publishes. The sample rate is taken
   from `GYO::Audio` (`kSampleRate`, `engine/audio/include/engine/audio/AudioFormat.hpp`);
   the editor keeps no copy of that number. The sample encoding is whatever the
   engine's `SoundLoader` and `ClipBank` load. When the engine changes its rate,
   rebuilding the editor follows it; only a new encoding (float, compressed)
   needs an editor change. Guard: every converted output is loaded through
   `SoundLoader` and accepted by `ClipBank`.
3. The editor writes only what it registers: for each exported clip, its output
   file and the catalog entry with that clip's id. One export may write several
   clips; within it the ids are distinct and no two clips are one file, and when
   any clip is refused nothing is written. A write that fails after others
   leaves the clips already written unregistered, and the editor names them. It
   leaves every other entry, every other file and product data (such as a
   product's sound cue table) untouched. The catalog is
   written by atomic replace and checked against the content contract (ids
   unique within the catalog, paths inside the asset root); duplicates across
   catalogs and overlaps with shader bundles are decided at load by Engine's
   `ContentManifest`. This differs from ui_editor, whose mounted catalog is
   read-only.
4. Editing is non-destructive. The source file is only read. Edits live in the
   editor's project file, and the output is always regenerated from the source
   and those settings.
5. The project file lives outside the asset root; saving it inside the root is
   refused, because every file under the root must be registered.
6. What the preview plays is what export writes: the same function and the same
   bytes. The preview volume applies only at playback and never reaches the
   output; the gain is the setting that changes the output.
7. Conversion is reproducible: the same source, the same settings and the same
   platform give the same bytes.
8. The editor is neutral. It knows clips, not what a product means by them: no
   cue names and no branch on a product.

### Replacement Criteria

Meeting one of these stops further repair: propose to the user whether to
repair or rebuild the editor.

- Loops, streaming or compressed formats are needed (also `GYO::Audio`'s own
  Replacement Criteria 1 and 2).
- Several tracks or a mixing timeline are needed, turning the editor into a
  sequencer.
- Registering a sound requires editing a product's own JSON.
- Formats other than WAV must be read, which needs decoders.

## Editing and preview

`gyo_audio_editor edit [<project.json> | <source.wav>]` opens the window
(`v8-tool-audio-editor-edit`); `info`, `convert` and `export <project.json>`
(every region of a project, `v8-tool-audio-editor-regions`) are command-line
only.

- **Settings** (`Edit.hpp`): a trim in the source's own frames (start, and an
  end or "to the source's end"), a gain in dB, from -60 to +24, and a fade in
  and a fade out, also in source frames. A trim that keeps no frame, before or
  after resampling, is refused (`InvalidSettings`), so export never writes a
  clip the engine's `ClipBank` refuses; so are fades longer together than the
  trim.
- **One render** (`RenderClip`): trim, gain (`x * 10^(dB / 20)`), fades,
  resample, quantize. The fades are linear over the source frames: of the n
  trimmed frames, frame i is multiplied by `i / fadeIn` while `i < fadeIn` and
  by `(n - 1 - i) / fadeOut` while `n - 1 - i < fadeOut`, so a fade starts or
  ends in silence. Export and the preview both call it with the same dither seed (the
  id's), so the preview's samples are export's data chunk (Design Invariant 6).
  The default settings give the bytes `convert` gave before.
- **Preview** (`PreviewPlayer.hpp`): plays on a `GYO::Audio` mixer opened through
  `SdlAudioOutput`, one clip at a time, at gain 1; the preview volume is the
  mixer's master gain. The mixer has two clip slots and no schedule delay, so a
  stop and an unload take effect in the block that takes them and a new clip
  that finds no free slot starts after the next block. The position shown is
  the mixer's `PlayheadOf`, for display only (`GYO::Audio` Design Invariant 4).
  Without an audio device the editor still edits and exports; nothing is heard.
- **Waveform** (`Waveform.hpp`): the extremes of each column's samples, drawn
  with the gain applied so clipping shows; outside the trim is dimmed.
- **Regions** (`SoundExport.hpp`): a project cuts any number of clips from one
  source, each with its own id, path and settings; they may overlap in the
  source and have no order in time (no timeline, no mixing: the Replacement
  Criteria). `ExportSounds` checks every region and renders every clip before
  it writes anything: no region, two regions with one id, two paths that name
  one file, a path that is the source or the catalog, or a region the catalog
  or its settings refuse, and nothing is written. Two resolved paths are one
  file when the OS layer compares each name equal (`Files::SamePathComponent`:
  ignoring case on Windows, byte for byte elsewhere, so on macOS's default
  volume two paths that differ only in case are still missed). Then the clips
  are written in order and the catalog once, last, with every entry. A write
  that fails leaves the catalog untouched; the clips written before it stay,
  unregistered, and the error names them. A path one region gives up and
  another registers is not reported as left behind. `ExportSound` is the same
  for one region. The window edits the selected region, previews it and
  exports all of them; opening a WAV keeps every region and names those that
  do not fit it.
- **Window**: the framework's ImGui shell (`RunImGuiShell`); its context lends
  Engine's platform, whose path dialogs open WAVs, projects, the asset root and
  the catalog. The editor reaches `GYO::PlatformSDL` only through that binding's
  public interface and does not name the target. Only the latest dialog's answer counts; a Failed answer does not
  end the wait (platform-sdl Design Invariant 10).

## Project file

The editor's own format (Design Invariants 4 and 5); the game never reads it,
and only the editor writes it, so it is not a Data Contract with the runtime.
A UTF-8 JSON object with exactly these fields (version 2,
`v8-tool-audio-editor-regions`):

| Field | Value |
|---|---|
| `format` | `"gyo.audio_editor.project"` |
| `version` | `2`; version 1 is read as one region without fades, another version is refused |
| `source` | the source WAV, as it was opened (UTF-8) |
| `asset_root`, `asset_catalog` | where export writes, as for `convert` |
| `regions` | a non-empty array of regions, each an object with exactly the fields below |

| Region field | Value |
|---|---|
| `id`, `path` | the region's catalog entry |
| `trim_start` | first source frame kept |
| `trim_end` | one past the last frame kept, or `null` for the source's end |
| `gain_db` | the gain, written with every digit it needs |
| `fade_in`, `fade_out` | the fades, in source frames |

Version 1 had `id`, `path`, `trim_start`, `trim_end` and `gain_db` at the top
level, one region; the editor reads it and writes version 2.

The same project always gives the same text. A file with other fields, a field
of another type, another `format` or no region is refused (`InvalidProject`); saving it
inside its own asset root is refused too. The settings are checked against the
source when it is opened and again on export.
