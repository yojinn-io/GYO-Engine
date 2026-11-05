// The windowed shell (docs/framework/architecture.md, Design Invariant 6), on
// SDL's dummy video driver.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/framework/WindowShell.hpp"

#include "engine/base/Log.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace Gyo::Tools::Framework;
using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;
using Engine::Runtime::FrameContext;
using Engine::Runtime::RuntimeControl;

namespace {

WindowShellOptions SmallWindow() {
    WindowShellOptions options;
    options.platform.title = "window shell test";
    options.platform.width = 320;
    options.platform.height = 240;
    return options;
}

// Records what the shell calls, in order; stops after stopAt updates.
struct Record {
    std::vector<std::string> calls;
    std::vector<std::uint64_t> updateFrames;
    std::vector<std::uint64_t> renderFrames;
    bool videoUpInDestructor{};
    bool destroyed{};
};

class RecordingClient : public WindowShellClient {
public:
    RecordingClient(SdlPlatform& platform, Record& record, int stopAt, int exitCode)
        : platform_(platform), record_(record), stopAt_(stopAt), exitCode_(exitCode) {}
    ~RecordingClient() override {
        record_.videoUpInDestructor = (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0;
        record_.destroyed = true;
    }
    void OnEvent(const PlatformEvent& event) override {
        const SDL_Event& native = Engine::Platform::Sdl::NativeEvent(event);
        if (native.type == SDL_EVENT_USER) {
            record_.calls.push_back("event");
        }
    }
    RuntimeControl Update(const FrameContext& frame) override {
        record_.calls.push_back("update");
        record_.updateFrames.push_back(frame.frameIndex);
        ++updates_;
        return updates_ >= stopAt_ ? RuntimeControl::Stop : RuntimeControl::Continue;
    }
    RuntimeControl Render(const FrameContext& frame) override {
        record_.calls.push_back("render");
        record_.renderFrames.push_back(frame.frameIndex);
        return RuntimeControl::Continue;
    }
    int ExitCode() const noexcept override { return exitCode_; }

protected:
    SdlPlatform& platform_;
    Record& record_;
    int stopAt_;
    int exitCode_;
    int updates_{};
};

bool PushUserEvent(SdlPlatform& platform) {
    SDL_Event event{};
    event.type = SDL_EVENT_USER;
    return Engine::Platform::Sdl::PushInjectedEvent(platform, event);
}

} // namespace

TEST_CASE("W1: a platform that cannot start returns 1 without calling the factory") {
    WindowShellOptions options = SmallWindow();
    options.platform.width = 0;
    int factoryCalls = 0;
    const int code = RunWindowShell(options, [&](SdlPlatform&) -> std::unique_ptr<WindowShellClient> {
        ++factoryCalls;
        return nullptr;
    });
    CHECK(code == 1);
    CHECK(factoryCalls == 0);
}

TEST_CASE("W2: a factory that returns no client makes the shell return 1") {
    int factoryCalls = 0;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform&) -> std::unique_ptr<WindowShellClient> {
        ++factoryCalls;
        return nullptr;
    });
    CHECK(code == 1);
    CHECK(factoryCalls == 1);
}

TEST_CASE("W3, W4: the client's code is returned; Update and Render alternate on one frame; no Render after Stop") {
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<RecordingClient>(platform, record, 3, 5);
    });
    CHECK(code == 5);
    CHECK(record.calls == std::vector<std::string>{"update", "render", "update", "render", "update"});
    REQUIRE(record.updateFrames.size() == 3);
    REQUIRE(record.renderFrames.size() == 2);
    CHECK(record.updateFrames[0] == record.renderFrames[0]);
    CHECK(record.updateFrames[1] == record.renderFrames[1]);
    CHECK(record.updateFrames[0] != record.updateFrames[1]);
}

