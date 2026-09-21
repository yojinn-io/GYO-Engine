#include "engine/runtime/RuntimeLoop.hpp"

#include <chrono>
#include <cstdint>

namespace Engine::Runtime {

RuntimeLoop::RuntimeLoop(IRuntimeClient& client) noexcept
    : client_(&client) {}

void RuntimeLoop::Run() {
    using Clock = std::chrono::steady_clock;

    std::uint64_t frameIndex = 0;
    auto previousFrameTime = Clock::now();
    bool isFirstFrame = true;

    for (;;) {
        const auto currentFrameTime = Clock::now();
        const double deltaSeconds = isFirstFrame
            ? 0.0
            : std::chrono::duration<double>(currentFrameTime - previousFrameTime).count();

        previousFrameTime = currentFrameTime;
        isFirstFrame = false;

        const FrameContext frame{
            .frameIndex = frameIndex,
            .deltaSeconds = deltaSeconds,
        };

        if (client_->ProcessEvents(frame) == RuntimeControl::Stop) {
            break;
        }

        if (client_->Update(frame) == RuntimeControl::Stop) {
            break;
        }

        if (client_->Render(frame) == RuntimeControl::Stop) {
            break;
        }

        ++frameIndex;
    }
}

} // namespace Engine::Runtime
