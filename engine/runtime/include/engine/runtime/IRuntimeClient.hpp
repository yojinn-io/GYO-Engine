#pragma once

#include "engine/runtime/FrameContext.hpp"
#include "engine/runtime/RuntimeControl.hpp"

namespace Engine::Runtime {

class IRuntimeClient {
public:
    virtual ~IRuntimeClient() = default;

    [[nodiscard]] virtual RuntimeControl ProcessEvents(const FrameContext& frame) = 0;
    [[nodiscard]] virtual RuntimeControl Update(const FrameContext& frame) = 0;
    [[nodiscard]] virtual RuntimeControl Render(const FrameContext& frame) = 0;
};

} // namespace Engine::Runtime
