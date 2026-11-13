#include "doctest/doctest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <thread>
#include <vector>

#include "PlayheadBoard.hpp"
#include "engine/audio/AudioFormat.hpp"
#include "engine/audio/Mixer.hpp"

using namespace Engine::Audio;
using namespace std::chrono_literals;
using Engine::Time::Duration;
using Engine::Time::TimePoint;

namespace {

const TimePoint kAnchor = TimePoint{} + std::chrono::duration_cast<Duration>(10s);

Duration Ns(const std::int64_t ns) { return std::chrono::duration_cast<Duration>(std::chrono::nanoseconds(ns)); }
Duration Ms(const std::int64_t ms) { return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(ms)); }

struct Rig final {
    Mixer mixer;
    Submitter submitter;
    ClipId clip; // 1000 frames
};

Rig MakeRig(const std::uint32_t voices = 4) {
    ClipBank bank;
    const std::vector<std::int16_t> samples(1000, 100);
    auto clip = bank.Add(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 1, kSampleRate);
    REQUIRE(clip.has_value());
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    config.maxVoices = voices;
    auto mixer = Mixer::Create(config, std::move(bank));
    REQUIRE(mixer.has_value());
    auto submitter = mixer->CreateSubmitter();
    REQUIRE(submitter.has_value());
    return Rig{std::move(mixer).value(), std::move(submitter).value(), *clip};
}

std::vector<std::int16_t> RenderBlock(Mixer& mixer, const std::uint32_t frames, const TimePoint anchor = kAnchor) {
    std::vector<std::int16_t> out(std::size_t{frames} * kChannels, 0);
    mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
    return out;
}

// A board whose stores and loads a test can pause at: before each load the
// reader calls onLoad; after each store the writer counts down and throws when
// its budget is spent, so a writer sequence can stop after any store.
struct StopWriter final {};
struct TestHook final {
    static constexpr bool kNoexcept = false;
    static inline int storesLeft = -1; // -1: unlimited
    static inline std::function<void()> onLoad;
    static void AfterStore() {
        if (storesLeft < 0) return;
        if (--storesLeft == 0) throw StopWriter{};
    }
    static void BeforeLoad() {
        if (onLoad) onLoad();
    }
};
using TestBoard = Detail::PlayheadBoard<TestHook>;
constexpr std::uint64_t kStarted = TestBoard::kStarted;

} // namespace

TEST_CASE("Audio Mixer reports a sound scheduled, playing, then not playing") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor + Ms(20)}); // frame 960
    REQUIRE(sound);
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::NotPlaying); // not taken yet
    static_cast<void>(RenderBlock(rig.mixer, 480));
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::Scheduled);
    CHECK(rig.mixer.PlayheadOf(sound).clipFrame == 0);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)));
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::Scheduled); // starts at the next frame
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(20)));
    const Playhead playing = rig.mixer.PlayheadOf(sound);
    CHECK(playing.status == Playhead::Status::Playing);
    CHECK(playing.clipFrame == 480);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(30)));
    CHECK(rig.mixer.PlayheadOf(sound).clipFrame == 960);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(40))); // the clip ends at 1000
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::NotPlaying);
}

TEST_CASE("Audio Mixer reports a stopped sound as not playing") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    static_cast<void>(RenderBlock(rig.mixer, 480));
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::Playing);
    REQUIRE(rig.submitter.Stop(sound, kAnchor));
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)));
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::NotPlaying);
}

TEST_CASE("Audio Mixer reports a position that only grows and never passes the clip") {
    Rig rig = MakeRig();
    // Starts at frame 60, so the first 61-frame block mixes exactly one frame.
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor + Ns(1'250'000)});
    std::uint32_t last = 0;
    for (int block = 0; block < 20; ++block) {
        static_cast<void>(RenderBlock(rig.mixer, 61, kAnchor + Ns(block * 1'270'833)));
        const Playhead now = rig.mixer.PlayheadOf(sound);
        if (block == 0) {
            CHECK(now.status == Playhead::Status::Playing);
            CHECK(now.clipFrame == 1);
        }
        if (now.status == Playhead::Status::NotPlaying) break;
        if (now.status == Playhead::Status::Scheduled) CHECK(now.clipFrame == 0);
        if (now.status == Playhead::Status::Playing) CHECK(now.clipFrame > 0);
        CHECK(now.clipFrame >= last);
        CHECK(now.clipFrame <= 1000);
        last = now.clipFrame;
    }
    CHECK(last > 0);
}

