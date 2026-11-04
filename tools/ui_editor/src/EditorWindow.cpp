#include "gyo/ui_editor/EditorGui.hpp"

#include "gyo/ui_editor/AssetPreviewContext.hpp"
#include "gyo/ui_editor/EditorApp.hpp"
#include "gyo/ui_editor/SdlPathDialogs.hpp"

#include "gyo/framework/imgui/ImGuiShell.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "engine/base/Error.hpp"
#include "engine/base/Log.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"

namespace Gyo::Tools::UiEditor {
namespace {

void LogError(const std::string_view text) {
    Engine::Base::Log(Engine::Base::LogLevel::Error, "ui_editor", text);
}

template <Engine::Base::CodedError Error>
void LogError(const char* context, const Error& error) {
    LogError(std::string(context) + ": " + Engine::Base::Describe(error));
}

// The editor in the framework's ImGui shell.
class EditorClient final : public Framework::ImGuiShellClient {
public:
    EditorClient(const Framework::ImGuiShellContext& context, CommandLineOptions options)
        : dialogs_(context.platform),
          editor_(std::move(options), previewAssets_, &dialogs_) {}

    [[nodiscard]] bool Initialize(const Framework::ImGuiShellContext& context) override {
        input_ = &context.input;
        // The preview makes its textures on the native renderer until Engine's
        // SDL renderer has a texture API (platform-sdl Replacement Criterion 2).
        auto* const renderer = Engine::Render::Backend::Sdl::NativeRenderer(context.renderer);
        if (renderer == nullptr) {
            LogError("Asset preview initialization failed: no renderer");
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
        editor_.SetPhysicalInput(&input_->Frame());
    }

    void Draw() override {
        editor_.Draw();
        // The preview's text field takes text input over its own rect
        // (v8-input-ui-adapter); otherwise ImGui's fields keep it.
        if (const auto request = editor_.TakeTextInputRequest()) {
            input_->RequestTextInput({static_cast<int>(request->x), static_cast<int>(request->y),
                                      static_cast<int>(request->width), static_cast<int>(request->height)},
                                     request->cursorPixels);
        }
    }

    [[nodiscard]] bool WantsExit() const override { return editor_.WantsExit(); }
    // Unsaved changes: the editor asks in the next frame (v8-framework-close-request).
    [[nodiscard]] bool CloseRequested() override { return editor_.CloseRequested(); }

    void EndFrame() override {
        editor_.SetPhysicalInput(nullptr);
        previewAssets_.EndFrame();
    }

private:
    Framework::ImGuiShellInput* input_{};
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
