#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Gyo::Tools::UiEditor {

// What a chosen path is for. The editor acts on a result by its purpose; the
// OS dialog only returns a path with the request's tag.
enum class PathDialogPurpose {
    OpenDocument,    // an existing UI JSON file
    ExportDocument,  // a UI JSON file to write
    AssetFolder,     // an asset root to mount with its content.json
    AssetRootField,  // fills the Asset Root field of the mount dialog
    CatalogField,    // fills the Catalog field of the mount dialog
};

enum class PathDialogOutcome {
    Chosen,
    Cancelled,
    Failed,
};

struct PathDialogResult final {
    PathDialogPurpose purpose{};
    PathDialogOutcome outcome{};
    std::string path;   // UTF-8, set when Chosen
    std::string error;  // set when Failed
    // False when the answer is for a request the editor no longer waits for
    // (abandoned, superseded or already answered); purpose is then meaningless.
    bool current{true};
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
    // Stops waiting for the dialog last shown: an answer that still comes is
    // returned with current == false. The OS dialog itself stays open.
    virtual void Abandon() = 0;
};

// Which dialog answer the editor still waits for. The OS answers a request at
// most once, may never answer, and on Windows may follow a Failed answer with
// one more (docs/architecture/platform-sdl.md, Design Invariant 10). Each Begin
// starts a request with a new tag, and only the latest request's answers
// count: Chosen and Cancelled end it; Failed does not, so the answer that
// follows a Failed one still applies; Abandon ends it without an answer.
class PathDialogRequests final {
public:
    // The tag to send with the request.
    [[nodiscard]] std::uint64_t Begin(PathDialogPurpose purpose) noexcept;
    void Abandon() noexcept;
    // The purpose of the request the answer belongs to, or nothing when it
    // answers no request the editor waits for.
    [[nodiscard]] std::optional<PathDialogPurpose> Accept(std::uint64_t tag, PathDialogOutcome outcome) noexcept;

private:
    std::uint64_t lastTag_{};
    std::optional<PathDialogPurpose> waiting_;
};

} // namespace Gyo::Tools::UiEditor
