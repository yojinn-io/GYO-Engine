#pragma once

#include "gyo/ui_editor/PathDialog.hpp"

#include <memory>

struct SDL_Window;

namespace Gyo::Tools::UiEditor {

// The operating system's file dialogs through SDL3. SDL may call back on
// another thread, so results go through a shared inbox that outlives this
// object until every pending dialog has answered.
class SdlPathDialogs final : public PathDialogs {
public:
    explicit SdlPathDialogs(SDL_Window* window);

    void Show(PathDialogPurpose purpose, std::string_view defaultLocation) override;
    [[nodiscard]] std::vector<PathDialogResult> TakeResults() override;

private:
    SDL_Window* window_;
    std::shared_ptr<PathDialogInbox> inbox_;
};

} // namespace Gyo::Tools::UiEditor
