#include "gyo/ui_editor/EditorGui.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/EditorApp.hpp"
#include "SdlPathDialogs.hpp"

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

namespace Gyo::Tools::UiEditor {
namespace {

using Engine::Platform::Sdl::SdlPlatform;
using Engine::Platform::Sdl::SdlPlatformOptions;
using Engine::Render::Backend::Sdl::Color;
using Engine::Render::Backend::Sdl::SdlRenderer;
using Engine::Runtime::FrameContext;
using Engine::Runtime::IRuntimeClient;
using Engine::Runtime::RuntimeControl;

constexpr float kEditorChromeFontSize = 16.0F;

// The font files the editor chrome may use, by priority, relative to a font
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

// Editor chrome is operating-system UI, not game content. It therefore uses a
// system CJK font and never reaches into an app asset root. Canvas text preview
// remains driven by the document's FontAsset through AssetPreviewContext.
void ConfigureEditorChromeFont(ImGuiIO& io) {
    std::vector<std::string> directories;
    if (auto found = Engine::Os::Files::FontDirectories()) {
        directories = std::move(found.value());
    } else {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "GYO UI Editor cannot list the font directories: %s",
            Engine::Base::Describe(found.error()).c_str());
    }
    const std::string primary = FirstExistingFont(directories, kPrimaryFontNames);
    const std::string japaneseFallback = FirstExistingFont(directories, kJapaneseFontNames);

    ImFont* editorFont = nullptr;
    if (!primary.empty()) {
        editorFont = io.Fonts->AddFontFromFileTTF(
            primary.c_str(), kEditorChromeFontSize);
        if (editorFont != nullptr) {
            io.FontDefault = editorFont;
            SDL_Log("GYO UI Editor chrome font: %s", primary.c_str());
        }
    }

    if (editorFont != nullptr && !japaneseFallback.empty() &&
        japaneseFallback != primary) {
        ImFontConfig merge;
        merge.MergeMode = true;
        if (io.Fonts->AddFontFromFileTTF(
                japaneseFallback.c_str(), kEditorChromeFontSize, &merge) != nullptr) {
            SDL_Log(
                "GYO UI Editor Japanese font fallback: %s",
                japaneseFallback.c_str());
        }
    }

    if (editorFont == nullptr) {
        io.Fonts->AddFontDefault();
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "No system CJK font was found; Editor chrome will use ImGui's "
            "Latin-only fallback font");
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

class EditorClient final : public IRuntimeClient {
public:
    EditorClient(
        SdlPlatform& platform,
        SdlRenderer& renderer,
        CommandLineOptions options)
        : platform_(platform),
          renderer_(renderer),
          dialogs_(Engine::Platform::Sdl::NativeWindow(platform)),
          editor_(std::move(options), previewAssets_, &dialogs_) {}

    ~EditorClient() override {
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

    [[nodiscard]] bool Initialize() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        contextCreated_ = true;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // The editor's only authored output is the exported UI JSON. ImGui
        // window state must never create an adjacent sidecar.
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        ConfigureEditorChromeFont(io);
        ImGui::StyleColorsDark();

        if (!ImGui_ImplSDL3_InitForSDLRenderer(
                Engine::Platform::Sdl::NativeWindow(platform_), Engine::Render::Backend::Sdl::NativeRenderer(renderer_))) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "ImGui SDL3 platform backend initialization failed");
            return false;
        }
        platformBackendInitialized_ = true;

        if (!ImGui_ImplSDLRenderer3_Init(Engine::Render::Backend::Sdl::NativeRenderer(renderer_))) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "ImGui SDLRenderer backend initialization failed");
            return false;
        }
        rendererBackendInitialized_ = true;

        if (const auto preview = previewAssets_.Initialize(*Engine::Render::Backend::Sdl::NativeRenderer(renderer_)); !preview) {
            LogError("Asset preview initialization failed", preview.error());
            return false;
        }
        if (const auto editor = editor_.Initialize(); !editor) {
            LogError("UI editor initialization failed", editor.error());
            return false;
        }
        return true;
    }

    RuntimeControl ProcessEvents(const FrameContext&) override {
        return platform_.PumpEvents([](const Engine::Platform::Sdl::PlatformEvent& view_) {
        const SDL_Event& event = Engine::Platform::Sdl::NativeEvent(view_);
            ImGui_ImplSDL3_ProcessEvent(&event);
        });
    }

    RuntimeControl Update(const FrameContext&) override {
        editor_.ApplyPathDialogResults();
        editor_.ApplyPendingAssetChanges();
        previewAssets_.BeginFrame();
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        editor_.Draw();
        if (editor_.WantsExit()) {
            ImGui::EndFrame();
            previewAssets_.EndFrame();
            return RuntimeControl::Stop;
        }
        return RuntimeControl::Continue;
    }

    RuntimeControl Render(const FrameContext&) override {
        ImGui::Render();
        auto cleared = renderer_.Clear(Color{12, 14, 18, 255});
        if (!cleared) {
            previewAssets_.EndFrame();
            LogError("clear failed", cleared.error());
            exitCode_ = 1;
            return RuntimeControl::Stop;
        }

        ImGui_ImplSDLRenderer3_RenderDrawData(
            ImGui::GetDrawData(), Engine::Render::Backend::Sdl::NativeRenderer(renderer_));
        auto presented = renderer_.Present();
        previewAssets_.EndFrame();
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
    AssetPreviewContext previewAssets_;
    SdlPathDialogs dialogs_;
    EditorApp editor_;
    bool contextCreated_{};
    bool platformBackendInitialized_{};
    bool rendererBackendInitialized_{};
    int exitCode_{};
};

} // namespace

int RunEditorGui(const CommandLineOptions& options) {
    SdlPlatformOptions platformOptions;
    platformOptions.title = "GYO UI Editor";
    platformOptions.width = 1600;
    platformOptions.height = 900;
    platformOptions.resizable = true;

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

    EditorClient client(*platform, *renderer, options);
    if (!client.Initialize()) {
        return 1;
    }
    Engine::Runtime::RuntimeLoop loop(client);
    loop.Run();
    return client.ExitCode();
}

} // namespace Gyo::Tools::UiEditor
