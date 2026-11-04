# GYO Audio Editor

Turns a WAV from anywhere into a clip the engine plays, writes it into a
product's asset root and registers it in that product's catalog. Design and
Design Invariants: [docs/audio_editor/architecture.md](../../docs/audio_editor/architecture.md).

The window (`edit`) plays the source, shows its waveform and information, and
cuts it into regions: each region is one clip with its own id, trim, gain and
fades, and Export writes them all. A project file keeps the source, the
destination and the regions. `info`, `convert` and `export` are the command
line.

## Build

```sh
cmake --preset dev -B build/target/_build/audio -DGYO_APPS= -DGYO_TOOLS=audio_editor -DBUILD_TESTING=ON
cmake --build build/target/_build/audio --target gyo_audio_editor
```

The tool is local (`release=false` in `engine/config/tools.csv`); the
executable is under `build/target/_tools/audio_editor/bin`. The default `gui`
variant needs SDL (renderer, input, audio) and ImGui; `-DGYO_TOOLS=audio_editor:cli`
builds the command line alone.

## Use

```sh
gyo_audio_editor edit [project.json | source.wav]
gyo_audio_editor info source.wav
gyo_audio_editor convert source.wav --asset-root assets/<product> \
    --asset-catalog assets/<product>/asset_catalog.json \
    --id <product>.audio.reload --path audio/reload.wav
gyo_audio_editor export project.json
```

- Input: RIFF/WAVE, integer PCM of 8, 16, 24 or 32 bits or 32-bit float, 1 or 2
  channels, any sample rate.
- Output: the engine's clip format (`GYO::Audio` `kSampleRate`, PCM16), the
  source's channel count. Resampling is that of
  `docs/architecture/plans/audio/scripts/convert_wav.py`; the dither is the
  tool's own, seeded by the asset id, so one id always gets the same bytes on a
  platform.
- Registration: the `sound` entry with that id is added after the last entry or
  rewritten in place; nothing else in the catalog changes. An id registered with
  another type, or a path registered under another id, is refused. When an
  entry's path changes, the old file is reported, not deleted.
- In the window, Play renders the selected region as export would and plays it
  (Space too); the preview volume changes only what is heard. Left and right
  clicks on the waveform set the selected region's start and end; the other
  regions are outlined. Add region copies the selected one's settings. Save the
  project outside the asset root (inside is refused).
- Export (the window's, and `export`) checks every region first: when one is
  refused (an id or a file twice, settings that do not fit), nothing is written.
- The source is only read, and a clip path that is the source or the catalog is
  refused. Keep sources outside the asset root: every file there must be
  registered.

Exit status: 0 done, 2 usage, 3 a file could not be read or written, 4 an
invalid WAV, catalog, registration or project.
