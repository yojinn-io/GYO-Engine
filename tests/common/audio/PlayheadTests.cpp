#include "doctest/doctest.h"

#include <atomic>
#include <chrono>
#include <cstdint>
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

TEST_CASE("Audio Mixer playheads stay consistent while another thread polls them") {
    // Declared for ThreadSanitizer: 5 runs of 10^5 polls each (one run per
    // test execution), no time bound. Every reported position of one sound only
    // grows and stays within its clip.
    // The rendering thread also owns the submitter: it keeps playing clips of
    // different lengths and stopping some, so voices are reused all along. The
    // reader polls every handle issued so far and checks that each one's
    // position never passes its own clip, only grows, and that a sound never
    // comes back once reported not playing after it played.
    constexpr int kSounds = 400;
    ClipBank bank;
    std::vector<ClipId> clips;
    for (const std::uint32_t frames : {150u, 300u, 600u}) {
        const std::vector<std::int16_t> samples(frames, 100);
        clips.push_back(*bank.Add(Engine::Base::ConstSpan<std::int16_t>(samples.data(), samples.size()), 1, kSampleRate));
    }
    MixerConfig config;
    config.scheduleDelay = Duration::zero();
    config.maxVoices = 3;
    auto created = Mixer::Create(config, std::move(bank));
    REQUIRE(created.has_value());
    Mixer mixer = std::move(created).value();
    std::vector<std::atomic<std::uint64_t>> handles(kSounds);
    std::atomic<int> issued{0};
    std::atomic<bool> done{false};
    std::atomic<int> bad{0};
    std::thread reader([&] {
        const std::uint32_t lengths[] = {150, 300, 600};
        std::vector<std::uint32_t> last(kSounds, 0);
        std::vector<bool> ended(kSounds, false);
        for (int poll = 0; poll < 100000; ++poll) {
            const int known = issued.load(std::memory_order_acquire);
            for (int s = 0; s < known; ++s) {
                const SoundHandle sound{handles[s].load(std::memory_order_relaxed)};
                if (!sound) continue;
                const Playhead now = mixer.PlayheadOf(sound);
                if (now.status == Playhead::Status::NotPlaying) {
                    if (last[s] > 0) ended[s] = true;
                    continue;
                }
                if (ended[s] || now.clipFrame < last[s] || now.clipFrame > lengths[s % 3]) ++bad;
                last[s] = now.clipFrame;
            }
        }
        done.store(true, std::memory_order_release);
    });
    auto submitter = mixer.CreateSubmitter();
    REQUIRE(submitter.has_value());
    TimePoint anchor = kAnchor;
    int next = 0;
    std::vector<std::int16_t> out(std::size_t{64} * kChannels);
    while (!done.load(std::memory_order_acquire)) {
        if (next < kSounds) {
            const SoundHandle sound = submitter->Play({clips[static_cast<std::size_t>(next % 3)], anchor});
            handles[static_cast<std::size_t>(next)].store(sound.value, std::memory_order_relaxed);
            ++next;
            issued.store(next, std::memory_order_release);
            if (next % 4 == 0) static_cast<void>(submitter->Stop(sound, anchor + Ms(1)));
        }
        mixer.Render(Engine::Base::Span<std::int16_t>(out.data(), out.size()), anchor);
        anchor += Ns(1'333'333);
    }
    reader.join();
    CHECK(next > 0);
    CHECK(bad.load() == 0);
}
