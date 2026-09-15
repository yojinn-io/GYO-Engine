#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Gyo::Tools::UiEditor {

// Snapshot-based on purpose: UI documents are small authoring files, and a
// complete snapshot makes hierarchy reparent/delete operations exactly undoable.
class UndoStack final {
public:
    explicit UndoStack(std::size_t capacity = 256U);

    void Reset(std::string initialState);
    void MarkSaved();
    void MarkUnsaved();

    [[nodiscard]] bool IsDirty() const noexcept;
    [[nodiscard]] bool CanUndo() const noexcept;
    [[nodiscard]] bool CanRedo() const noexcept;
    [[nodiscard]] const std::string& Current() const noexcept;

    void Apply(std::string label, std::string state);

    void BeginTransaction(std::string label);
    void UpdateTransaction(std::string state);
    void CommitTransaction();
    void CancelTransaction();
    [[nodiscard]] bool HasTransaction() const noexcept;

    [[nodiscard]] const std::string* Undo();
    [[nodiscard]] const std::string* Redo();

private:
    struct Entry final {
        std::string label;
        std::string state;
    };

    struct Transaction final {
        std::string label;
        std::string original;
        std::string current;
    };

    void Push(std::string label, std::string state);
    void TrimToCapacity();

    std::size_t capacity_{256U};
    std::vector<Entry> entries_;
    std::size_t cursor_{};
    std::string savedState_;
    std::optional<Transaction> transaction_;
};

} // namespace Gyo::Tools::UiEditor
