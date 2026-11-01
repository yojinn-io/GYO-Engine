#pragma once

#include "gyo/framework/PathDialog.hpp"

#include <memory>
#include <vector>

struct SDL_Window;

namespace Gyo::Tools::Framework {

// The operating system's file dialogs through SDL3. Show is called on the main
// thread and returns without waiting; the answer arrives later through
// TakeAnswers. SDL may call back on another thread, so answers go through a
// shared inbox that outlives this object until every pending dialog has
// answered.
class SdlPathDialogs final {
public:
    explicit SdlPathDialogs(SDL_Window* window);

    void Show(PathDialogRequest request);
    [[nodiscard]] std::vector<PathDialogAnswer> TakeAnswers();

private:
    SDL_Window* window_;
    std::shared_ptr<PathDialogInbox> inbox_;
};

} // namespace Gyo::Tools::Framework
