#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/input/PhysicalInputFrame.hpp"
#include "platform/sdl/SdlPlatform.hpp"

#include <SDL3/SDL_events.h>

namespace Engine::Input::Backend::Sdl {

enum class SdlInputErrorCode {
    None = 0,
    RelativeMouseModeFailed,
};

using SdlInputError = Base::Error<SdlInputErrorCode>;

class SdlInput final {
public:
    explicit SdlInput(Platform::Sdl::SdlPlatform& platform) noexcept;

    void BeginFrame() noexcept;
    void HandleEvent(const SDL_Event& event) noexcept;
    void EndFrame() noexcept;

    [[nodiscard]] Base::Result<void, SdlInputError>
    SetRelativeMouseMode(bool enabled);

    [[nodiscard]] const PhysicalInputFrame& Snapshot() const noexcept;

private:
    void SetKey(Key key, bool down) noexcept;
    void SetMouseButton(MouseButton button, bool down) noexcept;
    void ReleaseAll() noexcept;

    Platform::Sdl::SdlPlatform* platform_{};
    PhysicalInputFrame frame_{};
    bool suppressRelativeMotion_{};
};

} // namespace Engine::Input::Backend::Sdl
