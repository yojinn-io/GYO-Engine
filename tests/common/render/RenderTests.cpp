// The render Module's data and arithmetic (docs/architecture/target/v2/render.zh-Hant.md, RD-1, RD-3, RD-4): the
// view to clip space, the clip to pixels, the texture ids and asks. Exact values: every input is chosen so the float
// arithmetic is exact. Each CHECK carries a tag in brackets ("[rd-...]"): the mutants expect it
// (docs/architecture/target/v2/scripts/render_mutations.json).
#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <variant>
#include <vector>

#include "AssertTestSupport.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTargets.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render/ShaderAbi.hpp"

namespace R = GYO::Engine::Render;
using GYO::Engine::Math::Rect;
using GYO::Engine::Math::Vec2;

namespace {

Vec2 Apply(const R::ViewTransform& t, const Vec2 p) { return {p.x * t.scale.x + t.offset.x, p.y * t.scale.y + t.offset.y}; }

std::vector<std::uint8_t> Pixels(std::uint32_t w, std::uint32_t h) { return std::vector<std::uint8_t>(std::size_t{w} * h * 4, 7); }

bool PassTargetOfOk(const std::optional<std::uint32_t>& got, const std::uint32_t expected) { return got.has_value() && *got == expected; }

} // namespace

TEST_CASE("render: the view's corners go to the clip space's corners, y turned round once") {
    const Rect view{0.0F, 0.0F, 100.0F, 50.0F};
    const R::ViewTransform t = R::ViewToClip(view);
    const Vec2 topLeft = Apply(t, {0.0F, 0.0F});
    const Vec2 bottomRight = Apply(t, {100.0F, 50.0F});
    const Vec2 centre = Apply(t, {50.0F, 25.0F});
    CHECK_MESSAGE((topLeft.x == -1.0F && topLeft.y == 1.0F), "[rd-clip-top-left] the view's top-left is clip (-1, 1)");
    CHECK_MESSAGE((bottomRight.x == 1.0F && bottomRight.y == -1.0F), "[rd-clip-bottom-right] the bottom-right is clip (1, -1)");
    CHECK_MESSAGE((centre.x == 0.0F && centre.y == 0.0F), "[rd-clip-centre] the centre is clip (0, 0)");

    const R::ViewTransform moved = R::ViewToClip(Rect{20.0F, -10.0F, 40.0F, 20.0F});
    const Vec2 movedTopLeft = Apply(moved, {20.0F, -10.0F});
    const Vec2 movedBottomRight = Apply(moved, {60.0F, 10.0F});
    CHECK_MESSAGE((movedTopLeft.x == -1.0F && movedTopLeft.y == 1.0F), "[rd-clip-origin] a view's origin is its top-left corner");
    CHECK_MESSAGE((movedBottomRight.x == 1.0F && movedBottomRight.y == -1.0F), "[rd-clip-origin-size] origin plus size is the bottom-right");
}

TEST_CASE("render: a view of zero size is a Programmer Error") {
    GYO_CHECK_ASSERTS(R::ViewToClip(Rect{0.0F, 0.0F, 0.0F, 10.0F}));
    GYO_CHECK_ASSERTS(R::ViewToClip(Rect{0.0F, 0.0F, 10.0F, 0.0F}));
}

TEST_CASE("render: a pass without a view uses the target's pixels") {
    R::Pass2D pass;
    const Rect own = R::ViewOf(pass, R::TargetSize{640, 360});
    CHECK_MESSAGE((own.x == 0.0F && own.y == 0.0F && own.width == 640.0F && own.height == 360.0F),
                  "[rd-view-default] nullopt view is the target's pixels");
    pass.view = Rect{1.0F, 2.0F, 3.0F, 4.0F};
    const Rect given = R::ViewOf(pass, R::TargetSize{640, 360});
    CHECK_MESSAGE((given.x == 1.0F && given.y == 2.0F && given.width == 3.0F && given.height == 4.0F), "[rd-view-given] a given view is used");
}

