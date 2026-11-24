// The 2D pipeline's functions (docs/architecture/target/v2/render.zh-Hant.md section 7, RD-5 to RD-7): shapes to
// triangle lists, sprite geometry, the textures of images and one camera's pass (viewport, clear, layers, back to front
// by z, world units to viewport pixels, batching). No World, no backend. Exact values: every input is chosen so the
// float arithmetic is exact, but for the camera's matrices (compared to a thousandth of a pixel). Tags "[p2-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/image/ImageData.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/transform/Transform.hpp"

namespace C = GYO::Engine::Camera;
namespace I = GYO::Engine::Image;
namespace M = GYO::Engine::Math;
namespace R = GYO::Engine::Render;
namespace T = GYO::Engine::Transform;

namespace {

M::Matrix4 At(const float x, const float y, const float z) {
    T::Transform t;
    t.translation = {x, y, z};
    return T::ToMatrix(t);
}

// A 2D camera (Default2d: a larger z in front) showing 1280 x 720 world units, letterboxed into a 2560 x 1600 target:
// the viewport is (0, 80) 2560 x 1440, two pixels a unit.
R::ExtractedCamera Camera2d(const float x = 0.0F, const float y = 0.0F) {
    C::OrthographicProjection o = C::OrthographicProjection::Default2d();
    o.scalingMode = C::ScalingFixed{1280.0F, 720.0F};
    const C::TargetSize size{{2560, 1600}, 1.0F};
    const C::Letterbox box{1280.0F, 720.0F};
    R::ExtractedCamera camera;
    camera.computed = C::Compute(C::Camera{}, C::Projection{o}, &size, &box);
    camera.view = At(x, y, 0.0F);
    camera.is2d = true;
    camera.clear = R::Color8{0, 0, 0, 255};
    return camera;
}

// A square of side 2 units around the item's position, one colour.
R::Extracted2dItem Square(const std::uint64_t entity, const float x, const float y, const float z, const R::TextureId texture = {}) {
    R::Extracted2dItem item;
    item.entity = entity;
    item.world = At(x, y, z);
    item.texture = texture;
    item.vertices = {{{-1, 1}, {0, 0}, {}}, {{1, 1}, {1, 0}, {}}, {{1, -1}, {1, 1}, {}}, {{-1, -1}, {0, 1}, {}}};
    item.indices = {0, 1, 2, 0, 2, 3};
    return item;
}

// The camera's matrices are not exact in floats (2 / 1280): positions are compared to a thousandth of a pixel.
bool Near(const float a, const float b) { return std::fabs(a - b) <= 1e-3F; }

std::shared_ptr<const I::ImageData> Picture(std::uint32_t w, std::uint32_t h) {
    return std::make_shared<const I::ImageData>(w, h, I::ColorSpace::Srgb, std::vector<std::uint8_t>(std::size_t{w} * h * 4, 9));
}

} // namespace

TEST_CASE("pipeline2d: a list is its indices; a strip unrolls into triangles; no indices is the vertices in order") {
    R::Shape2d list;
    list.vertices.resize(4);
    list.indices = {0, 1, 2, 2, 3};
    CHECK_MESSAGE((R::TriangleListOf(list) == std::vector<std::uint32_t>{0, 1, 2}), "[p2-list-partial] a partial triangle is dropped");
    R::Shape2d strip;
    strip.topology = R::Topology::TriangleStrip;
    strip.vertices.resize(5);
    CHECK_MESSAGE((R::TriangleListOf(strip) == std::vector<std::uint32_t>{0, 1, 2, 2, 1, 3, 2, 3, 4}),
                  "[p2-strip] each new vertex makes a triangle with the two before it, odd ones turned");
    strip.indices = {4, 3, 2};
    CHECK_MESSAGE((R::TriangleListOf(strip) == std::vector<std::uint32_t>{4, 3, 2}), "[p2-strip-indices] a strip through indices");
    strip.indices = {0, 1};
    CHECK_MESSAGE(R::TriangleListOf(strip).empty(), "[p2-strip-short] fewer than three: nothing");
}

TEST_CASE("pipeline2d: a sprite is its picture's size around its anchor, y up, the top row at the top") {
    std::vector<R::Vertex2D> v;
    std::vector<std::uint32_t> i;
    R::Sprite sprite;
    R::SpriteGeometry(sprite, M::Vec2{4.0F, 2.0F}, v, i);
    REQUIRE(v.size() == 4);
    CHECK_MESSAGE((v[0].position.x == -2.0F && v[0].position.y == 1.0F && v[0].uv.x == 0.0F && v[0].uv.y == 0.0F),
                  "[p2-sprite-top-left] the picture's top-left at the top-left corner");
    CHECK_MESSAGE((v[2].position.x == 2.0F && v[2].position.y == -1.0F && v[2].uv.x == 1.0F && v[2].uv.y == 1.0F),
                  "[p2-sprite-bottom-right]");
    CHECK((i == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3}));
    v.clear();
    i.clear();
    sprite.customSize = M::Vec2{10.0F, 10.0F};
    sprite.anchor = M::Vec2{-0.5F, -0.5F};  // the bottom-left corner at the position
    sprite.rect = M::Rect{1.0F, 0.0F, 2.0F, 1.0F};
    sprite.flipX = true;
    R::SpriteGeometry(sprite, M::Vec2{4.0F, 2.0F}, v, i);
    CHECK_MESSAGE((v[0].position.x == 0.0F && v[0].position.y == 10.0F && v[2].position.x == 10.0F && v[2].position.y == 0.0F),
                  "[p2-sprite-anchor-size] custom size from the anchor");
    CHECK_MESSAGE((v[0].uv.x == 0.75F && v[1].uv.x == 0.25F && v[0].uv.y == 0.0F && v[2].uv.y == 0.5F),
                  "[p2-sprite-rect-flip] the region's uv, flipped on x");
}

