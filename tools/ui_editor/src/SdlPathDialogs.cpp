#include "SdlPathDialogs.hpp"

#include "engine/platform/sdl/SdlPlatform.hpp"

#include <string>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

namespace Sdl = Engine::Platform::Sdl;

Sdl::PathDialogRequest RequestFor(const PathDialogPurpose purpose, const std::string_view defaultLocation) {
    Sdl::PathDialogRequest request;
    request.defaultLocation = std::string(defaultLocation);
    switch (purpose) {
    case PathDialogPurpose::OpenDocument:
        request.kind = Sdl::PathDialogKind::OpenFile;
        request.filters = {{"UI JSON", "json"}};
        break;
    case PathDialogPurpose::ExportDocument:
        request.kind = Sdl::PathDialogKind::SaveFile;
        request.filters = {{"UI JSON", "json"}};
        break;
    case PathDialogPurpose::CatalogField:
        request.kind = Sdl::PathDialogKind::OpenFile;
        request.filters = {{"Asset catalog JSON", "json"}};
        break;
    case PathDialogPurpose::AssetFolder:
    case PathDialogPurpose::AssetRootField:
        request.kind = Sdl::PathDialogKind::OpenFolder;
        break;
    }
    return request;
}

PathDialogOutcome OutcomeOf(const Sdl::PathDialogOutcome outcome) noexcept {
    switch (outcome) {
    case Sdl::PathDialogOutcome::Chosen:
        return PathDialogOutcome::Chosen;
    case Sdl::PathDialogOutcome::Cancelled:
        return PathDialogOutcome::Cancelled;
    case Sdl::PathDialogOutcome::Failed:
        break;
    }
    return PathDialogOutcome::Failed;
}

} // namespace

SdlPathDialogs::SdlPathDialogs(Sdl::SdlPlatform& platform)
    : platform_(platform) {}

void SdlPathDialogs::Show(const PathDialogPurpose purpose, const std::string_view defaultLocation) {
    Sdl::PathDialogRequest request = RequestFor(purpose, defaultLocation);
    request.tag = requests_.Begin(purpose);
    platform_.ShowPathDialog(std::move(request));
}

std::vector<PathDialogResult> SdlPathDialogs::TakeResults() {
    std::vector<PathDialogResult> results;
    for (Sdl::PathDialogAnswer& answer : platform_.TakePathDialogAnswers()) {
        const PathDialogOutcome outcome = OutcomeOf(answer.outcome);
        const auto purpose = requests_.Accept(answer.tag, outcome);
        results.push_back({.purpose = purpose.value_or(PathDialogPurpose::OpenDocument),
                           .outcome = outcome,
                           .path = std::move(answer.path),
                           .error = std::move(answer.error),
                           .current = purpose.has_value()});
    }
    return results;
}

void SdlPathDialogs::Abandon() {
    requests_.Abandon();
}

} // namespace Gyo::Tools::UiEditor