TEST_CASE("render: no clip lets the whole target through") {
    CHECK_MESSAGE((R::ClipToPixels(std::nullopt, Rect{0, 0, 10, 10}, R::TargetSize{64, 32}) == R::PixelRect{0, 0, 64, 32}),
                  "[rd-clip-none] nullopt clip is the whole target");
}

TEST_CASE("render: a clip on whole pixels is exact; a fractional edge takes the pixels whose centre is inside") {
    const Rect pixels{0.0F, 0.0F, 100.0F, 100.0F};
    const R::TargetSize target{100, 100};
    CHECK_MESSAGE((R::ClipToPixels(Rect{10.0F, 20.0F, 30.0F, 40.0F}, pixels, target) == R::PixelRect{10, 20, 30, 40}),
                  "[rd-clip-exact] whole-pixel edges");
    // Left 10.4: pixel 10's centre (10.5) is inside. Right 20.6: pixel 20's centre (20.5) is inside.
    CHECK_MESSAGE((R::ClipToPixels(Rect{10.4F, 10.4F, 10.2F, 10.2F}, pixels, target) == R::PixelRect{10, 10, 11, 11}),
                  "[rd-clip-centre-in] centres inside are let through");
    // Left 10.6: pixel 10's centre is outside. Right 20.4: pixel 20's centre is outside.
    CHECK_MESSAGE((R::ClipToPixels(Rect{10.625F, 10.625F, 9.75F, 9.75F}, pixels, target) == R::PixelRect{11, 11, 9, 9}),
                  "[rd-clip-centre-out] centres outside are not");
    // A centre on the left edge is inside, on the right edge outside.
    CHECK_MESSAGE((R::ClipToPixels(Rect{10.5F, 10.5F, 10.0F, 10.0F}, pixels, target) == R::PixelRect{10, 10, 10, 10}),
                  "[rd-clip-edge-rule] left and top edges included, right and bottom not");
}

TEST_CASE("render: a clip follows the view's scale and origin and is cut to the target") {
    // A 640 by 360 view over a 1280 by 720 target: one view unit is two pixels.
    CHECK_MESSAGE((R::ClipToPixels(Rect{10.0F, 5.0F, 20.0F, 10.0F}, Rect{0.0F, 0.0F, 640.0F, 360.0F}, R::TargetSize{1280, 720}) ==
                      R::PixelRect{20, 10, 40, 20}),
                  "[rd-clip-scale] view units to pixels");
    CHECK_MESSAGE((R::ClipToPixels(Rect{110.0F, 60.0F, 10.0F, 10.0F}, Rect{100.0F, 50.0F, 100.0F, 100.0F}, R::TargetSize{100, 100}) ==
                      R::PixelRect{10, 10, 10, 10}),
                  "[rd-clip-view-origin] measured from the view's origin");
    CHECK_MESSAGE((R::ClipToPixels(Rect{-20.0F, 90.0F, 50.0F, 50.0F}, Rect{0, 0, 100, 100}, R::TargetSize{100, 100}) ==
                      R::PixelRect{0, 90, 30, 10}),
                  "[rd-clip-cut] cut to the target");
    const R::PixelRect outside = R::ClipToPixels(Rect{200.0F, 200.0F, 10.0F, 10.0F}, Rect{0, 0, 100, 100}, R::TargetSize{100, 100});
    CHECK_MESSAGE((outside.width == 0 || outside.height == 0), "[rd-clip-outside] a clip outside lets nothing through");
}

