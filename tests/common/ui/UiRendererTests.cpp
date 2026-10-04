#include <doctest/doctest.h>
#include "AssertTestSupport.hpp"

#include "engine/asset/AssetCatalog.hpp"
#include "engine/asset/AssetManager.hpp"
#include "engine/asset/catalog/CatalogParser.hpp"
#include "engine/asset/core/AssetCachePolicy.hpp"
#include "engine/asset/core/AssetLifetime.hpp"
#include "engine/asset/core/AssetStorage.hpp"
#include "engine/asset/loading/AssetPipeline.hpp"
#include "engine/asset/loading/IAssetSource.hpp"
#include "engine/asset/loading/LoaderRegistry.hpp"
#include "engine/asset/loaders/FontLoader.hpp"
#include "engine/asset/loaders/TextureLoader.hpp"
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "render/IRenderDevice.hpp"
#include "RenderDeviceStub.hpp"
#include "render/RenderQueue.hpp"
#include "text/ITextRasterizer.hpp"
#include "ui/UiRenderer.hpp"

#include <limits>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Ui::Tests {
namespace {

class EmptyAssetSource final : public Asset::Loading::IAssetSource {
public:
    Base::Result<std::vector<std::byte>, Asset::AssetError> ReadAll(
        std::string_view) override {
        return std::vector<std::byte>{std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    }
};

class UnusedRasterizer final : public Text::ITextRasterizer {
public:
    std::size_t callCount{};

    Base::Result<Text::TextBitmap, Text::TextError> Rasterize(
        std::span<const std::byte>,
        const Text::TextRasterRequest&) override {
        ++callCount;
        Text::TextBitmap bitmap;
        bitmap.width = 2;
        bitmap.height = 2;
        bitmap.rowPitch = 8;
        bitmap.rgba8.resize(16, std::byte{0xFF});
        return std::move(bitmap);
    }
};

class FakeRenderDevice final : public Gyo::Tests::RenderDeviceStub {
public:
    std::size_t createTextureCount{};
    std::vector<Render::TextureColorSpace> uploadedColorSpaces;
    std::vector<Render::TextureHandle> releasedTextures;

    Base::Result<Render::MeshHandle, Render::RenderError> CreateMesh(
        const Render::MeshView&) override {
        return Engine::Base::Err(Render::RenderError::Make(
                Render::RenderErrorCode::ResourceCreationFailed,
                "unexpected call"));
    }

    Base::Result<Render::TextureHandle, Render::RenderError> CreateTexture(
        const Render::ImageView& image) override {
        ++createTextureCount;
        uploadedColorSpaces.push_back(image.colorSpace);
        return Render::TextureHandle::FromParts(
                static_cast<std::uint32_t>(createTextureCount),
                1);
    }

    Base::Result<void, Render::RenderError> ReleaseMesh(
        Render::MeshHandle) override {
        return {};
    }

    Base::Result<void, Render::RenderError> ReleaseTexture(
        Render::TextureHandle handle) override {
        releasedTextures.push_back(handle);
        return {};
    }

};

struct RendererFixture final {
    Asset::AssetCatalog catalog;
    Asset::Loading::LoaderRegistry registry;
    EmptyAssetSource source;
    Asset::Loading::AssetPipeline pipeline{source, registry};
    Asset::Core::AssetStorage storage;
    Asset::Core::AssetLifetime lifetime;
    Asset::Core::AssetCachePolicy policy{{}};
    Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};
    FakeRenderDevice renderDevice;
    UnusedRasterizer rasterizer;
};

struct FontRendererFixture final {
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        "gyo_ui_renderer_font_cache_test";
    Asset::AssetCatalog catalog;
    Asset::Loading::LoaderRegistry registry;
    EmptyAssetSource source;
    Asset::Loading::AssetPipeline pipeline{source, registry};
    Asset::Core::AssetStorage storage;
    Asset::Core::AssetLifetime lifetime;
    Asset::Core::AssetCachePolicy policy{{}};
    Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};
    FakeRenderDevice renderDevice;
    UnusedRasterizer rasterizer;

