#include "gyo/ui_editor/EditorGui.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/EditorApp.hpp"
#include "gyo/ui_editor/SdlPathDialogs.hpp"

#include "gyo/framework/imgui/ImGuiShell.hpp"

#include <memory>
#include <utility>

#include <SDL3/SDL.h>

#include "engine/base/Error.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

namespace Gyo::Tools::UiEditor {
namespace {

template <Engine::Base::CodedError Error>
void LogError(const char* context, const Error& error) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "%s: %s",
        context,
        Engine::Base::Describe(error).c_str());
}

// The editor in the framework's ImGui shell.
class EditorClient final : public Framework::ImGuiShellClient {
public:
    EditorClient(const Framework::ImGuiShellContext& context, CommandLineOptions options)
        : dialogs_(context.platform),
          editor_(std::move(options), previewAssets_, &dialogs_) {}

    [[nodiscard]] bool Initialize(const Framework::ImGuiShellContext& context) override {
        // The preview makes its textures on the native renderer until Engine's
        // SDL renderer has a texture API (platform-sdl Replacement Criterion 2).
        SDL_Renderer* const renderer = Engine::Render::Backend::Sdl::NativeRenderer(context.renderer);
        if (renderer == nullptr) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Asset preview initialization failed: no renderer");
            return false;
        }
        if (const auto preview = previewAssets_.Initialize(*renderer); !preview) {
            LogError("Asset preview initialization failed", preview.error());
            return false;
        }
        if (const auto editor = editor_.Initialize(); !editor) {
            LogError("UI editor initialization failed", editor.error());
            return false;
        }
        return true;
    }

    void BeginFrame() override {
        editor_.ApplyPathDialogResults();
        editor_.ApplyPendingAssetChanges();
        previewAssets_.BeginFrame();
    }

    void Draw() override { editor_.Draw(); }

    [[nodiscard]] bool WantsExit() const override { return editor_.WantsExit(); }

    void EndFrame() override { previewAssets_.EndFrame(); }

private:
    AssetPreviewContext previewAssets_;
    SdlPathDialogs dialogs_;
    EditorApp editor_;
};

} // namespace

int RunEditorGui(const CommandLineOptions& options) {
    Framework::WindowShellOptions shell;
    shell.platform.title = "GYO UI Editor";
    shell.platform.width = 1600;
    shell.platform.height = 900;
    shell.platform.resizable = true;
    return Framework::RunImGuiShell(shell, [&options](const Framework::ImGuiShellContext& context) {
        return std::make_unique<EditorClient>(context, options);
    });
}

} // namespace Gyo::Tools::UiEditor
