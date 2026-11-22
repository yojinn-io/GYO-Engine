#include "engine/audio/Mixer.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ClipValidation.hpp"
#include "PlayheadBoard.hpp"
#include "VoiceMixer.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/base/Assert.hpp"

namespace GYO::Engine::Audio {

namespace Detail {

enum class CommandKind : std::uint8_t { Play, Stop, StopAll };

// One entry of a submitter's ring, trivially copyable: a play carries its clip's
// view (the samples stay valid while its table entry holds the clip), its table
// entry and its handle as the voice's cookie; a stop names a handle; a stop-all
// names the submitter's handle prefix.
struct Command final {
    CommandKind kind = CommandKind::Play;
    Time::TimePoint at{};
    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    ClipView view;               // Play only
    std::uint64_t handle = 0;
    std::uint32_t entry = 0;     // Play only: the table entry holding the clip
};
static_assert(std::is_trivially_copyable_v<Command>);

// One bounded SPSC ring. The producer owns tail, the audio thread owns head;
// each publishes its index with release and reads the other's with acquire, so
// a slot is written before the consumer sees it and read before the producer
// reuses it. Indices only grow; the ring index is the index modulo capacity.
//
// The table holds a reference to the clip of every play queued or sounding
// (Design Invariant 7): only the producer (whoever uses the submitter, one at a
// time) touches it and the stack of free entries. The audio thread holds views
// and entry numbers, never a reference; it hands an entry back through the
// returned ring (the audio thread owns returnedTail, the producer
// returnedHead). An entry is in exactly one place at a time (free, in the ring,
// in a voice or its ended list, in the returned ring, or in the producer's hands
// while it lets go of it), and the returned ring has one slot per entry, so it
// never fills. The table never runs out: every Play first takes back what was
// returned, after which the entries in use are in the ring (at most its
// capacity) or in voices of earlier blocks (at most maxVoices); there is one
// entry more than both.
struct SubmitterSlot final {
    SubmitterSlot(const std::uint32_t capacity, const std::uint64_t prefix, const std::uint32_t entries)
        : ring(capacity), handlePrefix(prefix), held(entries), returned(entries) {
        freeEntries.reserve(entries);
        for (std::uint32_t i = entries; i > 0; --i) freeEntries.push_back(i - 1);
    }

