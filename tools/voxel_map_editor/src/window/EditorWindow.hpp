#pragma once

#include <cstdint>
#include <string>

// The window (variant gui) and the preview's frame-time measure. The cli
// variant links a stand-in that says the build has no window (judgment W8).
namespace Gyo::Tools::VoxelMapEditor {

struct WindowOptions final {
    std::string gridPath; // UTF-8; a new grid when the file does not exist
    std::string meshPath; // UTF-8; written with the grid on every save
    std::string gpuDriver{"auto"};
};

[[nodiscard]] int RunEditorWindow(const WindowOptions& options);

// P1 of the trial batch: per frame, one cell edit, the whole build, Engine's
// expansion, the mesh upload, the drawing, the readback and the upload into a
// texture of the window's renderer. One CSV row per frame; p50/p95/p99/max on
// stdout.
struct MeasureOptions final {
    std::string gridPath;  // UTF-8; empty with synthetic
    bool synthetic{};      // a 64 x 16 x 64 grid, 30% filled, fixed seed
    std::string csvPath;   // UTF-8
    std::uint32_t frames{600};
    std::uint32_t width{1280}, height{720};
    std::string gpuDriver{"auto"};
};

[[nodiscard]] int RunPreviewMeasure(const MeasureOptions& options);

} // namespace Gyo::Tools::VoxelMapEditor