TEST_CASE("Audio Mixer reports an invalid handle or a moved-from Mixer as not playing") {
    Rig rig = MakeRig();
    // Idle voices carry tag 0; an invalid handle must not match them.
    static_cast<void>(RenderBlock(rig.mixer, 480));
    CHECK(rig.mixer.PlayheadOf(SoundHandle{}).status == Playhead::Status::NotPlaying);
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    static_cast<void>(RenderBlock(rig.mixer, 480));
    Mixer moved = std::move(rig.mixer);
    CHECK(moved.PlayheadOf(sound).status == Playhead::Status::Playing);
    CHECK(rig.mixer.PlayheadOf(sound).status == Playhead::Status::NotPlaying);
}

TEST_CASE("Audio Mixer reports each of several sounds in its own voice") {
    Rig rig = MakeRig(3);
    const SoundHandle a = rig.submitter.Play({rig.clip, kAnchor});
    const SoundHandle b = rig.submitter.Play({rig.clip, kAnchor + Ms(1)}); // frame 48
    const SoundHandle c = rig.submitter.Play({rig.clip, kAnchor + Ms(2)}); // frame 96, the last voice
    REQUIRE((a && b && c));
    static_cast<void>(RenderBlock(rig.mixer, 480));
    CHECK(rig.mixer.PlayheadOf(a).clipFrame == 480);
    CHECK(rig.mixer.PlayheadOf(b).clipFrame == 432);
    CHECK(rig.mixer.PlayheadOf(c).clipFrame == 384);
    CHECK(rig.mixer.PlayheadOf(c).status == Playhead::Status::Playing);
}

TEST_CASE("Audio Mixer reports a voice taken by another sound within one block") {
    // One voice: A is stopped at the block's first frame and B takes the voice
    // in the same block, so the voice changes sound without ever being idle at
    // the end of a Render.
    Rig rig = MakeRig(1);
    const SoundHandle a = rig.submitter.Play({rig.clip, kAnchor});
    static_cast<void>(RenderBlock(rig.mixer, 480));
    REQUIRE(rig.submitter.Stop(a, kAnchor + Ms(10)));
    const SoundHandle b = rig.submitter.Play({rig.clip, kAnchor + Ms(10)});
    REQUIRE(b);
    static_cast<void>(RenderBlock(rig.mixer, 480, kAnchor + Ms(10)));
    CHECK(rig.mixer.PlayheadOf(a).status == Playhead::Status::NotPlaying);
    const Playhead now = rig.mixer.PlayheadOf(b);
    CHECK(now.status == Playhead::Status::Playing);
    CHECK(now.clipFrame == 480); // A would be at 960
}

TEST_CASE("Audio Mixer reports a handle it never issued as not playing") {
    Rig rig = MakeRig();
    const SoundHandle sound = rig.submitter.Play({rig.clip, kAnchor});
    static_cast<void>(RenderBlock(rig.mixer, 480));
    REQUIRE(rig.mixer.PlayheadOf(sound).status == Playhead::Status::Playing);
    CHECK(rig.mixer.PlayheadOf(SoundHandle{sound.value + 1}).status == Playhead::Status::NotPlaying);
    CHECK(rig.mixer.PlayheadOf(SoundHandle{(std::uint64_t{64} << 57) | 1}).status == Playhead::Status::NotPlaying);
}

