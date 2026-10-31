#include "gyo/framework/PathDialog.hpp"

#include <utility>

namespace Gyo::Tools::Framework {

void PathDialogInbox::Post(PathDialogAnswer answer) {
    const std::lock_guard lock(mutex_);
    answers_.push_back(std::move(answer));
}

std::vector<PathDialogAnswer> PathDialogInbox::Take() {
    const std::lock_guard lock(mutex_);
    return std::exchange(answers_, {});
}

} // namespace Gyo::Tools::Framework
