#pragma once

#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/input/PhysicalInputFrame.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

namespace Engine::Input::Backend::Sdl {

// Zero is not a valid code; the numeric values are not a data contract.
enum class SdlInputErrorCode {
    RelativeMouseModeFailed = 1,
    TextInputFailed,
    InjectionFailed,
};

[[nodiscard]] constexpr const char* ToString(const SdlInputErrorCode code) noexcept {
    switch (code) {
    case SdlInputErrorCode::RelativeMouseModeFailed: return "RelativeMouseModeFailed";
    case SdlInputErrorCode::TextInputFailed: return "TextInputFailed";
    case SdlInputErrorCode::InjectionFailed: return "InjectionFailed";
    }
    return "Unknown";
}

using SdlInputError = Base::Error<SdlInputErrorCode>;
static_assert(Base::CodedError<SdlInputError>);

class SdlInput final {
public:
    explicit SdlInput(Platform::Sdl::SdlPlatform& platform) noexcept;

    void BeginFrame() noexcept;
    // An event from SdlPlatform::PumpEvents. Code that builds native events
    // itself uses HandleNativeEvent (SdlInputNative.hpp).
    void HandleEvent(const Platform::Sdl::PlatformEvent& event) noexcept;
    void EndFrame() noexcept;

    [[nodiscard]] Base::Result<void, SdlInputError>
    SetRelativeMouseMode(bool enabled);

    // Turns this window's text input on or off. While it is on, the frame's
    // text collects typed text and key presses (docs/architecture/input.md).
    // Whether it is on is read from the platform each time, not kept here.
    [[nodiscard]] Base::Result<void, SdlInputError>
    SetTextInput(bool enabled);

    // Diagnostics only (v8-tool-ui-ime-observe): while on, every text input
    // switch, composition (TEXT_EDITING and its candidates), typed text and
    // text key this backend receives is written to Engine::Base::Log under
    // the category "input.text", with whether the frame took it. The frame is
    // the same either way. Off by default.
    void SetTextDiagnostics(bool enabled) noexcept;

    [[nodiscard]] const PhysicalInputFrame& Snapshot() const noexcept;

private:
    void SetKey(Key key, bool down) noexcept;
    void SetMouseButton(MouseButton button, bool down) noexcept;
    void ReleaseAll() noexcept;

    Platform::Sdl::SdlPlatform* platform_{};
    PhysicalInputFrame frame_{};
    bool suppressRelativeMotion_{};
    bool textDiagnostics_{};
};

} // namespace Engine::Input::Backend::Sdl