TEST_CASE("W5: an event pushed during Update reaches OnEvent before the next Update") {
    class Client final : public RecordingClient {
    public:
        using RecordingClient::RecordingClient;
        RuntimeControl Update(const FrameContext& frame) override {
            const RuntimeControl control = RecordingClient::Update(frame);
            if (updates_ == 1) {
                REQUIRE(PushUserEvent(platform_));
            }
            return control;
        }
    };
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<Client>(platform, record, 2, 0);
    });
    CHECK(code == 0);
    CHECK(record.calls == std::vector<std::string>{"update", "render", "event", "update"});
}

TEST_CASE("W6: the window's close request ends the loop with the client's code") {
    class Client final : public RecordingClient {
    public:
        using RecordingClient::RecordingClient;
        RuntimeControl Update(const FrameContext& frame) override {
            const RuntimeControl control = RecordingClient::Update(frame);
            if (updates_ == 1) {
                SDL_Event close{};
                close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
                close.window.windowID = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(platform_));
                REQUIRE(Engine::Platform::Sdl::PushInjectedEvent(platform_, close));
            }
            return control;
        }
    };
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<Client>(platform, record, 1000, 4);
    });
    CHECK(code == 4);
    CHECK(record.calls == std::vector<std::string>{"update", "render"});
}

TEST_CASE("W7: the client is destroyed while the platform still exists, and the platform after it") {
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<RecordingClient>(platform, record, 1, 0);
    });
    CHECK(code == 0);
    CHECK(record.destroyed);
    CHECK(record.videoUpInDestructor);
    CHECK((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0);
}

TEST_CASE("W8: an exception leaves the shell uncaught, and the client still goes before the platform") {
    class Client final : public RecordingClient {
    public:
        using RecordingClient::RecordingClient;
        RuntimeControl Update(const FrameContext&) override { throw std::runtime_error("update failed"); }
    };
    Record record;
    CHECK_THROWS_AS(static_cast<void>(RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
                        return std::make_unique<Client>(platform, record, 1, 0);
                    })),
                    std::runtime_error);
    CHECK(record.destroyed);
    CHECK(record.videoUpInDestructor);
    CHECK((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0);
}

TEST_CASE("W9: the shell runs no live frame: a live redraw request during a pump runs no Update") {
    class Client final : public RecordingClient {
    public:
        using RecordingClient::RecordingClient;
        void OnEvent(const PlatformEvent& event) override {
            const SDL_Event& native = Engine::Platform::Sdl::NativeEvent(event);
            if (native.type != SDL_EVENT_USER) {
                return;
            }
            // What SDL sends during a live resize: an exposure with data1 = 1.
            SDL_Event exposed{};
            exposed.type = SDL_EVENT_WINDOW_EXPOSED;
            exposed.window.windowID = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(platform_));
            exposed.window.data1 = 1;
            inside_ = true;
            REQUIRE(SDL_PushEvent(&exposed));
            inside_ = false;
        }
        RuntimeControl Update(const FrameContext& frame) override {
            if (inside_) {
                record_.calls.push_back("update inside a pump");
            }
            const RuntimeControl control = RecordingClient::Update(frame);
            if (updates_ == 1) {
                REQUIRE(PushUserEvent(platform_));
            }
            return control;
        }

    private:
        bool inside_{};
    };
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<Client>(platform, record, 3, 0);
    });
    CHECK(code == 0);
    CHECK(record.calls == std::vector<std::string>{"update", "render", "update", "render", "update"});
}

namespace {
void RecordingHandler(const Engine::Base::LogMessage&) noexcept {}
Engine::Base::LogHandler CurrentHandler() noexcept {
    const Engine::Base::LogHandler current = Engine::Base::SetLogHandler(nullptr);
    Engine::Base::SetLogHandler(current);
    return current;
}
} // namespace

