#pragma once

#include <cstdint>

namespace Engine::Runtime {

struct FrameContext {
    std::uint64_t frameIndex{};
    double deltaSeconds{};
};

} // namespace Engine::Runtime
