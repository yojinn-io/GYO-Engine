// ui_render's pass functions without a World (docs/architecture/target/v2/ui.zh-Hant.md, section 4.7): a draw list
// becomes a Pass2D on the camera's viewport; colours as sRGB bytes; clipping; batching; glyph uv in the atlas page.
// Tags "[ui-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <optional>
#include <vector>

#include "engine/math/scalar/ColorSpace.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/ui/UiTypes.hpp"
#include "engine/ui_render/UiPass.hpp"
#include "engine/ui_render/UiRenderModule.hpp"

namespace R = GYO::Engine::Render;
namespace U = GYO::Engine::UiRender;
namespace Ui = GYO::Engine::Ui;

namespace {

R::ExtractedCamera CameraOn(const std::uint64_t entity, const int x, const int y, const int w, const int h) {
    R::ExtractedCamera camera;
    camera.entity = entity;
    camera.target = 0;
    camera.computed.valid = true;
    camera.computed.viewport.position = {x, y};
    camera.computed.viewport.size = {w, h};
    return camera;
}

U::UiAtlasTextures OnePage() {
    U::UiAtlasTextures atlas;
    atlas.pages.push_back(R::TextureId{7});
    atlas.sizes.push_back(R::TextureDesc{256, 128, R::TextureFilter::Nearest});
    return atlas;
}

} // namespace

TEST_CASE("ui_render: linear colours become sRGB-encoded bytes, alpha scaled") {
    const R::Color8 c = U::ToColor8(Ui::UiColor{0.5F, 0.0F, 1.0F, 0.5F});
    CHECK_MESSAGE(c.r == 188, "[ui-srgb-encode] " << int(c.r));
    CHECK(c.g == 0);
    CHECK(c.b == 255);
    CHECK_MESSAGE(c.a == 128, "[ui-alpha-linear] " << int(c.a));
}

TEST_CASE("ui_render: a root's draw list becomes one pass on its camera's viewport, in its pixels") {
    U::ExtractedUiRoot root;
    root.entity = 2;
    root.camera = 1;
    root.drawList.commands.push_back(Ui::UiQuadDraw{{10.0F, 20.0F, 30.0F, 40.0F}, Ui::UiColor{1, 1, 1, 1}, Ui::kNoClip});
    root.drawList.commands.push_back(Ui::UiGlyphDraw{{50.0F, 60.0F, 8.0F, 16.0F}, 0, 128, 64, Ui::UiColor{1, 1, 1, 1}, {0.0F, 0.0F, 100.0F, 100.0F}});
    U::ExtractedUiRoot other = root;
    other.camera = 9;
    const std::optional<R::Pass2D> pass = U::QueueUi(CameraOn(1, 0, 80, 2560, 1440), {root, other}, OnePage());
    REQUIRE(pass);
    REQUIRE(pass->viewport);
    CHECK_MESSAGE((*pass->viewport == R::PixelRect{0, 80, 2560, 1440}), "[ui-pass-viewport]");
    CHECK_MESSAGE(!pass->view.has_value(), "[ui-pass-view-pixels]");
    CHECK_MESSAGE(!pass->clear.has_value(), "[ui-pass-no-clear]");
    // The other camera's root is not in this pass.
    REQUIRE(pass->vertices.size() == 8);
    REQUIRE(pass->draws.size() == 2);
    CHECK(pass->vertices[0].position.x == 10.0F);
    CHECK(pass->vertices[2].position.y == 60.0F);
    CHECK(pass->draws[0].texture == R::TextureId{});
    CHECK_MESSAGE(!pass->draws[0].clip.has_value(), "[ui-no-clip]");
    CHECK(pass->draws[1].texture == R::TextureId{7});
    REQUIRE(pass->draws[1].clip);
    CHECK(pass->draws[1].clip->width == 100.0F);
    // The glyph's uv: its atlas rectangle over the 256 x 128 page.
    CHECK_MESSAGE(pass->vertices[4].uv.x == 0.5F, "[ui-glyph-uv]");
    CHECK(pass->vertices[4].uv.y == 0.5F);
    CHECK(pass->vertices[6].uv.x == (128.0F + 8.0F) / 256.0F);
    CHECK(pass->vertices[6].uv.y == (64.0F + 16.0F) / 128.0F);
}

TEST_CASE("ui_render: rectangles with one texture and one clip share a draw; an empty clip or rectangle draws nothing") {
    R::Pass2D pass;
    U::AppendUiRect(pass, {0, 0, 10, 10}, {0, 0, 1, 1}, {}, R::TextureId{}, Ui::kNoClip);
    U::AppendUiRect(pass, {10, 0, 10, 10}, {0, 0, 1, 1}, {}, R::TextureId{}, Ui::kNoClip);
    CHECK_MESSAGE(pass.draws.size() == 1, "[ui-batch]");
    CHECK(pass.draws[0].indexCount == 12);
    U::AppendUiRect(pass, {0, 0, 10, 10}, {0, 0, 1, 1}, {}, R::TextureId{}, {0, 0, 5, 5});
    CHECK(pass.draws.size() == 2);
    U::AppendUiRect(pass, {0, 0, 10, 10}, {0, 0, 1, 1}, {}, R::TextureId{}, {0, 0, 0, 5});
    U::AppendUiRect(pass, {0, 0, 0, 10}, {0, 0, 1, 1}, {}, R::TextureId{}, Ui::kNoClip);
    CHECK_MESSAGE(pass.vertices.size() == 12, "[ui-empty-skipped]");
}

TEST_CASE("ui_render: an image whose picture is not loaded is skipped; nothing to draw is no pass") {
    U::ExtractedUiRoot root;
    root.camera = 1;
    root.drawList.commands.push_back(Ui::UiImageDraw{{0, 0, 10, 10}, {0, 0, 1, 1}, "pic", Ui::UiColor{}, Ui::kNoClip});
    root.images.push_back(R::TextureId{});
    CHECK_MESSAGE(!U::QueueUi(CameraOn(1, 0, 0, 100, 100), {root}, OnePage()).has_value(), "[ui-image-wait]");
    root.images[0] = R::TextureId{3};
    const auto pass = U::QueueUi(CameraOn(1, 0, 0, 100, 100), {root}, OnePage());
    REQUIRE(pass);
    CHECK(pass->draws.size() == 1);
    CHECK(pass->draws[0].texture == R::TextureId{3});
}
