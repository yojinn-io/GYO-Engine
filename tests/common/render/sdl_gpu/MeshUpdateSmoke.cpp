#include "platform/sdl/SdlPlatform.hpp"
#include "render/PrimitiveMesh.hpp"
#include "render/RenderQueue.hpp"
#include "render/Renderer.hpp"
#include "engine/asset/loading/NativeFileAssetSource.hpp"
#include "render/backend/sdl_gpu/SdlGpuRenderDevice.hpp"
#include "RenderFeatureSmoke.hpp"

#include <cmath>
#include <array>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <string_view>
#include <thread>

namespace {

using namespace Engine::Render;
using Device = Backend::SdlGpu::SdlGpuRenderDevice;

template <class Result>
bool ExpectError(const Result& result, RenderErrorCode code, const char* name) {
    if (!result && result.error().code == code) return true;
    std::cerr << name << " did not return the expected error\n";
    return false;
}

bool CheckPixel(const SceneCapture& image,
                std::uint32_t x, std::uint32_t y,
                std::array<int, 4> expected, const char* label) {
    const std::size_t offset = (static_cast<std::size_t>(y) * image.width + x) * 4U;
    for (std::size_t channel = 0; channel < expected.size(); ++channel) {
        if (std::abs(static_cast<int>(image.rgba8[offset + channel]) - expected[channel]) > 2) {
            std::cerr << label << " capture mismatch at " << x << ',' << y
                      << " channel " << channel << ": "
                      << static_cast<int>(image.rgba8[offset + channel])
                      << " expected " << expected[channel] << '\n';
            return false;
        }
    }
    return true;
}

bool CheckCapturedLayers(Device& device, Renderer& renderer) {
    const MeshData quad = MakeUnitQuadXY();
    const auto created = device.CreateMesh(quad.View());
    if (!created) return false;
    const MeshHandle handle = created.value();
    RenderQueue queue({{}, {-8.0F, 1.0F}});
    PerspectiveCamera3D camera;
    camera.verticalFieldOfViewRadians = 0.85F;
    queue.SetCamera(camera);
    queue.SetViewModelCamera(camera);

    MeshSubmission wall;
    wall.mesh = handle;
    wall.transform = {{0, 0, 1}, {}, {10, 10, 1}};
    wall.material.tint = {0, 0, 1, 1};
    wall.doubleSided = true;
    if (!queue.Submit(wall)) return false;
    MeshSubmission nearPart = wall;
    nearPart.layer = MeshLayer::ViewModel;
    nearPart.transform = {{0, 0, 2}, {}, {0.5F, 0.5F, 1}};
    nearPart.material.tint = {1, 0, 0, 1};
    // Draw the farther part last: the centre remains red only if viewmodel
    // meshes retain self-depth testing, while clearing the nearer world wall.
    MeshSubmission farPart = nearPart;
    farPart.transform = {{0, 0, 3}, {}, {1.5F, 1.5F, 1}};
    farPart.material.tint = {0, 1, 0, 1};
    if (!queue.Submit(nearPart) || !queue.Submit(farPart)) return false;
    SpriteSubmission sceneSprite;
    sceneSprite.destinationPixels = {0, 0, 12, 12};
    sceneSprite.layer = CompositeLayer::Scene;
    sceneSprite.material.tint = {1, 1, 0, 1};
    if (!queue.Submit(sceneSprite)) return false;
    SpriteSubmission overlay;
    overlay.destinationPixels = {0, 0, 1000, 1000};
    overlay.material.tint = {1, 0, 1, 1};
    if (!queue.Submit(overlay)) return false;

    if (renderer.TakeSceneCapture()) return false;
    renderer.RequestSceneCapture();
    const auto rendered = renderer.Render(queue);
    if (!rendered) {
        std::cerr << rendered.error().message << ' ' << rendered.error().detail << '\n';
        return false;
    }
    auto captured = renderer.TakeSceneCapture();
    if (!captured || captured->width < 64 || captured->height < 64 ||
        captured->rgba8.size() != static_cast<std::size_t>(captured->width) * captured->height * 4U) {
        std::cerr << "scene capture did not return tightly packed RGBA8 pixels\n";
        return false;
    }
    const auto centerX = captured->width / 2U;
    const auto centerY = captured->height / 2U;
    if (!CheckPixel(*captured, centerX, centerY, {255, 0, 0, 255},
            "viewmodel over wall and self-depth") ||
        !CheckPixel(*captured, centerX + captured->height / 5U, centerY,
            {0, 255, 0, 255}, "far viewmodel part") ||
        !CheckPixel(*captured, captured->width - 4U, 4, {0, 0, 255, 255},
            "world outside viewmodel") ||
        !CheckPixel(*captured, 4, 4, {255, 255, 0, 255}, "top-left Scene sprite")) return false;
    // The bright scene colours also prove capture precedes exposure -8 and
    // excludes the magenta full-screen Overlay. Taking consumes the result,
    // and a frame without Request must not silently perform another readback.
    if (renderer.TakeSceneCapture() || !renderer.Render(queue) || renderer.TakeSceneCapture()) return false;
    return static_cast<bool>(device.ReleaseMesh(handle));
}

bool CheckExteriorAndSkyCulling(Device& device, Renderer& renderer) {
    const MeshData cube = MakeUnitCube();
    // MakeUnitCube lists the outward +Z face, then the outward -Z face.
    // Keep both single-sided; reversing their projected front-face convention
    // exposes the far green interior in place of the near red exterior.
    const auto farFace = device.CreateMesh({
        cube.vertices, std::span<const std::uint32_t>{cube.indices}.first(6)});
    const auto nearFace = device.CreateMesh({
        cube.vertices, std::span<const std::uint32_t>{cube.indices}.subspan(6, 6)});
    if (!farFace || !nearFace) return false;
    const auto renderFaces = [&](PerspectiveCamera3D camera,
                                 std::array<int, 4> expected) {
        RenderQueue queue;
        queue.SetCamera(camera);
        MeshSubmission near;
        near.mesh = nearFace.value();
        near.material.tint = {1, 0, 0, 1};
        MeshSubmission far = near;
        far.mesh = farFace.value();
        far.material.tint = {0, 1, 0, 1};
        if (!queue.Submit(near) || !queue.Submit(far)) return false;
        renderer.RequestSceneCapture();
        const auto result = renderer.Render(queue);
        if (!result) {
            std::cerr << result.error().message << '\n';
            return false;
        }
        const auto captured = renderer.TakeSceneCapture();
        return captured && CheckPixel(*captured,
            captured->width / 2U, captured->height / 2U, expected,
            "single-sided cube exterior");
    };
    if (!renderFaces({{0, 0, -2}}, {255, 0, 0, 255}) ||
        !renderFaces({{0, 0, 2}, {0, std::numbers::pi_v<float>, 0}},
            {0, 255, 0, 255})) return false;
    if (!device.ReleaseMesh(nearFace.value()) ||
        !device.ReleaseMesh(farFace.value())) return false;

    const auto sphere = MakeUvSphere(16, 32);
    if (!sphere) return false;
    const auto sphereHandle = device.CreateMesh(sphere.value().View());
    if (!sphereHandle) return false;
    RenderQueue skyQueue;
    skyQueue.SetCamera({});
    MeshSubmission sky;
    sky.mesh = sphereHandle.value();
    sky.surface = SurfaceMode::Sky;
    sky.transform.scale = {10, 10, 10};
    sky.material.tint = {0, 0, 1, 1};
    if (!skyQueue.Submit(sky)) return false;
    renderer.RequestSceneCapture();
    const auto rendered = renderer.Render(skyQueue);
    if (!rendered) return false;
    const auto captured = renderer.TakeSceneCapture();
    if (!captured || !CheckPixel(*captured,
            captured->width / 2U, captured->height / 2U,
            {0, 0, 255, 255}, "sky interior") ||
        !CheckPixel(*captured, captured->width / 4U, captured->height / 3U,
            {0, 0, 255, 255}, "sky interior off-centre")) return false;
    return static_cast<bool>(device.ReleaseMesh(sphereHandle.value()));
}

bool Exercise(Device& device, Renderer& renderer, Engine::Platform::Sdl::SdlPlatform& platform) {
    MeshData data = MakeUnitCube();
    const auto created = device.CreateMesh(data.View());
    if (!created) {
        std::cerr << created.error().message << '\n';
        return false;
    }
    const MeshHandle handle = created.value();
    if (!ExpectError(device.UpdateMeshVertices({}, data.vertices),
            RenderErrorCode::InvalidHandle, "invalid handle") ||
        !ExpectError(device.UpdateMeshVertices(handle, {}),
            RenderErrorCode::InvalidArgument, "empty vertex update") ||
        !ExpectError(device.UpdateMeshVertices(handle,
            std::span<const Vertex3D>{data.vertices}.first(data.vertices.size() - 1)),
            RenderErrorCode::InvalidArgument, "changed vertex count")) return false;

    const Vertex3D original = data.vertices.front();
    data.vertices.front().position.x = std::numeric_limits<float>::infinity();
    if (!ExpectError(device.UpdateMeshVertices(handle, data.vertices),
            RenderErrorCode::InvalidArgument, "non-finite position")) return false;
    data.vertices.front() = original;
    data.vertices.front().uv.y = std::numeric_limits<float>::quiet_NaN();
    if (!ExpectError(device.UpdateMeshVertices(handle, data.vertices),
            RenderErrorCode::InvalidArgument, "non-finite UV")) return false;
    data.vertices.front() = original;

    bool wrongThreadRejected = false;
    std::thread worker([&] {
        wrongThreadRejected = ExpectError(
            device.UpdateMeshVertices(handle, data.vertices),
            RenderErrorCode::WrongThread, "foreign thread update");
    });
    worker.join();
    if (!wrongThreadRejected) return false;

    RenderQueue queue;
    queue.SetCamera({{0, 0, -2}});
    queue.SetViewModelCamera({{0, 0, -4}, {}, 0.85F});
    MeshSubmission world;
    world.mesh = handle;
    world.material.tint = {0.2F, 0.3F, 0.8F, 1.0F};
    world.doubleSided = true;
    MeshSubmission weapon = world;
    weapon.layer = MeshLayer::ViewModel;
    weapon.material.tint = {0.9F, 0.2F, 0.1F, 1.0F};
    weapon.transform.scale = {0.7F, 0.7F, 0.7F};
    if (!queue.Submit(world) || !queue.Submit(weapon)) return false;

    // Also exercise Scene sprites between the world and viewmodel passes and
    // Overlay sprites after the scene color transform.
    SpriteSubmission sprite;
    sprite.destinationPixels = {0, 0, 15, 15};
    sprite.layer = CompositeLayer::Scene;
    if (!queue.Submit(sprite)) return false;
    sprite.layer = CompositeLayer::Overlay;
    sprite.destinationPixels.x = 140;
    if (!queue.Submit(sprite)) return false;

    // Render must revalidate cameras: a caller can clear them after Submit.
    queue.ClearViewModelCamera();
    if (!ExpectError(renderer.Render(queue), RenderErrorCode::InvalidArgument,
            "cleared viewmodel camera")) return false;
    queue.SetViewModelCamera({{0, 0, -4}, {}, 0.85F});
    for (unsigned frame = 0; frame < 16; ++frame) {
        static_cast<void>(platform.PumpEvents());
        data.vertices.front().position.x = original.position.x +
            0.1F * std::sin(static_cast<float>(frame));
        const auto updated = device.UpdateMeshVertices(handle, data.vertices);
        if (!updated) {
            std::cerr << updated.error().message << '\n';
            return false;
        }
        const auto rendered = renderer.Render(queue);
        if (!rendered || rendered.value() != PresentStatus::Presented) {
            std::cerr << "updated world/viewmodel frame was not presented\n";
            return false;
        }
    }

    // A viewmodel-only frame must not require the world camera.
    queue.Reset();
    queue.SetViewModelCamera({{0, 0, -4}});
    if (!queue.Submit(weapon) || !renderer.Render(queue)) return false;

    if (!device.ReleaseMesh(handle)) return false;
    const auto replacement = device.CreateMesh(data.View());
    if (!replacement || replacement.value() == handle) return false;
    if (!ExpectError(device.UpdateMeshVertices(handle, data.vertices),
            RenderErrorCode::InvalidHandle, "stale mesh update")) return false;
    return static_cast<bool>(device.ReleaseMesh(replacement.value()));
}

bool CheckFrameContracts(Device& device, const ShaderLibrary& library) {
    ShaderFormat format = ShaderFormat::Metallib;
    for (auto candidate : {ShaderFormat::DXIL, ShaderFormat::SPIRV, ShaderFormat::Metallib})
        if (device.GetInfo().shaderFormats & FormatBit(candidate)) { format = candidate; break; }
    auto program = library.FindProgram("builtin/unlit", format);
    if (!program) return false;
    auto vertex = device.CreateShader(*program.value().vertex);
    auto fragment = device.CreateShader(*program.value().fragment);
    if (!vertex || !fragment) return false;
    const auto image = std::array<std::uint8_t,4>{255,255,255,255};
    auto texture = device.CreateTexture(ImageView{1,1,4,std::as_bytes(std::span{image}),TextureColorSpace::Linear});
    auto mesh = device.CreateMesh(MakeUnitQuadXY().View());
    if (!texture || !mesh) return false;
    auto acquired = device.AcquireFrame();
    if (!acquired || !acquired.value()) return false;
    const auto frame = *acquired.value();
    if (!ExpectError(device.AcquireFrame(), RenderErrorCode::InvalidArgument, "double frame acquisition") ||
        !ExpectError(device.ReadTexture(texture.value()), RenderErrorCode::InvalidArgument, "readback during acquired frame")) return false;
    PipelineDesc pipelineDesc{vertex.value(), fragment.value(), frame.colorFormat,
        true, CullMode::None, false, false, false};
    auto pipeline = device.CreatePipeline(pipelineDesc);
    if (!pipeline) { std::cerr << pipeline.error().message << ' ' << pipeline.error().detail << '\n'; return false; }
    pipelineDesc.vertexShader = fragment.value();
    if (!ExpectError(device.CreatePipeline(pipelineDesc), RenderErrorCode::InvalidArgument, "reversed shader stages")) return false;
    // Native pipelines own their compiled shader relationship; their copied
    // layout metadata must remain valid after public shader handles are freed.
    if (!device.ReleaseShader(vertex.value()) || !device.ReleaseShader(fragment.value()) ||
        !ExpectError(device.ReleaseShader(vertex.value()), RenderErrorCode::InvalidHandle, "stale shader release")) return false;
    PreparedDraw draw;
    draw.pipeline = pipeline.value(); draw.mesh = mesh.value();
    draw.fragmentTextures.push_back({texture.value(), SamplerMode::LinearClamp});
    draw.vertexUniforms.emplace_back(64); draw.fragmentUniforms.emplace_back(47);
    PreparedPass pass; pass.colorLoad = AttachmentLoad::Clear; pass.draws.push_back(draw);
    PreparedFrame commands; commands.passes.push_back(pass);
    if (!ExpectError(device.SubmitFrame(frame, commands), RenderErrorCode::InvalidArgument, "uniform byte count mismatch") ||
        !ExpectError(device.SubmitFrame(frame, commands), RenderErrorCode::InvalidArgument, "consumed failed frame")) return false;
    acquired = device.AcquireFrame();
    if (!acquired || !acquired.value()) return false;
    auto invalidFrame = *acquired.value(); ++invalidFrame.token;
    if (!ExpectError(device.SubmitFrame(invalidFrame, {}), RenderErrorCode::InvalidArgument, "foreign frame token")) return false;
    device.AbandonFrame(*acquired.value());
    acquired = device.AcquireFrame();
    if (!acquired || !acquired.value()) return false;
    commands.passes[0].draws[0].fragmentUniforms[0].resize(48);
    if (!device.SubmitFrame(*acquired.value(), commands)) return false;
    if (!device.ReleasePipeline(pipeline.value()) ||
        !ExpectError(device.ReleasePipeline(pipeline.value()), RenderErrorCode::InvalidHandle, "stale pipeline release")) return false;
    return static_cast<bool>(device.ReleaseMesh(mesh.value())) && static_cast<bool>(device.ReleaseTexture(texture.value()));
}

} // namespace