TEST_CASE("render: a viewport is cut to the target; the view and clips are measured in it") {
    R::Pass2D pass;
    CHECK_MESSAGE((R::ViewportOf(pass, R::TargetSize{64, 32}) == R::PixelRect{0, 0, 64, 32}), "[rd-viewport-default] nullopt is the whole target");
    pass.viewport = R::PixelRect{8, 4, 32, 16};
    CHECK_MESSAGE((R::ViewportOf(pass, R::TargetSize{64, 32}) == R::PixelRect{8, 4, 32, 16}), "[rd-viewport-given] a given viewport is used");
    const Rect own = R::ViewOf(pass, R::TargetSize{64, 32});
    CHECK_MESSAGE((own.x == 0.0F && own.y == 0.0F && own.width == 32.0F && own.height == 16.0F),
                  "[rd-viewport-view] nullopt view is the viewport's pixels");
    pass.viewport = R::PixelRect{48, 30, 32, 16};
    CHECK_MESSAGE((R::ViewportOf(pass, R::TargetSize{64, 32}) == R::PixelRect{48, 30, 16, 2}), "[rd-viewport-cut] cut to the target");
    pass.viewport = R::PixelRect{70, 0, 4, 4};
    const R::PixelRect outside = R::ViewportOf(pass, R::TargetSize{64, 32});
    CHECK_MESSAGE((outside.width == 0), "[rd-viewport-outside] a viewport outside the target is empty");

    // A 1280 by 720 view over a letterboxed 1920 by 1080 viewport at (0, 60) of a 1920 by 1200 target: 1.5 pixels a unit.
    const R::PixelRect letterbox{0, 60, 1920, 1080};
    CHECK_MESSAGE((R::ClipToPixels(std::nullopt, Rect{0, 0, 1280, 720}, letterbox) == letterbox), "[rd-clip-viewport-none] nullopt clip is the viewport");
    CHECK_MESSAGE((R::ClipToPixels(Rect{100.0F, 40.0F, 200.0F, 100.0F}, Rect{0, 0, 1280, 720}, letterbox) == R::PixelRect{150, 120, 300, 150}),
                  "[rd-clip-viewport-offset] view units to pixels of the viewport, offset by its corner");
    CHECK_MESSAGE((R::ClipToPixels(Rect{-10.0F, 700.0F, 100.0F, 100.0F}, Rect{0, 0, 1280, 720}, letterbox) == R::PixelRect{0, 1110, 135, 30}),
                  "[rd-clip-viewport-cut] cut to the viewport, not the target");
}

TEST_CASE("render: texture ids start at 1, grow and are never given twice; the white texture is 0") {
    R::RenderTextures textures;
    const R::TextureId a = textures.Create(R::TextureDesc{2, 1}, Pixels(2, 1));
    const R::TextureId b = textures.Create(R::TextureDesc{1, 1, R::TextureFilter::Nearest}, Pixels(1, 1));
    CHECK_MESSAGE(a.value == 1, "[rd-tex-first-id] the first id is 1 (0 is the white texture)");
    CHECK_MESSAGE(b.value == 2, "[rd-tex-next-id] ids grow");
    textures.Release(a);
    const R::TextureId c = textures.Create(R::TextureDesc{1, 1}, Pixels(1, 1));
    CHECK_MESSAGE(c.value == 3, "[rd-tex-no-reuse] a released id is not given again");
    CHECK_MESSAGE(textures.Find(a) == nullptr, "[rd-tex-released-gone] a released texture is not live");
    REQUIRE(textures.Find(b) != nullptr);
    CHECK_MESSAGE((*textures.Find(b) == R::TextureDesc{1, 1, R::TextureFilter::Nearest}), "[rd-tex-find] its description is kept");
    CHECK_MESSAGE(textures.Find(R::TextureId{}) == nullptr, "[rd-tex-white-not-live] the white texture is not one of them");
    CHECK_MESSAGE(textures.LiveCount() == 2, "[rd-tex-live-count] two live");
}