    std::vector<Command> ring;
    const std::uint64_t handlePrefix; // (index + 1) << kSequenceBits
    alignas(64) std::atomic<std::uint64_t> tail{0};
    alignas(64) std::atomic<std::uint64_t> head{0};
    // The producer's own: the last handle sequence issued. 2^57 plays
    // would overflow it, which no mixer reaches.
    std::uint64_t lastSequence = 0;
    std::atomic<std::uint64_t> submitted{0};
    std::atomic<std::uint64_t> droppedQueueFull{0};
    std::atomic<std::uint64_t> stopsSubmitted{0};
    std::atomic<std::uint64_t> stopsDroppedQueueFull{0};
    // The producer's own: the clip references and the free entries.
    std::vector<Clip> held;
    std::vector<std::uint32_t> freeEntries;
    // Entries handed back by the audio thread.
    std::vector<std::uint32_t> returned;
    alignas(64) std::atomic<std::uint64_t> returnedTail{0};
    alignas(64) std::atomic<std::uint64_t> returnedHead{0};
};

// Handles: the submitter's index plus one in the top 7 bits (1 to 64), a
// sequence below that is never reused.
constexpr int kSequenceBits = 57;
constexpr std::uint64_t kSequenceMask = (std::uint64_t{1} << kSequenceBits) - 1;

// The lease mark a voice carries: the submitter's index plus one above 32 bits,
// the table entry below (up to 65536 + 1024 + 1 entries).
constexpr std::uint64_t LeaseMark(const std::uint64_t submitterPlusOne, const std::uint32_t entry) noexcept {
    return (submitterPlusOne << 32) | entry;
}

struct MixerState final {
    MixerState(VoiceMixer&& mixer, const std::uint32_t submitters, const std::uint32_t capacity,
               const std::int64_t delay)
        : voices(std::move(mixer)), delayFrames(delay), board(voices.VoiceCount()), published(voices.VoiceCount()) {
        const std::uint32_t entries = capacity + voices.VoiceCount() + 1;
        slots.reserve(submitters);
        for (std::uint32_t i = 0; i < submitters; ++i) {
            slots.push_back(std::make_unique<SubmitterSlot>(capacity, std::uint64_t{i + 1} << kSequenceBits, entries));
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
    // Where each voice is, for PlayheadOf. The audio thread writes it and keeps
    // its own copy of the tags it published, so it never reads the board.
    PlayheadBoard<> board;
    std::vector<std::uint64_t> published;
};

} // namespace Detail

namespace {

using Detail::Command;
using Detail::CommandKind;
using Detail::MixerState;
using Detail::PlayheadBoard;
using Detail::SubmitterSlot;

constexpr std::uint32_t kMaxSubmitters = 64;
constexpr std::uint32_t kMaxQueueCapacity = 65536;
// A table entry fits the low 32 bits of a lease mark (queue plus 1024 voices plus one).
static_assert(std::uint64_t{kMaxQueueCapacity} + 1024 + 1 <= 0xFFFF'FFFFu);
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

// The audio thread. Hands a table entry back to its submitter: the release
// orders every read of the clip's samples before the producer lets go of it.
// The ring has one slot per entry and an entry is in it at most once, so it is
// never full; the acquire orders the producer's read of the slot before this
// write.
void HandBack(MixerState& state, const std::uint64_t mark) noexcept {
    SubmitterSlot& slot = *state.slots[(mark >> 32) - 1];
    const std::uint64_t tail = slot.returnedTail.load(std::memory_order_relaxed);
    const std::uint64_t head = slot.returnedHead.load(std::memory_order_acquire);
    const std::uint64_t capacity = slot.returned.size();
    if (tail - head >= capacity) return; // cannot happen; the entry would stay held until the state goes
    slot.returned[tail % capacity] = static_cast<std::uint32_t>(mark & 0xFFFF'FFFFu);
    slot.returnedTail.store(tail + 1, std::memory_order_release);
}

// The audio thread, after each Render. Publishes every voice's sound and
// position; a voice that changed sound is assigned anew, an idle one cleared.
void PublishVoices(MixerState& state) noexcept {
    for (std::uint32_t i = 0; i < state.voices.VoiceCount(); ++i) {
        const Detail::VoiceState voice = state.voices.StateOf(i);
        if (!voice.active) {
            if (state.published[i] != 0) {
                state.board.Clear(i);
                state.published[i] = 0;
            }
            continue;
        }
        const std::uint64_t position = voice.position | (voice.started ? PlayheadBoard<>::kStarted : 0);
        if (state.published[i] != voice.cookie) {
            state.board.Assign(i, voice.cookie, position);
            state.published[i] = voice.cookie;
        } else {
            state.board.Publish(i, position);
        }
    }
}

// The audio thread. Hands back the table entries of the voices that ended since
// the last call.
void ReleaseEnded(MixerState& state) noexcept {
    for (const std::uint64_t mark : state.voices.Ended()) HandBack(state, mark);
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

Submitter::~Submitter() {
    if (slot_ != nullptr) Reclaim();
}

void Submitter::Reclaim() noexcept {
    std::uint64_t head = slot_->returnedHead.load(std::memory_order_relaxed);
    const std::uint64_t tail = slot_->returnedTail.load(std::memory_order_acquire);
    if (head == tail) return;
    const std::uint64_t capacity = slot_->returned.size();
    for (; head != tail; ++head) {
        const std::uint32_t entry = slot_->returned[head % capacity];
        slot_->held[entry] = Clip{}; // may let go of the last reference, here and never on the audio thread
        slot_->freeEntries.push_back(entry); // reserved for every entry: allocates nothing
    }
    slot_->returnedHead.store(head, std::memory_order_release);
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
    GYO_ASSERT(static_cast<bool>(request.clip));
    GYO_ASSERT(IsValidGain(request.gainLeft) && IsValidGain(request.gainRight));

    Reclaim();
    slot_->submitted.fetch_add(1, std::memory_order_relaxed);
    // An internal invariant of the mixer, not the caller's error (see SubmitterSlot).
    GYO_ASSERT(!slot_->freeEntries.empty());
    const std::uint32_t entry = slot_->freeEntries.back();
    slot_->freeEntries.pop_back();
    slot_->held[entry] = request.clip;
    const std::uint64_t handle = slot_->handlePrefix | (slot_->lastSequence + 1);
    if (!Enqueue(Command{CommandKind::Play, request.at, request.gainLeft, request.gainRight, request.clip.View(),
                         handle, entry})) {
        slot_->held[entry] = Clip{};
        slot_->freeEntries.push_back(entry);
        slot_->droppedQueueFull.fetch_add(1, std::memory_order_relaxed);
        return SoundHandle{};
    }
    ++slot_->lastSequence;
    return SoundHandle{handle};
}

bool Submitter::Stop(const SoundHandle sound, const Time::TimePoint at) {
    GYO_ASSERT(slot_ != nullptr);
    if (!sound.Valid()) {
        Reclaim();
        return true;
    }
    GYO_ASSERT((sound.value & ~Detail::kSequenceMask) == slot_->handlePrefix);
    const std::uint64_t sequence = sound.value & Detail::kSequenceMask;
    GYO_ASSERT(sequence != 0 && sequence <= slot_->lastSequence);
    Reclaim();
    Command command;
    command.kind = CommandKind::Stop;
    command.at = at;
    command.handle = sound.value;
    return SendStop(command);
}

bool Submitter::StopAll(const Time::TimePoint at) {
    GYO_ASSERT(slot_ != nullptr);
    Reclaim();
    Command command;
    command.kind = CommandKind::StopAll;
    command.at = at;
    command.handle = slot_->handlePrefix;
    return SendStop(command);
}

bool Submitter::SendStop(const Detail::Command& command) {
    slot_->stopsSubmitted.fetch_add(1, std::memory_order_relaxed);
    if (!Enqueue(command)) {
        slot_->stopsDroppedQueueFull.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

Base::Result<Mixer, AudioError> Mixer::Create(const MixerConfig& config) {
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
    auto voices = Detail::VoiceMixer::Create(config.maxVoices);
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
            if (command.kind == CommandKind::Play) {
                const std::int64_t offset = NsToFrames(ClampedOffset(Nanoseconds(command.at), anchorNs));
                const std::uint64_t lease = Detail::LeaseMark(i + 1, command.entry);
                const Detail::StartOutcome outcome = state.voices.ScheduleChecked(
                    Detail::ScheduledPlay{command.view, blockStart + offset + state.delayFrames, command.gainLeft,
                                          command.gainRight, command.handle, lease});
                if (outcome == Detail::StartOutcome::NoVoice) HandBack(state, lease);
                continue;
            }
            // A stop maps its time like a play (Design Invariant 3), on its own line.
            const std::int64_t stopFrame =
                blockStart + NsToFrames(ClampedOffset(Nanoseconds(command.at), anchorNs)) + state.delayFrames;
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
    PublishVoices(state);
}

Playhead Mixer::PlayheadOf(const SoundHandle sound) const noexcept {
    if (!state_ || !sound.Valid()) return Playhead{};
    std::uint64_t read = 0;
    if (!state_->board.Read(sound.value, read)) return Playhead{};
    const bool started = (read & PlayheadBoard<>::kStarted) != 0;
    return Playhead{started ? Playhead::Status::Playing : Playhead::Status::Scheduled,
                    static_cast<std::uint32_t>(read & 0xFFFF'FFFFu)};
}

} // namespace GYO::Engine::Audio
