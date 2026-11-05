// -preview judgments V2 and V3 on a GPU (docs/tools/plans/v8/model-viewer.md):
// every preview frame is the frame Engine's Renderer drew on the offscreen
// device, read back, and the texture the window shows holds those bytes.
#include "gyo/model_viewer/PreviewRenderer.hpp"

#include "gyo/framework/PreviewTexture.hpp"

#include "engine/base/Error.hpp"
#include "engine/model/Animation.hpp"
#include "engine/platform/sdl/SdlPlatform.hpp"
#include "engine/render/backend/sdl/SdlRenderer.hpp"
#include "engine/render/backend/sdl/SdlRendererNative.hpp"
#include "engine/render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>

using namespace Gyo::Tools::ModelViewer;
using Gyo::Tools::Framework::PreviewTexture;
using Gyo::Tools::Framework::UploadFrame;
namespace Model = Engine::Model;

namespace {

template <class Result>
bool Expect(const Result& result, const char* name) {
    if (result) return true;
    std::cerr << name << ": " << Engine::Base::Describe(result.error()) << '\n';
    return false;
}

// A red square of side 2 in the XY plane at z = 0, on one rigid node, both windings.
std::shared_ptr<const Model::ModelAsset> MakeSquare() {
    auto model = std::make_shared<Model::ModelAsset>();
    model->nodes.push_back({"root", std::nullopt, {{0, 0, 0}, {}, {1, 1, 1}}});
    model->materials.push_back({"red", {1.0F, 0.0F, 0.0F, 1.0F}});
    Model::MeshPart part;
    part.name = "square";
    part.vertices = {{{-1, -1, 0}, {0, 0, -1}, {0, 1}}, {{1, -1, 0}, {0, 0, -1}, {1, 1}},
                     {{1, 1, 0}, {0, 0, -1}, {1, 0}}, {{-1, 1, 0}, {0, 0, -1}, {0, 0}}};
    part.indices = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};
    model->meshes.push_back(part);
    Model::AnimationClip clip;
    clip.name = "Still";
    clip.durationSeconds = 1.0;
    model->clips.push_back(clip);
    if (!Model::ValidateModel(*model)) return nullptr;
    return model;
}

std::array<int, 4> Pixel(const Engine::Render::TextureReadback& frame, std::uint32_t x, std::uint32_t y) {
    const std::size_t offset = static_cast<std::size_t>(y) * frame.rowPitch + static_cast<std::size_t>(x) * 4U;
    return {std::to_integer<int>(frame.bytes[offset]), std::to_integer<int>(frame.bytes[offset + 1]),
            std::to_integer<int>(frame.bytes[offset + 2]), std::to_integer<int>(frame.bytes[offset + 3])};
}

bool IsRed(std::array<int, 4> pixel) { return pixel[0] > 200 && pixel[1] < 40 && pixel[2] < 40; }

// The frame the preview handed out is the device's last frame, byte for byte.
bool SameAsDevice(PreviewRenderer& preview, const Engine::Render::TextureReadback& frame, const char* label) {
    auto device = preview.Preview().Device().ReadFrame();
    if (!Expect(device, label)) return false;
    if (device.value().width != frame.width || device.value().height != frame.height ||
        device.value().rowPitch != frame.rowPitch || device.value().bytes != frame.bytes) {
        std::cerr << label << ": the preview handed out another frame than the device's last one\n";
        return false;
    }
    return true;
}

// The window's texture holds the uploaded bytes: drawn 1:1 by the SDL
// renderer and read back, every pixel matches the frame.
bool TextureHolds(Engine::Render::Backend::Sdl::SdlRenderer& renderer, const PreviewTexture& texture,
                  const Engine::Render::TextureReadback& frame) {
    SDL_Renderer* native = Engine::Render::Backend::Sdl::NativeRenderer(renderer);
    SDL_Texture* image = Engine::Render::Backend::Sdl::NativeTexture(renderer, texture.handle);
    if (image == nullptr) {
        std::cerr << "the preview texture does not exist\n";
        return false;
    }
    SDL_SetTextureBlendMode(image, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(image, SDL_SCALEMODE_NEAREST);
    if (!renderer.Clear({0, 0, 0, 255})) return false;
    const SDL_FRect area{0, 0, static_cast<float>(frame.width), static_cast<float>(frame.height)};
    if (!SDL_RenderTexture(native, image, nullptr, &area)) return false;
    const SDL_Rect read{0, 0, static_cast<int>(frame.width), static_cast<int>(frame.height)};
    SDL_Surface* surface = SDL_RenderReadPixels(native, &read);
    if (surface == nullptr) return false;
    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (rgba == nullptr) return false;
    bool same = true;
    for (std::uint32_t y = 0; y < frame.height && same; ++y) {
        const auto* row = static_cast<const std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(y) * rgba->pitch;
        for (std::uint32_t x = 0; x < frame.width && same; ++x) {
            const auto expected = Pixel(frame, x, y);
            for (std::size_t channel = 0; channel < 3; ++channel) {
                if (row[x * 4U + channel] != expected[channel]) {
                    std::cerr << "texture pixel " << x << ',' << y << " channel " << channel << " is "
                              << static_cast<int>(row[x * 4U + channel]) << ", the frame has " << expected[channel] << '\n';
                    same = false;
                    break;
                }
            }
        }
    }
    SDL_DestroySurface(rgba);
    return same;
}

} // namespace