    FontRendererFixture() {
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const std::filesystem::path catalogPath = directory / "catalog.json";
        std::ofstream output(catalogPath, std::ios::binary);
        output << R"({"version":1,"assets":[{"id":"test.font","type":"font","path":"font.ttf"}]})";
        output.close();

        registry.Register(std::make_unique<Asset::Loaders::FontLoader>());
        Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = (directory / "assets").string();
        Asset::Resolver::AssetPathResolver resolver(options);
        Asset::Catalog::CatalogParser parser;
        REQUIRE(catalog.LoadFromFile(catalogPath.string(), parser, resolver));
    }

    ~FontRendererFixture() {
        std::filesystem::remove_all(directory);
    }
};

// Serves a 1x1 white P3 PPM for every path so TextureLoader can decode it.
class PpmAssetSource final : public Asset::Loading::IAssetSource {
public:
    Base::Result<std::vector<std::byte>, Asset::AssetError> ReadAll(
        std::string_view) override {
        constexpr std::string_view kPpm = "P3\n1 1\n255\n255 255 255\n";
        std::vector<std::byte> bytes;
        for (const char value : kPpm) {
            bytes.push_back(static_cast<std::byte>(value));
        }
        return std::move(bytes);
    }
};

struct TextureRendererFixture final {
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        "gyo_ui_renderer_texture_clip_test";
    Asset::AssetCatalog catalog;
    Asset::Loading::LoaderRegistry registry;
    PpmAssetSource source;
    Asset::Loading::AssetPipeline pipeline{source, registry};
    Asset::Core::AssetStorage storage;
    Asset::Core::AssetLifetime lifetime;
    Asset::Core::AssetCachePolicy policy{{}};
    Asset::AssetManager assets{catalog, pipeline, storage, lifetime, policy};
    FakeRenderDevice renderDevice;
    UnusedRasterizer rasterizer;

    TextureRendererFixture() {
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        const std::filesystem::path catalogPath = directory / "catalog.json";
        std::ofstream output(catalogPath, std::ios::binary);
        output << R"({"version":1,"assets":[{"id":"test.texture","type":"texture","path":"atlas.ppm"}]})";
        output.close();

        registry.Register(std::make_unique<Asset::Loaders::TextureLoader>());
        Asset::Resolver::AssetPathResolver::Options options;
        options.assetsRoot = (directory / "assets").string();
        Asset::Resolver::AssetPathResolver resolver(options);
        Asset::Catalog::CatalogParser parser;
        REQUIRE(catalog.LoadFromFile(catalogPath.string(), parser, resolver));
    }

    ~TextureRendererFixture() {
        std::filesystem::remove_all(directory);
    }
};

using Bits4 = std::array<std::uint32_t, 4>;

template <class Rect>
[[nodiscard]] Bits4 RectBits(const Rect& rect) noexcept {
    return {
        std::bit_cast<std::uint32_t>(rect.x),
        std::bit_cast<std::uint32_t>(rect.y),
        std::bit_cast<std::uint32_t>(rect.width),
        std::bit_cast<std::uint32_t>(rect.height),
    };
}

[[nodiscard]] Bits4 ArrayBits(const std::array<float, 4>& values) noexcept {
    return {
        std::bit_cast<std::uint32_t>(values[0]),
        std::bit_cast<std::uint32_t>(values[1]),
        std::bit_cast<std::uint32_t>(values[2]),
        std::bit_cast<std::uint32_t>(values[3]),
    };
}

[[nodiscard]] UiImageDraw ImageDraw(
    std::array<float, 4> destination,
    std::array<float, 4> sourceUv) {
    UiImageDraw draw;
    draw.destinationPixels = {destination[0], destination[1], destination[2], destination[3]};
    draw.sourceUv = {sourceUv[0], sourceUv[1], sourceUv[2], sourceUv[3]};
    draw.textureAssetId = "test.texture";
    return draw;
}

} // namespace

