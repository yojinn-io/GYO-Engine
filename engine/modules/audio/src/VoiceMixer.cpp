#include "VoiceMixer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "engine/audio/AudioFormat.hpp"
#include "engine/base/Assert.hpp"

namespace GYO::Engine::Audio::Detail {

namespace {

// Frames mixed per pass through the accumulator; any block size is split into
// passes of at most this many frames.
constexpr std::uint32_t kChunkFrames = 256;
constexpr int kGainShift = 16;
// With int16 samples and gains up to kMaxGain in Q16, this many voices times the
// master gain still fit the int64 accumulator (2^34 per voice, 2^19 master).
constexpr std::uint32_t kMaxVoices = 1024;

std::int64_t ToQ16(const float gain) noexcept {
    return static_cast<std::int64_t>(std::lround(static_cast<double>(gain) * (1 << kGainShift)));
}

void RaiseMax(std::atomic<std::uint64_t>& target, const std::uint64_t value) noexcept {
    // Only the audio thread writes it, so a load and a store suffice.
    if (value > target.load(std::memory_order_relaxed)) target.store(value, std::memory_order_relaxed);
}

} // namespace

Base::Result<VoiceMixer, AudioError> VoiceMixer::Create(const std::uint32_t maxVoices) {
    if (maxVoices == 0 || maxVoices > kMaxVoices) {
        return Base::Err(AudioError::Make(AudioErrorCode::InvalidConfig, "a mixer has 1 to 1024 voices",
                                          "maxVoices=" + std::to_string(maxVoices)));
    }
    return VoiceMixer(maxVoices);
}

VoiceMixer::VoiceMixer(const std::uint32_t maxVoices)
    : voices_(maxVoices), accumulator_(std::size_t{kChunkFrames} * kChannels), ended_(maxVoices) {}

VoiceMixer::VoiceMixer(VoiceMixer&& other) noexcept
    : voices_(std::move(other.voices_)),
      accumulator_(std::move(other.accumulator_)),
      ended_(std::move(other.ended_)),
      endedCount_(other.endedCount_),
      nextFrame_(other.nextFrame_),
      masterGainQ16_(other.masterGainQ16_.load(std::memory_order_relaxed)) {
    auto copy = [](std::atomic<std::uint64_t>& to, const std::atomic<std::uint64_t>& from) {
        to.store(from.load(std::memory_order_relaxed), std::memory_order_relaxed);
    };
    copy(counters_.accepted, other.counters_.accepted);
    copy(counters_.onTime, other.counters_.onTime);
    copy(counters_.late, other.counters_.late);
    copy(counters_.noVoice, other.counters_.noVoice);
    copy(counters_.maxLateFrames, other.counters_.maxLateFrames);
    copy(counters_.renderedFrames, other.counters_.renderedFrames);
    copy(counters_.clippedSamples, other.counters_.clippedSamples);
}

StartOutcome VoiceMixer::Schedule(const ScheduledPlay& play) {
    GYO_ASSERT(play.clip.frames != 0 && (play.clip.channels == 1 || play.clip.channels == 2));
    GYO_ASSERT(IsValidGain(play.gainLeft) && IsValidGain(play.gainRight));
    return ScheduleChecked(play);
}

StartOutcome VoiceMixer::ScheduleChecked(const ScheduledPlay& play) noexcept {
    counters_.accepted.fetch_add(1, std::memory_order_relaxed);

    const auto free = std::find_if(voices_.begin(), voices_.end(), [](const Voice& v) { return !v.active; });
    if (free == voices_.end()) {
        counters_.noVoice.fetch_add(1, std::memory_order_relaxed);
        return StartOutcome::NoVoice;
    }

    StartOutcome outcome = StartOutcome::OnTime;
    std::int64_t start = play.startFrame;
    if (start < nextFrame_) {
        RaiseMax(counters_.maxLateFrames, static_cast<std::uint64_t>(nextFrame_ - start));
        start = nextFrame_;
        outcome = StartOutcome::Late;
        counters_.late.fetch_add(1, std::memory_order_relaxed);
    } else {
        counters_.onTime.fetch_add(1, std::memory_order_relaxed);
    }
    const std::int64_t left = ToQ16(play.gainLeft);
    const std::int64_t right = ToQ16(play.gainRight);
    *free = Voice{true, play.loop, start, std::numeric_limits<std::int64_t>::max(), play.cookie, play.lease, 0,
                  GainRamp{left, left}, GainRamp{right, right}, start, 0, play.clip};
    return outcome;
}

std::int64_t VoiceMixer::GainRamp::At(const std::int64_t frame, const std::int64_t rampStart,
                                      const std::uint32_t rampFrames) const noexcept {
    if (frame < rampStart) return from;
    const std::int64_t into = frame - rampStart;
    if (into >= std::int64_t{rampFrames}) return to;
    // Truncated toward zero, the same on every platform; |to - from| * rampFrames < 2^48.
    return from + (to - from) * into / std::int64_t{rampFrames};
}

GainResult VoiceMixer::SetGain(const std::uint64_t cookie, const std::int64_t startFrame,
                               const std::uint32_t rampFrames, const float gainLeft, const float gainRight) noexcept {
    GainResult result;
    const std::int64_t start = std::max(startFrame, nextFrame_);
    for (Voice& voice : voices_) {
        if (!voice.active || voice.cookie != cookie) continue;
        ++result.changed;
        // Late as a stop is: only a started voice can have been heard at the old gain.
        if (startFrame < nextFrame_ && voice.startFrame < nextFrame_) result.late = true;
        voice.left = GainRamp{voice.left.At(nextFrame_, voice.rampStart, voice.rampFrames), ToQ16(gainLeft)};
        voice.right = GainRamp{voice.right.At(nextFrame_, voice.rampStart, voice.rampFrames), ToQ16(gainRight)};
        voice.rampStart = start;
        voice.rampFrames = rampFrames;
    }
    return result;
}

StopResult VoiceMixer::Stop(const std::uint64_t cookie, const std::uint64_t mask, const std::int64_t stopFrame) noexcept {
    StopResult result;
    for (Voice& voice : voices_) {
        if (!voice.active || (voice.cookie & mask) != (cookie & mask)) continue;
        ++result.stopped;
        const bool started = voice.startFrame < nextFrame_;
        if (!started && stopFrame <= voice.startFrame) {
            End(voice); // stopped before it started: never heard
            continue;
        }
        if (started && stopFrame <= nextFrame_) {
            // Nothing more is mixed: it stops at this block's first frame.
            if (stopFrame < nextFrame_) result.late = true;
            End(voice);
            continue;
        }
        voice.stopFrame = std::min(voice.stopFrame, stopFrame); // the earliest stop wins
    }
    return result;
}

void VoiceMixer::Render(const Base::Span<std::int16_t> interleavedOut) noexcept {
    std::int16_t* out = interleavedOut.data();
    std::uint64_t remaining = interleavedOut.size() / kChannels;
    while (remaining > 0) {
        const auto frames = static_cast<std::uint32_t>(std::min<std::uint64_t>(remaining, kChunkFrames));
        RenderChunk(out, frames);
        out += std::size_t{frames} * kChannels;
        remaining -= frames;
    }
    // An odd sample count is the caller's error; the stray sample is silence.
    if (interleavedOut.size() % kChannels != 0) interleavedOut[interleavedOut.size() - 1] = 0;
}

void VoiceMixer::RenderChunk(std::int16_t* const out, const std::uint32_t frames) noexcept {
    std::fill_n(accumulator_.begin(), std::size_t{frames} * kChannels, std::int64_t{0});
    const std::int64_t chunkStart = nextFrame_;
    const std::int64_t chunkEnd = chunkStart + frames;

    for (Voice& voice : voices_) {
        if (!voice.active || voice.startFrame >= chunkEnd) continue;
        // First frame of this chunk the voice contributes to.
        std::uint32_t at = voice.startFrame > chunkStart ? static_cast<std::uint32_t>(voice.startFrame - chunkStart) : 0;
        const std::int16_t* samples = voice.clip.samples.data();
        const std::int64_t untilFrame = std::min(voice.stopFrame, chunkEnd);
        // A steady gain is read once; a ramp is evaluated at every output frame.
        const bool steady = voice.left.from == voice.left.to && voice.right.from == voice.right.to;
        while (chunkStart + at < untilFrame && voice.position < voice.clip.frames) {
            std::int64_t left = 0;
            std::int64_t right = 0;
            if (voice.clip.channels == 1) {
                left = right = samples[voice.position];
            } else {
                left = samples[std::size_t{voice.position} * 2];
                right = samples[std::size_t{voice.position} * 2 + 1];
            }
            std::int64_t gainLeft = voice.left.to;
            std::int64_t gainRight = voice.right.to;
            if (!steady) {
                const std::int64_t frame = chunkStart + at;
                gainLeft = voice.left.At(frame, voice.rampStart, voice.rampFrames);
                gainRight = voice.right.At(frame, voice.rampStart, voice.rampFrames);
            }
            accumulator_[std::size_t{at} * 2] += left * gainLeft;
            accumulator_[std::size_t{at} * 2 + 1] += right * gainRight;
            ++at;
            ++voice.position;
            if (voice.loop && voice.position == voice.clip.frames) voice.position = 0;
        }
        // A finished ramp is steady from here on.
        if (!steady && chunkEnd >= voice.rampStart + std::int64_t{voice.rampFrames}) {
            voice.left.from = voice.left.to;
            voice.right.from = voice.right.to;
        }
        if (voice.position >= voice.clip.frames || chunkStart + at >= voice.stopFrame) End(voice);
    }

    const std::int64_t master = masterGainQ16_.load(std::memory_order_relaxed);
    constexpr std::int64_t kHalf = std::int64_t{1} << (2 * kGainShift - 1);
    std::uint64_t clipped = 0;
    for (std::size_t i = 0; i < std::size_t{frames} * kChannels; ++i) {
        // Both gains are Q16, so the product is Q32; round half up, then saturate.
        const std::int64_t value = (accumulator_[i] * master + kHalf) >> (2 * kGainShift);
        const std::int64_t bounded = std::clamp<std::int64_t>(value, std::numeric_limits<std::int16_t>::min(),
                                                              std::numeric_limits<std::int16_t>::max());
        if (bounded != value) ++clipped;
        out[i] = static_cast<std::int16_t>(bounded);
    }

    nextFrame_ = chunkEnd;
    counters_.renderedFrames.store(static_cast<std::uint64_t>(nextFrame_), std::memory_order_relaxed);
    if (clipped != 0) counters_.clippedSamples.fetch_add(clipped, std::memory_order_relaxed);
}

void VoiceMixer::End(Voice& voice) noexcept {
    voice.active = false;
    // At most maxVoices voices end between two drains: no voice starts in
    // Render, and the caller drains after every Stop.
    if (voice.lease != 0 && endedCount_ < ended_.size()) ended_[endedCount_++] = voice.lease;
}

void VoiceMixer::SetMasterGain(const float gain) {
    GYO_ASSERT(IsValidGain(gain));
    masterGainQ16_.store(ToQ16(gain), std::memory_order_relaxed);
}

VoiceMixerStats VoiceMixer::Stats() const noexcept {
    return VoiceMixerStats{
        counters_.accepted.load(std::memory_order_relaxed),
        counters_.onTime.load(std::memory_order_relaxed),
        counters_.late.load(std::memory_order_relaxed),
        counters_.noVoice.load(std::memory_order_relaxed),
        counters_.maxLateFrames.load(std::memory_order_relaxed),
        counters_.renderedFrames.load(std::memory_order_relaxed),
        counters_.clippedSamples.load(std::memory_order_relaxed),
    };
}

std::uint64_t VoiceMixer::RenderedFrames() const noexcept {
    return counters_.renderedFrames.load(std::memory_order_relaxed);
}

} // namespace GYO::Engine::Audio::Detail