TEST_CASE("render: the asks are kept in order with their pixels until cleared; the textures stay") {
    R::RenderTextures textures;
    std::vector<std::uint8_t> pixels{1, 2, 3, 4, 5, 6, 7, 8};
    const R::TextureId id = textures.Create(R::TextureDesc{2, 1}, pixels);
    textures.Update(id, R::TextureRegion{1, 0, 1, 1}, {9, 9, 9, 9});
    textures.Release(id);
    const auto pending = textures.Pending();
    REQUIRE(pending.size() == 3);
    const auto* made = std::get_if<R::TextureCreate>(&pending[0]);
    const auto* updated = std::get_if<R::TextureUpdate>(&pending[1]);
    const auto* released = std::get_if<R::TextureRelease>(&pending[2]);
    REQUIRE(made != nullptr);
    REQUIRE(updated != nullptr);
    REQUIRE(released != nullptr);
    CHECK_MESSAGE((made->id == id && made->pixels == pixels && made->desc.width == 2), "[rd-tex-ask-create] the make ask carries its pixels");
    CHECK_MESSAGE((updated->region == R::TextureRegion{1, 0, 1, 1} && updated->pixels == std::vector<std::uint8_t>{9, 9, 9, 9}),
                  "[rd-tex-ask-update] the update ask carries its region and pixels");
    CHECK_MESSAGE(released->id == id, "[rd-tex-ask-release] the release ask names the id");

    R::RenderTextures kept;
    const R::TextureId stays = kept.Create(R::TextureDesc{1, 1}, Pixels(1, 1));
    kept.ClearPending();
    CHECK_MESSAGE(kept.Pending().empty(), "[rd-tex-clear-asks] cleared asks are gone");
    CHECK_MESSAGE(kept.Find(stays) != nullptr, "[rd-tex-clear-keeps-live] the texture stays live");
}

TEST_CASE("render: wrong texture asks are Programmer Errors") {
    R::RenderTextures textures;
    GYO_CHECK_ASSERTS(textures.Create(R::TextureDesc{0, 1}, {}));
    GYO_CHECK_ASSERTS(textures.Create(R::TextureDesc{2, 2}, Pixels(2, 1)));
    const R::TextureId id = textures.Create(R::TextureDesc{4, 4}, Pixels(4, 4));
    GYO_CHECK_ASSERTS(textures.Update(R::TextureId{99}, R::TextureRegion{0, 0, 1, 1}, Pixels(1, 1)));
    GYO_CHECK_ASSERTS(textures.Update(id, R::TextureRegion{3, 0, 2, 1}, Pixels(2, 1)));
    GYO_CHECK_ASSERTS(textures.Update(id, R::TextureRegion{0, 4, 1, 1}, Pixels(1, 1)));
    GYO_CHECK_ASSERTS(textures.Update(id, R::TextureRegion{0, 0, 0, 1}, {}));
    GYO_CHECK_ASSERTS(textures.Update(id, R::TextureRegion{0, 0, 2, 2}, Pixels(1, 1)));
    GYO_CHECK_ASSERTS(textures.Release(R::TextureId{}));
    textures.Release(id);
    GYO_CHECK_ASSERTS(textures.Release(id));
    // A region that touches the far edges is inside.
    const R::TextureId edge = textures.Create(R::TextureDesc{4, 4}, Pixels(4, 4));
    textures.Update(edge, R::TextureRegion{3, 3, 1, 1}, Pixels(1, 1));
    CHECK_MESSAGE(textures.Pending().size() == 4, "[rd-tex-edge-region] a region on the far edges is accepted");
}

// ---- Render targets and formats (render-pipeline declaration section 13, A1)

