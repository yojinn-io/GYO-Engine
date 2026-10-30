# GYO audio mixer

Owner: Engine (`engine/audio` → `GYO::Audio`; the SDL output backend `engine/audio/backend/sdl` →
`GYO::AudioBackendSDL` arrives with v8-engine-audio-sdl).
Plan and record: [audio](plans/audio/README.md), batches v8-engine-audio-core, -schedule, -sdl and -record.

**Status (2026-10-30): v8-engine-audio-core landed.** The clip bank and the sample-domain core (`VoiceMixer`,
internal to `engine/audio/src`) exist and are tested offline. The public `Mixer` (submitters and the timestamp
mapping) comes with v8-engine-audio-schedule and the output backend with v8-engine-audio-sdl. Each invariant below
names the milestone that makes it true and the guard that keeps it true.

## What it is

```text
GYO::Base ◄── GYO::Time ◄── GYO::Audio ◄── GYO::AudioBackendSDL ──(PRIVATE)──► SDL3
```

- **Output format.** One format only: interleaved stereo PCM16 at 48 kHz (`AudioFormat.hpp`). Converting to the
  device's format belongs to the output backend (SDL's audio stream).
- **Clip bank.** `ClipBank::Add` copies an interleaved PCM16 clip of 1 or 2 channels at 48 kHz and returns a
  `ClipId` (never zero). Anything else is an `InvalidClip` Runtime Error: an empty clip, other channel counts,
  samples that are not whole frames, another sample rate (no resampling). Clips are immutable and are registered
  before output starts; the mixer owns the bank afterwards. A sound asset from `SoundLoader` (`GYO::Engine`) is
  handed over by its fields; `GYO::Audio` does not link the asset library.
- **Voices (`VoiceMixer`, internal).** A fixed number of voices (1 to 1024, `InvalidConfig` otherwise), each playing
  one clip once from a start frame with a left and a right gain in [0, `kMaxGain`]. A scheduled voice that has not
  started yet occupies its voice. When every voice is busy a new play is dropped and counted (`noVoice`). A finished
  voice is free again.
- **Mix.** Gains are converted to Q16 fixed point; voices are summed in int64, multiplied by the master gain (Q16),
  rounded half up and saturated to int16; each saturated output sample is counted (`clippedSamples`). Integer
  arithmetic makes the output identical on every platform.
- **Late start.** A play whose start frame is before the next frame to render starts at that frame instead, and is
  counted as late with how many frames late it was (`maxLateFrames`).
- **Counters.** `accepted` (plays scheduled), `onTime`, `late`, `noVoice`, `maxLateFrames`, `renderedFrames`,
  `clippedSamples`; relaxed atomics, readable from any thread, for display and acceptance only.

## Contracts

- `Schedule` and `Render` run on one thread (the audio thread once a backend exists). `Stats` and `SetMasterGain`
  may run on any thread. A gain outside [0, `kMaxGain`] or a clip outside the bank is a Programmer Error
  (`GYO_ASSERT`), checked before anything is counted.
- `Render` fills a block of any length and advances `renderedFrames` by its frame count. It works in internal
  passes of at most 256 frames over an accumulator allocated at creation.
- The master gain is read once per internal pass; a change takes effect at the next pass.

## Design Invariants

1. **Neutral.** `GYO::Audio` and its backend know clips, times, gains and samples only: no event kinds, weapons,
   characters or product names, and no API named after product meaning. Guard: review (no automated guard).
2. **SDL only in the backend.** `GYO::Audio` contains no SDL: its links are exactly `GYO::Base` and `GYO::Time`
   (configure-time check `gyo_expect_audio_links`, `tests/common/audio/CMakeLists.txt`; from v8-engine-audio-core).
   The backend follows [platform-sdl.md](platform-sdl.md) Invariants 1, 2 and 8: neutral headers without SDL, SDL3
   linked PRIVATE, the AUDIO subsystem reference-counted by its owner, no exception across SDL's C stack (G1–G3;
   from v8-engine-audio-sdl).
3. **One time base.** Play requests carry `Time::MonotonicClock` time points. Conversion to sample positions happens
   only inside the mixer, only from the current block's anchor; the backend samples `Now()` once per block; no fixed
   offset is carried across blocks. From v8-engine-audio-schedule.
4. **The mixer is the authority for the playback position.** Only the mixer owns the count of rendered frames;
   consumers and the backend keep no copy of it to decide anything (AGENTS §3, State Authority). Guard: review.
5. **Spacing is kept under one anchor.** Two requests taken under the same anchor, neither late, start
   `round(Δ × 48000)` frames apart for timestamps Δ apart. The sample-domain half holds from v8-engine-audio-core
   (a voice starts exactly at its start frame whatever the block split; guards "starts a clip exactly at its start
   frame", "starts a clip scheduled in a later block at its frame", "output does not depend on how blocks split it");
   the mapping half from v8-engine-audio-schedule.
6. **The balance adds up.** Every request ends in exactly one of on time, late, dropped because the queue was full,
   dropped because no voice was free; submitted equals their sum. No drop goes uncounted. The voice half
   (`accepted == onTime + late + noVoice`) holds from v8-engine-audio-core (guards "drops a play when every voice is
   busy", "starts a late play at the next frame"); the queue half from v8-engine-audio-schedule.
7. **The audio thread is real-time safe.** `Render` allocates, frees, locks, waits, logs and throws nothing
   (`noexcept`; every capacity is allocated at creation); clips do not change while output runs. From
   v8-engine-audio-core for `VoiceMixer::Render`. Guard: review and the `noexcept` signature.
8. **Deterministic.** The same clip bank, request sequence and anchor sequence give a bit-identical output, however
   the output is split into blocks. Guard: "output does not depend on how blocks split it" (block sizes 1 to 2200,
   across the 256-frame pass). From v8-engine-audio-core.
9. **Fixed output format.** The mixer outputs 48 kHz stereo PCM16 only; clips at another rate are rejected, never
   resampled. Guard: "ClipBank rejects clips the mixer cannot play". From v8-engine-audio-core.

### Replacement Criteria

Stop patching and let the user decide between patching and rebuilding when any of these holds:

1. Streaming, compressed formats, or loading or unloading clips while output runs is needed (the clip bank's lifetime
   premise no longer holds).
2. Signal processing beyond per-voice gains (buses, effects, HRTF), or stopping or changing a sound after it was
   sent (loops, music), is needed.
3. A second, non-SDL backend is needed and the mapping must be re-verified per backend.
4. Anchor jitter measured in L2 makes interval errors (beyond Invariant 5) unacceptable for the consumer, so the
   mapping needs filtering, and the filter needs backend-specific information.
5. The number or threads of producers cannot be fixed at creation (any-thread submission; the SPSC premise fails).
6. Each new playback feature requires re-verifying the mapping, the late rule or real-time safety.
