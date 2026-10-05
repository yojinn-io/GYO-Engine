#pragma once

#include <chrono>
#include <cstdint>

#include "engine/runtime/FrameContext.hpp"
#include "engine/runtime/IRuntimeClient.hpp"

namespace Engine::Runtime {

// Single-threaded loop: ProcessEvents, Update, Render, repeated until a phase
// returns Stop.
//
// Live frames: while ProcessEvents has not returned, the platform may be held
// inside an OS modal loop (a live window resize on macOS; a window move or
// resize, or an open system menu, on Windows). RunLiveFrame lets the platform run Update and Render from
// there. A frame index belongs to one Update/Render pair; ProcessEvents carries
// the index of the pair that follows it. Without live frames each frame's three
// phases share one FrameContext, exactly as before. When live frames run, the
// pending ProcessEvents context is followed by pairs with consecutive indices:
// each live frame, then the regular pair after ProcessEvents returns. Each
// pair's delta is the time since the previous pair started (zero for the first
// pair), so the deltas of all pairs add up to the elapsed time.
class RuntimeLoop final {
public:
    explicit RuntimeLoop(IRuntimeClient& client) noexcept;

    void Run();

    // Runs Update and Render once while ProcessEvents is in progress. Call it
    // on the thread running Run. ProcessEvents is not called again: the pair
    // sees no new input. A Stop from the pair ends Run once ProcessEvents
    // returns, and later calls do nothing. Returns false without running
    // anything outside ProcessEvents, inside another live frame, or after a
    // Stop.
    bool RunLiveFrame();

private:
    using Clock = std::chrono::steady_clock;

    [[nodiscard]] FrameContext MakeFrame(Clock::time_point start) const noexcept;
    [[nodiscard]] RuntimeControl RunPair(const FrameContext& frame, Clock::time_point start);

    IRuntimeClient* client_;
    std::uint64_t frameIndex_{};
    Clock::time_point previousPairStart_{};
    bool hasPreviousPair_{};
    bool processingEvents_{};
    bool inLiveFrame_{};
    bool stopRequested_{};
    std::uint64_t liveFramesThisPump_{};
};

} // namespace Engine::Runtime