TEST_CASE("Audio Mixer output does not depend on someone reading playheads") {
    const auto run = [](const bool poll) {
        Rig rig = MakeRig(2);
        std::atomic<bool> done{false};
        std::vector<SoundHandle> sounds;
        for (int i = 0; i < 4; ++i) sounds.push_back(rig.submitter.Play({rig.clip, kAnchor + Ms(5 * i)}));
        std::thread reader([&] {
            while (poll && !done.load(std::memory_order_acquire)) {
                for (const SoundHandle sound : sounds) static_cast<void>(rig.mixer.PlayheadOf(sound));
            }
        });
        std::vector<std::int16_t> all;
        for (int block = 0; block < 10; ++block) {
            const auto out = RenderBlock(rig.mixer, 256, kAnchor);
            all.insert(all.end(), out.begin(), out.end());
        }
        done.store(true, std::memory_order_release);
        reader.join();
        return all;
    };
    CHECK(run(true) == run(false));
}

TEST_CASE("Audio playhead board never reports another sound's state, in any interleaving") {
    // One voice plays sound 1 at frame 100. The writer then reuses the voice,
    // either after clearing it (Clear, Assign, Publish: five stores) or
    // directly, as the mixer does when a voice ends and is taken again within a
    // block (Assign, Publish: four stores); sound 2 is assigned scheduled and
    // published playing at 7. The reader's three loads see the board after
    // n0 <= n1 <= n2 of those stores (every interleaving of one read with the
    // transition).
    constexpr std::uint64_t kOne = 0x11;
    constexpr std::uint64_t kTwo = 0x22;
    const std::uint64_t oneState = kStarted | 100;
    const std::uint64_t twoInit = 0;
    const std::uint64_t twoState = kStarted | 7;
    TestBoard board(1);
    bool viaClear = true;
    // The board as it is after the first n stores of the transition.
    const auto reach = [&](const int n) {
        TestHook::storesLeft = -1;
        board.Clear(0);
        board.Assign(0, kOne, oneState);
        if (n == 0) return;
        TestHook::storesLeft = n;
        try {
            if (viaClear) board.Clear(0);
            board.Assign(0, kTwo, twoInit);
            board.Publish(0, twoState);
        } catch (const StopWriter&) {
        }
        TestHook::storesLeft = -1;
    };

    int checked = 0;
    for (const bool clearFirst : {true, false}) {
        viaClear = clearFirst;
        const int kStores = clearFirst ? 5 : 4;
        const int kAssigned = clearFirst ? 4 : 3; // stores after which sound 2's tag is set
        for (int n0 = 0; n0 <= kStores; ++n0) {
            for (int n1 = n0; n1 <= kStores; ++n1) {
                for (int n2 = n1; n2 <= kStores; ++n2) {
                    for (const std::uint64_t tag : {kOne, kTwo}) {
                        CAPTURE(clearFirst);
                        CAPTURE(n0);
                        CAPTURE(n1);
                        CAPTURE(n2);
                        CAPTURE(tag);
                        // Before the reader's k-th load the board is as after steps[k]
                        // stores (the writer only stores, so reach() never pauses).
                        const int steps[] = {n0, n1, n2};
                        int load = 0;
                        TestHook::onLoad = [&] {
                            reach(steps[load < 2 ? load : 2]);
                            ++load;
                        };
                        std::uint64_t state = 0;
                        const bool found = board.Read(tag, state);
                        TestHook::onLoad = nullptr;
                        if (tag == kOne) {
                            // Sound 1 is reported with its own state, or not at all;
                            // once the Clear has happened before the read, not at all.
                            if (found) CHECK(state == oneState);
                            if (n0 >= 1) CHECK_FALSE(found); // its tag is cleared by the first store
                        } else {
                            // Sound 2 is reported with its own states only; once its
                            // assignment was complete before the read, always.
                            if (found) CHECK((state == twoInit || state == twoState));
                            if (n0 >= kAssigned) CHECK(found);
                        }
                        ++checked;
                    }
                }
            }
        }
    }
    CHECK(checked == 2 * (56 + 35));
}

