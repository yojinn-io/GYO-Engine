#pragma once

#include "gyo/ui_editor/PathDialog.hpp"

namespace Engine::Platform::Sdl {
class SdlPlatform;
}

namespace Gyo::Tools::UiEditor {

// The editor's dialogs on Engine's platform dialogs: each purpose becomes a
// request of the matching kind and filters, with a tag from PathDialogRequests.
// Show runs in the editor's frame, never inside an event observer.
class SdlPathDialogs final : public PathDialogs {
public:
    explicit SdlPathDialogs(Engine::Platform::Sdl::SdlPlatform& platform);

    void Show(PathDialogPurpose purpose, std::string_view defaultLocation) override;
    [[nodiscard]] std::vector<PathDialogResult> TakeResults() override;
    void Abandon() override;

private:
    Engine::Platform::Sdl::SdlPlatform& platform_;
    PathDialogRequests requests_;
};

} // namespace Gyo::Tools::UiEditor
