// Composing a frame from its views (Compose.hpp).
#include "engine/render/Compose.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace GYO::Engine::Render {

void ComposePasses(const ExtractedCameras& cameras, CameraPasses& passes, RenderCommands& out) {
    for (RenderPass& pass : passes.beforeCameras) out.passes.push_back(std::move(pass));
    passes.beforeCameras.clear();
    // The targets of the cameras before this one (a camera that drew nothing still had its turn on its target).
    std::vector<std::optional<TextureId>> cameraTargets;
    for (std::size_t i = 0; i < cameras.cameras.size(); ++i) {
        const ExtractedCamera& camera = cameras.cameras[i];
        const bool first = std::find(cameraTargets.begin(), cameraTargets.end(), camera.target) == cameraTargets.end() &&
                           std::none_of(out.passes.begin(), out.passes.end(),
                                        [&](const RenderPass& pass) { return WritesOf(pass).WritesColour(camera.target); });
        cameraTargets.push_back(camera.target);
        if (camera.clear) {
            Pass2D clear;
            clear.target = camera.target;
            if (first) {
                clear.clear = camera.clear;
            } else {
                const Camera::Viewport& vp = camera.computed.viewport;
                clear.viewport = PixelRect{static_cast<std::uint32_t>(vp.position.x), static_cast<std::uint32_t>(vp.position.y),
                                           static_cast<std::uint32_t>(vp.size.x), static_cast<std::uint32_t>(vp.size.y)};
                const auto w = static_cast<float>(vp.size.x);
                const auto h = static_cast<float>(vp.size.y);
                const Color8 c = *camera.clear;
                clear.vertices = {{{0.0F, 0.0F}, {}, c}, {{w, 0.0F}, {}, c}, {{w, h}, {}, c}, {{0.0F, h}, {}, c}};
                clear.indices = {0, 1, 2, 0, 2, 3};
                clear.draws.push_back(Draw2D{TextureId{}, BlendMode::Opaque, std::nullopt, 0, 6, 0});
            }
            out.passes.push_back(std::move(clear));
        }
        if (i < passes.byCamera.size()) {
            for (std::vector<RenderPass>& phase : passes.byCamera[i].InOrder()) {
                for (RenderPass& pass : phase) out.passes.push_back(std::move(pass));
                phase.clear();
            }
        }
    }
}

} // namespace GYO::Engine::Render
