#include "MuzzleProbe.hpp"
#include "platform/sdl/SdlPlatform.hpp"

#include "RetroFPS/App/WeaponPresentationDefinition.hpp"
#include "RetroFPS/App/WeaponViewModel.hpp"
#include "RetroFPS/Game/CampaignContent.hpp"
#include "RetroFPS/Gameplay/Weapon/WeaponShotGeometry.hpp"
#include "render/PrimitiveMesh.hpp"
#include "render/RenderQueue.hpp"
#include "render/Renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace {
namespace Render = Engine::Render;
using Capture = Render::SceneCapture;

Capture CaptureFrame(Engine::Platform::Sdl::SdlPlatform& platform,
                     Render::Renderer& renderer,
                     const Render::RenderQueue& queue) {
    renderer.RequestSceneCapture();
    for (int retry = 0; retry < 180; ++retry) {
        if (platform.PumpEvents() == Engine::Runtime::RuntimeControl::Stop) {
            throw std::runtime_error("muzzle probe was closed before validation completed");
        }
        const auto rendered = renderer.Render(queue);
        if (!rendered) throw std::runtime_error(rendered.error().message);
        if (auto frame = renderer.TakeSceneCapture()) return std::move(*frame);
        SDL_Delay(16);
    }
    throw std::runtime_error("muzzle probe could not capture a frame");
}

void Save(const Capture& frame, const std::filesystem::path& directory,
          const std::string& name) {
    if (directory.empty()) return;
    SDL_Surface* surface = SDL_CreateSurfaceFrom(
        static_cast<int>(frame.width), static_cast<int>(frame.height),
        SDL_PIXELFORMAT_RGBA32, const_cast<std::uint8_t*>(frame.rgba8.data()),
        static_cast<int>(frame.width * 4));
    if (!surface) throw std::runtime_error(SDL_GetError());
    const bool saved = SDL_SaveBMP(surface, (directory / (name + ".bmp")).string().c_str());
    SDL_DestroySurface(surface);
    if (!saved) throw std::runtime_error(SDL_GetError());
}

Render::Float2 MarkerCenter(const Capture& capture) {
    double xSum = 0, ySum = 0;
    std::size_t count = 0;
    for (std::uint32_t y = 0; y < capture.height; ++y) {
        for (std::uint32_t x = 0; x < capture.width; ++x) {
            const auto pixel = (static_cast<std::size_t>(y) * capture.width + x) * 4;
            if (capture.rgba8[pixel] > 200 && capture.rgba8[pixel + 1] > 200 &&
                capture.rgba8[pixel + 2] > 200) {
                xSum += x + 0.5; ySum += y + 0.5; ++count;
            }
        }
    }
    if (count < 4) throw std::runtime_error("GPU muzzle marker is missing or too small");
    return {static_cast<float>(xSum / count), static_cast<float>(ySum / count)};
}

float Distance(Render::Float2 a, Render::Float2 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

struct MarkerMesh final {
    Render::IRenderDevice& device;
    Render::MeshHandle handle;
    ~MarkerMesh() { static_cast<void>(device.ReleaseMesh(handle)); }
};
} // namespace

