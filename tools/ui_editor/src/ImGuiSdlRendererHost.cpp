#include "gyo/ui_editor/EditorGui.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/EditorApp.hpp"

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include "engine/runtime/RuntimeLoop.hpp"
#include "platform/sdl/SdlPlatform.hpp"
#include "render/backend/sdl/SdlRenderer.hpp"

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

#if defined(_WIN32)
[[nodiscard]] std::filesystem::path FirstExistingFont(
    const std::filesystem::path& directory,
    const std::span<const std::string_view> fileNames) {
    for (const std::string_view fileName : fileNames) {
        const std::filesystem::path candidate = directory / fileName;
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            return candidate;
        }
    }
    return {};
}
#else

[[nodiscard]] std::filesystem::path FirstExistingFont(
    const std::span<const std::string_view> paths) {
    for (const std::string_view path : paths) {
        const std::filesystem::path candidate{path};
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error) && !error) {
            return candidate;
        }
    }
    return {};
}
#endif

// Editor chrome is operating-system UI, not game content. It therefore uses a
// system CJK font and never reaches into an app asset root. Canvas text preview
// remains driven by the document's FontAsset through AssetPreviewContext.
void ConfigureEditorChromeFont(ImGuiIO& io) {
    std::filesystem::path primary;
    std::filesystem::path japaneseFallback;

#if defined(_WIN32)
    std::filesystem::path fontsDirectory{"C:\\Windows\\Fonts"};
    if (const char* windowsDirectory = SDL_getenv("WINDIR");
        windowsDirectory != nullptr && windowsDirectory[0] != '\0') {
        fontsDirectory = std::filesystem::path{windowsDirectory} / "Fonts";
    }
    constexpr std::array primaryNames{
        std::string_view{"msjh.ttc"},
        std::string_view{"msyh.ttc"},
        std::string_view{"simsun.ttc"},
    };
    constexpr std::array japaneseNames{
        std::string_view{"YuGothM.ttc"},
        std::string_view{"meiryo.ttc"},
        std::string_view{"msgothic.ttc"},
    };
    primary = FirstExistingFont(fontsDirectory, primaryNames);
    japaneseFallback = FirstExistingFont(fontsDirectory, japaneseNames);
#elif defined(__APPLE__)
    constexpr std::array primaryNames{
        std::string_view{"/System/Library/Fonts/PingFang.ttc"},
        std::string_view{"/System/Library/Fonts/STHeiti Medium.ttc"},
    };
    constexpr std::array japaneseNames{
        std::string_view{"/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc"},
        std::string_view{"/System/Library/Fonts/ヒラギノ丸ゴ ProN W4.ttc"},
    };
    primary = FirstExistingFont(primaryNames);
    japaneseFallback = FirstExistingFont(japaneseNames);
#else
    constexpr std::array primaryNames{
        std::string_view{"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"},
        std::string_view{"/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"},
        std::string_view{"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"},
        std::string_view{"/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc"},
    };
    primary = FirstExistingFont(primaryNames);
#endif

    ImFont* editorFont = nullptr;
    if (!primary.empty()) {
        const std::string nativePath = primary.string();
        editorFont = io.Fonts->AddFontFromFileTTF(
            nativePath.c_str(), kEditorChromeFontSize);
        if (editorFont != nullptr) {
            io.FontDefault = editorFont;
            SDL_Log("GYO UI Editor chrome font: %s", nativePath.c_str());
        }
    }

    if (editorFont != nullptr && !japaneseFallback.empty() &&
        japaneseFallback != primary) {
        ImFontConfig merge;
        merge.MergeMode = true;
        const std::string nativePath = japaneseFallback.string();
        if (io.Fonts->AddFontFromFileTTF(
                nativePath.c_str(), kEditorChromeFontSize, &merge) != nullptr) {
            SDL_Log(
                "GYO UI Editor Japanese font fallback: %s",
                nativePath.c_str());
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

template <typename Error>
void LogError(const char* context, const Error& error) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "%s: %s%s%s",
        context,
        error.message.c_str(),
        error.detail.empty() ? "" : ": ",
        error.detail.c_str());
}

class EditorClient final : public IRuntimeClient {
public:
    EditorClient(
        SdlPlatform& platform,
        SdlRenderer& renderer,
        CommandLineOptions options)
        : platform_(platform),
          renderer_(renderer),
          editor_(std::move(options), previewAssets_) {}

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
                platform_.NativeWindow(), renderer_.NativeRenderer())) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "ImGui SDL3 platform backend initialization failed");
            return false;
        }
        platformBackendInitialized_ = true;

        if (!ImGui_ImplSDLRenderer3_Init(renderer_.NativeRenderer())) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "ImGui SDLRenderer backend initialization failed");
            return false;
        }
        rendererBackendInitialized_ = true;

        std::string error;
        if (!previewAssets_.Initialize(*renderer_.NativeRenderer(), error)) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "Asset preview initialization failed: %s",
                error.c_str());
            return false;
        }
        if (!editor_.Initialize(error)) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "UI editor initialization failed: %s",
                error.c_str());
            return false;
        }
        return true;
    }

    RuntimeControl ProcessEvents(const FrameContext&) override {
        return platform_.PumpEvents([](const SDL_Event& event) {
            ImGui_ImplSDL3_ProcessEvent(&event);
        });
    }

    RuntimeControl Update(const FrameContext&) override {
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
            ImGui::GetDrawData(), renderer_.NativeRenderer());
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
