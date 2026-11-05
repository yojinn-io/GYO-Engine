#include "ViewerWindow.hpp"

#include "FileAccess.hpp"

#include "gyo/model_viewer/ModelContent.hpp"
#include "gyo/model_viewer/Playback.hpp"
#include "gyo/model_viewer/PreviewRenderer.hpp"

#include "engine/base/Error.hpp"
#include "engine/model/Animation.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <span>
#include <sstream>
#include <iostream>
#include <vector>

// The frame time of the preview (judgment P1 of -trial): per frame, the pose
// (SamplePose), Engine's ModelRenderer update and Renderer::Render on the
// offscreen device, the readback (waiting for the GPU) and the upload into a
// texture of the window's SDL renderer: everything the window does for its
// preview but ImGui. The clip is the model's longest, played at speed 1 in
// 60 Hz steps.
namespace Gyo::Tools::ModelViewer {

int RunPreviewMeasure(const MeasureOptions& options) {
    Engine::Platform::Sdl::SdlPlatformOptions platformOptions;
    platformOptions.title = "GYO Model Viewer preview measure";
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
    PreviewOptions previewOptions;
    previewOptions.shaderBundle = base.value() / "shaders" / "builtin";
    previewOptions.width = options.width;
    previewOptions.height = options.height;
    previewOptions.driver = options.gpuDriver;
    auto preview = PreviewRenderer::Create(*platform.value(), previewOptions);
    if (!preview) {
        std::cerr << "error: " << Engine::Base::Describe(preview.error()) << '\n';
        return 4;
    }
    auto content = ModelContent::Mount(std::filesystem::path(std::u8string(options.assetRoot.begin(), options.assetRoot.end())),
                                       std::nullopt);
    if (!content) {
        std::cerr << "error: " << Engine::Base::Describe(content.error()) << '\n';
        return 4;
    }
    // Written once at the end, through engine/io (the OS layer's file rule).
    std::ostringstream csv;
    csv << "model,clip,frame,clip_seconds,frame_ms\n";
    int failures = 0;
    for (const auto* asset : content->Models()) {
        auto loaded = content->LoadModel(asset->id);
        if (!loaded) {
            std::cerr << "error: " << Engine::Base::Describe(loaded.error()) << '\n';
            return 4;
        }
        const auto& model = *loaded->model;
        std::optional<std::size_t> clip;
        for (std::size_t index = 0; index < model.clips.size(); ++index) {
            if (!clip || model.clips[index].durationSeconds > model.clips[*clip].durationSeconds) clip = index;
        }
        PlaybackClock clock;
        clock.durationSeconds = clip ? model.clips[*clip].durationSeconds : 0.0;
        clock.playing = true;
        Engine::Model::Pose pose;
        if (!PreviewPose(model, clip, clock, pose) || !preview.value()->Show(loaded->model, pose)) {
            std::cerr << "error: " << asset->id << ": cannot show the model\n";
            return 4;
        }
        const OrbitCamera camera = FrameModel(model, pose, {});
        PreviewTexture texture;
        std::vector<double> times;
        times.reserve(options.frames);
        for (std::uint32_t frame = 0; frame < options.frames; ++frame) {
            static_cast<void>(platform.value()->PumpEvents());
            const auto started = std::chrono::steady_clock::now();
            bool ok = static_cast<bool>(PreviewPose(model, clip, clock, pose));
            auto drawn = preview.value()->Render(pose, camera);
            ok = ok && drawn && UploadFrame(*renderer.value(), texture, *drawn.value());
            const auto ended = std::chrono::steady_clock::now();
            if (!ok) {
                ++failures;
                continue;
            }
            const double ms = std::chrono::duration<double, std::milli>(ended - started).count();
            times.push_back(ms);
            csv << asset->id << ',' << (clip ? model.clips[*clip].name : std::string()) << ',' << frame << ','
                << clock.seconds << ',' << ms << '\n';
            // The window shows the texture each frame; so does this.
            static_cast<void>(renderer.value()->Clear({0, 0, 0, 255}));
            static_cast<void>(renderer.value()->Present());
            static_cast<void>(clock.Advance(1.0 / 60.0));
        }
        if (texture.handle) static_cast<void>(renderer.value()->ReleaseTexture(texture.handle));
        std::vector<double> sorted = times;
        std::sort(sorted.begin(), sorted.end());
        const auto at = [&](double q) {
            if (sorted.empty()) return 0.0;
            const auto index = static_cast<std::size_t>(std::ceil(q * static_cast<double>(sorted.size()))) - 1;
            return sorted[std::min(index, sorted.size() - 1)];
        };
        std::printf("%s clip=%s frames=%zu/%u p50=%.3f p95=%.3f p99=%.3f max=%.3f ms\n", asset->id.c_str(),
                    clip ? model.clips[*clip].name.c_str() : "(none)", times.size(), options.frames, at(0.5), at(0.95),
                    at(0.99), sorted.empty() ? 0.0 : sorted.back());
    }
    const std::string text = csv.str();
    if (auto written = WriteReplacing(options.csvPath, std::as_bytes(std::span(text.data(), text.size()))); !written) {
        std::cerr << "error: " << Engine::Base::Describe(written.error()) << '\n';
        return 3;
    }
    return failures == 0 ? 0 : 4;
}

} // namespace Gyo::Tools::ModelViewer
