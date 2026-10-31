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

#include "VoiceMixer.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/base/Assert.hpp"

namespace Engine::Audio {

namespace Detail {

enum class CommandKind : std::uint8_t { Play, Stop, StopAll };

// One entry of a submitter's ring. A play carries its handle as the voice's
// cookie; a stop names a handle; a stop-all names the submitter's handle prefix.
struct Command final {
    CommandKind kind = CommandKind::Play;
    PlayRequest play;          // Play: the request; Stop and StopAll: only `at`
    std::uint64_t handle = 0;
};

// One bounded SPSC ring. The producer owns tail, the audio thread owns head;
// each publishes its index with release and reads the other's with acquire, so
// a slot is written before the consumer sees it and read before the producer
// reuses it. Indices only grow; the ring index is the index modulo capacity.
struct SubmitterSlot final {
    SubmitterSlot(const std::uint32_t capacity, const std::uint64_t prefix) : ring(capacity), handlePrefix(prefix) {}

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
};

// Handles: the submitter's index plus one in the top 7 bits (1 to 64), a
// sequence below that is never reused.
constexpr int kSequenceBits = 57;
constexpr std::uint64_t kSequenceMask = (std::uint64_t{1} << kSequenceBits) - 1;

struct MixerState final {
    MixerState(VoiceMixer&& mixer, const std::uint32_t submitters, const std::uint32_t capacity,
               const std::int64_t delay)
        : voices(std::move(mixer)), delayFrames(delay) {
        slots.reserve(submitters);
        for (std::uint32_t i = 0; i < submitters; ++i) {
            slots.push_back(std::make_unique<SubmitterSlot>(capacity, std::uint64_t{i + 1} << kSequenceBits));
        }
    }

    VoiceMixer voices;
    std::vector<std::unique_ptr<SubmitterSlot>> slots; // all allocated at creation
    std::atomic<std::uint32_t> claimed{0};             // slots handed out, never shrinks
    const std::int64_t delayFrames;
    // Stop outcomes, written by the audio thread only.
    std::atomic<std::uint64_t> stopsMatched{0};
    std::atomic<std::uint64_t> stopsUnmatched{0};
    std::atomic<std::uint64_t> stopsLate{0};
};

} // namespace Detail

namespace {

using Detail::Command;
using Detail::CommandKind;
using Detail::MixerState;
using Detail::SubmitterSlot;

constexpr std::uint32_t kMaxSubmitters = 64;
constexpr std::uint32_t kMaxQueueCapacity = 65536;
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
    GYO_ASSERT(state_->voices.Clips().Contains(request.clip));
    GYO_ASSERT(IsValidGain(request.gainLeft) && IsValidGain(request.gainRight));
    ClaimOwner();

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
                                              delay));
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
    for (const auto& slot : state_->slots) {
        stats.droppedQueueFull += slot->droppedQueueFull.load(std::memory_order_relaxed);
        stats.submitted += slot->submitted.load(std::memory_order_relaxed);
        stats.stopsDroppedQueueFull += slot->stopsDroppedQueueFull.load(std::memory_order_relaxed);
        stats.stopsSubmitted += slot->stopsSubmitted.load(std::memory_order_relaxed);
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
                state.voices.ScheduleChecked(Detail::ScheduledPlay{
                    request.clip, blockStart + offset + state.delayFrames, request.gainLeft, request.gainRight,
                    command.handle});
                continue;
            }
            // A stop maps its time like a play (Design Invariant 3), on its own line.
            const std::int64_t stopFrame =
                blockStart + NsToFrames(ClampedOffset(Nanoseconds(request.at), anchorNs)) + state.delayFrames;
            const std::uint64_t mask = command.kind == CommandKind::Stop ? ~std::uint64_t{0} : ~Detail::kSequenceMask;
            const Detail::StopResult result = state.voices.Stop(command.handle, mask, stopFrame);
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
}

} // namespace Engine::Audio
