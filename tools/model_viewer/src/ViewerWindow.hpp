#pragma once

#include <cstdint>
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

// The preview's frame time for every model of a content root (judgment P1 of
// -trial): one CSV row per frame, and p50/p95/p99/max per model on stdout.
struct MeasureOptions final {
    std::string assetRoot;   // UTF-8, a content root
    std::string csvPath;     // UTF-8
    std::uint32_t frames{600};
    std::uint32_t width{1280}, height{720};
    std::string gpuDriver{"auto"};
};

[[nodiscard]] int RunPreviewMeasure(const MeasureOptions& options);

} // namespace Gyo::Tools::ModelViewer