TEST_CASE("UiRenderer CPU-clips sprites and always submits Overlay") {
    RendererFixture fixture;
    UiRenderer renderer;
    REQUIRE(renderer.Initialize(
        fixture.renderDevice,
        fixture.rasterizer,
        fixture.assets));

    UiDrawList drawList;
    drawList.commands.emplace_back(UiQuadDraw{
        {0.0F, 0.0F, 100.0F, 100.0F},
        {0.5F, 0.25F, 0.125F, 1.0F},
        {25.0F, 25.0F, 50.0F, 50.0F},
    });
    Render::RenderQueue queue;
    REQUIRE(renderer.Submit(drawList, queue));
    REQUIRE(queue.Sprites().size() == 1);
    const Render::SpriteSubmission& sprite = queue.Sprites().front();
    CHECK(sprite.layer == Render::CompositeLayer::Overlay);
    CHECK(sprite.destinationPixels.x == doctest::Approx(25.0F));
    CHECK(sprite.destinationPixels.y == doctest::Approx(25.0F));
    CHECK(sprite.destinationPixels.width == doctest::Approx(50.0F));
    CHECK(sprite.sourceUv.x == doctest::Approx(0.25F));
    CHECK(sprite.sourceUv.y == doctest::Approx(0.25F));
    CHECK(sprite.sourceUv.width == doctest::Approx(0.5F));
    CHECK(sprite.material.tint.green == doctest::Approx(0.25F));
}

TEST_CASE("UiRenderer use before initialization is a Programmer Error") {
    UiRenderer renderer;
    Render::RenderQueue queue;
    GYO_CHECK_ASSERTS(renderer.Submit({}, queue));
}

TEST_CASE("UiRenderer caches whole UTF-8 runs and evicts retained LRU textures") {
    FontRendererFixture fixture;
    UiRenderer renderer;
    REQUIRE(renderer.Initialize(
        fixture.renderDevice,
        fixture.rasterizer,
        fixture.assets,
        UiRendererOptions{2}));

    const auto draw = [](std::string text, float size = 12.0F) {
        UiTextDraw result;
        result.boundsPixels = {0.0F, 0.0F, 100.0F, 20.0F};
        result.utf8 = std::move(text);
        result.fontAssetId = "test.font";
        result.pointSizePixels = size;
        return UiDrawCommand{std::move(result)};
    };

    Render::RenderQueue queue;
    UiDrawList repeated;
    repeated.commands.push_back(draw("日本語"));
    repeated.commands.push_back(draw("日本語"));
    REQUIRE(renderer.Submit(repeated, queue));
    CHECK(fixture.rasterizer.callCount == 1);
    CHECK(fixture.renderDevice.createTextureCount == 1);
    REQUIRE(queue.Sprites().size() == 2);
    CHECK(queue.Sprites()[0].material.texture == queue.Sprites()[1].material.texture);
    CHECK(queue.Sprites()[0].layer == Render::CompositeLayer::Overlay);
    CHECK(fixture.renderDevice.uploadedColorSpaces[0] ==
          Render::TextureColorSpace::Linear);

    queue.Reset();
    REQUIRE(renderer.Submit({{draw("SECOND")}}, queue));
    CHECK(fixture.rasterizer.callCount == 2);
    queue.Reset();
    REQUIRE(renderer.Submit({{draw("THIRD", 14.0F)}}, queue));
    CHECK(fixture.rasterizer.callCount == 3);

    // A frame may temporarily enlarge the working set because its submitted
    // handles must remain alive until Render(). The next Submit safely trims
    // retained LRU entries before resolving the new frame.
    queue.Reset();
    REQUIRE(renderer.Submit({{draw("日本語")}}, queue));
    CHECK(fixture.renderDevice.releasedTextures.size() == 1);
    CHECK(fixture.rasterizer.callCount == 4);
    CHECK(fixture.renderDevice.createTextureCount == 4);

    renderer.Reset();
    CHECK(fixture.renderDevice.releasedTextures.size() == 4);
}