// The two cases below share one workload: clips of three lengths, three voices,
// one new sound at a time and every fourth sound stopped 1 ms after it starts,
// so voices are reused all along. Every reported position of one sound only
// grows, stays within its own clip, and a sound never comes back once reported
// not playing after it played.
namespace {

constexpr std::uint32_t kReuseLengths[] = {150, 300, 600};

struct ReuseRig final {
    Mixer mixer;
    std::vector<ClipId> clips;
};

ReuseRig MakeReuseRig() {
    ClipBank bank;
    std::vector<ClipId> clips;
    for (const std::uint32_t frames : kReuseLengths) {
        const std::vector<std::int16_t> samples(frames, 100);
        clips.push_back(*bank.Add(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 1, kSampleRate));
    }
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    config.maxVoices = 3;
    auto created = Mixer::Create(config, std::move(bank));
    REQUIRE(created.has_value());
    return ReuseRig{std::move(created).value(), std::move(clips)};
}

// What one reader knows about each sound, and the checks on each read.
struct ReuseTrack final {
    std::vector<std::uint32_t> last;
    std::vector<char> ended;
    std::vector<char> playing; // read Playing at least once
    int bad = 0;
    std::uint64_t playingReads = 0;
    std::uint64_t sequenceReads = 0; // reads compared with an earlier read of the same sound

    explicit ReuseTrack(const std::size_t sounds) : last(sounds, 0), ended(sounds, 0), playing(sounds, 0) {}

    void Reset(const std::size_t s) {
        last[s] = 0;
        ended[s] = 0;
        playing[s] = 0;
    }

    void Check(const std::size_t s, const std::uint32_t length, const Playhead now) {
        if (now.status == Playhead::Status::NotPlaying) {
            if (last[s] > 0) ended[s] = 1;
            return;
        }
        if (now.status == Playhead::Status::Playing) {
            playing[s] = 1;
            ++playingReads;
        }
        if (ended[s] || last[s] > 0) ++sequenceReads;
        if (ended[s] || now.clipFrame < last[s] || now.clipFrame > length) ++bad;
        last[s] = now.clipFrame;
    }
};

} // namespace

TEST_CASE("Audio Mixer playheads stay consistent while voices are reused, block by block") {
    // Deterministic: one thread plays, renders and reads every issued sound
    // after each block. A sound every 4 blocks of 64 frames keeps at most three
    // sounding (the longest clip lasts 10 blocks), so none is dropped and every
    // sound that is not stopped is read Playing. Each sound starts at the first
    // frame of the block it is played in, so after k blocks a playing sound is
    // exactly at frame 64 k, and it is reported Playing exactly while it is not
    // stopped and 64 k is short of its clip: a position that belongs to another
    // sound, is not published, or a sound reported ended too early, shows here.
    constexpr int kSounds = 400;
    constexpr int kBlocksPerSound = 4;
    ReuseRig rig = MakeReuseRig();
    auto submitter = rig.mixer.CreateSubmitter();
    REQUIRE(submitter.has_value());
    std::vector<SoundHandle> sounds;
    std::vector<int> firstBlock;
    ReuseTrack track(kSounds);
    int wrongFrame = 0;
    int wrongStatus = 0;
    TimePoint anchor = kAnchor;
    std::vector<std::int16_t> out(std::size_t{64} * kChannels);
    int extra = 0;
    for (int block = 0;; ++block) {
        if (block % kBlocksPerSound == 0 && sounds.size() < std::size_t{kSounds}) {
            const std::size_t s = sounds.size();
            const SoundHandle sound = submitter->Play({rig.clips[s % 3], anchor});
            REQUIRE(sound);
            sounds.push_back(sound);
            firstBlock.push_back(block);
            if ((s + 1) % 4 == 0) REQUIRE(submitter->Stop(sound, anchor + Ms(1)));
        }
        rig.mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
        anchor += Ns(1'333'333);
        for (std::size_t s = 0; s < sounds.size(); ++s) {
            const Playhead now = rig.mixer.PlayheadOf(sounds[s]);
            track.Check(s, kReuseLengths[s % 3], now);
            const auto blocks = static_cast<std::uint32_t>(block - firstBlock[s] + 1);
            if (now.status == Playhead::Status::Playing && now.clipFrame != 64 * blocks) ++wrongFrame;
            // A stopped sound ends 48 frames into its first block; no clip length is a multiple of 64.
            const bool stopped = (s + 1) % 4 == 0;
            const bool expectPlaying = !stopped && 64 * blocks < kReuseLengths[s % 3];
            if ((now.status == Playhead::Status::Playing) != expectPlaying) ++wrongStatus;
        }
        if (sounds.size() == std::size_t{kSounds} && ++extra > 20) break; // the last clip ends within 10 blocks
    }
    int missed = 0;    // not stopped, yet never read Playing
    int notEnded = 0;  // still reported scheduled or playing at the end
    int played = 0;
    for (std::size_t s = 0; s < sounds.size(); ++s) {
        if ((s + 1) % 4 != 0 && !track.playing[s]) ++missed;
        if (rig.mixer.PlayheadOf(sounds[s]).status != Playhead::Status::NotPlaying) ++notEnded;
        if (track.playing[s]) ++played;
    }
    CHECK(rig.mixer.Stats().droppedNoVoice == 0);
    CHECK(missed == 0);
    CHECK(notEnded == 0);
    CHECK(played > 3); // more sounds than voices: voices were reused
    CHECK(track.bad == 0);
    CHECK(wrongFrame == 0);
    CHECK(wrongStatus == 0);
}

