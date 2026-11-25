#pragma once

// sdl_window's Product-level state (not public): the adapter, the queue, the requests last applied and the counts.
// Held by the unit (made in Configure, reset in OnDestroy); the World holds only a handle Resource pointing here.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/input/Input.hpp"
#include "engine/window/Window.hpp"

#include "EventQueue.hpp"
#include "WindowBackend.hpp"

namespace GYO::Plugins::SDLWindow {

// What SDL accepted last (the request side's own cache, not a copy of input's or window's state: SDL-7; Bevy's
// CachedWindow, bevy_winit/src/system.rs:302). A refused call leaves it, so the request is tried again next frame.
struct AppliedRequests final {
    Engine::Input::TextInputRequest textInput{};
    std::string title;
    bool resizable = true;
    Engine::Window::CursorIcon cursorIcon = Engine::Window::CursorIcon::Default;
    bool cursorVisible = true;
    Engine::Window::CursorGrab cursorGrab = Engine::Window::CursorGrab::None;
};

class Service final {
public:
    Service(std::unique_ptr<WindowBackend> backend, std::size_t inputCapacity, std::size_t windowCapacity,
            const Engine::Window::WindowSettings& settings)
        : backend_(std::move(backend)), queue_(inputCapacity, windowCapacity) {
        applied_.title = settings.title;
        applied_.resizable = settings.resizable;
    }

    [[nodiscard]] WindowBackend* Backend() noexcept { return backend_.get(); }
    [[nodiscard]] EventQueue& Queue() noexcept { return queue_; }
    [[nodiscard]] AppliedRequests& Applied() noexcept { return applied_; }
    // The one-shot requests sent in a live frame (nothing is asked of the window inside the OS's modal loop): kept
    // for the next frame outside it, in order (the output side's own cache, SDL-7).
    struct DeferredRequests final {
        std::vector<Engine::Window::WarpCursor> warps;
        std::vector<Engine::Window::SetWindowMode> modes;
        std::vector<Engine::Window::ResizeWindow> resizes;
        std::vector<Engine::Window::CopyToClipboard> copies;
    };
    [[nodiscard]] DeferredRequests& Deferred() noexcept { return deferred_; }

    void Pump() {
        if (backend_) backend_->Pump(queue_);
    }
    // The os shutdown request, once: it reaches the game as a close request (source Shutdown), like the window's
    // close button, so the game can save first (the runner looks at os's flag, never at the events). Seen only once
    // it is in the queue: a full queue tries again next frame.
    void ShutdownRequested(Engine::Time::TimePoint now) {
        if (shutdownSeen_) return;
        Translated translated;
        translated.window.emplace_back(Engine::Window::WindowCloseRequested{Engine::Window::CloseSource::Shutdown, now});
        shutdownSeen_ = queue_.Accept(std::move(translated));
    }
    void Close() {
        if (backend_) backend_->Close();
        closed_ = true;
    }
    [[nodiscard]] bool Closed() const noexcept { return closed_; }
    void Refused(std::string text) {
        ++refused_;
        lastRefusal_ = std::move(text);
    }
    [[nodiscard]] std::uint64_t RefusedRequests() const noexcept { return refused_; }
    [[nodiscard]] const std::string& LastRefusal() const noexcept { return lastRefusal_; }
    // The Product's end (OnDestroy): the window and SDL's video go.
    void Shutdown() { backend_.reset(); }

private:
    std::unique_ptr<WindowBackend> backend_;
    EventQueue queue_;
    AppliedRequests applied_;
    DeferredRequests deferred_;
    bool closed_ = false;
    bool shutdownSeen_ = false;
    std::uint64_t refused_ = 0;
    std::string lastRefusal_;
};

// The World's handle (registered by Build; bound to the thread that puts it, DI-KS 14). Hashes nothing: it points at
// a service outside the World (as world_asset's AssetRequests). The two flags: the World has not had its first
// round yet, so its writers first write the present state (SDL-7 exception (2)).
struct SDLWindowHandle final {
    Service* service = nullptr;
    bool inputSnapshotPending = true;
    bool windowSnapshotPending = true;
};
inline void HashValue(Kernel::Hasher&, const SDLWindowHandle&) {}

} // namespace GYO::Plugins::SDLWindow
