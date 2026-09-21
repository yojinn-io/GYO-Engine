#include "gyo/ui_editor/UndoStack.hpp"

#include <algorithm>
#include <utility>

namespace Gyo::Tools::UiEditor {

UndoStack::UndoStack(const std::size_t capacity)
    : capacity_(std::max<std::size_t>(capacity, 2U)) {}

void UndoStack::Reset(std::string initialState) {
    entries_.clear();
    entries_.push_back({"Open", std::move(initialState)});
    cursor_ = 0U;
    savedState_ = entries_.front().state;
    transaction_.reset();
}

void UndoStack::MarkSaved() {
    savedState_ = Current();
}

void UndoStack::MarkUnsaved() {
    savedState_.clear();
}

bool UndoStack::IsDirty() const noexcept {
    return Current() != savedState_;
}

bool UndoStack::CanUndo() const noexcept {
    return !entries_.empty() && cursor_ > 0U && !transaction_.has_value();
}

bool UndoStack::CanRedo() const noexcept {
    return !entries_.empty() && cursor_ + 1U < entries_.size() &&
           !transaction_.has_value();
}

const std::string& UndoStack::Current() const noexcept {
    static const std::string empty;
    if (transaction_.has_value()) {
        return transaction_->current;
    }
    return entries_.empty() ? empty : entries_[cursor_].state;
}

void UndoStack::Apply(std::string label, std::string state) {
    if (transaction_.has_value()) {
        return;
    }
    Push(std::move(label), std::move(state));
}

void UndoStack::BeginTransaction(std::string label) {
    if (transaction_.has_value()) {
        return;
    }
    transaction_ = Transaction{
        std::move(label),
        Current(),
        Current(),
    };
}

void UndoStack::UpdateTransaction(std::string state) {
    if (transaction_.has_value()) {
        transaction_->current = std::move(state);
    }
}

void UndoStack::CommitTransaction() {
    if (!transaction_.has_value()) {
        return;
    }
    Transaction transaction = std::move(*transaction_);
    transaction_.reset();
    if (transaction.current != transaction.original) {
        Push(std::move(transaction.label), std::move(transaction.current));
    }
}

void UndoStack::CancelTransaction() {
    transaction_.reset();
}

bool UndoStack::HasTransaction() const noexcept {
    return transaction_.has_value();
}

const std::string* UndoStack::Undo() {
    if (!CanUndo()) {
        return nullptr;
    }
    --cursor_;
    return &entries_[cursor_].state;
}

const std::string* UndoStack::Redo() {
    if (!CanRedo()) {
        return nullptr;
    }
    ++cursor_;
    return &entries_[cursor_].state;
}

void UndoStack::Push(std::string label, std::string state) {
    if (!entries_.empty() && state == entries_[cursor_].state) {
        return;
    }
    if (cursor_ + 1U < entries_.size()) {
        entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(cursor_ + 1U),
                       entries_.end());
    }
    entries_.push_back({std::move(label), std::move(state)});
    cursor_ = entries_.size() - 1U;
    TrimToCapacity();
}

void UndoStack::TrimToCapacity() {
    if (entries_.size() <= capacity_) {
        return;
    }
    const std::size_t removeCount = entries_.size() - capacity_;
    entries_.erase(
        entries_.begin(),
        entries_.begin() + static_cast<std::ptrdiff_t>(removeCount));
    cursor_ -= std::min(cursor_, removeCount);
}

} // namespace Gyo::Tools::UiEditor
