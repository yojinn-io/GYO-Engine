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

// P1 of v8-tool-voxel-window-1 (docs/tools/plans/v8/voxel-map-window-goxel.md):
// per frame, with edit, one cell edit from a fixed sequence (the centre of a
// chunk, then a cell on a chunk's side, chunk after chunk in key order; the
// cell filled when empty and emptied when filled), the rebake of the chunks it
// touched (MapPreview), Engine's expansion and upload of those chunks; then,
// with or without edit, Engine's drawing on the framework's offscreen preview,
// the readback (waiting for the GPU) and the upload into a texture of the
// window's SDL renderer: what the window does for an edit but ImGui.
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
    Vec3i low{0, 0, 0};
    Vec3i high{1, 1, 1};
    std::vector<Vec3i> chunkKeys;
    for (const auto& [at, chunk] : document.chunks) {
        chunkKeys.push_back(at);
        for (int axis = 0; axis < 3; ++axis) {
            low[axis] = std::min(low[axis], at[axis] * ChunkSize);
            high[axis] = std::max(high[axis], at[axis] * ChunkSize + ChunkSize);
        }
    }
    if (chunkKeys.empty()) {
        std::cerr << "error: the grid has no filled cell\n";
        return 1;
    }
    const auto editedCell = [&chunkKeys](const std::uint32_t frame) {
        const auto& key = chunkKeys[(frame / 2) % chunkKeys.size()];
        const std::int32_t x = frame % 2 == 0 ? ChunkSize / 2 : 0; // the centre, then the -X side
        return Vec3i{key[0] * ChunkSize + x, key[1] * ChunkSize + ChunkSize / 2, key[2] * ChunkSize + ChunkSize / 2};
    };

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

    auto preview = MapPreview::Create(document);
    if (!preview) {
        std::cerr << "error: " << Engine::Base::Describe(preview.error()) << '\n';
        return 1;
    }
    // Setting up the map is not measured: the window does it once when it opens.
    if (auto shown = scene.value()->Apply(preview->TakeChanges()); !shown) {
        std::cerr << "error: " << Engine::Base::Describe(shown.error()) << '\n';
        return 4;
    }
    EditSession session(std::move(document));
    Framework::PreviewTexture texture;
    std::vector<double> times;
    times.reserve(options.frames);
    std::ostringstream csv;
    csv << "map,frame,edit,chunks_uploaded,frame_ms\n";
    const std::string name = options.synthetic ? std::string("synthetic") : options.gridPath;
    std::uint32_t failures = 0;
    std::uint32_t measured = 0;
    for (std::uint32_t frame = 0; frame < options.frames; ++frame) {
        static_cast<void>(platform.value()->PumpEvents());
        const auto started = std::chrono::steady_clock::now();
        bool ok = true;
        std::size_t uploaded = 0;
        if (options.edit) {
            const Vec3i cell = editedCell(frame);
            const Cell value = session.Document().At(cell).block == 0 ? Cell{1, 0} : Cell{};
            ok = static_cast<bool>(session.SetCell(cell, value));
            ok = ok && preview->Apply(session.Document(), session.TakeChanges());
            if (ok) {
                const auto changes = preview->TakeChanges();
                uploaded = changes.chunks.size();
                ok = static_cast<bool>(scene.value()->Apply(changes));
            }
        }
        auto drawn = scene.value()->Render(camera, options.width, options.height);
        ok = ok && drawn && Framework::UploadFrame(*renderer.value(), texture, drawn.value());
        const auto ended = std::chrono::steady_clock::now();
        if (!ok) {
            ++failures;
            continue;
        }
        ++measured;
        const double ms = std::chrono::duration<double, std::milli>(ended - started).count();
        if (frame >= options.warmup) times.push_back(ms);
        csv << name << ',' << frame << ',' << (options.edit ? 1 : 0) << ',' << uploaded << ',' << ms << '\n';
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
    std::printf("%s edit=%d requested=%u measured=%u failed=%u warmup=%u counted=%zu p50=%.3f p95=%.3f p99=%.3f "
                "max=%.3f ms chunks=%zu\n",
                name.c_str(), options.edit ? 1 : 0, options.frames, measured, failures, options.warmup, times.size(),
                at(0.5), at(0.95), at(0.99), sorted.empty() ? 0.0 : sorted.back(), scene.value()->ChunkCount());
    if (auto written = WriteFileReplacing(options.csvPath, csv.str()); !written) {
        std::cerr << "error: " << Engine::Base::Describe(written.error()) << '\n';
        return 3;
    }
    return failures == 0 ? 0 : 4;
}

} // namespace Gyo::Tools::VoxelMapEditor
