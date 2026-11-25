#pragma once

// render's PresentMode to the swapchain's (window-mode-vsync declaration, section 4.3): the first mode of the chain
// the window supports, Bevy's fallbacks (bevy_render/src/view/window/mod.rs:891-905) without FIFO-relaxed (SDL_GPU
// has none). No SDL: the adapter asks SDL what the window supports and sets what this picks.

#include "engine/render/Presentation.hpp"
#include "plugins/sdl_gpu/SDLGPUPlugin.hpp"

namespace GYO::Plugins::SDLGPU {

[[nodiscard]] constexpr SwapchainPresentMode ChoosePresentMode(const Engine::Render::PresentMode wanted, const bool immediateSupported,
                                                               const bool mailboxSupported) noexcept {
    switch (wanted) {
    case Engine::Render::PresentMode::AutoVsync: return SwapchainPresentMode::Vsync;
    case Engine::Render::PresentMode::AutoNoVsync:
        if (immediateSupported) return SwapchainPresentMode::Immediate;
        if (mailboxSupported) return SwapchainPresentMode::Mailbox;
        return SwapchainPresentMode::Vsync;
    }
    return SwapchainPresentMode::Vsync;
}

} // namespace GYO::Plugins::SDLGPU
