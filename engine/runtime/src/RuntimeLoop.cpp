#include "engine/runtime/RuntimeLoop.hpp"

namespace Engine::Runtime {

namespace {

// Clears a flag when the scope ends, also when the client throws.
class FlagScope final {
public:
    explicit FlagScope(bool& flag) noexcept : flag_(&flag) { *flag_ = true; }
    ~FlagScope() { *flag_ = false; }

    FlagScope(const FlagScope&) = delete;
    FlagScope& operator=(const FlagScope&) = delete;

private:
    bool* flag_;
};

} // namespace

RuntimeLoop::RuntimeLoop(IRuntimeClient& client) noexcept
    : client_(&client) {}

void RuntimeLoop::Run() {
    // Each run starts at frame zero, as a fresh loop.
    frameIndex_ = 0;
    hasPreviousPair_ = false;
    stopRequested_ = false;

    for (;;) {
        const auto frameStart = Clock::now();
        const FrameContext frame = MakeFrame(frameStart);

        liveFramesThisPump_ = 0;
        RuntimeControl control = RuntimeControl::Continue;
        {
            const FlagScope processing(processingEvents_);
            control = client_->ProcessEvents(frame);
        }
        if (control == RuntimeControl::Stop || stopRequested_) {
            break;
        }

        // After live frames the regular pair starts now, with the next index.
        if (liveFramesThisPump_ == 0) {
            control = RunPair(frame, frameStart);
        } else {
            const auto pairStart = Clock::now();
            control = RunPair(MakeFrame(pairStart), pairStart);
        }
        if (control == RuntimeControl::Stop) {
            break;
        }
    }
}

bool RuntimeLoop::RunLiveFrame() {
    if (!processingEvents_ || inLiveFrame_ || stopRequested_) {
        return false;
    }

    const FlagScope live(inLiveFrame_);
    ++liveFramesThisPump_;
    const auto pairStart = Clock::now();
    if (RunPair(MakeFrame(pairStart), pairStart) == RuntimeControl::Stop) {
        stopRequested_ = true;
    }
    return true;
}

FrameContext RuntimeLoop::MakeFrame(const Clock::time_point start) const noexcept {
    const double deltaSeconds = hasPreviousPair_
        ? std::chrono::duration<double>(start - previousPairStart_).count()
        : 0.0;
    return FrameContext{
        .frameIndex = frameIndex_,
        .deltaSeconds = deltaSeconds,
    };
}

RuntimeControl RuntimeLoop::RunPair(const FrameContext& frame, const Clock::time_point start) {
    previousPairStart_ = start;
    hasPreviousPair_ = true;

    if (client_->Update(frame) == RuntimeControl::Stop) {
        return RuntimeControl::Stop;
    }
    const RuntimeControl control = client_->Render(frame);
    ++frameIndex_;
    return control;
}

} // namespace Engine::Runtime