TEST_CASE("pipeline2d: one camera's pass has its viewport and clear; world units go to viewport pixels") {
    const R::ExtractedCamera camera = Camera2d();
    REQUIRE(camera.computed.valid);
    std::vector<R::Extracted2dItem> items{Square(1, 0.0F, 0.0F, 0.0F)};
    items[0].vertices = {{{-640, 360}, {}, {}}, {{640, 360}, {}, {}}, {{640, -360}, {}, {}}, {{-640, -360}, {}, {}}};
    const std::optional<R::Pass2D> pass = R::Queue2d(camera, items);
    REQUIRE(pass.has_value());
    CHECK_MESSAGE((pass->viewport == R::PixelRect{0, 80, 2560, 1440}), "[p2-viewport] the camera's viewport (letterbox)");
    CHECK_MESSAGE((pass->clear == R::Color8{0, 0, 0, 255}), "[p2-clear] the camera's clear");
    CHECK_MESSAGE(!pass->view.has_value(), "[p2-view-pixels] the view is the viewport's pixels");
    REQUIRE(pass->vertices.size() == 4);
    CHECK_MESSAGE((Near(pass->vertices[0].position.x, 0.0F) && Near(pass->vertices[0].position.y, 0.0F)), "[p2-map-top-left] world (-640, 360)");
    CHECK_MESSAGE((Near(pass->vertices[2].position.x, 2560.0F) && Near(pass->vertices[2].position.y, 1440.0F)), "[p2-map-bottom-right] world (640, -360)");
    // The camera moved right: what is at its position is at the viewport's centre.
    const std::optional<R::Pass2D> moved = R::Queue2d(Camera2d(100.0F, 0.0F), std::vector<R::Extracted2dItem>{Square(1, 100.0F, 0.0F, 0.0F)});
    REQUIRE(moved.has_value());
    CHECK_MESSAGE((Near(moved->vertices[0].position.x, 1278.0F) && Near(moved->vertices[0].position.y, 718.0F)), "[p2-map-view] the camera's position");
}

TEST_CASE("pipeline2d: items are drawn back to front by z, ties by entity") {
    const std::vector<R::Extracted2dItem> items{Square(5, 0, 0, 2.0F, R::TextureId{5}), Square(3, 0, 0, -1.0F, R::TextureId{3}),
                                                Square(9, 0, 0, 2.0F, R::TextureId{9})};
    const std::optional<R::Pass2D> pass = R::Queue2d(Camera2d(), items);
    REQUIRE(pass.has_value());
    const std::vector<R::Draw2D>& d = pass->draws;
    CHECK_MESSAGE((d.size() == 3 && d[0].texture.value == 3), "[p2-z-order] the smallest z first");
    CHECK_MESSAGE((d.size() == 3 && d[1].texture.value == 5 && d[2].texture.value == 9), "[p2-z-tie] equal z by entity");
    CHECK_MESSAGE((d.size() == 3 && d[2].indexOffset == 12), "[p2-offsets] each draw names its own indices");
    CHECK_MESSAGE((pass->indices.size() == 18 && pass->indices[12] == 8), "[p2-index-base] indices point at the item's own vertices");
}

TEST_CASE("pipeline2d: a camera draws only the items on its layers") {
    std::vector<R::Extracted2dItem> items{Square(1, 0, 0, 0.0F, R::TextureId{1}), Square(2, 0, 0, 1.0F, R::TextureId{2})};
    items[1].layers = 2;
    R::ExtractedCamera camera = Camera2d();
    const std::optional<R::Pass2D> world = R::Queue2d(camera, items);
    REQUIRE(world.has_value());
    CHECK_MESSAGE((world->draws.size() == 1 && world->draws[0].texture.value == 1), "[p2-layers] layer 0 only");
    camera.layers = 2;
    const std::optional<R::Pass2D> hud = R::Queue2d(camera, items);
    REQUIRE(hud.has_value());
    CHECK_MESSAGE((hud->draws.size() == 1 && hud->draws[0].texture.value == 2), "[p2-layers] layer 1 only");
}

