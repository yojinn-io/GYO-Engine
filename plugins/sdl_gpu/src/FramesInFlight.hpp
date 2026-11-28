#pragma once

// The frames handed to the GPU whose fence was not seen done yet (render-pipeline declaration 15.3, 15.9), and the
// latest one seen done. A fence has no time of its own, so each look without waiting narrows when a frame was done:
// after the last look that found it not done, at most at the look that found it done. No SDL here: the adapter
// (src/adapters/sdl/GPUDevice.cpp) gives its fences and how to query and let go of one; the tests give numbers.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/time/MonotonicClock.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

namespace GYO::Plugins::SDLGPU {

template <class Fence>
class FramesInFlight final {
public:
    // The frame was handed to the GPU at handed, with its fence (the account holds it until it is seen done).
    void Hand(const std::uint64_t frame, const Fence fence, const Engine::Time::TimePoint handed) {
        frames_.push_back(Entry{frame, fence, handed, Engine::Time::Duration{0}});
    }

    // Looks at each fence at now, without waiting (isDone(fence)): one done gives its frame as seen done and is let go
    // (release(fence)); one not done keeps now as its last look not done. The latest frame seen done is kept. A fence
    // that never reads done (a lost device: SDL's query is false on an error too) stays until Clear.
    template <class IsDone, class Release>
    void Look(const Engine::Time::TimePoint now, IsDone&& isDone, Release&& release) {
        std::size_t kept = 0;
        for (Entry& e : frames_) {
            if (!isDone(e.fence)) {
                e.notDoneAfter = now - e.handed;
                frames_[kept++] = e;
                continue;
            }
            if (!seen_ || seen_->frame < e.frame) seen_ = FrameSeenDone{e.frame, e.notDoneAfter, now - e.handed};
            release(e.fence);
        }
        frames_.resize(kept);
    }

    // Lets every fence still in flight go (the device's end).
    template <class Release>
    void Clear(Release&& release) {
        for (const Entry& e : frames_) release(e.fence);
        frames_.clear();
    }

    [[nodiscard]] const std::optional<FrameSeenDone>& LastSeenDone() const noexcept { return seen_; }
    [[nodiscard]] std::size_t InFlight() const noexcept { return frames_.size(); }

private:
    struct Entry {
        std::uint64_t frame;
        Fence fence;
        Engine::Time::TimePoint handed;
        // Since its handing, at the last look that found it not done (0: no look did).
        Engine::Time::Duration notDoneAfter;
    };
    std::vector<Entry> frames_;
    std::optional<FrameSeenDone> seen_;
};

} // namespace GYO::Plugins::SDLGPU
