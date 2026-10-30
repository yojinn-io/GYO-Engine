# GYO audio mixer

Owner: Engine (`engine/audio` → `GYO::Audio`; the SDL output backend `engine/audio/backend/sdl` →
`GYO::AudioBackendSDL`, built when a product declares the `SDL_AUDIO` component).
Plan and record: [audio](plans/audio/README.md), batches v8-engine-audio-core, -schedule, -sdl and -record.

**Status (2026-10-30): v8-engine-audio-core, -schedule and -sdl landed.** The clip bank, the sample-domain core
(`VoiceMixer`, internal to `engine/audio/src`), the public `Mixer` with its submitters and timestamp mapping, and
the SDL output backend exist. The mixer is tested offline; the backend on SDL's `dummy` and `disk` drivers. Each invariant below
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
- **Submitters (`Mixer::CreateSubmitter`).** Each is the producer end of one bounded SPSC ring. All rings
  (`maxSubmitters`, 1 to 64, of `queueCapacity`, 1 to 65536) are allocated when the mixer is created; creating a
  submitter only hands out the next one (any thread, any time; `TooManySubmitters` when none is left). The first
  thread that plays through a submitter owns it. A destroyed submitter does not give its ring back, so a consumer
  creates one per producer thread and keeps it. A full ring drops the request and counts it (`droppedQueueFull`).
  A submitter shares the mixer's state, so it may outlive the `Mixer`.
- **Timestamp mapping (`Mixer::Render(block, anchor)`).** The backend reads the Engine time once at the start of each
  block and passes it as `anchor`. Every queued request starts at
  `blockStart + floor((at - anchor) × 48000 + 1/2) + delayFrames` frames (halves toward +infinity, also for negative
  offsets), with `at - anchor` clamped to an hour either way and `delayFrames` the `scheduleDelay` rounded the same
  way. A request is counted on time or late when the audio thread takes it, so a play counted on time may still be
  ahead (up to the delay, or up to an hour for a timestamp in the future). `scheduleDelay` (0 to 1 s)
  lets a request sent shortly after its time still start on time.
- **Counters.** `accepted` (plays scheduled), `onTime`, `late`, `noVoice`, `maxLateFrames`, `renderedFrames`,
  `clippedSamples`; relaxed atomics, readable from any thread, for display and acceptance only.

- **SDL output (`Sdl::SdlAudioOutput`).** `Open(Mixer&&, config)` takes the mixer's ownership, starts SDL's AUDIO
  subsystem (reference-counted; Engine never calls the global `SDL_Init`/`SDL_Quit`) and opens an SDL audio stream
  of 48 kHz stereo PCM16 on the default playback device; SDL converts to the device's format and follows
  default-device changes. On SDL's audio thread the stream's callback reads `Time::Now()` once and renders the
  frames SDL asks for with it as the anchor (a request above 4096 frames is rendered in passes, each anchored where
  the previous one ended). Opening fails with `DeviceUnavailable` and SDL's message as the detail; submitters keep
  accepting plays, which are never heard. Destroying the output destroys the stream first (no callback runs after
  that) and then releases the subsystem. `DriverName` (diagnostics only), `DeviceSampleFrames` and callback
  `Stats` (count, frames, smallest and largest callback, longest gap between callback anchors) are for logs and
  acceptance. `deviceSampleFrames` in the config is a hint to SDL.
- **SDL is never reached from outside the backend.** The backend links SDL3 PRIVATE and offers no `*Native` target;
  consumers and tests use only its neutral header. Tests choose SDL's driver and hints through the environment
  (`SDL_AUDIO_DRIVER`, `SDL_AUDIO_DISK_*`, `SDL_AUDIO_DEVICE_SAMPLE_FRAMES`, format hints), never by calling SDL.

## Contracts