TEST_CASE("render: targets take their ids from the textures' counter, never given twice; Find and FindTarget keep them apart") {
    R::RenderTextures textures;
    const R::TextureId picture = textures.Create(R::TextureDesc{1, 1}, Pixels(1, 1));
    const R::RenderTargetDesc fixed{R::TargetSize{8, 8}, R::TextureFormat::Rgba8Unorm, R::TextureFilter::Nearest};
    const R::TextureId target = textures.CreateTarget(fixed);
    CHECK_MESSAGE(target.value == 2, "[rd-target-id-shared] a target's id comes after the texture's, from the same counter");
    textures.Release(target);
    const R::TextureId again = textures.CreateTarget(R::RenderTargetDesc{});
    CHECK_MESSAGE(again.value == 3, "[rd-target-no-reuse] a released target's id is not given again");
    CHECK_MESSAGE(textures.FindTarget(target) == nullptr, "[rd-target-released-gone] a released target is not live");
    REQUIRE(textures.FindTarget(again) != nullptr);
    CHECK_MESSAGE(!textures.FindTarget(again)->size.has_value(), "[rd-target-find] its description is kept (follows the main target)");
    CHECK_MESSAGE((textures.Find(again) == nullptr && textures.FindTarget(picture) == nullptr),
                  "[rd-target-apart] a target is not a texture of pixels, nor the other way round");
    CHECK_MESSAGE(textures.LiveCount() == 2, "[rd-target-live-count] the texture and the target");
    const auto pending = textures.Pending();
    REQUIRE_MESSAGE(pending.size() == 4, "[rd-target-ask] making a target is an ask, in order");
    const auto* made = std::get_if<R::TextureTargetCreate>(&pending[1]);
    REQUIRE_MESSAGE(made != nullptr, "[rd-target-ask] making a target is an ask, in order");
    CHECK_MESSAGE((made->id == target && made->desc == fixed), "[rd-target-ask] the ask carries the id and the description");
    CHECK(std::holds_alternative<R::TextureRelease>(pending[2]));
}

TEST_CASE("render: a texture keeps its format; Rgba8Unorm is the default") {
    R::RenderTextures textures;
    const R::TextureId plain = textures.Create(R::TextureDesc{1, 1}, Pixels(1, 1));
    const R::TextureId colour = textures.Create(R::TextureDesc{1, 1, R::TextureFilter::Linear, R::TextureFormat::Rgba8UnormSrgb}, Pixels(1, 1));
    CHECK_MESSAGE(textures.Find(plain)->format == R::TextureFormat::Rgba8Unorm, "[rd-format-default] sampled as stored unless asked");
    CHECK_MESSAGE(textures.Find(colour)->format == R::TextureFormat::Rgba8UnormSrgb, "[rd-format-kept] the format is kept");
    const auto* made = std::get_if<R::TextureCreate>(&textures.Pending()[1]);
    REQUIRE(made != nullptr);
    CHECK_MESSAGE(made->desc.format == R::TextureFormat::Rgba8UnormSrgb, "[rd-format-ask] the make ask carries the format");
}

TEST_CASE("render: wrong target asks are Programmer Errors") {
    R::RenderTextures textures;
    GYO_CHECK_ASSERTS(textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{0, 8}}));
    const R::TextureId target = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{8, 8}});
    GYO_CHECK_ASSERTS(textures.Update(target, R::TextureRegion{0, 0, 1, 1}, Pixels(1, 1)));
    R::RenderTargets targets;
    GYO_CHECK_ASSERTS(targets.Create(R::RenderTargetDesc{R::TargetSize{8, 0}}));
    GYO_CHECK_ASSERTS(targets.Release(GYO::Engine::Camera::CameraTarget{1}));
}

TEST_CASE("render: the source's targets: ids from 1 for cameras, never given twice; the sizes a camera computes with") {
    namespace C = GYO::Engine::Camera;
    R::RenderTargets targets;
    const C::CameraTarget a = targets.Create(R::RenderTargetDesc{R::TargetSize{8, 4}});
    const C::CameraTarget b = targets.Create(R::RenderTargetDesc{});
    CHECK_MESSAGE((a.id == 1 && b.id == 2), "[rt-ids] from 1 (0 is the main target), growing");
    targets.Release(a);
    CHECK_MESSAGE(targets.Create(R::RenderTargetDesc{}).id == 3, "[rt-no-reuse] a released id is not given again");
    CHECK_MESSAGE((targets.Find(a) == nullptr && targets.Find(b) != nullptr && targets.Find(C::CameraTarget{0}) == nullptr), "[rt-find]");
    CHECK_MESSAGE(targets.Live().size() == 2, "[rt-live]");

    const C::TargetSize main{{1920, 1080}, 2.0F};
    const auto fixed = R::TargetSizeOf(R::RenderTargetDesc{R::TargetSize{8, 4}}, &main);
    REQUIRE(fixed.has_value());
    CHECK_MESSAGE((fixed->physicalSize == GYO::Engine::Math::Vec2i{8, 4} && fixed->scaleFactor == 1.0F),
                  "[rt-size-fixed] a fixed size, scale factor 1 whatever the window's");
    const auto follows = R::TargetSizeOf(R::RenderTargetDesc{}, &main);
    REQUIRE_MESSAGE(follows.has_value(), "[rt-size-follows] a following target has the main target's size");
    CHECK_MESSAGE((follows->physicalSize == main.physicalSize && follows->scaleFactor == 2.0F), "[rt-size-follows] the main target's size and scale");
    CHECK_MESSAGE(!R::TargetSizeOf(R::RenderTargetDesc{}, nullptr).has_value(), "[rt-size-unknown] none before the main size is known");
}

