#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace Engine::Audio::Detail {

// Wait-free reads and writes need lock-free 64-bit atomics.
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

// No pause points: the production board.
struct NoBoardHook final {
    static constexpr bool kNoexcept = true;
    static void AfterStore() noexcept {}
    static void BeforeLoad() noexcept {}
};

// Where each voice is, published by the audio thread for display (Design
// Invariant 4): per voice, the handle it plays (0 for none) and its state. Wait-
// free on both sides. The audio thread only writes it; any thread reads it.
//
// The writer assigns a voice by clearing its tag (relaxed: the release store
// of the state that follows carries it), then writing the initial state, then
// the tag, with release. A reader that sees the tag reads the state and the
// tag again: a state read after the tag of one sound belongs to that sound or
// a later one, and a later one has cleared the tag first, so the second read no
// longer matches. Tags (handles) are never reused, and a tag only ever lives in
// one voice (a sound never moves to another voice), so a reader finds it there.
//
// Hook pauses the board between stores and before loads, so a test can
// interleave the two sides deterministically (it may also throw there).
template <class Hook = NoBoardHook>
class PlayheadBoard final {
public:
    // State bits: the clip frames mixed in the low 32 bits, kStarted when the
    // voice has started.
    static constexpr std::uint64_t kStarted = std::uint64_t{1} << 32;

    explicit PlayheadBoard(const std::uint32_t voices) : entries_(voices) {}

    [[nodiscard]] std::uint32_t Voices() const noexcept { return static_cast<std::uint32_t>(entries_.size()); }

    // The writer (the audio thread).
    void Assign(const std::uint32_t voice, const std::uint64_t tag, const std::uint64_t state) noexcept(Hook::kNoexcept) {
        Entry& entry = entries_[voice];
        entry.tag.store(0, std::memory_order_relaxed);
        Hook::AfterStore();
        entry.state.store(state, std::memory_order_release);
        Hook::AfterStore();
        entry.tag.store(tag, std::memory_order_release);
        Hook::AfterStore();
    }
    void Publish(const std::uint32_t voice, const std::uint64_t state) noexcept(Hook::kNoexcept) {
        entries_[voice].state.store(state, std::memory_order_release);
        Hook::AfterStore();
    }
    void Clear(const std::uint32_t voice) noexcept(Hook::kNoexcept) {
        entries_[voice].tag.store(0, std::memory_order_release);
        Hook::AfterStore();
    }

    // Any thread. The state of the voice playing tag, or false when none does.
    [[nodiscard]] bool Read(const std::uint64_t tag, std::uint64_t& state) const noexcept(Hook::kNoexcept) {
        for (const Entry& entry : entries_) {
            Hook::BeforeLoad();
            if (entry.tag.load(std::memory_order_acquire) != tag) continue;
            Hook::BeforeLoad();
            const std::uint64_t read = entry.state.load(std::memory_order_acquire);
            Hook::BeforeLoad();
            if (entry.tag.load(std::memory_order_acquire) != tag) return false; // reassigned: the sound ended
            state = read;
            return true;
        }
        return false;
    }

private:
    struct Entry final {
        std::atomic<std::uint64_t> tag{0};
        std::atomic<std::uint64_t> state{0};
    };
    std::vector<Entry> entries_;
};

} // namespace Engine::Audio::Detail