int main(int argc, char** argv) {
    std::string requestedDriver="auto";
    for (int index=1; index<argc; ++index) {
        if (std::string_view(argv[index])!="--gpu-driver" || index+1>=argc) {
            std::cerr << "usage: gyo_sdl_gpu_mesh_smoke [--gpu-driver auto|d3d12|vulkan|metal]\n";
            return 2;
        }
        requestedDriver=argv[++index];
        if (requestedDriver!="auto" && requestedDriver!="d3d12" && requestedDriver!="vulkan" && requestedDriver!="metal") {
            std::cerr << "unknown GPU driver: " << requestedDriver << '\n'; return 2;
        }
    }
    Engine::Platform::Sdl::SdlPlatformOptions options;
    options.title = "GYO mesh update / viewmodel smoke";
    // An odd width exercises padded RGBA16F download rows and tight RGBA8 output.
    options.width = 163;
    options.height = 121;
    options.resizable = false;
    auto platform = Engine::Platform::Sdl::SdlPlatform::Create(options);
    if (!platform) {
        std::cerr << platform.error().message << '\n';
        return 1;
    }
    Engine::Asset::Loading::NativeFileAssetSource source;
    Engine::Asset::Resolver::AssetPathResolver::Options resolverOptions;
    resolverOptions.assetsRoot = GYO_TEST_SHADER_BUNDLE;
    ShaderLibrary library;
    auto bundle = library.AppendBundle(source, Engine::Asset::Resolver::AssetPathResolver(resolverOptions));
    if (!bundle) { std::cerr << bundle.error().message << ' ' << bundle.error().detail << '\n'; return 1; }
    resolverOptions.assetsRoot = GYO_TEST_CUSTOM_SHADER_BUNDLE;
    bundle = library.AppendBundle(source, Engine::Asset::Resolver::AssetPathResolver(resolverOptions));
    if (!bundle) { std::cerr << bundle.error().message << ' ' << bundle.error().detail << '\n'; return 1; }
    Backend::SdlGpu::SdlGpuOptions gpuOptions;
    gpuOptions.driver=requestedDriver;
    gpuOptions.availableShaderFormats = library.CompleteFormats();
    gpuOptions.vsync = false;
    gpuOptions.debugMode = true;
    auto device = Device::Create(*platform.value(), gpuOptions);
    if (!device) {
        std::cerr << device.error().message << '\n';
        return 1;
    }
    Renderer renderer;
    auto initialized = renderer.Initialize(*device.value(), library);
    if (!initialized) { std::cerr << initialized.error().message << '\n'; return 1; }
    std::cout << "SDL_GPU smoke requested=" << requestedDriver
              << " actual=" << device.value()->GetInfo().driver
              << " shader=" << ShaderFormatName(*renderer.ActiveShaderFormat()) << '\n';
    if (!CheckFrameContracts(*device.value(), library) ||
        !CheckRenderFeaturePixels(*device.value(), renderer, library) ||
        !Exercise(*device.value(), renderer, *platform.value()) ||
        !CheckCapturedLayers(*device.value(), renderer) ||
        !CheckExteriorAndSkyCulling(*device.value(), renderer)) return 1;
    std::cout << "SDL_GPU lifecycle, vertex updates, viewmodel depth, culling, UV, projection, blend and color pixels passed\n";
    return 0;
}
