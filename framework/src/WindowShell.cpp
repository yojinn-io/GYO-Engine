#include "gyo/framework/WindowShell.hpp"

#include <string>
#include <utility>

#include "engine/base/Assert.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/runtime/IRuntimeClient.hpp"
#include "engine/runtime/RuntimeLoop.hpp"

namespace Gyo::Tools::Framework {
namespace {

using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;
using Engine::Runtime::FrameContext;
using Engine::Runtime::RuntimeControl;

constexpr const char* kLogCategory = "framework.shell";

// Hands the loop's phases to the client; the pump's observer is OnEvent.
class LoopAdapter final : public Engine::Runtime::IRuntimeClient {
public:
    LoopAdapter(SdlPlatform& platform, WindowShellClient& client) noexcept : platform_(platform), client_(client) {}

    // PumpEvents stops only for a close request or quit: the client may keep the window open.
    RuntimeControl ProcessEvents(const FrameContext&) override {
        const RuntimeControl control =
            platform_.PumpEvents([this](const PlatformEvent& event) { client_.OnEvent(event); });
        if (control == RuntimeControl::Stop && !client_.CloseRequested()) return RuntimeControl::Continue;
        return control;
    }
    RuntimeControl Update(const FrameContext& frame) override { return client_.Update(frame); }
    RuntimeControl Render(const FrameContext& frame) override { return client_.Render(frame); }

private:
    SdlPlatform& platform_;
    WindowShellClient& client_;
};

} // namespace

int RunWindowShell(const WindowShellOptions& options, const WindowShellClientFactory& factory) {
    GYO_ASSERT(factory);
    const std::string& title = options.platform.title;
    auto created = SdlPlatform::Create(options.platform);
    if (!created) {
        Engine::Base::Log(Engine::Base::LogLevel::Error, kLogCategory,
                          title + ": platform initialization failed: " + Engine::Base::Describe(created.error()));
        return 1;
    }
    // Declared before the client, so the client goes first, also while an
    // exception unwinds.
    std::unique_ptr<SdlPlatform> platform = std::move(created).value();
    std::unique_ptr<WindowShellClient> client = factory(*platform);
    if (client == nullptr) {
        Engine::Base::Log(Engine::Base::LogLevel::Error, kLogCategory, title + ": the client could not start");
        return 1;
    }
    LoopAdapter adapter(*platform, *client);
    Engine::Runtime::RuntimeLoop loop(adapter);
    loop.Run();
    const int exitCode = client->ExitCode();
    client.reset();
    return exitCode;
}

} // namespace Gyo::Tools::Framework
