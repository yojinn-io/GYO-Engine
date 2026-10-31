#include "engine/audio/Mixer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "ClipValidation.hpp"
#include "VoiceMixer.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/base/Assert.hpp"

namespace Engine::Audio {

namespace Detail {

enum class CommandKind : std::uint8_t { Play, Stop, StopAll, Unload };

// One entry of a submitter's ring. A play carries its handle as the voice's
// cookie; a stop names a handle; a stop-all names the submitter's handle prefix.
struct Command final {
    CommandKind kind = CommandKind::Play;
    PlayRequest play;          // Play: the request; Stop and StopAll: only `at`; Unload: only `clip`
    std::uint64_t handle = 0;
};

enum class SlotState : std::uint8_t { Free, Loaded, Unloading };

// One clip a submitter loads while output runs. The owner producer allocates
// and frees the samples; the audio thread reads them through `view` only for
// plays queued after the load, and counts the voices reading them in its own
// lease. Once it has taken the clip's unload and no voice reads it, it resets
// its lease and hands the slot back with a release store; the owner frees the
// samples only after an acquire load sees that (Design Invariant 7).
struct ClipSlot final {
    // The owner producer's own.
    std::vector<std::int16_t> samples;
    SlotState state = SlotState::Free;
    // The generation of the next or current clip; never 0. It wraps after 2^32 - 1
    // unloads of one slot, which no consumer reaches; an id that old would alias.
    std::uint32_t generation = 1;
    // Written by the owner before the plays that read it are queued.
    ClipView view;
    // The audio thread's own lease.
    std::uint32_t voices = 0;
    bool unloadTaken = false;
    std::atomic<bool> handedBack{false};
};

// One bounded SPSC ring. The producer owns tail, the audio thread owns head;
// each publishes its index with release and reads the other's with acquire, so
// a slot is written before the consumer sees it and read before the producer
// reuses it. Indices only grow; the ring index is the index modulo capacity.
struct SubmitterSlot final {
    SubmitterSlot(const std::uint32_t capacity, const std::uint64_t prefix, const std::uint32_t clipSlotCount)
        : ring(capacity), handlePrefix(prefix), clips(clipSlotCount) {}

    std::vector<Command> ring;
    const std::uint64_t handlePrefix; // (index + 1) << kSequenceBits
    alignas(64) std::atomic<std::uint64_t> tail{0};
    alignas(64) std::atomic<std::uint64_t> head{0};
    alignas(64) std::atomic<std::thread::id> owner{};
    // The owner producer's own: the last handle sequence issued. 2^57 plays
    // would overflow it, which no mixer reaches.
    std::uint64_t lastSequence = 0;
    std::atomic<std::uint64_t> submitted{0};
    std::atomic<std::uint64_t> droppedQueueFull{0};
    std::atomic<std::uint64_t> stopsSubmitted{0};
    std::atomic<std::uint64_t> stopsDroppedQueueFull{0};
    std::atomic<std::uint64_t> unloadsSubmitted{0};
    std::atomic<std::uint64_t> unloadsDroppedQueueFull{0};
    std::vector<ClipSlot> clips; // all allocated at creation, without samples
};

// Handles: the submitter's index plus one in the top 7 bits (1 to 64), a
// sequence below that is never reused.
constexpr int kSequenceBits = 57;
constexpr std::uint64_t kSequenceMask = (std::uint64_t{1} << kSequenceBits) - 1;

// Runtime clip ids: the slot's generation in the high 32 bits (never 0, so the
// id is above every bank id), the submitter's index plus one and the slot
// below. The low 32 bits alone are the lease mark of the voices reading it.
constexpr std::uint64_t kLeaseMask = 0xFFFF'FFFFu;
constexpr bool IsRuntimeClip(const ClipId clip) noexcept { return clip.value > kLeaseMask; }
constexpr std::uint64_t LeaseMark(const std::uint64_t submitterPlusOne, const std::uint32_t slot) noexcept {
    return (submitterPlusOne << 16) | slot;
}

struct MixerState final {
    MixerState(VoiceMixer&& mixer, const std::uint32_t submitters, const std::uint32_t capacity,
               const std::uint32_t clipSlots, const std::int64_t delay)
        : voices(std::move(mixer)), delayFrames(delay) {
        slots.reserve(submitters);
        for (std::uint32_t i = 0; i < submitters; ++i) {
            slots.push_back(
                std::make_unique<SubmitterSlot>(capacity, std::uint64_t{i + 1} << kSequenceBits, clipSlots));
        }
    }

