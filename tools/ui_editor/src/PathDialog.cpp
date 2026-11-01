#include "gyo/ui_editor/PathDialog.hpp"

namespace Gyo::Tools::UiEditor {

std::uint64_t PathDialogRequests::Begin(const PathDialogPurpose purpose) noexcept {
    ++lastTag_;
    waiting_ = purpose;
    return lastTag_;
}

void PathDialogRequests::Abandon() noexcept {
    waiting_.reset();
}

std::optional<PathDialogPurpose> PathDialogRequests::Accept(const std::uint64_t tag,
                                                            const PathDialogOutcome outcome) noexcept {
    if (!waiting_.has_value() || tag != lastTag_) return std::nullopt;
    const PathDialogPurpose purpose = *waiting_;
    if (outcome != PathDialogOutcome::Failed) waiting_.reset();
    return purpose;
}

} // namespace Gyo::Tools::UiEditor
