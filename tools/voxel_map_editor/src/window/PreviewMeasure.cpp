#include "window/EditorWindow.hpp"
#include "window/PreviewScene.hpp"

#include "gyo/voxel_map_editor/Editing.hpp"

#include "gyo/framework/PreviewCamera.hpp"
#include "gyo/framework/PreviewTexture.hpp"

#include "engine/base/Error.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

// P1 of the trial batch (docs/tools/plans/v8/voxel-map-editor.md): per frame
// one cell edit (placed, then erased, in turn), the whole build with Engine's
// read-back (PreviewGeometry), the mesh and palette upload, Engine's drawing
// on the framework's offscreen preview, the readback (waiting for the GPU) and
// the upload into a texture of the window's SDL renderer: what the window does
// for an edit but ImGui.
namespace Gyo::Tools::VoxelMapEditor {
namespace {

namespace Math = Engine::Math;

// 64 x 16 x 64 cells, 30% filled with four blocks of the default shapes in
// every orientation. std::minstd_rand and a modulo give the same grid on
// every platform.
GridDocument SyntheticGrid() {
    GridDocument document = NewGrid();
    document.blocks = {{"grey", 0, {150, 150, 150, 255}},
                       {"red", 0, {190, 70, 60, 255}},
                       {"slab", 1, {90, 140, 190, 255}},
                       {"stair", 2, {200, 180, 90, 255}}};
    std::minstd_rand random(20261106U);
    for (std::int32_t y = 0; y < 16; ++y)
        for (std::int32_t z = 0; z < 64; ++z)
            for (std::int32_t x = 0; x < 64; ++x) {
                if (random() % 100U >= 30U) continue;
                const auto block = static_cast<std::uint16_t>(1U + random() % 4U);
                const auto orientation = static_cast<std::uint8_t>(random() % OrientationCount);
                document.Set({x, y, z}, {block, orientation});
            }
    return document;
}

} // namespace

int RunPreviewMeasure(const MeasureOptions& options) {
    GridDocument document;
    if (options.synthetic) {
        document = SyntheticGrid();
    } else {
        auto bytes = ReadFile(options.gridPath, MaxGridTextBytes + 1);
        if (!bytes) {
            std::cerr << "error: " << Engine::Base::Describe(bytes.error()) << '\n';
            return 3;
        }
        auto parsed = ParseGrid({reinterpret_cast<const char*>(bytes->data()), bytes->size()});
        if (!parsed) {
            std::cerr << "error: " << Engine::Base::Describe(parsed.error()) << '\n';
            return 1;
        }
        document = std::move(*parsed);
    }
    // The edited cell: one above the map's highest chunk, at its first column.
    Vec3i low{0, 0, 0};
    Vec3i high{1, 1, 1};
    for (const auto& [at, chunk] : document.chunks) {
        for (int axis = 0; axis < 3; ++axis) {
            low[axis] = std::min(low[axis], at[axis] * ChunkSize);
            high[axis] = std::max(high[axis], at[axis] * ChunkSize + ChunkSize);
        }
    }
    const Vec3i edited{low[0], high[1], low[2]};

    Engine::Platform::Sdl::SdlPlatformOptions platformOptions;
    platformOptions.title = "GYO Voxel Map Editor preview measure";
    platformOptions.width = 640;
    platformOptions.height = 360;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(platformOptions);
    if (!platform) {
        std::cerr << "error: " << Engine::Base::Describe(platform.error()) << '\n';
        return 3;
    }
    auto renderer = Engine::Render::Backend::Sdl::SdlRenderer::Create(*platform.value(), {false});
    if (!renderer) {
        std::cerr << "error: " << Engine::Base::Describe(renderer.error()) << '\n';
        return 3;
    }
    auto base = Engine::Platform::Sdl::BaseDirectory();
    if (!base) {
        std::cerr << "error: " << Engine::Base::Describe(base.error()) << '\n';
        return 3;
    }
    auto scene = PreviewScene::Create(*platform.value(), base.value() / "shaders" / "builtin", options.gpuDriver);
    if (!scene) {
        std::cerr << "error: " << Engine::Base::Describe(scene.error()) << '\n';
        return 4;
    }
    Framework::OrbitCamera orbit;
    const float size = document.cellSize;
    const Math::Vec3 lowWorld = CellCorner(document, low);
    const Math::Vec3 highWorld = CellCorner(document, high);
    orbit.target = (lowWorld + highWorld) * 0.5F;
    orbit.yawRadians = 0.6F;
    orbit.pitchRadians = 0.7F;
    orbit.distance = std::max(Math::Length(highWorld - lowWorld) * 0.9F, 4.0F * size);
    const auto camera = orbit.Camera();

    EditSession session(std::move(document));
    Framework::PreviewTexture texture;
    std::vector<double> times;
    times.reserve(options.frames);
    std::ostringstream csv;
    csv << "map,frame,vertices,frame_ms\n";
    const std::string name = options.synthetic ? std::string("synthetic") : options.gridPath;
    int failures = 0;
    for (std::uint32_t frame = 0; frame < options.frames; ++frame) {
        static_cast<void>(platform.value()->PumpEvents());
        const auto started = std::chrono::steady_clock::now();
        const Cell value = frame % 2 == 0 ? Cell{1, 0} : Cell{};
        bool ok = static_cast<bool>(session.SetCell(edited, value));
        auto geometry = PreviewGeometry(session.Document());
        ok = ok && geometry && scene.value()->Show(*geometry);
        auto drawn = scene.value()->Render(camera, options.width, options.height);
        ok = ok && drawn && Framework::UploadFrame(*renderer.value(), texture, drawn.value());
        const auto ended = std::chrono::steady_clock::now();
        if (!ok) {
            ++failures;
            continue;
        }
        const double ms = std::chrono::duration<double, std::milli>(ended - started).count();
        times.push_back(ms);
        csv << name << ',' << frame << ',' << geometry->vertices.size() << ',' << ms << '\n';
        // The window shows the texture each frame; so does this.
        static_cast<void>(renderer.value()->Clear({0, 0, 0, 255}));
        static_cast<void>(renderer.value()->Present());
    }
    if (texture.handle) static_cast<void>(renderer.value()->ReleaseTexture(texture.handle));
    std::vector<double> sorted = times;
    std::sort(sorted.begin(), sorted.end());
    const auto at = [&](const double q) {
        if (sorted.empty()) return 0.0;
        const auto index = static_cast<std::size_t>(std::ceil(q * static_cast<double>(sorted.size()))) - 1;
        return sorted[std::min(index, sorted.size() - 1)];
    };
    std::printf("%s frames=%zu/%u p50=%.3f p95=%.3f p99=%.3f max=%.3f ms\n", name.c_str(), times.size(), options.frames,
                at(0.5), at(0.95), at(0.99), sorted.empty() ? 0.0 : sorted.back());
    if (auto written = WriteFileReplacing(options.csvPath, csv.str()); !written) {
        std::cerr << "error: " << Engine::Base::Describe(written.error()) << '\n';
        return 3;
    }
    return failures == 0 ? 0 : 4;
}

} // namespace Gyo::Tools::VoxelMapEditor
