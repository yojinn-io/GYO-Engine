#include "gyo/ui_editor/EditorGui.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/EditorApp.hpp"
#include "SdlPathDialogs.hpp"

#include "gyo/framework/ImGuiSdlHost.hpp"

#include <memory>
#include <utility>

#include <SDL3/SDL.h>

#include "engine/base/Error.hpp"

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

// The editor in the framework's window host.
class EditorClient final : public Framework::ImGuiSdlClient {
public:
    EditorClient(const Framework::ImGuiSdlHostContext& context, CommandLineOptions options)
        : dialogs_(context.window),
          editor_(std::move(options), previewAssets_, &dialogs_) {}

    [[nodiscard]] bool Initialize(const Framework::ImGuiSdlHostContext& context) override {
        if (const auto preview = previewAssets_.Initialize(*context.renderer); !preview) {
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
    Framework::ImGuiSdlHostOptions host;
    host.title = "GYO UI Editor";
    host.width = 1600;
    host.height = 900;
    host.resizable = true;
    return Framework::RunImGuiSdlHost(host, [&options](const Framework::ImGuiSdlHostContext& context) {
        return std::make_unique<EditorClient>(context, options);
    });
}

} // namespace Gyo::Tools::UiEditor
