// Composing a frame from its views (Compose.hpp).
#include "engine/render/Compose.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "engine/render/Tonemapping.hpp"

namespace GYO::Engine::Render {

namespace {

// A camera's viewport in whole pixels of its target.
PixelRect PixelsOf(const Camera::Viewport& vp) noexcept {
    return PixelRect{static_cast<std::uint32_t>(vp.position.x), static_cast<std::uint32_t>(vp.position.y),
                     static_cast<std::uint32_t>(vp.size.x), static_cast<std::uint32_t>(vp.size.y)};
}

} // namespace

void ComposePasses(const ExtractedCameras& cameras, CameraPasses& passes, RenderCommands& out) {
    out.retained.insert(out.retained.end(), cameras.retained.begin(), cameras.retained.end());
    for (RenderPass& pass : passes.beforeCameras) out.passes.push_back(std::move(pass));
    passes.beforeCameras.clear();
    // The targets of the cameras before this one (a camera that drew nothing still had its turn on its target), and
    // the HDR pictures they drew on.
    std::vector<std::optional<TextureId>> cameraTargets;
    std::vector<TextureId> pictures;
    for (std::size_t i = 0; i < cameras.cameras.size(); ++i) {
        const ExtractedCamera& camera = cameras.cameras[i];
        const bool first = std::find(cameraTargets.begin(), cameraTargets.end(), camera.target) == cameraTargets.end() &&
                           std::none_of(out.passes.begin(), out.passes.end(),
                                        [&](const RenderPass& pass) { return WritesOf(pass).WritesColour(camera.target); });
        cameraTargets.push_back(camera.target);
        // A Hdr camera's tonemapping replaces its viewport on its target: only the first camera there clears it (the
        // bars). Its picture is cleared whole by the first camera of the frame on it (to transparent black without a
        // colour: what it draws over is what earlier cameras drew this frame, and the first has none) and by a camera
        // with a colour (Bevy's composition.rs:4-6): drawn every frame before its tonemapping samples it (RP-3).
        if (camera.hdr) {
            const bool firstOnPicture = std::find(pictures.begin(), pictures.end(), *camera.hdr) == pictures.end();
            pictures.push_back(*camera.hdr);
            if (camera.clear && first) {
                Pass2D clear;
                clear.target = camera.target;
                clear.clear = camera.clear;
                out.passes.push_back(std::move(clear));
            }
            if (firstOnPicture || camera.clear) {
                Pass2D clear;
                clear.target = camera.hdr;
                clear.clear = camera.clear.value_or(Color8{0, 0, 0, 0});
                out.passes.push_back(std::move(clear));
            }
        } else if (camera.clear) {
            Pass2D clear;
            clear.target = camera.target;
            if (first) {
                clear.clear = camera.clear;
            } else {
                const Camera::Viewport& vp = camera.computed.viewport;
                clear.viewport = PixelsOf(vp);
                const auto w = static_cast<float>(vp.size.x);
                const auto h = static_cast<float>(vp.size.y);
                const Color8 c = *camera.clear;
                clear.vertices = {{{0.0F, 0.0F}, {}, c}, {{w, 0.0F}, {}, c}, {{w, h}, {}, c}, {{0.0F, h}, {}, c}};
                clear.indices = {0, 1, 2, 0, 2, 3};
                clear.draws.push_back(Draw2D{TextureId{}, BlendMode::Opaque, std::nullopt, 0, 6, 0});
            }
            out.passes.push_back(std::move(clear));
        }
        CameraPhases none;
        CameraPhases& phases = i < passes.byCamera.size() ? passes.byCamera[i] : none;
        std::size_t index = 0;
        for (std::vector<RenderPass>& phase : phases.InOrder()) {
            // A Hdr camera's tonemapping is its last pass before Overlay (Bevy's tonemapping in PostProcess, the UI
            // after it): the Overlay draws bytes on its target (RP-4).
            if (static_cast<CameraPhase>(index++) == CameraPhase::Overlay && camera.hdr) {
                out.passes.push_back(TonemappingPass(*camera.hdr, camera.target, PixelsOf(camera.computed.viewport), camera.tonemapping));
            }
            for (RenderPass& pass : phase) out.passes.push_back(std::move(pass));
            phase.clear();
        }
    }
}

} // namespace GYO::Engine::Render
