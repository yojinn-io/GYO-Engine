#include "gyo/ui_editor/PathDialog.hpp"

#include <utility>

namespace Gyo::Tools::UiEditor {

void PathDialogInbox::Post(PathDialogResult result) {
    const std::lock_guard lock(mutex_);
    results_.push_back(std::move(result));
}

std::vector<PathDialogResult> PathDialogInbox::Take() {
    const std::lock_guard lock(mutex_);
    return std::exchange(results_, {});
}

} // namespace Gyo::Tools::UiEditor
