#include "SdlPathDialogs.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace Gyo::Tools::UiEditor {
namespace {

Framework::PathDialogRequest RequestFor(const PathDialogPurpose purpose, const std::string_view defaultLocation) {
    Framework::PathDialogRequest request;
    request.tag = static_cast<std::uint32_t>(purpose);
    request.defaultLocation = std::string(defaultLocation);
    switch (purpose) {
    case PathDialogPurpose::OpenDocument:
        request.kind = Framework::PathDialogKind::OpenFile;
        request.filters = {{"UI JSON", "json"}};
        break;
    case PathDialogPurpose::ExportDocument:
        request.kind = Framework::PathDialogKind::SaveFile;
        request.filters = {{"UI JSON", "json"}};
        break;
    case PathDialogPurpose::CatalogField:
        request.kind = Framework::PathDialogKind::OpenFile;
        request.filters = {{"Asset catalog JSON", "json"}};
        break;
    case PathDialogPurpose::AssetFolder:
    case PathDialogPurpose::AssetRootField:
        request.kind = Framework::PathDialogKind::OpenFolder;
        break;
    }
    return request;
}

} // namespace

SdlPathDialogs::SdlPathDialogs(SDL_Window* window)
    : dialogs_(window) {}

void SdlPathDialogs::Show(const PathDialogPurpose purpose, const std::string_view defaultLocation) {
    dialogs_.Show(RequestFor(purpose, defaultLocation));
}

std::vector<PathDialogResult> SdlPathDialogs::TakeResults() {
    std::vector<PathDialogResult> results;
    for (Framework::PathDialogAnswer& answer : dialogs_.TakeAnswers()) {
        results.push_back({static_cast<PathDialogPurpose>(answer.tag), answer.outcome, std::move(answer.path),
                           std::move(answer.error)});
    }
    return results;
}

} // namespace Gyo::Tools::UiEditor