int RunMuzzleProbe(
    Engine::Platform::Sdl::SdlPlatform& platform,
    Render::IRenderDevice& device,
    Render::Renderer& renderer,
    Engine::Asset::AssetManager& assets,
    const fps::CampaignContent& content,
    const std::filesystem::path& captureDirectory) {
    try {
        if (!captureDirectory.empty()) std::filesystem::create_directories(captureDirectory);
        const auto& weapon = content.Data().weapons.GetDefinitions().front();
        const auto* geometry = content.FindWeaponShotGeometry(weapon.id);
        if (!geometry) throw std::runtime_error("muzzle probe requires weapon shot geometry");
        std::string error;
        const auto definition = fps::LoadWeaponPresentationDefinition(assets, weapon.presentationAssetId, error);
        if (!definition) throw std::runtime_error(error);
        fps::WeaponViewModel viewModel;
        if (!viewModel.Initialize(device, assets, weapon.presentationAssetId, error)) {
            throw std::runtime_error(error);
        }
        Render::RenderQueue modelQueue;
        const fps::WeaponPresentationSnapshot shot{
            weapon.id, fps::WeaponAction::Shoot, 0, weapon.fireIntervalSeconds, 1};
        if (!viewModel.Submit(shot, modelQueue, error)) throw std::runtime_error(error);
        const auto nozzle = viewModel.GetMuzzleViewCameraPosition();
        Engine::Model::Pose pose;
        if (const auto sampled = Engine::Model::SamplePose(*definition->model,
                definition->clips[1], 0, Engine::Model::PlaybackMode::Clamp, pose); !sampled) {
            throw std::runtime_error(sampled.error());
        }
        const auto modelPoint = Engine::Model::TransformPoint(
            pose.globalTransforms[definition->muzzleNodeIndex], definition->muzzleLocalPosition);
        const Render::Float3 anchoredPoint{
            modelPoint.x - definition->idleAnchor.x,
            modelPoint.y - definition->idleAnchor.y,
            modelPoint.z - definition->idleAnchor.z};
        const auto quad = Render::MakeUnitQuadXY();
        const auto created = device.CreateMesh(quad.View());
        if (!created) throw std::runtime_error(created.error().message);
        MarkerMesh marker{device, created.value()};

        struct View { const char* name; float yaw, pitch, feet, fov; };
        constexpr float radians = std::numbers::pi_v<float> / 180;
        constexpr std::array views{
            View{"forward", 0, 0, 0, 60 * radians},
            View{"yaw", 75 * radians, 0, 0, 60 * radians},
            View{"look_up", -40 * radians, -30 * radians, 0, 60 * radians},
            View{"look_down", 130 * radians, 35 * radians, 0, 60 * radians},
            View{"jump", 0, 0, 0.6F, 60 * radians},
            View{"recoil_first_frame", 40 * radians, -1.5F * radians, 0.6F, 60 * radians},
            View{"world_fov_75", -75 * radians, 25 * radians, 0.3F, 75 * radians},
        };
        float maximumPairError = 0, maximumReferenceError = 0;
        for (const auto& view : views) {
            const Render::Float3 eye{3.0F, 1.6F + view.feet, -7.0F};
            const float sy = std::sin(view.yaw), cy = std::cos(view.yaw);
            const float sp = std::sin(view.pitch), cp = std::cos(view.pitch);
            const Render::Float3 right{cy, 0, -sy}, up{sy * sp, cp, cy * sp};
            const Render::Float3 forward{sy * cp, -sp, cy * cp};
            const auto offset = fps::ResolveWeaponMuzzleCameraPosition(*geometry, view.fov);
            const Render::Float3 world{
                eye.x + right.x * offset.x + up.x * offset.y + forward.x * offset.z,
                eye.y + right.y * offset.x + up.y * offset.y + forward.y * offset.z,
                eye.z + right.z * offset.x + up.z * offset.y + forward.z * offset.z};
            const float k = std::tan(view.fov / 2) /
                std::tan(definition->camera.verticalFieldOfViewRadians / 2);
            std::array<Render::Float2, 2> centers;
            for (int pass = 0; pass < 2; ++pass) {
                auto vertices = quad.vertices;
                for (auto& vertex : vertices) {
                    const float x = vertex.position.x * 0.01F;
                    const float y = vertex.position.y * 0.01F;
                    vertex.position = pass == 0 ? Render::Float3{
                        world.x + k * (right.x * x + up.x * y),
                        world.y + k * (right.y * x + up.y * y),
                        world.z + k * (right.z * x + up.z * y)} : Render::Float3{
                        anchoredPoint.x + x, anchoredPoint.y + y, anchoredPoint.z};
                }
                const auto updated = device.UpdateMeshVertices(marker.handle, vertices);
                if (!updated) throw std::runtime_error(updated.error().message);
                Render::RenderQueue queue;
                queue.SetCamera({eye, {view.pitch, view.yaw, 0}, view.fov, 0.01F, 100});
                queue.SetViewModelCamera(definition->camera);
                Render::MeshSubmission point;
                point.mesh = marker.handle;
                point.doubleSided = true;
                point.layer = pass == 0 ? Render::MeshLayer::World : Render::MeshLayer::ViewModel;
                // GPU applies the actual weapon placement to the raw bone point;
                // this independently checks the CPU placement used by gameplay.
                if (pass == 1) point.transform = definition->placement;
                if (const auto submitted = queue.Submit(point); !submitted) {
                    throw std::runtime_error(submitted.error().message);
                }
                auto capture = CaptureFrame(platform, renderer, queue);
                centers[pass] = MarkerCenter(capture);
                const float focal = static_cast<float>(capture.height) /
                    (2 * std::tan(definition->camera.verticalFieldOfViewRadians / 2));
                const Render::Float2 expected{
                    capture.width * 0.5F + focal * nozzle.x / nozzle.z,
                    capture.height * 0.5F - focal * nozzle.y / nozzle.z};
                maximumReferenceError = (std::max)(maximumReferenceError, Distance(centers[pass], expected));
                Save(capture, captureDirectory, std::string{view.name} + (pass == 0 ? "_world" : "_model"));
            }
            const float pairError = Distance(centers[0], centers[1]);
            maximumPairError = (std::max)(maximumPairError, pairError);
            if (pairError >= 1 || maximumReferenceError >= 1) {
                throw std::runtime_error("GPU muzzle projection error exceeds one pixel");
            }
        }

        // Review image: a small cyan marker sits at the measured bore center.
        // Move its depth forward while preserving projection so the marker
        // remains readable over the weapon, independently of its own depth.
        auto vertices = quad.vertices;
        for (auto& vertex : vertices) vertex.position = {
            nozzle.x * 0.1F + vertex.position.x * 0.0006F,
            nozzle.y * 0.1F + vertex.position.y * 0.0006F, nozzle.z * 0.1F};
        if (const auto updated = device.UpdateMeshVertices(marker.handle, vertices); !updated) {
            throw std::runtime_error(updated.error().message);
        }
        Render::MeshSubmission point;
        point.mesh = marker.handle;
        point.doubleSided = true;
        point.layer = Render::MeshLayer::ViewModel;
        point.material.tint = {0, 1, 1, 1};
        if (const auto submitted = modelQueue.Submit(point); !submitted) {
            throw std::runtime_error(submitted.error().message);
        }
        Save(CaptureFrame(platform, renderer, modelQueue), captureDirectory, "muzzle_model_review");
        SDL_Log("Muzzle GPU validation passed: %zu camera cases, pair error %.4f px, reference error %.4f px",
            views.size(), maximumPairError, maximumReferenceError);
        return 0;
    } catch (const std::exception& exception) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Muzzle GPU validation failed: %s", exception.what());
        return 1;
    }
}