TEST_CASE("pipeline2d: an item before the near plane or past the far plane is not drawn") {
    // Default2d: near at z 1000, far at z -1000.
    const std::vector<R::Extracted2dItem> items{Square(1, 0, 0, 1500.0F, R::TextureId{1}), Square(2, 0, 0, -1500.0F, R::TextureId{2}),
                                                Square(3, 0, 0, 999.0F, R::TextureId{3})};
    const std::optional<R::Pass2D> pass = R::Queue2d(Camera2d(), items);
    REQUIRE(pass.has_value());
    CHECK_MESSAGE((pass->draws.size() == 1 && pass->draws[0].texture.value == 3), "[p2-near-far] only the item between the planes");
}

TEST_CASE("pipeline2d: consecutive items with one texture and blend are one draw") {
    std::vector<R::Extracted2dItem> items{Square(1, 0, 0, 0), Square(2, 0, 0, 1), Square(3, 0, 0, 2, R::TextureId{7}), Square(4, 0, 0, 3),
                                          Square(5, 0, 0, 4)};
    items[4].blend = R::BlendMode::Opaque;
    const std::optional<R::Pass2D> pass = R::Queue2d(Camera2d(), items);
    REQUIRE(pass.has_value());
    const std::vector<R::Draw2D>& d = pass->draws;
    CHECK_MESSAGE((!d.empty() && d[0].indexOffset == 0 && d[0].indexCount == 12), "[p2-batch] two white squares in one draw");
    CHECK_MESSAGE((d.size() >= 2 && d[1].texture.value == 7 && d[1].indexCount == 6), "[p2-batch-texture] another texture breaks it");
    CHECK_MESSAGE((d.size() == 4 && d[2].indexCount == 6 && d[3].blend == R::BlendMode::Opaque), "[p2-batch-blend] another blend breaks it");
}

TEST_CASE("pipeline2d: a camera without a target size or not 2D draws no 2D pass") {
    R::ExtractedCamera camera = Camera2d();
    camera.is2d = false;
    camera.is3d = true;
    CHECK_MESSAGE(!R::Queue2d(camera, {}).has_value(), "[p2-3d-camera] a 3D camera");
    R::ExtractedCamera invalid = Camera2d();
    invalid.computed.valid = false;
    CHECK_MESSAGE(!R::Queue2d(invalid, {}).has_value(), "[p2-invalid-camera] not valid");
    R::ExtractedCamera hud = Camera2d();
    hud.clear = std::nullopt;
    const std::optional<R::Pass2D> pass = R::Queue2d(hud, {});
    REQUIRE(pass.has_value());
    CHECK_MESSAGE(!pass->clear.has_value(), "[p2-clear-none] a camera that does not clear");
}

TEST_CASE("pipeline2d: a picture's texture is made once; an asset's stays, a held image's goes when unused") {
    R::RenderTextures textures;
    R::RenderImages images;
    M::Vec2 size{};
    CHECK_MESSAGE((images.Use(R::ImageKey{}, nullptr, nullptr, textures, size) == R::TextureId{} && size.x == 1.0F),
                  "[p2-image-none] no picture: the white texture, 1 x 1");
    const auto held = Picture(4, 2);
    const R::ResolvedImage resolved = R::ResolveImage(R::ImageRef{held}, R::TextureFilter::Nearest, nullptr);
    CHECK(resolved.data == held.get());
    const std::optional<R::TextureId> first = images.Use(resolved.key, resolved.data, resolved.holder, textures, size);
    REQUIRE(first.has_value());
    CHECK_MESSAGE((size.x == 4.0F && size.y == 2.0F), "[p2-image-size] the picture's size");
    CHECK_MESSAGE(textures.Find(*first)->filter == R::TextureFilter::Nearest, "[p2-image-filter]");
    const std::optional<R::TextureId> again = images.Use(resolved.key, resolved.data, resolved.holder, textures, size);
    CHECK_MESSAGE((again == first && textures.LiveCount() == 1), "[p2-image-once] one texture for one picture");

    R::ImageKey asset;
    asset.asset = "node.heart";
    asset.life = 1;
    CHECK_MESSAGE(!images.Use(asset, nullptr, nullptr, textures, size).has_value(), "[p2-image-not-loaded] skipped until loaded");
    const auto loaded = Picture(2, 2);
    REQUIRE(images.Use(asset, loaded.get(), nullptr, textures, size).has_value());
    images.EndFrame(textures);  // both used this frame
    CHECK(textures.LiveCount() == 2);
    images.EndFrame(textures);  // neither used this frame: the held one goes, the asset's stays
    CHECK_MESSAGE((textures.LiveCount() == 1 && textures.Find(*first) == nullptr), "[p2-image-held-released] an unused held image is let go");
    CHECK_MESSAGE(images.Count() == 1, "[p2-image-asset-kept] an asset's texture stays");
    CHECK_MESSAGE(!R::ResolveImage(R::ImageRef{std::shared_ptr<const I::ImageData>{}}, R::TextureFilter::Linear, nullptr).key.asset.empty(),
                  "[p2-image-null-held] a null image is skipped, not white");
}