int main(int argc, char** argv) {
    std::string driver = "auto";
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]) != "--gpu-driver" || index + 1 >= argc) {
            std::cerr << "usage: gyo_model_viewer_preview_smoke [--gpu-driver auto|d3d12|vulkan|metal]\n";
            return 2;
        }
        driver = argv[++index];
    }
    Engine::Platform::Sdl::SdlPlatformOptions options;
    options.title = "model viewer preview smoke";
    options.width = 128;
    options.height = 96;
    options.resizable = false;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(options);
    if (!Expect(platform, "platform")) return 1;
    // As in the window: the shell's SDL renderer has the window.
    auto renderer = Engine::Render::Backend::Sdl::SdlRenderer::Create(*platform.value(), {false});
    if (!Expect(renderer, "SDL renderer")) return 1;

    PreviewOptions previewOptions;
    previewOptions.shaderBundle = GYO_TEST_SHADER_BUNDLE;
    previewOptions.width = 61;
    previewOptions.height = 45;
    previewOptions.driver = driver;
    previewOptions.skeleton = false;
    auto created = PreviewRenderer::Create(*platform.value(), previewOptions);
    if (!Expect(created, "preview")) return 1;
    PreviewRenderer& preview = *created.value();

    const auto square = MakeSquare();
    if (!square) {
        std::cerr << "the square model is not valid\n";
        return 1;
    }
    Model::Pose pose;
    if (!Expect(Model::MakeDefaultPose(*square, pose), "pose") || !Expect(preview.Show(square, pose), "show")) return 1;
    OrbitCamera facing;
    facing.pitchRadians = 0.0F;
    facing.distance = 4.0F;
    PreviewTexture texture;

    // V2: the square is drawn, the frame is the device's, the texture holds it.
    auto drawn = preview.Render(pose, facing, 61, 45);
    if (!Expect(drawn, "frame with the square")) return 1;
    const auto& first = drawn.value();
    if (!IsRed(Pixel(first, first.width / 2U, first.height / 2U))) {
        std::cerr << "the square is not at the middle of the frame\n";
        return 1;
    }
    if (!SameAsDevice(preview, first, "frame with the square") ||
        !Expect(UploadFrame(*renderer.value(), texture, first), "upload") ||
        !TextureHolds(*renderer.value(), texture, first)) return 1;

    // The next frame is its own: turned away from the square, the middle is the background.
    OrbitCamera away = facing;
    away.yawRadians = std::numbers::pi_v<float>;
    away.target = {0, 0, -8};
    auto turned = preview.Render(pose, away, 61, 45);
    if (!Expect(turned, "frame turned away")) return 1;
    const auto& second = turned.value();
    if (IsRed(Pixel(second, second.width / 2U, second.height / 2U))) {
        std::cerr << "the frame turned away still shows the square: an earlier frame was handed out\n";
        return 1;
    }
    if (!SameAsDevice(preview, second, "frame turned away") ||
        !Expect(UploadFrame(*renderer.value(), texture, second), "second upload") ||
        !TextureHolds(*renderer.value(), texture, second)) return 1;

    // A new size makes a new texture of that size.
    auto resized = preview.Render(pose, facing, 40, 30);
    if (!Expect(resized, "resized frame") || resized.value().width != 40 || resized.value().height != 30 ||
        !Expect(UploadFrame(*renderer.value(), texture, resized.value()), "resized upload") ||
        texture.width != 40 || texture.height != 30 || !TextureHolds(*renderer.value(), texture, resized.value())) {
        std::cerr << "the resized frame does not reach a texture of its size\n";
        return 1;
    }

    // V3: no model shows the background only; a model without meshes draws its skeleton.
    if (!Expect(preview.Show(nullptr, pose), "show none")) return 1;
    auto empty = preview.Render(pose, facing, 40, 30);
    if (!Expect(empty, "frame without a model") ||
        IsRed(Pixel(empty.value(), empty.value().width / 2U, empty.value().height / 2U))) {
        std::cerr << "a frame without a model still shows the model before\n";
        return 1;
    }
    // One bone along +X: drawn across the middle row, left and right of the
    // centre, and not above or below it (its yaw and pitch put it along X).
    auto bones = std::make_shared<Model::ModelAsset>();
    bones->nodes.push_back({"hip", std::nullopt, {{0, 0, 0}, {}, {1, 1, 1}}});
    bones->nodes.push_back({"knee", 0, {{1, 0, 0}, {}, {1, 1, 1}}});
    Model::Pose bonePose;
    if (!Model::ValidateModel(*bones) || !Expect(Model::MakeDefaultPose(*bones, bonePose), "skeleton pose")) return 1;
    preview.SetSkeleton(true);
    if (!Expect(preview.Show(bones, bonePose), "show skeleton")) return 1;
    auto skeleton = preview.Render(bonePose, FrameModel(*bones, bonePose, facing), 241, 181);
    if (!Expect(skeleton, "skeleton frame")) return 1;
    const auto& boneFrame = skeleton.value();
    const auto isBone = [](std::array<int, 4> p) { return p[0] > 200 && p[1] > 150 && p[2] < 170; };
    const std::uint32_t cx = boneFrame.width / 2U, cy = boneFrame.height / 2U;
    const std::uint32_t along = boneFrame.width / 6U, across = boneFrame.height / 6U;
    if (!isBone(Pixel(boneFrame, cx - along, cy)) || !isBone(Pixel(boneFrame, cx + along, cy)) ||
        isBone(Pixel(boneFrame, cx, cy - across)) || isBone(Pixel(boneFrame, cx, cy + across))) {
        std::cerr << "the bone is not drawn along X across the middle of the frame\n";
        return 1;
    }
    if (texture.handle) static_cast<void>(renderer.value()->ReleaseTexture(texture.handle));
    std::cout << "model viewer preview: frames are the device's, uploaded unchanged; no model and bones only draw\n";
    return 0;
}
