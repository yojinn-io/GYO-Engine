#include "gyo/ui_editor/SdlPathDialogs.hpp"

#include <string>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

namespace Sdl = Engine::Platform::Sdl;

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

Sdl::PathDialogRequest PathDialogRequestFor(const PathDialogPurpose purpose, const std::string_view defaultLocation) {
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

PathDialogResult PathDialogResultFor(PathDialogRequests& requests, Sdl::PathDialogAnswer answer) {
    const PathDialogOutcome outcome = OutcomeOf(answer.outcome);
    const auto purpose = requests.Accept(answer.tag, outcome);
    return {.purpose = purpose.value_or(PathDialogPurpose::OpenDocument),
            .outcome = outcome,
            .path = std::move(answer.path),
            .error = std::move(answer.error),
            .current = purpose.has_value()};
}

SdlPathDialogs::SdlPathDialogs(Sdl::SdlPlatform& platform)
    : platform_(platform) {}

void SdlPathDialogs::Show(const PathDialogPurpose purpose, const std::string_view defaultLocation) {
    Sdl::PathDialogRequest request = PathDialogRequestFor(purpose, defaultLocation);
    request.tag = requests_.Begin(purpose);
    platform_.ShowPathDialog(std::move(request));
}

std::vector<PathDialogResult> SdlPathDialogs::TakeResults() {
    std::vector<PathDialogResult> results;
    for (Sdl::PathDialogAnswer& answer : platform_.TakePathDialogAnswers()) {
        results.push_back(PathDialogResultFor(requests_, std::move(answer)));
    }
    return results;
}

void SdlPathDialogs::Abandon() {
    requests_.Abandon();
}

} // namespace Gyo::Tools::UiEditor
