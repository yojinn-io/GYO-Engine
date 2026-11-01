#include "gyo/framework/imgui/ImGuiShell.hpp"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "engine/base/Assert.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/os/Files.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

namespace Gyo::Tools::Framework {
namespace {

using Engine::Base::LogLevel;
using Engine::Platform::Sdl::PlatformEvent;
using Engine::Platform::Sdl::SdlPlatform;
using Engine::Render::Backend::Sdl::Color;
using Engine::Render::Backend::Sdl::SdlRenderer;
using Engine::Runtime::FrameContext;
using Engine::Runtime::RuntimeControl;

constexpr const char* kLogCategory = "framework.imgui";
constexpr float kChromeFontSize = 16.0F;

// The font files the chrome may use, by priority, relative to a font
// directory from engine/os. One list serves every platform: each file exists
// only where its platform installs it (Windows, macOS, then Linux names;
// os-subsystem D56).
constexpr std::array kPrimaryFontNames{
    std::string_view{"msjh.ttc"},
    std::string_view{"msyh.ttc"},
    std::string_view{"simsun.ttc"},
    std::string_view{"PingFang.ttc"},
    std::string_view{"STHeiti Medium.ttc"},
    std::string_view{"opentype/noto/NotoSansCJK-Regular.ttc"},
    std::string_view{"truetype/noto/NotoSansCJK-Regular.ttc"},
    std::string_view{"truetype/wqy/wqy-zenhei.ttc"},
};
constexpr std::array kJapaneseFontNames{
    std::string_view{"YuGothM.ttc"},
    std::string_view{"meiryo.ttc"},
    std::string_view{"msgothic.ttc"},
    std::string_view{"ヒラギノ角ゴシック W3.ttc"},
    std::string_view{"ヒラギノ丸ゴ ProN W4.ttc"},
};

void Log(const LogLevel level, const std::string& title, const std::string& text) noexcept {
    try {
        Engine::Base::Log(level, kLogCategory, title + ": " + text);
    } catch (...) {
        // A log line is never worth a failure.
    }
}

// The first name that is a regular file, trying every name in a directory
// before the next directory (system directories come first). UTF-8, as ImGui
// opens it.
[[nodiscard]] std::string FirstExistingFont(const std::span<const std::string> directories,
                                            const std::span<const std::string_view> fileNames) {
    Engine::IO::FS::NativeFileSystem files;
    for (const std::string& directory : directories) {
        for (const std::string_view fileName : fileNames) {
            std::string candidate = directory + "/" + std::string(fileName);
            const auto uri = Engine::IO::FS::NativeFileUri(candidate);
            if (!uri) {
                continue;
            }
            const auto info = files.Stat(uri.value());
            if (info && info.value().IsFile()) {
                return candidate;
            }
        }
    }
    return {};
}

// The chrome is operating-system UI, not game content: it uses a system CJK
// font and never reaches into an asset root; content a client previews brings
// its own fonts.
void ConfigureChromeFont(ImGuiIO& io, const std::string& title) {
    std::vector<std::string> directories;
    if (auto found = Engine::Os::Files::FontDirectories()) {
        directories = std::move(found.value());
    } else {
        Log(LogLevel::Warning, title, "cannot list the font directories: " + Engine::Base::Describe(found.error()));
    }
    const std::string primary = FirstExistingFont(directories, kPrimaryFontNames);
    const std::string japaneseFallback = FirstExistingFont(directories, kJapaneseFontNames);

    ImFont* chromeFont = nullptr;
    if (!primary.empty()) {
        chromeFont = io.Fonts->AddFontFromFileTTF(primary.c_str(), kChromeFontSize);
        if (chromeFont != nullptr) {
            io.FontDefault = chromeFont;
            Log(LogLevel::Info, title, "chrome font: " + primary);
        }
    }
    if (chromeFont != nullptr && !japaneseFallback.empty() && japaneseFallback != primary) {
        ImFontConfig merge;
        merge.MergeMode = true;
        if (io.Fonts->AddFontFromFileTTF(japaneseFallback.c_str(), kChromeFontSize, &merge) != nullptr) {
            Log(LogLevel::Info, title, "Japanese font fallback: " + japaneseFallback);
        }
    }
    if (chromeFont == nullptr) {
        io.Fonts->AddFontDefault();
        Log(LogLevel::Warning, title, "no system CJK font was found; the chrome uses ImGui's Latin-only font");
    }
}

// ImGui's context and its two SDL backends, shut down in reverse when it goes.
class ImGuiScope final {
public:
    ImGuiScope() = default;
    ImGuiScope(const ImGuiScope&) = delete;
    ImGuiScope& operator=(const ImGuiScope&) = delete;
    ~ImGuiScope() {
        if (rendererBackend_) {
            ImGui_ImplSDLRenderer3_Shutdown();
        }
        if (platformBackend_) {
            ImGui_ImplSDL3_Shutdown();
        }
        if (context_) {
            ImGui::DestroyContext();
        }
    }

