#include "gyo/framework/ImGuiSdlHost.hpp"

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "engine/base/Error.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/os/Files.hpp"
#include "engine/runtime/RuntimeLoop.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "engine/platform/sdl/SdlPlatformNative.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

namespace Gyo::Tools::Framework {
namespace {

using Engine::Platform::Sdl::SdlPlatform;
using Engine::Platform::Sdl::SdlPlatformOptions;
using Engine::Render::Backend::Sdl::Color;
using Engine::Render::Backend::Sdl::SdlRenderer;
using Engine::Runtime::FrameContext;
using Engine::Runtime::IRuntimeClient;
using Engine::Runtime::RuntimeControl;

constexpr float kChromeFontSize = 16.0F;

// The font files the tool chrome may use, by priority, relative to a font
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

// The first name that is a regular file, trying every name in a directory
// before the next directory (system directories come first). UTF-8, as ImGui
// opens it.
[[nodiscard]] std::string FirstExistingFont(
    const std::span<const std::string> directories,
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

// Tool chrome is operating-system UI, not game content. It therefore uses a
// system CJK font and never reaches into an app asset root; content a tool
// previews brings its own fonts.
void ConfigureChromeFont(ImGuiIO& io, const char* title) {
    std::vector<std::string> directories;
    if (auto found = Engine::Os::Files::FontDirectories()) {
        directories = std::move(found.value());
    } else {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "%s cannot list the font directories: %s",
            title,
            Engine::Base::Describe(found.error()).c_str());
    }
    const std::string primary = FirstExistingFont(directories, kPrimaryFontNames);
    const std::string japaneseFallback = FirstExistingFont(directories, kJapaneseFontNames);

    ImFont* chromeFont = nullptr;
    if (!primary.empty()) {
        chromeFont = io.Fonts->AddFontFromFileTTF(
            primary.c_str(), kChromeFontSize);
        if (chromeFont != nullptr) {
            io.FontDefault = chromeFont;
            SDL_Log("%s chrome font: %s", title, primary.c_str());
        }
    }

    if (chromeFont != nullptr && !japaneseFallback.empty() &&
        japaneseFallback != primary) {
        ImFontConfig merge;
        merge.MergeMode = true;
        if (io.Fonts->AddFontFromFileTTF(
                japaneseFallback.c_str(), kChromeFontSize, &merge) != nullptr) {
            SDL_Log(
                "%s Japanese font fallback: %s",
                title,
                japaneseFallback.c_str());
        }
    }

    if (chromeFont == nullptr) {
        io.Fonts->AddFontDefault();
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "No system CJK font was found; %s chrome will use ImGui's "
            "Latin-only fallback font",
            title);
    }
}

template <Engine::Base::CodedError Error>
void LogError(const char* context, const Error& error) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "%s: %s",
        context,
        Engine::Base::Describe(error).c_str());
}

class HostClient final : public IRuntimeClient {
public:
    HostClient(
        SdlPlatform& platform,
        SdlRenderer& renderer,
        std::string title)
        : platform_(platform),
          renderer_(renderer),
          title_(std::move(title)),
          context_{Engine::Platform::Sdl::NativeWindow(platform),
                   Engine::Render::Backend::Sdl::NativeRenderer(renderer)} {}

    // ImGui goes down before the client, as members are destroyed after this
    // body runs.
    ~HostClient() override {
        if (rendererBackendInitialized_) {
            ImGui_ImplSDLRenderer3_Shutdown();
        }
        if (platformBackendInitialized_) {
            ImGui_ImplSDL3_Shutdown();
        }
        if (contextCreated_) {
            ImGui::DestroyContext();
        }
    }

    [[nodiscard]] bool Initialize(const ImGuiSdlClientFactory& factory) {
        client_ = factory(context_);
        if (client_ == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s has no client to run", title_.c_str());
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        contextCreated_ = true;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // A tool writes only what it means to (Design Invariant 4). ImGui
        // window state must never create an adjacent sidecar.
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        ConfigureChromeFont(io, title_.c_str());
        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL3_InitForSDLRenderer(context_.window, context_.renderer)) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "ImGui SDL3 platform backend initialization failed");
            return false;
        }
        platformBackendInitialized_ = true;

        if (!ImGui_ImplSDLRenderer3_Init(context_.renderer)) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "ImGui SDLRenderer backend initialization failed");
            return false;
        }
        rendererBackendInitialized_ = true;

        return client_->Initialize(context_);
    }

    RuntimeControl ProcessEvents(const FrameContext&) override {
        return platform_.PumpEvents([](const Engine::Platform::Sdl::PlatformEvent& view_) {
        const SDL_Event& event = Engine::Platform::Sdl::NativeEvent(view_);
            ImGui_ImplSDL3_ProcessEvent(&event);
        });
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
        auto cleared = renderer_.Clear(Color{12, 14, 18, 255});
        if (!cleared) {
            client_->EndFrame();
            LogError("clear failed", cleared.error());
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), context_.renderer);
        auto presented = renderer_.Present();
        client_->EndFrame();
        if (!presented) {
            LogError("present failed", presented.error());
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }
        return RuntimeControl::Continue;
    }

    [[nodiscard]] int ExitCode() const noexcept { return exitCode_; }

private:
    SdlPlatform& platform_;
    SdlRenderer& renderer_;
    std::string title_;
    ImGuiSdlHostContext context_;
    std::unique_ptr<ImGuiSdlClient> client_;
    bool contextCreated_{};
    bool platformBackendInitialized_{};
    bool rendererBackendInitialized_{};
    int exitCode_{};
};

} // namespace

int RunImGuiSdlHost(const ImGuiSdlHostOptions& options, const ImGuiSdlClientFactory& factory) {
    SdlPlatformOptions platformOptions;
    platformOptions.title = options.title;
    platformOptions.width = options.width;
    platformOptions.height = options.height;
    platformOptions.resizable = options.resizable;

    auto platformResult = SdlPlatform::Create(platformOptions);
    if (!platformResult) {
        LogError("platform initialization failed", platformResult.error());
        return 1;
    }
    std::unique_ptr<SdlPlatform> platform = std::move(platformResult).value();

    auto rendererResult = SdlRenderer::Create(*platform);
    if (!rendererResult) {
        LogError("renderer initialization failed", rendererResult.error());
        return 1;
    }
    std::unique_ptr<SdlRenderer> renderer = std::move(rendererResult).value();

    HostClient host(*platform, *renderer, options.title);
    if (!host.Initialize(factory)) {
        return 1;
    }
    Engine::Runtime::RuntimeLoop loop(host);
    loop.Run();
    return host.ExitCode();
}

} // namespace Gyo::Tools::Framework
