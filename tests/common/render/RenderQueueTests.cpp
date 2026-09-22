#include <doctest/doctest.h>

#include <limits>

#include "render/RenderQueue.hpp"

namespace {

using namespace Engine::Render;

[[nodiscard]] constexpr Float2 TransformUv(
    UvTransform transform,
    Float2 uv) noexcept {
    return {
        uv.x * transform.scale.x + transform.offset.x,
        uv.y * transform.scale.y + transform.offset.y,
    };
}

TEST_CASE("RenderQueue stores neutral mesh and sprite submissions in order") {
    RenderQueue queue({Color{0.1F, 0.2F, 0.3F, 1.0F}});
    queue.SetCamera(PerspectiveCamera3D{});

    MeshSubmission first{};
    first.mesh = MeshHandle::FromParts(4, 2);
    first.material.tint.red = 0.25F;
    MeshSubmission second{};
    second.mesh = MeshHandle::FromParts(7, 3);
    second.surface = SurfaceMode::AlphaMasked;
    REQUIRE(queue.Submit(first));
    REQUIRE(queue.Submit(second));

    SpriteSubmission sprite{};
    sprite.destinationPixels = {10.0F, 20.0F, 30.0F, 40.0F};
    REQUIRE(queue.Submit(sprite));

    REQUIRE(queue.Meshes().size() == 2);
    CHECK(queue.Meshes()[0].mesh == first.mesh);
    CHECK(queue.Meshes()[1].surface == SurfaceMode::AlphaMasked);
    REQUIRE(queue.Sprites().size() == 1);
    CHECK(queue.Sprites()[0].destinationPixels.x == doctest::Approx(10.0F));
    CHECK(queue.Sprites()[0].layer == CompositeLayer::Overlay);
    CHECK(queue.Camera().has_value());
}

TEST_CASE("RenderQueue preserves explicit scene and overlay sprite layers") {
    RenderQueue queue;

    SpriteSubmission scene;
    scene.destinationPixels = {0.0F, 0.0F, 10.0F, 10.0F};
    scene.layer = CompositeLayer::Scene;
    REQUIRE(queue.Submit(scene));

    SpriteSubmission overlay;
    overlay.destinationPixels = {10.0F, 0.0F, 10.0F, 10.0F};
    REQUIRE(queue.Submit(overlay));

    REQUIRE(queue.Sprites().size() == 2);
    CHECK(queue.Sprites()[0].layer == CompositeLayer::Scene);
    CHECK(queue.Sprites()[1].layer == CompositeLayer::Overlay);
}

TEST_CASE("Sprite source UV maps visual top-left for full textures") {
    const UvTransform transform =
        MakeSpriteUvTransform({0.0F, 0.0F, 1.0F, 1.0F});

    // The shared XY quad's v=1 vertices appear at the top in screen space.
    const Float2 topLeft = TransformUv(transform, {0.0F, 1.0F});
    const Float2 topRight = TransformUv(transform, {1.0F, 1.0F});
    const Float2 bottomRight = TransformUv(transform, {1.0F, 0.0F});

    CHECK(topLeft.x == doctest::Approx(0.0F));
    CHECK(topLeft.y == doctest::Approx(0.0F));
    CHECK(topRight.x == doctest::Approx(1.0F));
    CHECK(topRight.y == doctest::Approx(0.0F));
    CHECK(bottomRight.x == doctest::Approx(1.0F));
    CHECK(bottomRight.y == doctest::Approx(1.0F));
}

TEST_CASE("Sprite source UV preserves atlas sub-rectangle boundaries") {
    const UvTransform transform =
        MakeSpriteUvTransform({0.25F, 0.125F, 0.5F, 0.25F});

    const Float2 topLeft = TransformUv(transform, {0.0F, 1.0F});
    const Float2 topRight = TransformUv(transform, {1.0F, 1.0F});
    const Float2 bottomLeft = TransformUv(transform, {0.0F, 0.0F});
    const Float2 bottomRight = TransformUv(transform, {1.0F, 0.0F});

    CHECK(topLeft.x == doctest::Approx(0.25F));
    CHECK(topLeft.y == doctest::Approx(0.125F));
    CHECK(topRight.x == doctest::Approx(0.75F));
    CHECK(topRight.y == doctest::Approx(0.125F));
    CHECK(bottomLeft.x == doctest::Approx(0.25F));
    CHECK(bottomLeft.y == doctest::Approx(0.375F));
    CHECK(bottomRight.x == doctest::Approx(0.75F));
    CHECK(bottomRight.y == doctest::Approx(0.375F));
}

TEST_CASE("RenderQueue rejects invalid handles and non-finite presentation data") {
    RenderQueue queue;
    MeshSubmission mesh{};
    CHECK_FALSE(queue.Submit(mesh));

    mesh.mesh = MeshHandle::FromParts(0, 1);
    queue.SetCamera(PerspectiveCamera3D{});
    mesh.transform.translation.x = std::numeric_limits<float>::infinity();
    CHECK_FALSE(queue.Submit(mesh));

    SpriteSubmission sprite{};
    sprite.destinationPixels = {0.0F, 0.0F, -1.0F, 10.0F};
    CHECK_FALSE(queue.Submit(sprite));

    sprite.destinationPixels.width = 10.0F;
    sprite.material.tint.alpha = std::numeric_limits<float>::quiet_NaN();
    CHECK_FALSE(queue.Submit(sprite));

    sprite.material.tint.alpha = 1.0F;
    sprite.layer = static_cast<CompositeLayer>(255);
    CHECK_FALSE(queue.Submit(sprite));
}

TEST_CASE("RenderQueue reset clears per-frame state without changing capacity semantics") {
    RenderQueue queue;
    queue.SetCamera(PerspectiveCamera3D{});
    queue.SetViewModelCamera(PerspectiveCamera3D{});
    SpriteSubmission sprite{};
    sprite.destinationPixels = {0.0F, 0.0F, 1.0F, 1.0F};
    REQUIRE(queue.Submit(sprite));

    queue.Reset({
        Color{1.0F, 0.0F, 0.0F, 1.0F},
        SceneColorTransform{1.25F, 1.1F},
    });

    CHECK_FALSE(queue.Camera().has_value());
    CHECK_FALSE(queue.ViewModelCamera().has_value());
    CHECK(queue.Meshes().empty());
    CHECK(queue.Sprites().empty());
    CHECK(queue.Frame().clearColor.red == doctest::Approx(1.0F));
    CHECK(queue.Frame().sceneColorTransform.exposureEv == doctest::Approx(1.25F));
    CHECK(queue.Frame().sceneColorTransform.gammaAdjustment == doctest::Approx(1.1F));
}

TEST_CASE("Mesh layers require independent valid cameras and retain layer identity") {
    RenderQueue queue;
    MeshSubmission world;
    world.mesh = MeshHandle::FromParts(1, 1);
    MeshSubmission viewModel = world;
    viewModel.layer = MeshLayer::ViewModel;

    CHECK(world.layer == MeshLayer::World);
    CHECK_FALSE(queue.Submit(world));
    CHECK_FALSE(queue.Submit(viewModel));
    CHECK(queue.Meshes().empty());

    PerspectiveCamera3D viewModelCamera;
    viewModelCamera.verticalFieldOfViewRadians = 0.9F;
    queue.SetViewModelCamera(viewModelCamera);
    REQUIRE(queue.Submit(viewModel));
    CHECK_FALSE(queue.Submit(world));
    queue.SetCamera(PerspectiveCamera3D{});
    REQUIRE(queue.Submit(world));
    CHECK(queue.Meshes()[0].layer == MeshLayer::ViewModel);
    CHECK(queue.Meshes()[1].layer == MeshLayer::World);
    CHECK(queue.ViewModelCamera()->verticalFieldOfViewRadians == doctest::Approx(0.9F));
    CHECK(queue.Camera()->verticalFieldOfViewRadians != doctest::Approx(0.9F));

    queue.ClearViewModelCamera();
    CHECK_FALSE(queue.Submit(viewModel));
    CHECK(queue.Camera().has_value());
    queue.SetViewModelCamera(viewModelCamera);
    queue.ClearCamera();
    CHECK_FALSE(queue.Submit(world));
    REQUIRE(queue.Submit(viewModel));

    MeshSubmission invalid = viewModel;
    invalid.layer = static_cast<MeshLayer>(255);
    CHECK_FALSE(queue.Submit(invalid));
}

TEST_CASE("Mesh submission validates the selected camera projection and finite pose") {
    RenderQueue queue;
    MeshSubmission mesh;
    mesh.mesh = MeshHandle::FromParts(1, 1);
    for (const MeshLayer layer : {MeshLayer::World, MeshLayer::ViewModel, MeshLayer::WorldOverlay}) {
        mesh.layer = layer;
        const auto setCamera = [&](const PerspectiveCamera3D& camera) {
            if (layer == MeshLayer::ViewModel) queue.SetViewModelCamera(camera);
            else queue.SetCamera(camera);
        };
        PerspectiveCamera3D camera;
        camera.nearClip = 0.0F;
        setCamera(camera);
        CHECK_FALSE(queue.Submit(mesh));
        camera = {};
        camera.farClip = camera.nearClip;
        setCamera(camera);
        CHECK_FALSE(queue.Submit(mesh));
        camera = {};
        camera.verticalFieldOfViewRadians = 4.0F;
        setCamera(camera);
        CHECK_FALSE(queue.Submit(mesh));
        camera = {};
        camera.position.y = std::numeric_limits<float>::quiet_NaN();
        setCamera(camera);
        CHECK_FALSE(queue.Submit(mesh));
        camera = {};
        camera.rotationRadians.x = std::numeric_limits<float>::infinity();
        setCamera(camera);
        CHECK_FALSE(queue.Submit(mesh));
        setCamera({});
        REQUIRE(queue.Submit(mesh));
    }
    CHECK(queue.Meshes().size() == 3);
}

TEST_CASE("World overlay submissions require the world camera and preserve legacy layer values") {
    CHECK(static_cast<int>(MeshLayer::World) == 0);
    CHECK(static_cast<int>(MeshLayer::ViewModel) == 1);
    RenderQueue queue;
    queue.SetViewModelCamera({});
    MeshSubmission mesh;
    mesh.mesh = MeshHandle::FromParts(1, 1);
    mesh.layer = MeshLayer::WorldOverlay;
    CHECK_FALSE(queue.Submit(mesh));
    queue.SetCamera({});
    REQUIRE(queue.Submit(mesh));
    CHECK(queue.Meshes().front().layer == MeshLayer::WorldOverlay);
}

TEST_CASE("Scene color transform defaults are identity") {
    const FrameDescription frame;
    CHECK(frame.sceneColorTransform.exposureEv == doctest::Approx(0.0F));
    CHECK(frame.sceneColorTransform.gammaAdjustment == doctest::Approx(1.0F));
}

TEST_CASE("Resource handles carry type-safe index and generation values") {
    const MeshHandle mesh = MeshHandle::FromParts(12, 8);
    const TextureHandle texture = TextureHandle::FromParts(12, 8);
    CHECK(mesh.IsValid());
    CHECK(mesh.Index() == 12);
    CHECK(mesh.Generation() == 8);
    CHECK_FALSE(MeshHandle{}.IsValid());
    CHECK(texture.Index() == mesh.Index());
}

TEST_CASE("Material shader IDs and raster states reject empty or unsupported values") {
    RenderQueue queue; queue.SetCamera({});
    MeshSubmission mesh; mesh.mesh = MeshHandle::FromParts(1,1);
    mesh.material.shader.clear(); CHECK_FALSE(queue.Submit(mesh));
    mesh.material.shader = "builtin/unlit";
    mesh.surface = static_cast<SurfaceMode>(255); CHECK_FALSE(queue.Submit(mesh));
    mesh.surface = SurfaceMode::Opaque;
    mesh.material.sampler = static_cast<SamplerMode>(255); CHECK_FALSE(queue.Submit(mesh));
    SpriteSubmission sprite; sprite.material.sampler = static_cast<SamplerMode>(255);
    CHECK_FALSE(queue.Submit(sprite));
}

} // namespace
