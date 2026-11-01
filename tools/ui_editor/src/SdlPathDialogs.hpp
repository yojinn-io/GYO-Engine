#pragma once

#include "gyo/ui_editor/PathDialog.hpp"

#include "gyo/framework/SdlPathDialogs.hpp"

struct SDL_Window;

namespace Gyo::Tools::UiEditor {

// The editor's dialogs on the framework's SDL dialogs: each purpose becomes a
// request of the matching kind and filters, with the purpose as its tag.
class SdlPathDialogs final : public PathDialogs {
public:
    explicit SdlPathDialogs(SDL_Window* window);

    void Show(PathDialogPurpose purpose, std::string_view defaultLocation) override;
    [[nodiscard]] std::vector<PathDialogResult> TakeResults() override;

private:
    Framework::SdlPathDialogs dialogs_;
};

} // namespace Gyo::Tools::UiEditor
