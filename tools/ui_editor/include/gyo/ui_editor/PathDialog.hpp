#pragma once

#include "gyo/framework/PathDialog.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace Gyo::Tools::UiEditor {

// What a chosen path is for. The editor acts on a result by its purpose; the
// framework's dialog only returns a path with the purpose as its tag.
enum class PathDialogPurpose {
    OpenDocument,    // an existing UI JSON file
    ExportDocument,  // a UI JSON file to write
    AssetFolder,     // an asset root to mount with its content.json
    AssetRootField,  // fills the Asset Root field of the mount dialog
    CatalogField,    // fills the Catalog field of the mount dialog
};

using PathDialogOutcome = Framework::PathDialogOutcome;

struct PathDialogResult final {
    PathDialogPurpose purpose{};
    PathDialogOutcome outcome{};
    std::string path;   // UTF-8, set when Chosen
    std::string error;  // set when Failed
};

// A host's file dialogs. Show is called on the main thread and returns without
// waiting; the result arrives later through TakeResults. The editor works
// without one (manual path entry only).
class PathDialogs {
public:
    virtual ~PathDialogs() = default;
    // defaultLocation is UTF-8 and may be empty.
    virtual void Show(PathDialogPurpose purpose, std::string_view defaultLocation) = 0;
    [[nodiscard]] virtual std::vector<PathDialogResult> TakeResults() = 0;
};

} // namespace Gyo::Tools::UiEditor
