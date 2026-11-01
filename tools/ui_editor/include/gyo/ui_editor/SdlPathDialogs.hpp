#pragma once

#include "gyo/ui_editor/PathDialog.hpp"

#include "engine/platform/sdl/SdlPlatform.hpp"

#include <string_view>

namespace Gyo::Tools::UiEditor {

// The request for a purpose: its kind and filters, without a tag.
[[nodiscard]] Engine::Platform::Sdl::PathDialogRequest PathDialogRequestFor(PathDialogPurpose purpose,
                                                                            std::string_view defaultLocation);
// What the editor sees of an answer: the purpose of the request it belongs to,
// or current == false when requests waits for no such answer.
[[nodiscard]] PathDialogResult PathDialogResultFor(PathDialogRequests& requests,
                                                   Engine::Platform::Sdl::PathDialogAnswer answer);

// The editor's dialogs on Engine's platform dialogs, with tags from
// PathDialogRequests. Show runs in the editor's frame, never inside an event
// observer. The platform outlives this object.
class SdlPathDialogs final : public PathDialogs {
public:
    explicit SdlPathDialogs(Engine::Platform::Sdl::SdlPlatform& platform);

    void Show(PathDialogPurpose purpose, std::string_view defaultLocation) override;
    [[nodiscard]] std::vector<PathDialogResult> TakeResults() override;
    void Abandon() override;

private:
    Engine::Platform::Sdl::SdlPlatform& platform_;
    PathDialogRequests requests_;
};

} // namespace Gyo::Tools::UiEditor