TEST_CASE("UiRenderer clips finite sprites to exact destination and source UV bits") {
    TextureRendererFixture fixture;
    UiRenderer renderer;
    REQUIRE(renderer.Initialize(
        fixture.renderDevice,
        fixture.rasterizer,
        fixture.assets));

    UiDrawList drawList;
    UiQuadDraw quad;
    quad.destinationPixels = {10.5F, 20.25F, 100.0F, 50.0F};
    quad.clipPixels = {30.1F, 0.0F, 50.3F, 40.0F};
    drawList.commands.emplace_back(quad);

    UiImageDraw leftTop = ImageDraw({-12.3F, 7.7F, 64.0F, 33.3F}, {0.125F, 0.25F, 0.5F, 0.75F});
    leftTop.clipPixels = {0.0F, 10.0F, 40.0F, 1000.0F};
    drawList.commands.emplace_back(leftTop);

    UiImageDraw allSides = ImageDraw({0.0F, 0.0F, 200.0F, 100.0F}, {0.1F, 0.2F, 0.3F, 0.4F});
    allSides.clipPixels = {12.5F, 7.5F, 50.25F, 60.75F};
    drawList.commands.emplace_back(allSides);

    Render::RenderQueue queue;
    REQUIRE(renderer.Submit(drawList, queue));
    REQUIRE(queue.Sprites().size() == 3);

    // Bit patterns captured on master 43bccad (ClipSprite in UiRenderer.cpp).
    // A clipped quad keeps the default {0, 0, 1, 1} source rectangle scaled by
    // the visible fraction; images scale their own source rectangle.
    const Render::SpriteSubmission& clippedQuad = queue.Sprites()[0];
    CHECK(RectBits(clippedQuad.destinationPixels) ==
          Bits4{0x41F0CCCDU, 0x41A20000U, 0x42493334U, 0x419E0000U});
    CHECK(RectBits(clippedQuad.sourceUv) ==
          Bits4{0x3E48B43AU, 0x00000000U, 0x3F00C49CU, 0x3ECA3D71U});

    const Render::SpriteSubmission& clippedLeftTop = queue.Sprites()[1];
    CHECK(RectBits(clippedLeftTop.destinationPixels) ==
          Bits4{0x00000000U, 0x41200000U, 0x42200000U, 0x41F80000U});
    CHECK(RectBits(clippedLeftTop.sourceUv) ==
          Bits4{0x3E626666U, 0x3E9A85C4U, 0x3EA00000U, 0x3F32BD1EU});

    const Render::SpriteSubmission& clippedAllSides = queue.Sprites()[2];
    CHECK(RectBits(clippedAllSides.destinationPixels) ==
          Bits4{0x41480000U, 0x40F00000U, 0x42490000U, 0x42730000U});
    CHECK(RectBits(clippedAllSides.sourceUv) ==
          Bits4{0x3DF33334U, 0x3E6B851FU, 0x3D9A5E36U, 0x3E78D4FFU});
}

