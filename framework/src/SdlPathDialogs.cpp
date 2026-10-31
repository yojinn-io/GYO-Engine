#include "gyo/framework/SdlPathDialogs.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <utility>

namespace Gyo::Tools::Framework {
namespace {

// Owned by one pending dialog and deleted by its callback. It keeps the inbox,
// the filters and the default location alive for as long as SDL may use them.
struct PendingDialog final {
    std::shared_ptr<PathDialogInbox> inbox;
    PathDialogRequest request;
    std::vector<SDL_DialogFileFilter> filters; // points into request.filters
};

void SDLCALL OnDialogClosed(void* userdata, const char* const* files, int) {
    std::unique_ptr<PendingDialog> pending(static_cast<PendingDialog*>(userdata));
    PathDialogAnswer answer{.tag = pending->request.tag};
    if (files == nullptr) {
        answer.outcome = PathDialogOutcome::Failed;
        answer.error = SDL_GetError();
    } else if (files[0] == nullptr) {
        answer.outcome = PathDialogOutcome::Cancelled;
    } else {
        answer.outcome = PathDialogOutcome::Chosen;
        answer.path = files[0];
    }
    pending->inbox->Post(std::move(answer));
}

} // namespace

SdlPathDialogs::SdlPathDialogs(SDL_Window* window)
    : window_(window),
      inbox_(std::make_shared<PathDialogInbox>()) {}

void SdlPathDialogs::Show(PathDialogRequest request) {
    auto pending = std::make_unique<PendingDialog>(PendingDialog{inbox_, std::move(request), {}});
    for (const PathDialogFilter& filter : pending->request.filters) {
        pending->filters.push_back({filter.name.c_str(), filter.pattern.c_str()});
    }
    const std::string& defaultLocation = pending->request.defaultLocation;
    const char* location = defaultLocation.empty() ? nullptr : defaultLocation.c_str();
    const SDL_DialogFileFilter* filters = pending->filters.empty() ? nullptr : pending->filters.data();
    const int filterCount = static_cast<int>(pending->filters.size());
    const PathDialogKind kind = pending->request.kind;
    // From here SDL owns the request until OnDialogClosed runs, which may be
    // before the Show call returns.
    void* userdata = pending.release();
    switch (kind) {
    case PathDialogKind::OpenFile:
        SDL_ShowOpenFileDialog(OnDialogClosed, userdata, window_, filters, filterCount, location, false);
        break;
    case PathDialogKind::SaveFile:
        SDL_ShowSaveFileDialog(OnDialogClosed, userdata, window_, filters, filterCount, location);
        break;
    case PathDialogKind::OpenFolder:
        SDL_ShowOpenFolderDialog(OnDialogClosed, userdata, window_, location, false);
        break;
    }
}

std::vector<PathDialogAnswer> SdlPathDialogs::TakeAnswers() {
    return inbox_->Take();
}

} // namespace Gyo::Tools::Framework