TEST_CASE("render: the render World keeps one target of its own per source target, in step; cameras map to it") {
    namespace C = GYO::Engine::Camera;
    R::RenderTargets source;
    R::RenderTextures textures;
    R::TargetTextures mapped;
    const R::TextureId picture = textures.Create(R::TextureDesc{1, 1}, Pixels(1, 1));  // an image's texture, id 1
    const C::CameraTarget minimap = source.Create(R::RenderTargetDesc{R::TargetSize{8, 8}});
    R::SyncTargetTextures(&source, textures, mapped);
    REQUIRE(mapped.Find(minimap.id) != nullptr);
    const R::TextureId made = *mapped.Find(minimap.id);
    CHECK_MESSAGE((made.value != picture.value && textures.FindTarget(made) != nullptr), "[rt-sync-made] a render target of RenderTextures, its own id");
    CHECK_MESSAGE((PassTargetOfOk(R::PassTargetOf(minimap.id, mapped), made.value)), "[rt-pass-target] a camera on it draws on that target");
    CHECK_MESSAGE((PassTargetOfOk(R::PassTargetOf(0, mapped), 0)), "[rt-pass-target-main] 0 stays 0");
    CHECK_MESSAGE(!R::PassTargetOf(9, mapped).has_value(), "[rt-pass-target-none] an id the source has no target for maps to nothing");

    textures.ClearPending();
    R::SyncTargetTextures(&source, textures, mapped);
    CHECK_MESSAGE((textures.Pending().empty() && *mapped.Find(minimap.id) == made), "[rt-sync-kept] the same target is kept, nothing asked");

    // A replaced source World gives id 1 again, to a target of another size: a new render target, the old let go.
    R::RenderTargets replaced;
    const C::CameraTarget other = replaced.Create(R::RenderTargetDesc{R::TargetSize{4, 4}});
    REQUIRE(other.id == minimap.id);
    R::SyncTargetTextures(&replaced, textures, mapped);
    REQUIRE(mapped.Find(other.id) != nullptr);
    CHECK_MESSAGE((*mapped.Find(other.id) != made && textures.FindTarget(made) == nullptr && textures.FindTarget(*mapped.Find(other.id)) != nullptr),
                  "[rt-sync-changed] another description: made again, the old one let go");

    textures.ClearPending();
    const R::TextureId last = *mapped.Find(other.id);
    R::SyncTargetTextures(nullptr, textures, mapped);
    CHECK_MESSAGE((mapped.entries.empty() && textures.FindTarget(last) == nullptr && textures.Pending().size() == 1),
                  "[rt-sync-gone] a target the source no longer has is let go");
}

TEST_CASE("render: the shader ABI's vertex layout is Vertex2D's") {
    CHECK_MESSAGE(R::ShaderAbi::kPass2DVertexStride == 20, "[rd-abi-stride] 20 bytes a vertex");
    const R::ShaderAbi::Pass2DViewUniform u = R::ShaderAbi::ToUniform(R::ViewTransform{{1.0F, 2.0F}, {3.0F, 4.0F}});
    CHECK_MESSAGE((u.lanes[0] == 1.0F && u.lanes[1] == 2.0F && u.lanes[2] == 3.0F && u.lanes[3] == 4.0F),
                  "[rd-abi-uniform] scale in xy, offset in zw");
}
