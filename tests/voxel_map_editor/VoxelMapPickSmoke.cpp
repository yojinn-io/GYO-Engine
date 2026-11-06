// Judgment W4 of the v8-tool-voxel-map-editor-window declaration
// (docs/tools/plans/v8/voxel-map-editor.md): the editor's picking agrees with
// what Engine drew. A cube is drawn on the framework's offscreen preview and
// read back; at every sampled pixel away from the cube's outline, Pick hits the
// cube exactly where the frame shows it.
#include "window/PreviewScene.hpp"

#include "gyo/voxel_map_editor/Editing.hpp"

#include "gyo/framework/PreviewCamera.hpp"

#include "engine/base/Error.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace Gyo::Tools::VoxelMapEditor;

namespace {

bool CubeAt(const Engine::Render::TextureReadback& frame, const std::uint32_t x, const std::uint32_t y) {
    const std::size_t offset = static_cast<std::size_t>(y) * frame.rowPitch + static_cast<std::size_t>(x) * 4U;
    const int red = std::to_integer<int>(frame.bytes[offset]);
    const int green = std::to_integer<int>(frame.bytes[offset + 1]);
    return red > green + 60;
}

} // namespace

int main(int argc, char** argv) {
    std::string driver = "auto";
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) != "--gpu-driver" || index + 1 >= argc) {
            std::cerr << "usage: gyo_voxel_map_editor_pick_smoke [--gpu-driver auto|d3d12|vulkan|metal]\n";
            return 2;
        }
        driver = argv[++index];
    }
    Engine::Platform::Sdl::SdlPlatformOptions options;
    options.title = "voxel map editor pick smoke";
    options.width = 128;
    options.height = 96;
    options.resizable = false;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(options);
    if (!platform) {
        std::cerr << "platform: " << Engine::Base::Describe(platform.error()) << '\n';
        return 1;
    }
    auto scene = PreviewScene::Create(*platform.value(), GYO_TEST_SHADER_BUNDLE, driver);
    if (!scene) {
        std::cerr << "preview: " << Engine::Base::Describe(scene.error()) << '\n';
        return 1;
    }
    // One red cube, with an offset origin and cell size, so the world mapping is part of the check.
    GridDocument document = NewGrid();
    document.blocks[0].color = {230, 20, 20, 255};
    document.origin = {3, 0, -2};
    document.cellSize = 0.5F;
    document.Set({1, 2, 1}, {1, 0});
    auto preview = MapPreview::Create(document);
    if (!preview || !scene.value()->Apply(preview->TakeChanges())) {
        std::cerr << "the cube cannot be shown\n";
        return 1;
    }
    constexpr std::uint32_t width = 320;
    constexpr std::uint32_t height = 200;
    int failures = 0;
    int checked = 0;
    for (const auto& [yaw, pitch] : std::array<std::array<float, 2>, 3>{{{0.6F, 0.7F}, {-2.2F, 0.3F}, {2.9F, -0.4F}}}) {
        Gyo::Tools::Framework::OrbitCamera orbit;
        orbit.target = CellCorner(document, {1, 2, 1}) + Engine::Math::Vec3{0.25F, 0.25F, 0.25F};
        orbit.yawRadians = yaw;
        orbit.pitchRadians = pitch;
        orbit.distance = 2.0F;
        const auto camera = orbit.Camera();
        const CameraPose pose{camera.position, camera.rotationRadians, camera.verticalFieldOfViewRadians,
                              camera.nearClip, camera.farClip};
        auto frame = scene.value()->Render(camera, width, height);
        if (!frame) {
            std::cerr << "render: " << Engine::Base::Describe(frame.error()) << '\n';
            return 1;
        }
        int inside = 0;
        int outside = 0;
        for (std::uint32_t y = 2; y + 2 < height; y += 3)
            for (std::uint32_t x = 2; x + 2 < width; x += 3) {
                const bool drawn = CubeAt(frame.value(), x, y);
                // Away from the outline: the 5 x 5 pixels around agree.
                bool edge = false;
                for (std::uint32_t dy = 0; dy < 5 && !edge; ++dy)
                    for (std::uint32_t dx = 0; dx < 5 && !edge; ++dx)
                        edge = CubeAt(frame.value(), x + dx - 2, y + dy - 2) != drawn;
                if (edge) continue;
                const auto ray = PixelRay(pose, static_cast<int>(width), static_cast<int>(height),
                                          static_cast<float>(x), static_cast<float>(y));
                const auto hit = Pick(document, ray, -1000);
                const bool picked = hit && !hit->onPlane && hit->cell == Vec3i{1, 2, 1};
                if (picked != drawn) {
                    ++failures;
                    if (failures <= 10)
                        std::cerr << "yaw " << yaw << " pixel " << x << ',' << y << ": drawn " << drawn << ", picked "
                                  << picked << '\n';
                }
                (drawn ? inside : outside) += 1;
            }
        std::cout << "yaw " << yaw << " pitch " << pitch << ": " << inside << " inside, " << outside << " outside\n";
        if (inside < 9 || outside < 9) {
            std::cerr << "too few pixels on one side of the outline\n";
            return 1;
        }
        checked += inside + outside;
    }
    std::cout << checked << " pixels checked, " << failures << " disagree\n";
    return failures == 0 ? 0 : 1;
}