- `Mixer::Render` (and inside it `VoiceMixer`'s schedule and render) runs on one thread, the audio thread.
  `CreateSubmitter`, `Stats` and `SetMasterGain` may run on any thread; each `Submitter` on its owner thread. A gain
  outside [0, `kMaxGain`], a clip outside the bank, playing from another thread than the owner, and a moved-from
  `Submitter` are Programmer Errors (the owner check is best effort: a later thread may reuse a joined thread's id) (`GYO_ASSERT` in `Submitter::Play`, on the producer, before anything is
  counted); the audio thread takes requests without checks (`VoiceMixer::ScheduleChecked`, `noexcept`).
- `MixerStats`: `submitted == playedOnTime + playedLate + droppedQueueFull + droppedNoVoice` once no request is in
  flight (every queued request taken, no producer playing); counters are read one by one, so a snapshot taken
  meanwhile may differ either way and is not subtracted to count requests in flight.
- Real-time bound: `Render` takes every queued request in one block, and each start looks for a free voice
  linearly; the worst case is `maxSubmitters × queueCapacity × maxVoices` comparisons in one block. The defaults
  (4 × 256 × 32) are small; the configured limits (64 × 65536 × 1024) are not real-time.
- Moving a `Mixer` while its audio thread renders is a data race; the backend stops output first. `maxLateness` is the latest late start, converted from frames.
- A moved-from `Mixer` renders silence; an odd sample count gets silence in its last sample.
- `Render` fills a block of any length and advances `renderedFrames` by its frame count. It works in internal
  passes of at most 256 frames over an accumulator allocated at creation.
- The master gain is read once per internal pass; a change takes effect at the next pass.

## Design Invariants

1. **Neutral.** `GYO::Audio` and its backend know clips, times, gains and samples only: no event kinds, weapons,
   characters or product names, and no API named after product meaning. Guard: review (no automated guard).
2. **SDL only in the backend.** `GYO::Audio` contains no SDL: its links are exactly `GYO::Base` and `GYO::Time`
   (configure-time check `gyo_expect_audio_links`, `tests/common/audio/CMakeLists.txt`; from v8-engine-audio-core).
   The backend follows [platform-sdl.md](platform-sdl.md) Invariants 1, 2 and 8: neutral headers without SDL, SDL3
   linked PRIVATE with no `*Native` target, the AUDIO subsystem reference-counted by its owner, no exception across
   SDL's C stack (the callback and `Mixer::Render` are `noexcept`). Guards from v8-engine-audio-sdl: G1
   (`GYO_HYGIENE_SDL_AUDIO` in `tests/common/platform_sdl/HeaderHygieneTests.cpp`), G2 (the scan covers the new
   include directory), G3 (`gyo_audio_sdl` in `gyo_expect_private_sdl_backends`); "outputs on the dummy driver share
   SDL's audio subsystem by reference count".
3. **One time base.** Play requests carry `Time::MonotonicClock` time points. Conversion to sample positions happens
   only inside the mixer, only from the current block's anchor; the backend samples `Now()` once per block; no fixed
   offset is carried across blocks. From v8-engine-audio-schedule for the mixer (guard "maps a request in a later
   block from that block's anchor"); the once-per-callback read is the backend's, from v8-engine-audio-sdl (review;
   the `disk` driver test checks a past request lands late at once).
4. **The mixer is the authority for the playback position.** Only the mixer owns the count of rendered frames;
   consumers and the backend keep no copy of it to decide anything (AGENTS §3, State Authority). Guard: review.
5. **Each start is its own rounded offset.** Every request taken under an anchor, with a timestamp within an hour of
   that anchor, starts at its offset from the anchor rounded to the nearest frame as floor(x + 1/2) (halves toward
   +infinity, also for negative offsets), plus the constant delay; so two such requests, neither late, start a
   number of frames apart that differs from Δ × 48000 by less than one frame for timestamps Δ apart. Offsets beyond
   an hour are clamped to an hour. (Revised 2026-10-30 with the user's consent, v8-engine-audio-invariant5: the
   original "exactly round(Δ × 48000) apart" cannot hold for three or more requests; the hour and the rounding
   direction were added after the xhigh review, v8-engine-audio-invariant5-clamp.) The sample-domain half holds from
   v8-engine-audio-core (a voice starts exactly at its start frame whatever the block split; guards "starts a clip
   exactly at its start frame", "starts a clip scheduled in a later block at its frame", "output does not depend on
   how blocks split it"); the mapping half from v8-engine-audio-schedule (guards "starts a request at its offset
   from the block's anchor", "rounds an offset to the nearest frame, halves up", "starts each request at its own
   rounded offset under one anchor", "maps a request in a later block from that block's anchor").
6. **The balance adds up.** Every request ends in exactly one of on time, late, dropped because the queue was full,
   dropped because no voice was free; submitted equals their sum. No drop goes uncounted. The voice half
   (`accepted == onTime + late + noVoice`) holds from v8-engine-audio-core (guards "drops a play when every voice is
   busy", "starts a late play at the next frame"); the queue half from v8-engine-audio-schedule (guards "drops and
   counts a request when its queue is full", "balances every request from concurrent producers").
7. **The audio thread is real-time safe.** `Render` allocates, frees, locks, waits, logs and throws nothing
   (`noexcept`; every capacity is allocated at creation, including every submitter ring); clips do not change while
   output runs. From v8-engine-audio-core for `VoiceMixer::Render`, from v8-engine-audio-schedule for `Mixer::Render`.
   Guard: review and the `noexcept` signatures.
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
