#include <doctest/doctest.h>

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
#include "engine/asset/resolver/AssetPathResolver.hpp"
#include "render/IRenderDevice.hpp"
#include "render/RenderQueue.hpp"
#include "text/ITextRasterizer.hpp"
#include "ui/UiRenderer.hpp"

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
        return Base::Result<std::vector<std::byte>, Asset::AssetError>::Ok(
            {std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03}});
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
        return Base::Result<Text::TextBitmap, Text::TextError>::Ok(
            std::move(bitmap));
    }
};

class FakeRenderDevice final : public Render::IRenderDevice {
public:
    std::size_t createTextureCount{};
    std::vector<Render::TextureColorSpace> uploadedColorSpaces;
    std::vector<Render::TextureHandle> releasedTextures;

    Base::Result<Render::MeshHandle, Render::RenderError> CreateMesh(
        const Render::MeshView&) override {
        return Base::Result<Render::MeshHandle, Render::RenderError>::Err(
            Render::RenderError::Make(
                Render::RenderErrorCode::ResourceCreationFailed,
                "unexpected call"));
    }

    Base::Result<Render::TextureHandle, Render::RenderError> CreateTexture(
        const Render::ImageView& image) override {
        ++createTextureCount;
        uploadedColorSpaces.push_back(image.colorSpace);
        return Base::Result<Render::TextureHandle, Render::RenderError>::Ok(
            Render::TextureHandle::FromParts(
                static_cast<std::uint32_t>(createTextureCount),
                1));
    }

    Base::Result<void, Render::RenderError> ReleaseMesh(
        Render::MeshHandle) override {
        return Base::Result<void, Render::RenderError>::Ok();
    }

    Base::Result<void, Render::RenderError> ReleaseTexture(
        Render::TextureHandle handle) override {
        releasedTextures.push_back(handle);
        return Base::Result<void, Render::RenderError>::Ok();
    }

    Base::Result<Render::PresentStatus, Render::RenderError> Render(
        const Render::RenderQueue&) override {
        return Base::Result<Render::PresentStatus, Render::RenderError>::Ok(
            Render::PresentStatus::Presented);
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
    CHECK(sprite.tint.green == doctest::Approx(0.25F));
}

TEST_CASE("UiRenderer rejects use before initialization") {
    UiRenderer renderer;
    Render::RenderQueue queue;
    auto result = renderer.Submit({}, queue);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == UiErrorCode::RuntimeState);
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
    CHECK(queue.Sprites()[0].texture == queue.Sprites()[1].texture);
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

} // namespace Engine::Ui::Tests