    [[nodiscard]] bool Start(const SdlPlatform& platform, const SdlRenderer& renderer, const std::string& title) {
        // One ImGui shell at a time per process: the backends keep global state.
        GYO_ASSERT(ImGui::GetCurrentContext() == nullptr);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        context_ = true;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // The binding writes no side file (Design Invariant 4): no window
        // state next to the program, no ImGui log.
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        ConfigureChromeFont(io, title);
        ImGui::StyleColorsDark();

        auto* const window = Engine::Platform::Sdl::NativeWindow(platform);
        auto* const nativeRenderer = Engine::Render::Backend::Sdl::NativeRenderer(renderer);
        if (!ImGui_ImplSDL3_InitForSDLRenderer(window, nativeRenderer)) {
            Log(LogLevel::Error, title, "ImGui's SDL3 platform backend could not start");
            return false;
        }
        platformBackend_ = true;
        if (!ImGui_ImplSDLRenderer3_Init(nativeRenderer)) {
            Log(LogLevel::Error, title, "ImGui's SDL renderer backend could not start");
            return false;
        }
        rendererBackend_ = true;
        return true;
    }

private:
    bool context_{};
    bool platformBackend_{};
    bool rendererBackend_{};
};

// The windowed shell's client for an ImGui window. Members are destroyed in
// reverse: ImGui, then the client, then the renderer.
class ImGuiWindowClient final : public WindowShellClient {
public:
    ImGuiWindowClient(SdlPlatform& platform, std::string title) : platform_(platform), title_(std::move(title)) {}

    [[nodiscard]] bool Start(const ImGuiShellClientFactory& factory) {
        auto created = SdlRenderer::Create(platform_);
        if (!created) {
            Log(LogLevel::Error, title_, "renderer initialization failed: " + Engine::Base::Describe(created.error()));
            return false;
        }
        renderer_ = std::move(created).value();
        context_.emplace(ImGuiShellContext{platform_, *renderer_});
        client_ = factory(*context_);
        if (client_ == nullptr) {
            Log(LogLevel::Error, title_, "there is no client to run");
            return false;
        }
        if (!imgui_.Start(platform_, *renderer_, title_)) {
            return false;
        }
        return client_->Initialize(*context_);
    }

    void OnEvent(const PlatformEvent& event) override {
        ImGui_ImplSDL3_ProcessEvent(&Engine::Platform::Sdl::NativeEvent(event));
    }

    RuntimeControl Update(const FrameContext&) override {
        client_->BeginFrame();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        client_->Draw();
        if (client_->WantsExit()) {
            ImGui::EndFrame();
            client_->EndFrame();
            return RuntimeControl::Stop;
        }
        return RuntimeControl::Continue;
    }

    RuntimeControl Render(const FrameContext&) override {
        ImGui::Render();
        auto cleared = renderer_->Clear(Color{12, 14, 18, 255});
        if (!cleared) {
            client_->EndFrame();
            Log(LogLevel::Error, title_, "clear failed: " + Engine::Base::Describe(cleared.error()));
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),
                                              Engine::Render::Backend::Sdl::NativeRenderer(*renderer_));
        auto presented = renderer_->Present();
        client_->EndFrame();
        if (!presented) {
            Log(LogLevel::Error, title_, "present failed: " + Engine::Base::Describe(presented.error()));
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }
        return RuntimeControl::Continue;
    }

    [[nodiscard]] int ExitCode() const noexcept override { return exitCode_; }

private:
    SdlPlatform& platform_;
    std::string title_;
    std::unique_ptr<SdlRenderer> renderer_;
    std::optional<ImGuiShellContext> context_;
    std::unique_ptr<ImGuiShellClient> client_;
    ImGuiScope imgui_;
    int exitCode_{};
};

} // namespace

int RunImGuiShell(const WindowShellOptions& options, const ImGuiShellClientFactory& factory) {
    GYO_ASSERT(factory);
    return RunWindowShell(options, [&](SdlPlatform& platform) -> std::unique_ptr<WindowShellClient> {
        auto client = std::make_unique<ImGuiWindowClient>(platform, options.platform.title);
        if (!client->Start(factory)) {
            return nullptr;
        }
        return client;
    });
}

} // namespace Gyo::Tools::Framework