TEST_CASE("UiRenderer keeps an unclipped image source UV bit for bit") {
    TextureRendererFixture fixture;
    UiRenderer renderer;
    REQUIRE(renderer.Initialize(
        fixture.renderDevice,
        fixture.rasterizer,
        fixture.assets));

    const std::array<float, 4> destination{12.0F, 34.0F, 56.0F, 78.0F};
    const std::array<float, 4> asymmetricUv{0.125F, 0.25F, 0.5F, 0.75F};
    const std::array<float, 4> nonBinaryUv{0.1F, 0.2F, 0.3F, 0.4F};

    UiDrawList drawList;
    // Default clip (0, 0, FLT_MAX, FLT_MAX).
    drawList.commands.emplace_back(ImageDraw(destination, asymmetricUv));
    drawList.commands.emplace_back(ImageDraw(destination, nonBinaryUv));
    // A finite clip that fully contains the destination.
    UiImageDraw contained = ImageDraw(destination, nonBinaryUv);
    contained.clipPixels = {0.0F, 0.0F, 1000.0F, 1000.0F};
    drawList.commands.emplace_back(contained);
    // A finite clip that shares the destination edges exactly.
    UiImageDraw sameEdges = ImageDraw(destination, asymmetricUv);
    sameEdges.clipPixels = {12.0F, 34.0F, 56.0F, 78.0F};
    drawList.commands.emplace_back(sameEdges);

    Render::RenderQueue queue;
    REQUIRE(renderer.Submit(drawList, queue));
    REQUIRE(queue.Sprites().size() == 4);
    const Bits4 destinationBits = ArrayBits(destination);
    const std::array<std::array<float, 4>, 4> expectedUv{
        asymmetricUv, nonBinaryUv, nonBinaryUv, asymmetricUv};
    for (std::size_t index = 0; index < queue.Sprites().size(); ++index) {
        CAPTURE(index);
        const Render::SpriteSubmission& sprite = queue.Sprites()[index];
        CHECK(RectBits(sprite.destinationPixels) == destinationBits);
        CHECK(RectBits(sprite.sourceUv) == ArrayBits(expectedUv[index]));
    }
}

TEST_CASE("UiRenderer drops finite sprites outside or on the edge of their clip") {
    TextureRendererFixture fixture;
    UiRenderer renderer;
    REQUIRE(renderer.Initialize(
        fixture.renderDevice,
        fixture.rasterizer,
        fixture.assets));

    UiDrawList drawList;
    UiQuadDraw touching;
    touching.destinationPixels = {0.0F, 0.0F, 10.0F, 10.0F};
    touching.clipPixels = {10.0F, 0.0F, 10.0F, 10.0F};
    drawList.commands.emplace_back(touching);
    UiImageDraw zeroClip = ImageDraw({0.0F, 0.0F, 10.0F, 10.0F}, {0.125F, 0.25F, 0.5F, 0.75F});
    zeroClip.clipPixels = {5.0F, 5.0F, 0.0F, 0.0F};
    drawList.commands.emplace_back(zeroClip);
    UiQuadDraw zeroSize;
    zeroSize.destinationPixels = {1.0F, 1.0F, 0.0F, 5.0F};
    drawList.commands.emplace_back(zeroSize);

    Render::RenderQueue queue;
    REQUIRE(renderer.Submit(drawList, queue));
    CHECK(queue.Sprites().empty());
}

TEST_CASE("UiRenderer reports every non-finite sprite destination as an error") {
    // B5: a non-finite destination is passed through ClipSprite unchanged so
    // RenderQueue rejects it. Before, x = -inf and y = +inf were silently
    // dropped, NaN errored, and +inf height was clipped into a sprite with a
    // zero-size source UV.
    const float infinity = std::numeric_limits<float>::infinity();
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const std::array<Math::Rect, 5> destinations{{
        {-infinity, 0.0F, 10.0F, 10.0F},
        {0.0F, infinity, 10.0F, 10.0F},
        {nan, 0.0F, 10.0F, 10.0F},
        {0.0F, 0.0F, nan, 10.0F},
        {0.0F, 0.0F, 10.0F, infinity},
    }};
    for (std::size_t index = 0; index < destinations.size(); ++index) {
        CAPTURE(index);
        TextureRendererFixture fixture;
        UiRenderer renderer;
        REQUIRE(renderer.Initialize(
            fixture.renderDevice,
            fixture.rasterizer,
            fixture.assets));
        UiDrawList drawList;
        UiQuadDraw quad;
        quad.destinationPixels = destinations[index];
        quad.clipPixels = {0.0F, 0.0F, 100.0F, 100.0F};
        drawList.commands.emplace_back(quad);
        Render::RenderQueue queue;
        CHECK_FALSE(renderer.Submit(drawList, queue));
        CHECK(queue.Sprites().empty());
    }
}

} // namespace Engine::Ui::Tests
