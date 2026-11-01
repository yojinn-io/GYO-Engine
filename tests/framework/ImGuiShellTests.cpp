// The ImGui binding (docs/framework/architecture.md, Design Invariants 4 and
// 6), on SDL's dummy video driver and software renderer, in a working
// directory of its own.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "gyo/framework/imgui/ImGuiShell.hpp"

#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

#include <imgui.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

using namespace Gyo::Tools::Framework;

namespace {

WindowShellOptions SmallWindow() {
    WindowShellOptions options;
    options.platform.title = "imgui shell test";
    options.platform.width = 640;
    options.platform.height = 480;
    return options;
}

struct Counts {
    int initialize{};
    int beginFrame{};
    int draw{};
    int endFrame{};
    bool contextAtInitialize{};
    bool contextInDestructor{true};
    bool rendererInDestructor{};
    bool destroyed{};
};

// Draws one window per frame and wants to exit after exitAfter frames
// (never when 0). Hooks let a case act at a given point.
class Client : public ImGuiShellClient {
public:
    Client(const ImGuiShellContext& context, Counts& counts, int exitAfter)
        : context_(context), counts_(counts), exitAfter_(exitAfter) {}
    ~Client() override {
        counts_.contextInDestructor = ImGui::GetCurrentContext() != nullptr;
        counts_.rendererInDestructor =
            SDL_GetRenderer(Engine::Platform::Sdl::NativeWindow(context_.platform)) != nullptr;
        counts_.destroyed = true;
    }
    bool Initialize(const ImGuiShellContext&) override {
        ++counts_.initialize;
        counts_.contextAtInitialize = ImGui::GetCurrentContext() != nullptr;
        return initializeResult;
    }
    void BeginFrame() override {
        ++counts_.beginFrame;
        if (onBeginFrame) {
            onBeginFrame(counts_.beginFrame);
        }
    }
    void Draw() override {
        ++counts_.draw;
        ImGui::Begin("shell test");
        ImGui::TextUnformatted("text");
        ImGui::End();
        if (onDraw) {
            onDraw(counts_.draw);
        }
    }
    bool WantsExit() const override { return exitAfter_ != 0 && counts_.draw >= exitAfter_; }
    void EndFrame() override { ++counts_.endFrame; }

    bool initializeResult{true};
    std::function<void(int)> onBeginFrame;
    std::function<void(int)> onDraw;

protected:
    ImGuiShellContext context_;
    Counts& counts_;
    int exitAfter_;
};

void PushInjected(const ImGuiShellContext& context, SDL_Event& event) {
    REQUIRE(Engine::Platform::Sdl::PushInjectedEvent(context.platform, event));
}

SDL_WindowID OwnWindow(const ImGuiShellContext& context) {
    return SDL_GetWindowID(Engine::Platform::Sdl::NativeWindow(context.platform));
}

} // namespace

TEST_CASE("I1: three frames, then the client's exit request returns 0; every BeginFrame has its EndFrame") {
    Counts counts;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        return std::make_unique<Client>(context, counts, 3);
    });
    CHECK(code == 0);
    CHECK(counts.initialize == 1);
    CHECK(counts.beginFrame == 3);
    CHECK(counts.draw == 3);
    CHECK(counts.endFrame == 3);
}

TEST_CASE("I2: the binding writes no imgui.ini and no ImGui log") {
    std::filesystem::remove("imgui.ini");
    std::filesystem::remove("imgui_log.txt");
    Counts counts;
    bool noIni = false;
    bool noLog = false;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 3);
        client->onDraw = [&](int) {
            noIni = ImGui::GetIO().IniFilename == nullptr;
            noLog = ImGui::GetIO().LogFilename == nullptr;
            ImGui::LogToFile();
            ImGui::LogText("logged");
            ImGui::LogFinish();
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(noIni);
    CHECK(noLog);
    CHECK_FALSE(std::filesystem::exists("imgui.ini"));
    CHECK_FALSE(std::filesystem::exists("imgui_log.txt"));
}