// The bound on one run of the stress case below. Under ThreadSanitizer every
// atomic and memory access is instrumented, and the same work took 11 to 19 s
// (slowest 19.4 s), so the bound there is three times the slowest. Other builds,
// CI among them, reach the counts in well under a second and keep 20 s. Chosen
// when the case is compiled, so a build without ThreadSanitizer never gets 60 s.
#if defined(__SANITIZE_THREAD__)
#define GYO_AUDIO_TEST_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define GYO_AUDIO_TEST_TSAN 1
#endif
#endif
#if defined(GYO_AUDIO_TEST_TSAN)
constexpr std::chrono::seconds kStressRunBound{60};
#else
constexpr std::chrono::seconds kStressRunBound{20};
#endif

TEST_CASE("Audio Mixer playheads stay consistent under a polling thread (ThreadSanitizer stress)") {
    // A ThreadSanitizer scene first: its own CTest (gyo_audio_playhead_stress).
    // The rendering thread owns the submitter and keeps playing a sound every
    // second 64-frame block (about 2.7 sounding on average, so a few are
    // dropped for want of a voice) and stopping every fourth, until the reader
    // has what it came for: kOverlapPolls polls made while the renderer runs
    // and kPlayingReads reads of a playing sound. Not reaching both within
    // kStressRunBound makes the run invalid, and it fails. The reader polls the last kWindow
    // sounds issued, with the checks of the case above. Handles live in a ring
    // the renderer never waits on; a slot the renderer may have reused since
    // the reader loaded it is skipped: all ring accesses are seq_cst and the
    // renderer raises `writing` before it stores a slot, so a load of the new
    // value is followed by a `writing` that shows it. Each execution makes three runs:
    // free, the reader polling first, and the renderer playing kHeadStart sounds
    // out first (the schedules that once left nothing checked). Declared for
    // ThreadSanitizer: 5 executions.
    // What this case guards: data races on the playhead path (ThreadSanitizer),
    // and that every single read, taken while voices are reused under it, is
    // within its clip. What it does not: that one sound's reads only grow and
    // never come back. Under ThreadSanitizer a poll outlasts the longest clip,
    // so a sound is seldom read twice ("sequence reads" counts the reads that
    // were compared with an earlier one, for information only). Those
    // properties rest on the case above, deterministic, and, while another
    // thread reads, on the board's exhaustive interleaving case and review.
    constexpr std::uint64_t kRing = 1024;
    constexpr std::uint64_t kWindow = 400; // as many sounds back as the case this replaced ever had
    constexpr std::uint64_t kOverlapPolls = 100000;
    constexpr std::uint64_t kPlayingReads = 1000;
    constexpr std::uint64_t kHeadStart = 1000;
    static_assert(kWindow < kRing);
    for (const int mode : {0, 1, 2}) {
        CAPTURE(mode); // 0 free, 1 reader first, 2 renderer first
        ReuseRig rig = MakeReuseRig();
        std::vector<std::atomic<std::uint64_t>> ring(kRing);
        std::atomic<std::uint64_t> writing{0}; // sounds whose slot the renderer has started to write
        std::atomic<std::uint64_t> issued{0};  // sounds whose slot is written
        std::atomic<std::uint64_t> polls{0};
        std::atomic<bool> active{false};
        std::atomic<bool> stop{false};
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + kStressRunBound;
        ReuseTrack track(kRing);
        std::vector<std::uint64_t> tracked(kRing, ~std::uint64_t{0}); // the sound each track slot follows
        std::uint64_t overlap = 0;
        double reachedMs = -1.0;
        std::thread reader([&] {
            std::vector<std::uint64_t> seen(kWindow);
            if (mode == 2) {
                while (issued.load() < kHeadStart && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            }
            for (std::uint64_t poll = 0;; ++poll) {
                const bool wasActive = active.load(std::memory_order_acquire);
                const std::uint64_t known = issued.load();
                // Newest first: under ThreadSanitizer a poll can take longer than a
                // clip plays, and the newest sounds are the ones still sounding.
                const std::uint64_t oldest = known > kWindow ? known - kWindow : 0;
                for (std::uint64_t n = known; n-- > oldest;) seen[known - 1 - n] = ring[n % kRing].load();
                const std::uint64_t written = writing.load(); // after every slot load above
                for (std::uint64_t n = known; n-- > oldest;) {
                    if (written > n + kRing) continue; // the slot may have held sound n + kRing when loaded
                    const SoundHandle sound{seen[known - 1 - n]};
                    const std::size_t slot = static_cast<std::size_t>(n % kRing);
                    if (tracked[slot] != n) {
                        tracked[slot] = n;
                        track.Reset(slot);
                    }
                    track.Check(slot, kReuseLengths[n % 3], rig.mixer.PlayheadOf(sound));
                }
                polls.store(poll + 1, std::memory_order_release);
                if (wasActive && active.load(std::memory_order_acquire)) ++overlap;
                if (overlap >= kOverlapPolls && track.playingReads >= kPlayingReads) {
                    reachedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
                    break;
                }
                if (poll % 256 == 0 && std::chrono::steady_clock::now() > deadline) break;
            }
            stop.store(true, std::memory_order_release);
        });
        if (mode == 1) {
            while (polls.load(std::memory_order_acquire) < kOverlapPolls && !stop.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
        auto submitter = rig.mixer.CreateSubmitter();
        REQUIRE(submitter.has_value());
        active.store(true, std::memory_order_release);
        TimePoint anchor = kAnchor;
        std::uint64_t next = 0;
        std::vector<std::int16_t> out(std::size_t{64} * kChannels);
        for (std::uint64_t block = 0; !stop.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline;
             ++block) {
            if (block % 2 == 0) {
                const SoundHandle sound = submitter->Play({rig.clips[static_cast<std::size_t>(next % 3)], anchor});
                writing.store(next + 1);
                ring[next % kRing].store(sound.value);
                ++next;
                issued.store(next);
                if (next % 4 == 0) static_cast<void>(submitter->Stop(sound, anchor + Ms(1)));
            }
            rig.mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
            anchor += Ns(1'333'333);
        }
        active.store(false, std::memory_order_release);
        stop.store(true, std::memory_order_release);
        reader.join();
        // Always printed: the time each platform takes to reach the counts is the
        // margin the thresholds are judged by.
        std::printf("playhead stress mode %d: reached in %.1f ms (-1: not reached, bound %lld s), overlap polls %llu, "
                    "playing reads %llu, sequence reads %llu, sounds issued %llu\n",
                    mode, reachedMs, static_cast<long long>(kStressRunBound.count()), static_cast<unsigned long long>(overlap),
                    static_cast<unsigned long long>(track.playingReads),
                    static_cast<unsigned long long>(track.sequenceReads), static_cast<unsigned long long>(next));
        CHECK(overlap >= kOverlapPolls);
        CHECK(track.playingReads >= kPlayingReads);
        CHECK(track.bad == 0);
    }
}