TEST_CASE("W10: the shell installs no log handler") {
    class Client final : public RecordingClient {
    public:
        Client(SdlPlatform& platform, Record& record, Engine::Base::LogHandler& seen)
            : RecordingClient(platform, record, 1, 0), seen_(seen) {}
        RuntimeControl Update(const FrameContext& frame) override {
            seen_ = CurrentHandler();
            return RecordingClient::Update(frame);
        }

    private:
        Engine::Base::LogHandler& seen_;
    };
    const Engine::Base::LogHandler previous = Engine::Base::SetLogHandler(&RecordingHandler);
    Record record;
    Engine::Base::LogHandler seen{};
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<Client>(platform, record, seen);
    });
    const Engine::Base::LogHandler after = CurrentHandler();
    Engine::Base::SetLogHandler(previous);
    CHECK(code == 0);
    CHECK(seen == &RecordingHandler);
    CHECK(after == &RecordingHandler);
}

namespace {

// Pushes a close request of the window (or SDL's quit) in the given updates,
// and answers CloseRequested from answers in turn.
class ClosingClient final : public RecordingClient {
public:
    ClosingClient(SdlPlatform& platform, Record& record, std::vector<int> pushAt, std::vector<bool> answers, bool quit,
                  bool both = false)
        : RecordingClient(platform, record, 1000, 3), pushAt_(std::move(pushAt)), answers_(std::move(answers)), quit_(quit),
          both_(both) {}
    RuntimeControl Update(const FrameContext& frame) override {
        const RuntimeControl control = RecordingClient::Update(frame);
        if (std::find(pushAt_.begin(), pushAt_.end(), updates_) != pushAt_.end()) {
            SDL_Event close{};
            if (quit_) {
                close.type = SDL_EVENT_QUIT;
            } else {
                close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
                close.window.windowID = SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(platform_));
            }
            REQUIRE(Engine::Platform::Sdl::PushInjectedEvent(platform_, close));
            if (both_) {
                SDL_Event quit{};
                quit.type = SDL_EVENT_QUIT;
                REQUIRE(Engine::Platform::Sdl::PushInjectedEvent(platform_, quit));
            }
        }
        return control;
    }
    bool CloseRequested() override {
        record_.calls.push_back("close?");
        const bool answer = asked_ < answers_.size() ? answers_[asked_] : true;
        ++asked_;
        return answer;
    }

private:
    std::vector<int> pushAt_;
    std::vector<bool> answers_;
    bool quit_;
    bool both_;
    std::size_t asked_{};
};

std::vector<std::string> WithoutEvents(const std::vector<std::string>& calls) {
    std::vector<std::string> kept;
    for (const auto& call : calls) {
        if (call != "event") kept.push_back(call);
    }
    return kept;
}

} // namespace

TEST_CASE("Q1: a client that says no to a close request keeps the loop running; a yes ends it with its code") {
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<ClosingClient>(platform, record, std::vector<int>{1, 3}, std::vector<bool>{false, true}, false);
    });
    CHECK(code == 3);
    // Asked once per request: the first answer keeps two more frames, the second ends the loop.
    CHECK_MESSAGE(WithoutEvents(record.calls) ==
                      (std::vector<std::string>{"update", "render", "close?", "update", "render", "update", "render", "close?"}),
                  "the close request was not handed to the client, or its answer was not kept");
}

TEST_CASE("Q2: a quit request is asked like a close request") {
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<ClosingClient>(platform, record, std::vector<int>{1, 2}, std::vector<bool>{false, true}, true);
    });
    CHECK(code == 3);
    CHECK_MESSAGE(WithoutEvents(record.calls) ==
                      (std::vector<std::string>{"update", "render", "close?", "update", "render", "close?"}),
                  "the quit request ended the loop without asking the client");
}

TEST_CASE("Q1: a close request and a quit in one pump are asked once") {
    Record record;
    const int code = RunWindowShell(SmallWindow(), [&](SdlPlatform& platform) {
        return std::make_unique<ClosingClient>(platform, record, std::vector<int>{1, 2}, std::vector<bool>{false, true}, false, true);
    });
    CHECK(code == 3);
    CHECK_MESSAGE(WithoutEvents(record.calls) ==
                      (std::vector<std::string>{"update", "render", "close?", "update", "render", "close?"}),
                  "the client was asked more than once for one pump");
}
