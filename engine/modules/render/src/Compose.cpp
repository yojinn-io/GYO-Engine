// Composing a frame from its views (Compose.hpp).
#include "engine/render/Compose.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "engine/render/ShaderAbi.hpp"

namespace GYO::Engine::Render {

namespace {

// A camera's viewport in whole pixels of its target.
PixelRect PixelsOf(const Camera::Viewport& vp) noexcept {
    return PixelRect{static_cast<std::uint32_t>(vp.position.x), static_cast<std::uint32_t>(vp.position.y),
                     static_cast<std::uint32_t>(vp.size.x), static_cast<std::uint32_t>(vp.size.y)};
}

// A pass that only clears target, whole (a GPU clear).
Pass2D ClearOnly(const std::optional<TextureId>& target, const Color8 colour) {
    Pass2D pass;
    pass.target = target;
    pass.clear = colour;
    return pass;
}

void Append(const std::span<std::vector<RenderPass>> phases, RenderCommands& out) {
    for (std::vector<RenderPass>& phase : phases) {
        for (RenderPass& pass : phase) out.passes.push_back(std::move(pass));
        phase.clear();
    }
}

} // namespace

void ComposePasses(const ExtractedCameras& cameras, CameraPasses& passes, RenderCommands& out) {
    out.retained.insert(out.retained.end(), cameras.retained.begin(), cameras.retained.end());
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
        // The camera's clear on its target. A later Hdr camera with a colour needs none: its picture is cleared to the
        // colour, and its tonemapping puts that opaque viewport over the target.
        if (camera.clear && first) {
            out.passes.push_back(ClearOnly(camera.target, *camera.clear));
        } else if (camera.clear && !camera.hdr) {
            Pass2D clear;
            clear.target = camera.target;
            const Camera::Viewport& vp = camera.computed.viewport;
            clear.viewport = PixelsOf(vp);
            const auto w = static_cast<float>(vp.size.x);
            const auto h = static_cast<float>(vp.size.y);
            const Color8 c = *camera.clear;
            clear.vertices = {{{0.0F, 0.0F}, {}, c}, {{w, 0.0F}, {}, c}, {{w, h}, {}, c}, {{0.0F, h}, {}, c}};
            clear.indices = {0, 1, 2, 0, 2, 3};
            clear.draws.push_back(Draw2D{TextureId{}, BlendMode::Opaque, std::nullopt, 0, 6, 0});
            out.passes.push_back(std::move(clear));
        }
        // A Hdr camera's picture, its own, cleared whole every frame (drawn before its tonemapping samples it, RP-3).
        if (camera.hdr) out.passes.push_back(ClearOnly(camera.hdr, camera.clear.value_or(kTransparentBlack)));
        CameraPhases none;
        CameraPhases& phases = i < passes.byCamera.size() ? passes.byCamera[i] : none;
        Append(phases.Through(CameraPhase::PostProcess), out);
        if (camera.hdr) {
            out.passes.push_back(TonemappingPass(*camera.hdr, camera.target, PixelsOf(camera.computed.viewport), camera.tonemapping,
                                                 first ? BlendMode::Opaque : BlendMode::Alpha));
        }
        Append(phases.After(CameraPhase::PostProcess), out);
    }
}

std::optional<TextureId> PhaseTarget(const ExtractedCamera& camera, const CameraPhase phase) noexcept {
    return camera.hdr && phase <= CameraPhase::PostProcess ? camera.hdr : camera.target;
}

PassFullscreen TonemappingPass(const TextureId picture, std::optional<TextureId> output, std::optional<PixelRect> viewport,
                               const Tonemapping::Method method, const BlendMode blend) {
    PassFullscreen pass;
    pass.target = std::move(output);
    pass.viewport = std::move(viewport);
    pass.shader = std::string(ShaderAbi::ShaderName(ShaderAbi::kTonemappingFragmentSource));
    pass.inputs = {picture};
    const ShaderAbi::TonemappingParameters parameters = ShaderAbi::ToParameters(static_cast<std::uint8_t>(method));
    pass.parameters.resize(sizeof(parameters));
    std::memcpy(pass.parameters.data(), &parameters, sizeof(parameters));
    pass.blend = blend;
    return pass;
}

} // namespace GYO::Engine::Render
