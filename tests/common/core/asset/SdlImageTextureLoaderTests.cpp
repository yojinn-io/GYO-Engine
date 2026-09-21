#include "doctest/doctest.h"

#include <array>
#include <cstddef>
#include <iterator>

#include "engine/asset/AssetError.hpp"
#include "engine/asset/AssetId.hpp"
#include "engine/asset/AssetType.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "engine/asset/loaders/sdl_image/SdlImageTextureLoader.hpp"
#include "engine/asset/loading/LoadContext.hpp"

using namespace Engine::Asset;

namespace {

// A complete 1x1 PNG kept in the test so the decoder contract does not depend
// on any game's asset catalog or content fixture.
constexpr std::array kSinglePixelPng{
    std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47},
    std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x0D},
    std::byte{0x49}, std::byte{0x48}, std::byte{0x44}, std::byte{0x52},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x01},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x01},
    std::byte{0x08}, std::byte{0x04}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0xB5}, std::byte{0x1C}, std::byte{0x0C},
    std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x0B}, std::byte{0x49}, std::byte{0x44}, std::byte{0x41},
    std::byte{0x54}, std::byte{0x78}, std::byte{0xDA}, std::byte{0x63},
    std::byte{0x64}, std::byte{0xF8}, std::byte{0x0F}, std::byte{0x00},
    std::byte{0x01}, std::byte{0x05}, std::byte{0x01}, std::byte{0x01},
    std::byte{0x27}, std::byte{0x18}, std::byte{0xE3}, std::byte{0x66},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x49}, std::byte{0x45}, std::byte{0x4E}, std::byte{0x44},
    std::byte{0xAE}, std::byte{0x42}, std::byte{0x60}, std::byte{0x82},
};

} // namespace

TEST_CASE("SDL_image texture loader decodes an in-memory PNG as RGBA8") {

    Loading::LoadContext context;
    context.id = AssetId::FromString("engine.test.texture.single_pixel");
    context.type = AssetType::Texture();
    context.resolvedPath = "memory://single_pixel.png";

    Loaders::SdlImage::SdlImageTextureLoader loader;
    auto decoded = loader.Load(
        Engine::Base::ConstSpan<std::byte>{kSinglePixelPng.data(), kSinglePixelPng.size()},
        context);
    REQUIRE(decoded);
    auto texture = decoded.value().ShareAs<Loaders::TextureAsset>();
    REQUIRE(static_cast<bool>(texture));
    CHECK(texture->width == 1);
    CHECK(texture->height == 1);
    CHECK(texture->rgba.size() == 4);
}

TEST_CASE("SDL_image texture loader rejects invalid bytes") {
    constexpr std::byte invalid[] = {std::byte{0x00}, std::byte{0x01}};
    Loading::LoadContext context;
    context.id = AssetId::FromString("engine.test.texture.invalid");
    context.type = AssetType::Texture();
    context.resolvedPath = "memory://invalid.png";

    Loaders::SdlImage::SdlImageTextureLoader loader;
    const auto result = loader.Load(
        Engine::Base::ConstSpan<std::byte>{invalid, std::size(invalid)}, context);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == AssetErrorCode::DecodeFailed);
}
