#pragma once

// How the main target's pictures are shown (docs/architecture/target/v2/render.zh-Hant.md, "Presentation"; the
// window-mode-vsync declaration, section 4.3): render_world's Resource, read by a backend every frame (sdl_gpu sets
// its swapchain's present mode when it changes). Data, no backend type (Design Invariant RD-1), as Bevy's
// Window::present_mode (bevy_window/src/window.rs:1229) but in the render World: GYO's window is not an entity and the
// render World has no window, while the swapchain belongs to the renderer. A game whose settings live in another
// World copies them here in the render World's Extract (ExtractRes); a single World writes it directly.

#include <cstdint>

#include "engine/kernel/Hash.hpp"

namespace GYO::Engine::Render {

// Bevy's PresentMode, its two values that fall back by themselves (bevy_render/src/view/window/mod.rs:882-911); a
// backend picks the first of the chain the target supports.
enum class PresentMode : std::uint8_t {
    // Waits for the display: no tearing, the frame rate held to the display's (VSync on). Every target supports it.
    AutoVsync = 0,
    // Does not wait for the display (VSync off): immediate (may tear), else mailbox (the newest picture at the
    // display's refresh, no tearing), else AutoVsync.
    AutoNoVsync,
};

[[nodiscard]] constexpr const char* ToString(const PresentMode mode) noexcept {
    switch (mode) {
    case PresentMode::AutoVsync: return "AutoVsync";
    case PresentMode::AutoNoVsync: return "AutoNoVsync";
    }
    return "Unknown";
}

// The main target's (target 0: the window's) presentation; a new World's is the default.
struct Presentation final {
    PresentMode presentMode = PresentMode::AutoVsync;

    friend bool operator==(const Presentation&, const Presentation&) = default;
};

// The render World is not decided state (as RenderCommands).
inline void HashValue(Kernel::Hasher&, const Presentation&) {}

} // namespace GYO::Engine::Render