    // The clip slot a lease mark (or a runtime id's low bits) names.
    ClipSlot& SlotOf(const std::uint64_t mark) noexcept {
        return slots[((mark >> 16) & 0xFFFF) - 1]->clips[mark & 0xFFFF];
    }

    VoiceMixer voices;
    std::vector<std::unique_ptr<SubmitterSlot>> slots; // all allocated at creation
    std::atomic<std::uint32_t> claimed{0};             // slots handed out, never shrinks
    const std::int64_t delayFrames;
    // Stop outcomes, written by the audio thread only.
    std::atomic<std::uint64_t> stopsMatched{0};
    std::atomic<std::uint64_t> stopsUnmatched{0};
    std::atomic<std::uint64_t> stopsLate{0};
    std::atomic<std::uint64_t> unloadsTaken{0};
};

} // namespace Detail

namespace {

using Detail::Command;
using Detail::CommandKind;
using Detail::MixerState;
using Detail::IsRuntimeClip;
using Detail::SubmitterSlot;

constexpr std::uint32_t kMaxSubmitters = 64;
constexpr std::uint32_t kMaxQueueCapacity = 65536;
constexpr std::uint32_t kMaxClipSlots = 256;
constexpr auto kMaxScheduleDelay = std::chrono::seconds(1);
// Offsets from the anchor are clamped to an hour either way, which keeps the
// frame arithmetic far from overflow; a request an hour off is late or far
// ahead either way.
constexpr std::int64_t kMaxOffsetNs = std::int64_t{3600} * 1'000'000'000;
// 48000 frames per 10^9 ns is 6 frames per 125000 ns.
constexpr std::int64_t kFramesPerUnit = 6;
constexpr std::int64_t kNsPerUnit = 125000;
static_assert(kSampleRate * kNsPerUnit == kFramesPerUnit * 1'000'000'000);

std::int64_t FloorDiv(const std::int64_t value, const std::int64_t divisor) noexcept {
    const std::int64_t quotient = value / divisor;
    return (value % divisor != 0 && value < 0) ? quotient - 1 : quotient;
}

// Nanoseconds to the nearest frame: floor(x + 1/2), so halves go toward +infinity.
std::int64_t NsToFrames(const std::int64_t ns) noexcept {
    return FloorDiv(ns * kFramesPerUnit + kNsPerUnit / 2, kNsPerUnit);
}

// Every supported standard library counts steady_clock in nanoseconds; a coarser
// period would make the conversion below overflow near the clock's ends.
static_assert(std::is_same_v<Time::Duration::period, std::nano>);

std::int64_t Nanoseconds(const Time::TimePoint point) noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(point.time_since_epoch()).count();
}

// a - b clamped to [-kMaxOffsetNs, kMaxOffsetNs], without overflowing.
std::int64_t ClampedOffset(const std::int64_t a, const std::int64_t b) noexcept {
    if (b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) return kMaxOffsetNs;
    if (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b) return -kMaxOffsetNs;
    return std::clamp(a - b, -kMaxOffsetNs, kMaxOffsetNs);
}

// The audio thread. Resets its lease, then hands the slot back: the release
// store orders every read of the samples before the owner frees them.
void HandBack(Detail::ClipSlot& slot) noexcept {
    slot.voices = 0;
    slot.unloadTaken = false;
    slot.handedBack.store(true, std::memory_order_release);
}

// The audio thread. Ends the leases of the voices that ended since the last call.
void ReleaseEnded(MixerState& state) noexcept {
    for (const std::uint64_t mark : state.voices.Ended()) {
        Detail::ClipSlot& slot = state.SlotOf(mark);
        --slot.voices;
        if (slot.voices == 0 && slot.unloadTaken) HandBack(slot);
    }
    state.voices.ClearEnded();
}

AudioError InvalidConfig(const char* message, const std::string& detail) {
    return AudioError::Make(AudioErrorCode::InvalidConfig, message, detail);
}

} // namespace

Submitter::Submitter(std::shared_ptr<MixerState> state, SubmitterSlot* const slot) noexcept
    : state_(std::move(state)), slot_(slot) {}

Submitter::Submitter(Submitter&& other) noexcept
    : state_(std::move(other.state_)), slot_(std::exchange(other.slot_, nullptr)) {}

Submitter& Submitter::operator=(Submitter&& other) noexcept {
    state_ = std::move(other.state_);
    slot_ = std::exchange(other.slot_, nullptr);
    return *this;
}

Submitter::~Submitter() = default;

void Submitter::ClaimOwner() {
    GYO_ASSERT(slot_ != nullptr);
    const std::thread::id self = std::this_thread::get_id();
    std::thread::id owner{};
    if (!slot_->owner.compare_exchange_strong(owner, self, std::memory_order_relaxed)) {
        GYO_ASSERT(owner == self);
    }
}

bool Submitter::Enqueue(const Detail::Command& command) {
    const std::uint64_t tail = slot_->tail.load(std::memory_order_relaxed);
    const std::uint64_t head = slot_->head.load(std::memory_order_acquire);
    const std::uint64_t capacity = slot_->ring.size();
    if (tail - head >= capacity) return false;
    slot_->ring[tail % capacity] = command;
    slot_->tail.store(tail + 1, std::memory_order_release);
    return true;
}

SoundHandle Submitter::Play(const PlayRequest& request) {
    GYO_ASSERT(slot_ != nullptr);
    ClaimOwner(); // before reading the owner's own clip slots
    GYO_ASSERT(IsRuntimeClip(request.clip) ? LoadedClip(request.clip) != nullptr
                                           : state_->voices.Clips().Contains(request.clip));
    GYO_ASSERT(IsValidGain(request.gainLeft) && IsValidGain(request.gainRight));

    slot_->submitted.fetch_add(1, std::memory_order_relaxed);
    const std::uint64_t handle = slot_->handlePrefix | (slot_->lastSequence + 1);
    if (!Enqueue(Command{CommandKind::Play, request, handle})) {
        slot_->droppedQueueFull.fetch_add(1, std::memory_order_relaxed);
        return SoundHandle{};
    }
    ++slot_->lastSequence;
    return SoundHandle{handle};
}

bool Submitter::Stop(const SoundHandle sound, const Time::TimePoint at) {
    GYO_ASSERT(slot_ != nullptr);
    ClaimOwner();
    if (!sound.Valid()) return true;
    GYO_ASSERT((sound.value & ~Detail::kSequenceMask) == slot_->handlePrefix);
    const std::uint64_t sequence = sound.value & Detail::kSequenceMask;
    GYO_ASSERT(sequence != 0 && sequence <= slot_->lastSequence);
    return SendStop(Command{CommandKind::Stop, PlayRequest{ClipId{}, at}, sound.value});
}

bool Submitter::StopAll(const Time::TimePoint at) {
    GYO_ASSERT(slot_ != nullptr);
    ClaimOwner();
    return SendStop(Command{CommandKind::StopAll, PlayRequest{ClipId{}, at}, slot_->handlePrefix});
}

Detail::ClipSlot* Submitter::LoadedClip(const ClipId clip) const noexcept {
    const std::uint64_t submitterPlusOne = (clip.value >> 16) & 0xFFFF;
    const std::uint64_t index = clip.value & 0xFFFF;
    if (submitterPlusOne != (slot_->handlePrefix >> Detail::kSequenceBits) || index >= slot_->clips.size()) {
        return nullptr;
    }
    Detail::ClipSlot& entry = slot_->clips[index];
    const bool current = entry.state == Detail::SlotState::Loaded && (clip.value >> 32) == entry.generation;
    return current ? &entry : nullptr;
}

Base::Result<ClipId, AudioError> Submitter::LoadClip(const Base::ConstSpan<std::int16_t> interleaved,
                                                     const std::uint16_t channels, const std::uint32_t sampleRate) {
    GYO_ASSERT(slot_ != nullptr);
    ClaimOwner();
    // Free the slots the audio thread handed back; the acquire load orders its
    // last read of the samples before this thread frees them.
    for (Detail::ClipSlot& entry : slot_->clips) {
        if (entry.state != Detail::SlotState::Unloading || !entry.handedBack.load(std::memory_order_acquire)) continue;
        std::vector<std::int16_t>().swap(entry.samples);
        entry.view = ClipView{};
        entry.handedBack.store(false, std::memory_order_relaxed);
        entry.state = Detail::SlotState::Free;
    }
    if (auto valid = Detail::ValidateClip(interleaved, channels, sampleRate); !valid) {
        return Base::Err(std::move(valid).error());
    }
    const auto free = std::find_if(slot_->clips.begin(), slot_->clips.end(), [](const Detail::ClipSlot& entry) {
        return entry.state == Detail::SlotState::Free;
    });
    if (free == slot_->clips.end()) {
        return Base::Err(AudioError::Make(AudioErrorCode::NoClipSlot,
                                          "every clip slot of the submitter is loaded or not handed back yet",
                                          "clipSlotsPerSubmitter=" + std::to_string(slot_->clips.size())));
    }
    free->samples.assign(interleaved.begin(), interleaved.end());
    free->view = ClipView{Base::ConstSpan<std::int16_t>(free->samples.data(), free->samples.size()), channels,
                          static_cast<std::uint32_t>(free->samples.size() / channels)};
    free->state = Detail::SlotState::Loaded;
    const auto index = static_cast<std::uint32_t>(free - slot_->clips.begin());
    const std::uint64_t mark = Detail::LeaseMark(slot_->handlePrefix >> Detail::kSequenceBits, index);
    return ClipId{(std::uint64_t{free->generation} << 32) | mark};
}

bool Submitter::UnloadClip(const ClipId clip) {
    GYO_ASSERT(slot_ != nullptr);
    ClaimOwner();
    GYO_ASSERT(IsRuntimeClip(clip));
    Detail::ClipSlot* const entry = LoadedClip(clip);
    GYO_ASSERT(entry != nullptr);
    slot_->unloadsSubmitted.fetch_add(1, std::memory_order_relaxed);
    if (!Enqueue(Command{CommandKind::Unload, PlayRequest{clip}, 0})) {
        slot_->unloadsDroppedQueueFull.fetch_add(1, std::memory_order_relaxed);
        return false; // still loaded: the caller may unload it again
    }
    entry->state = Detail::SlotState::Unloading;
    // The old id no longer names a loaded clip; generation 0 is never used.
    entry->generation = entry->generation == std::numeric_limits<std::uint32_t>::max() ? 1 : entry->generation + 1;
    return true;
}

bool Submitter::SendStop(const Detail::Command& command) {
    slot_->stopsSubmitted.fetch_add(1, std::memory_order_relaxed);
    if (!Enqueue(command)) {
        slot_->stopsDroppedQueueFull.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

Base::Result<Mixer, AudioError> Mixer::Create(const MixerConfig& config, ClipBank&& clips) {
    if (config.maxSubmitters == 0 || config.maxSubmitters > kMaxSubmitters) {
        return Base::Err(InvalidConfig("a mixer has 1 to 64 submitters",
                                       "maxSubmitters=" + std::to_string(config.maxSubmitters)));
    }
    if (config.queueCapacity == 0 || config.queueCapacity > kMaxQueueCapacity) {
        return Base::Err(InvalidConfig("a submitter queue holds 1 to 65536 requests",
                                       "queueCapacity=" + std::to_string(config.queueCapacity)));
    }
    if (config.clipSlotsPerSubmitter > kMaxClipSlots) {
        return Base::Err(InvalidConfig("a submitter has 0 to 256 clip slots",
                                       "clipSlotsPerSubmitter=" + std::to_string(config.clipSlotsPerSubmitter)));
    }
    if (config.scheduleDelay < Time::Duration::zero() || config.scheduleDelay > kMaxScheduleDelay) {
        return Base::Err(InvalidConfig(
            "the schedule delay is 0 to 1 s",
            "scheduleDelayNs=" +
                std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(config.scheduleDelay).count())));
    }
    auto voices = Detail::VoiceMixer::Create(config.maxVoices, std::move(clips));
    if (!voices) return Base::Err(std::move(voices).error());
    const std::int64_t delay =
        NsToFrames(std::chrono::duration_cast<std::chrono::nanoseconds>(config.scheduleDelay).count());
    return Mixer(std::make_shared<MixerState>(std::move(voices).value(), config.maxSubmitters, config.queueCapacity,
                                              config.clipSlotsPerSubmitter, delay));
}

Mixer::Mixer(std::shared_ptr<MixerState> state) noexcept : state_(std::move(state)) {}
Mixer::Mixer(Mixer&&) noexcept = default;
Mixer& Mixer::operator=(Mixer&&) noexcept = default;
Mixer::~Mixer() = default;

Base::Result<Submitter, AudioError> Mixer::CreateSubmitter() {
    GYO_ASSERT(state_ != nullptr);
    const auto limit = static_cast<std::uint32_t>(state_->slots.size());
    std::uint32_t index = state_->claimed.load(std::memory_order_relaxed);
    while (index < limit) {
        // The slot exists since creation; claiming only hands it out once.
        if (state_->claimed.compare_exchange_weak(index, index + 1, std::memory_order_relaxed)) {
            return Submitter(state_, state_->slots[index].get());
        }
    }
    return Base::Err(AudioError::Make(AudioErrorCode::TooManySubmitters, "every submitter of the mixer is taken",
                                      "maxSubmitters=" + std::to_string(limit)));
}

void Mixer::SetMasterGain(const float gain) {
    GYO_ASSERT(state_ != nullptr);
    state_->voices.SetMasterGain(gain);
}

MixerStats Mixer::Stats() const noexcept {
    MixerStats stats;
    if (!state_) return stats;
    // Outcomes first, submissions after, so a quiet snapshot rarely shows more
    // outcomes than submissions; the contract still promises equality only when
    // nothing is in flight.
    const Detail::VoiceMixerStats voices = state_->voices.Stats();
    stats.stopsMatched = state_->stopsMatched.load(std::memory_order_relaxed);
    stats.stopsUnmatched = state_->stopsUnmatched.load(std::memory_order_relaxed);
    stats.stopsLate = state_->stopsLate.load(std::memory_order_relaxed);
    stats.unloadsTaken = state_->unloadsTaken.load(std::memory_order_relaxed);
    for (const auto& slot : state_->slots) {
        stats.droppedQueueFull += slot->droppedQueueFull.load(std::memory_order_relaxed);
        stats.submitted += slot->submitted.load(std::memory_order_relaxed);
        stats.stopsDroppedQueueFull += slot->stopsDroppedQueueFull.load(std::memory_order_relaxed);
        stats.stopsSubmitted += slot->stopsSubmitted.load(std::memory_order_relaxed);
        stats.unloadsDroppedQueueFull += slot->unloadsDroppedQueueFull.load(std::memory_order_relaxed);
        stats.unloadsSubmitted += slot->unloadsSubmitted.load(std::memory_order_relaxed);
    }
    stats.playedOnTime = voices.onTime;
    stats.playedLate = voices.late;
    stats.droppedNoVoice = voices.noVoice;
    stats.maxLateness = std::chrono::duration_cast<Time::Duration>(std::chrono::nanoseconds(
        static_cast<std::int64_t>(voices.maxLateFrames) * kNsPerUnit / kFramesPerUnit));
    stats.renderedFrames = voices.renderedFrames;
    stats.clippedSamples = voices.clippedSamples;
    return stats;
}

void Mixer::Render(const Base::Span<std::int16_t> interleavedOut, const Time::TimePoint anchor) noexcept {
    if (!state_) {
        std::fill(interleavedOut.begin(), interleavedOut.end(), std::int16_t{0});
        return;
    }
    MixerState& state = *state_;
    const std::int64_t blockStart = state.voices.NextFrame();
    const std::int64_t anchorNs = Nanoseconds(anchor);
    const std::uint32_t claimed = std::min<std::uint32_t>(state.claimed.load(std::memory_order_relaxed),
                                                          static_cast<std::uint32_t>(state.slots.size()));
    for (std::uint32_t i = 0; i < claimed; ++i) {
        SubmitterSlot& slot = *state.slots[i];
        std::uint64_t head = slot.head.load(std::memory_order_relaxed);
        const std::uint64_t tail = slot.tail.load(std::memory_order_acquire);
        if (head == tail) continue;
        const std::uint64_t capacity = slot.ring.size();
        for (; head != tail; ++head) {
            const Command& command = slot.ring[head % capacity];
            const PlayRequest& request = command.play;
            if (command.kind == CommandKind::Play) {
                const std::int64_t offset = NsToFrames(ClampedOffset(Nanoseconds(request.at), anchorNs));
                Detail::ClipSlot* const leased =
                    IsRuntimeClip(request.clip) ? &state.SlotOf(request.clip.value & Detail::kLeaseMask) : nullptr;
                const Detail::StartOutcome outcome = state.voices.ScheduleChecked(Detail::ScheduledPlay{
                    request.clip, blockStart + offset + state.delayFrames, request.gainLeft, request.gainRight,
                    command.handle, leased != nullptr ? &leased->view : nullptr,
                    leased != nullptr ? request.clip.value & Detail::kLeaseMask : 0});
                if (leased != nullptr && outcome != Detail::StartOutcome::NoVoice) ++leased->voices;
                continue;
            }
            if (command.kind == CommandKind::Unload) {
                Detail::ClipSlot& unloaded = state.SlotOf(request.clip.value & Detail::kLeaseMask);
                state.unloadsTaken.fetch_add(1, std::memory_order_relaxed);
                unloaded.unloadTaken = true;
                if (unloaded.voices == 0) HandBack(unloaded);
                continue;
            }
            // A stop maps its time like a play (Design Invariant 3), on its own line.
            const std::int64_t stopFrame =
                blockStart + NsToFrames(ClampedOffset(Nanoseconds(request.at), anchorNs)) + state.delayFrames;
            const std::uint64_t mask = command.kind == CommandKind::Stop ? ~std::uint64_t{0} : ~Detail::kSequenceMask;
            const Detail::StopResult result = state.voices.Stop(command.handle, mask, stopFrame);
            ReleaseEnded(state);
            if (result.stopped == 0) {
                state.stopsUnmatched.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            state.stopsMatched.fetch_add(1, std::memory_order_relaxed);
            if (result.late) state.stopsLate.fetch_add(1, std::memory_order_relaxed);
        }
        slot.head.store(head, std::memory_order_release);
    }
    state.voices.Render(interleavedOut);
    ReleaseEnded(state);
}

} // namespace Engine::Audio
