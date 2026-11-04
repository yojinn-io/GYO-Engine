#pragma once

#include <optional>
#include <string>

// The window (variant gui): the model list, Engine's view of a model, the 3D
// preview and playback. Built only in the gui variant; the cli variant says
// so and returns 3.
namespace Gyo::Tools::ModelViewer {

struct WindowOptions final {
    std::optional<std::string> assetRoot;    // UTF-8; none: choose one in the window
    std::optional<std::string> assetCatalog; // UTF-8; with assetRoot
    std::string gpuDriver{"auto"};
};

[[nodiscard]] int RunViewerWindow(const WindowOptions& options);

} // namespace Gyo::Tools::ModelViewer