TEST_CASE("I3: ImGui is shut down before the client goes, and the renderer is still there") {
    Counts counts;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        return std::make_unique<Client>(context, counts, 1);
    });
    CHECK(code == 0);
    CHECK(counts.destroyed);
    CHECK_FALSE(counts.contextInDestructor);
    CHECK(counts.rendererInDestructor);
}

TEST_CASE("I4: the factory runs before ImGui, Initialize after it; no client or a failed Initialize returns 1") {
    bool contextAtFactory = true;
    Counts counts;
    CHECK(RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
              contextAtFactory = ImGui::GetCurrentContext() != nullptr;
              return std::make_unique<Client>(context, counts, 1);
          }) == 0);
    CHECK_FALSE(contextAtFactory);
    CHECK(counts.contextAtInitialize);

    CHECK(RunImGuiShell(SmallWindow(), [](const ImGuiShellContext&) { return nullptr; }) == 1);

    Counts failed;
    CHECK(RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
              auto client = std::make_unique<Client>(context, failed, 1);
              client->initializeResult = false;
              return client;
          }) == 1);
    CHECK(failed.initialize == 1);
    CHECK(failed.beginFrame == 0);
}

TEST_CASE("I5: the window's events reach ImGui") {
    Counts counts;
    ImVec2 seen{};
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 2);
        client->onBeginFrame = [&context](int frame) {
            if (frame != 1) {
                return;
            }
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = OwnWindow(context);
            motion.motion.x = 37.0F;
            motion.motion.y = 41.0F;
            PushInjected(context, motion);
        };
        client->onDraw = [&](int frame) {
            if (frame == 2) {
                seen = ImGui::GetIO().MousePos;
            }
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(seen.x == doctest::Approx(37.0));
    CHECK(seen.y == doctest::Approx(41.0));
}

TEST_CASE("I6: the frame is drawn: the font atlas reaches the renderer before the next frame") {
    Counts counts;
    int statusAtFrame2 = -1;
    int statusAtFrame3 = -1;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 3);
        client->onBeginFrame = [&](int frame) {
            ImTextureData* atlas = ImGui::GetIO().Fonts->TexData;
            const int status = atlas == nullptr ? -2 : static_cast<int>(atlas->Status);
            if (frame == 2) {
                statusAtFrame2 = status;
            } else if (frame == 3) {
                statusAtFrame3 = status;
            }
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(statusAtFrame2 == static_cast<int>(ImTextureStatus_OK));
    CHECK(statusAtFrame3 == static_cast<int>(ImTextureStatus_OK));
}

TEST_CASE("I7: closing the window returns 0") {
    Counts counts;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        auto client = std::make_unique<Client>(context, counts, 0);
        client->onBeginFrame = [&context](int frame) {
            if (frame != 2) {
                return;
            }
            SDL_Event close{};
            close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            close.window.windowID = OwnWindow(context);
            PushInjected(context, close);
        };
        return client;
    });
    CHECK(code == 0);
    CHECK(counts.beginFrame == 2);
    CHECK(counts.endFrame == 2);
}

TEST_CASE("I9: the options reach the platform as given") {
    Counts counts;
    Engine::Platform::Sdl::Rect rect{};
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext& context) {
        rect = context.platform.QueryWindow().rect;
        return std::make_unique<Client>(context, counts, 1);
    });
    CHECK(code == 0);
    CHECK(rect.width == 640);
    CHECK(rect.height == 480);
}

// Run only by gyo_framework.imgui_shell_no_renderer, where SDL_RENDER_DRIVER
// names no driver.
TEST_CASE("I8: a renderer that cannot start returns 1 without calling the factory" * doctest::skip()) {
    int factoryCalls = 0;
    const int code = RunImGuiShell(SmallWindow(), [&](const ImGuiShellContext&) {
        ++factoryCalls;
        return nullptr;
    });
    CHECK(code == 1);
    CHECK(factoryCalls == 0);
}
