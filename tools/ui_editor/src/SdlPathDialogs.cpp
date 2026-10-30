#include "SdlPathDialogs.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

// SDL reads the filters until the callback runs, so they are static.
constexpr SDL_DialogFileFilter kJsonFilters[]{
    {"UI JSON", "json"},
};
constexpr SDL_DialogFileFilter kCatalogFilters[]{
    {"Asset catalog JSON", "json"},
};

// Owned by one pending dialog and deleted by its callback. It keeps the inbox
// and the default location alive for as long as SDL may use them.
struct PendingDialog final {
    std::shared_ptr<PathDialogInbox> inbox;
    PathDialogPurpose purpose{};
    std::string defaultLocation;
};

void SDLCALL OnDialogClosed(void* userdata, const char* const* files, int) {
    std::unique_ptr<PendingDialog> pending(static_cast<PendingDialog*>(userdata));
    PathDialogResult result{.purpose = pending->purpose};
    if (files == nullptr) {
        result.outcome = PathDialogOutcome::Failed;
        result.error = SDL_GetError();
    } else if (files[0] == nullptr) {
        result.outcome = PathDialogOutcome::Cancelled;
    } else {
        result.outcome = PathDialogOutcome::Chosen;
        result.path = files[0];
    }
    pending->inbox->Post(std::move(result));
}

} // namespace

SdlPathDialogs::SdlPathDialogs(SDL_Window* window)
    : window_(window),
      inbox_(std::make_shared<PathDialogInbox>()) {}

void SdlPathDialogs::Show(const PathDialogPurpose purpose, const std::string_view defaultLocation) {
    auto pending = std::make_unique<PendingDialog>(
        PendingDialog{inbox_, purpose, std::string(defaultLocation)});
    const char* location = pending->defaultLocation.empty() ? nullptr : pending->defaultLocation.c_str();
    // From here SDL owns the request until OnDialogClosed runs, which may be
    // before the Show call returns.
    void* userdata = pending.release();
    switch (purpose) {
    case PathDialogPurpose::OpenDocument:
        SDL_ShowOpenFileDialog(OnDialogClosed, userdata, window_, kJsonFilters, SDL_arraysize(kJsonFilters),
            location, false);
        break;
    case PathDialogPurpose::ExportDocument:
        SDL_ShowSaveFileDialog(OnDialogClosed, userdata, window_, kJsonFilters, SDL_arraysize(kJsonFilters),
            location);
        break;
    case PathDialogPurpose::CatalogField:
        SDL_ShowOpenFileDialog(OnDialogClosed, userdata, window_, kCatalogFilters,
            SDL_arraysize(kCatalogFilters), location, false);
        break;
    case PathDialogPurpose::AssetFolder:
    case PathDialogPurpose::AssetRootField:
        SDL_ShowOpenFolderDialog(OnDialogClosed, userdata, window_, location, false);
        break;
    }
}

std::vector<PathDialogResult> SdlPathDialogs::TakeResults() {
    return inbox_->Take();
}

} // namespace Gyo::Tools::UiEditor
