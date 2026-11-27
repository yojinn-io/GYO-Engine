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
    const R::RenderTargetDesc fixed{R::TargetSize{8, 8}, R::RenderTargetFormat::Rgba8Unorm, R::TextureFilter::Nearest};
    const R::TextureId target = textures.CreateTarget(fixed);
    CHECK_MESSAGE(target.value == 2, "[rd-target-id-shared] a target's id comes after the texture's, from the same counter");
    textures.Release(target);
    const R::TextureId again = textures.CreateTarget(R::RenderTargetDesc{});
    CHECK_MESSAGE(again.value == 3, "[rd-target-no-reuse] a released target's id is not given again");
    CHECK_MESSAGE(textures.FindTarget(target) == nullptr, "[rd-target-released-gone] a released target is not live");
    REQUIRE(textures.FindTarget(again) != nullptr);
    CHECK_MESSAGE((textures.FindTarget(again)->size == std::variant<R::TargetSize, R::FollowTarget>{R::FollowTarget{}}),
                  "[rd-target-find] its description is kept (by default it follows the main target)");
    CHECK_MESSAGE((textures.Find(again) == nullptr && textures.FindTarget(picture) == nullptr),
                  "[rd-target-apart] a target is not a texture of pixels, nor the other way round");
    CHECK_MESSAGE(textures.LiveCount() == 2, "[rd-target-live-count] the texture and the target");
    const R::TextureId follower = textures.CreateTarget(R::RenderTargetDesc{R::FollowTarget{again}});
    CHECK_MESSAGE((std::get<R::FollowTarget>(textures.FindTarget(follower)->size).target == again), "[rd-target-follows] a target may follow another");
    const auto pending = textures.Pending();
    // The picture, the fixed target and its size (known at once), its release, the two that follow (no main size yet).
    REQUIRE_MESSAGE(pending.size() == 6, "[rd-target-ask] making a target is an ask, in order");
    const auto* made = std::get_if<R::TextureTargetCreate>(&pending[1]);
    REQUIRE_MESSAGE(made != nullptr, "[rd-target-ask] making a target is an ask, in order");
    CHECK_MESSAGE((made->id == target && made->desc == fixed), "[rd-target-ask] the ask carries the id and the description");
    const auto* sized = std::get_if<R::TextureTargetSize>(&pending[2]);
    CHECK_MESSAGE((sized != nullptr && sized->id == target && sized->size == R::TargetSize{8, 8}), "[rd-target-size-ask] then its size");
    CHECK(std::holds_alternative<R::TextureRelease>(pending[3]));
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
    namespace C = GYO::Engine::Camera;
    R::RenderTextures textures;
    GYO_CHECK_ASSERTS(textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{0, 8}}));
    GYO_CHECK_ASSERTS(textures.CreateTarget(R::RenderTargetDesc{R::FollowTarget{R::TextureId{9}}}));
    const R::TextureId target = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{8, 8}});
    GYO_CHECK_ASSERTS(textures.Update(target, R::TextureRegion{0, 0, 1, 1}, Pixels(1, 1)));
    R::RenderTargets targets;
    GYO_CHECK_ASSERTS(targets.Create(R::SourceTargetDesc{R::TargetSize{8, 0}}));
    GYO_CHECK_ASSERTS(targets.Create(R::SourceTargetDesc{C::CameraTarget{4}}));
    GYO_CHECK_ASSERTS(targets.Release(C::CameraTarget{1}));
}

TEST_CASE("render: a Depth target is made like any target") {
    R::RenderTextures textures;
    const R::TextureId depth = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{8, 8}, R::RenderTargetFormat::Depth});
    REQUIRE(textures.FindTarget(depth) != nullptr);
    CHECK_MESSAGE(textures.FindTarget(depth)->format == R::RenderTargetFormat::Depth, "[rd-depth-target] a target may be Depth");
}

TEST_CASE("render: what each kind of pass draws on and samples") {
    R::Pass2D flat;
    flat.target = R::TextureId{3};
    for (const std::uint32_t texture : {5U, 0U, 6U}) {
        flat.draws.emplace_back();
        flat.draws.back().texture = R::TextureId{texture};
    }
    R::Pass3D scene;
    scene.depth = R::TextureId{4};
    scene.draws.push_back(R::Draw3D{});
    scene.draws.back().texture = R::TextureId{7};
    R::PassFullscreen post;
    post.target = R::TextureId{8};
    post.inputs = {R::TextureId{9}, R::TextureId{2}};
    R::PassDepth shadow;
    shadow.target = R::TextureId{10};
    shadow.draws.push_back(R::MeshDraw{});
    CHECK_MESSAGE((R::TargetOf(flat) == R::TextureId{3} && R::TargetOf(scene) == std::nullopt && R::TargetOf(post) == R::TextureId{8} &&
                   R::TargetOf(shadow) == R::TextureId{10}),
                  "[rd-pass-target] the target each draws on (a depth-only pass's is its depth target)");
    CHECK_MESSAGE((R::ReadsOf(flat) == std::vector<R::TextureId>{R::TextureId{5}, R::TextureId{6}}), "[rd-pass-reads-draws] its draws' textures, the white one left out");
    CHECK_MESSAGE((R::ReadsOf(scene) == std::vector<R::TextureId>{R::TextureId{7}}), "[rd-pass-reads-depth-not-read] a 3D pass's depth target is not a read");
    CHECK_MESSAGE((R::ReadsOf(post) == std::vector<R::TextureId>{R::TextureId{9}, R::TextureId{2}}), "[rd-pass-reads-inputs] a full-screen pass's inputs in order");
    CHECK_MESSAGE(R::ReadsOf(shadow).empty(), "[rd-pass-reads-depth-only] a depth-only pass samples nothing");
    const R::PassWrites sceneWrites = R::WritesOf(scene);
    const R::PassWrites shadowWrites = R::WritesOf(shadow);
    CHECK_MESSAGE((sceneWrites.WritesColour(std::nullopt) && sceneWrites.Writes(R::TextureId{4}) && !sceneWrites.Writes(R::TextureId{7})),
                  "[rd-pass-writes-3d] a 3D pass draws colour on its target and depth on its depth target");
    CHECK_MESSAGE((!shadowWrites.colour && !shadowWrites.WritesColour(std::nullopt) && shadowWrites.Writes(R::TextureId{10})),
                  "[rd-pass-writes-depth-only] a depth-only pass draws no colour, only its depth target");
}

TEST_CASE("render: RP-3 as data: a target is sampled only after an earlier pass of the frame drew on it, or retained with a picture") {
    const auto sampling = [](const std::optional<R::TextureId> target, const R::TextureId texture) {
        R::Pass2D pass;
        pass.target = target;
        pass.draws.emplace_back();
        pass.draws.back().texture = texture;
        return R::RenderPass{pass};
    };
    R::RenderTextures textures;
    const R::TextureId target = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{8, 8}});
    const R::TextureId picture = textures.Create(R::TextureDesc{1, 1}, Pixels(1, 1));
    R::RenderCommands unwritten;
    unwritten.passes = {sampling(std::nullopt, target), sampling(std::nullopt, picture)};
    CHECK_MESSAGE((R::FirstUnwrittenRead(unwritten, textures) == R::UnwrittenRead{0, target}), "[rd-unwritten-found] sampled, neither drawn on nor retained");
    R::RenderCommands pictureOnly;
    pictureOnly.passes = {sampling(std::nullopt, picture)};
    CHECK_MESSAGE(!R::FirstUnwrittenRead(pictureOnly, textures).has_value(), "[rd-unwritten-picture-only] a texture that is no render target is not counted");
    R::RenderCommands drawnFirst;
    drawnFirst.passes = {sampling(target, R::TextureId{}), sampling(std::nullopt, target)};
    CHECK_MESSAGE(!R::FirstUnwrittenRead(drawnFirst, textures).has_value(), "[rd-unwritten-drawn-before] a pass drew on it earlier this frame");
    R::RenderCommands drawnAfter;
    drawnAfter.passes = {sampling(std::nullopt, target), sampling(target, R::TextureId{})};
    CHECK_MESSAGE((R::FirstUnwrittenRead(drawnAfter, textures) == R::UnwrittenRead{0, target}), "[rd-unwritten-drawn-after] a pass that draws on it later does not count");
    R::RenderCommands own;
    own.passes = {sampling(target, target)};
    CHECK_MESSAGE(!R::FirstUnwrittenRead(own, textures).has_value(), "[rd-unwritten-own] sampling what a pass draws on is not counted");
    // Retained: only with a picture (a target made, or made again, and not drawn on since has nothing to stand for
    // this frame's).
    R::RenderCommands retained;
    retained.passes = {sampling(std::nullopt, target)};
    retained.retained = {target};
    CHECK_MESSAGE((R::FirstUnwrittenRead(retained, textures) == R::UnwrittenRead{0, target}), "[rd-unwritten-retained-fresh] retained with no picture: not written");
    textures.Apply(R::AssumedReport(drawnFirst, textures.Pending()));
    CHECK_MESSAGE(!R::FirstUnwrittenRead(retained, textures).has_value(), "[rd-unwritten-retained] retained with its picture");
}

TEST_CASE("render: a 3D pass's depth target is the size of its colour target on the data side") {
    R::RenderTextures textures;
    textures.SetMainSize(R::TargetSize{16, 16});
    const R::TextureId follows = textures.CreateTarget(R::RenderTargetDesc{R::FollowTarget{}, R::RenderTargetFormat::Depth});
    const R::TextureId small = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{4, 4}, R::RenderTargetFormat::Depth});
    R::Pass3D onMain;
    onMain.depth = follows;
    R::RenderCommands same;
    same.passes.push_back(onMain);
    CHECK_MESSAGE(!R::FirstDepthOfAnotherSize(same, textures).has_value(), "[rd-depth-size-same] a depth target following the main target is its size");
    R::Pass3D other = onMain;
    other.depth = small;
    R::RenderCommands mismatched;
    mismatched.passes = {R::RenderPass{onMain}, R::RenderPass{other}};
    CHECK_MESSAGE(R::FirstDepthOfAnotherSize(mismatched, textures) == std::optional<std::size_t>{1}, "[rd-depth-size] a depth target of another size is found");
}

TEST_CASE("render: target sizes are decided on the data side; a size change is an ask and loses the picture") {
    R::RenderTextures textures;
    const R::TextureId follower = textures.CreateTarget(R::RenderTargetDesc{R::FollowTarget{}});
    CHECK_MESSAGE(!textures.SizeOf(follower).has_value(), "[rd-size-unknown] no size before the main target's is known");
    const R::TextureId fixed = textures.CreateTarget(R::RenderTargetDesc{R::TargetSize{4, 2}});
    const R::TextureId second = textures.CreateTarget(R::RenderTargetDesc{R::FollowTarget{fixed}});
    CHECK_MESSAGE((textures.SizeOf(fixed) == R::TargetSize{4, 2} && textures.SizeOf(second) == R::TargetSize{4, 2}),
                  "[rd-size-fixed] a fixed size, and a target following it, sized when made");
    const R::TargetSize window{8, 8};
    textures.SetMainSize(window);
    CHECK_MESSAGE((textures.SizeOf(follower) == window && textures.MainSize() == window), "[rd-size-follows-main] the main target's size");
    const auto sizeAsks = [&] {
        std::vector<std::uint32_t> ids;
        for (const R::TextureOp& op : textures.Pending()) {
            if (const auto* sized = std::get_if<R::TextureTargetSize>(&op)) ids.push_back(sized->id.value);
        }
        return ids;
    };
    CHECK_MESSAGE((sizeAsks() == std::vector<std::uint32_t>{fixed.value, second.value, follower.value}), "[rd-size-asks] one size ask each, when known");
    CHECK_MESSAGE((!textures.HasPicture(follower) && !textures.HasPicture(fixed)), "[rd-picture-none] no picture when made");
    // A frame that applied the sizes and drew on the follower.
    R::RenderCommands frame;
    R::Pass2D on;
    on.target = follower;
    frame.passes.push_back(on);
    textures.Apply(R::AssumedReport(frame, textures.Pending()));
    textures.ClearPending();
    CHECK_MESSAGE((textures.HasPicture(follower) && !textures.HasPicture(fixed)), "[rd-picture-drawn] a frame that drew on it gives it one");
    CHECK_MESSAGE(sizeAsks().empty(), "[rd-size-applied] an applied size is not asked for again");
    textures.SetMainSize(window);
    CHECK_MESSAGE((sizeAsks().empty() && textures.HasPicture(follower)), "[rd-size-same] the same size: no ask, the picture stays");
    textures.SetMainSize(R::TargetSize{16, 8});
    CHECK_MESSAGE((sizeAsks().size() == 1 && !textures.HasPicture(follower)), "[rd-size-changed] a new size: an ask, no picture");
    // The frame is not drawn (no report of it): the size is asked for again next frame, until it is applied.
    textures.Apply(R::RenderFrameReport{});
    textures.ClearPending();
    CHECK_MESSAGE((sizeAsks() == std::vector<std::uint32_t>{follower.value}), "[rd-size-asked-again] a size not applied is asked for again");
    textures.SetMainSize(std::nullopt);
    CHECK_MESSAGE((textures.SizeOf(follower) == R::TargetSize{16, 8}), "[rd-size-kept] an unknown main size keeps the size it has");
    textures.Release(fixed);
    CHECK_MESSAGE((textures.SizeOf(second) == R::TargetSize{4, 2}), "[rd-size-leader-released] a follower keeps its size when what it follows is let go");
}

TEST_CASE("render: the source's targets: ids from 1 for cameras, never given twice; the sizes a camera computes with") {
    namespace C = GYO::Engine::Camera;
    R::RenderTargets targets;
    const C::CameraTarget a = targets.Create(R::SourceTargetDesc{R::TargetSize{8, 4}});
    const C::CameraTarget b = targets.Create(R::SourceTargetDesc{});
    CHECK_MESSAGE((a.id == 1 && b.id == 2), "[rt-ids] from 1 (0 is the main target), growing");
    targets.Release(a);
    CHECK_MESSAGE(targets.Create(R::SourceTargetDesc{}).id == 3, "[rt-no-reuse] a released id is not given again");
    CHECK_MESSAGE((targets.Find(a) == nullptr && targets.Find(b) != nullptr && targets.Find(C::kMainTarget) == nullptr), "[rt-find]");
    CHECK_MESSAGE(targets.Live().size() == 2, "[rt-live]");
    CHECK_MESSAGE((R::RenderTargets{}.Table() != targets.Table() && R::RenderTargets{targets}.Table() == targets.Table()),
                  "[rt-table] a new table is another; a copy is the same");

    C::CameraTargetSizes sizes;
    const auto fixed = R::TargetSizeOf(R::SourceTargetDesc{R::TargetSize{8, 4}}, sizes);
    REQUIRE(fixed.has_value());
    CHECK_MESSAGE((fixed->physicalSize == GYO::Engine::Math::Vec2i{8, 4} && fixed->scaleFactor == 1.0F),
                  "[rt-size-fixed] a fixed size, scale factor 1 whatever the window's");
    CHECK_MESSAGE(!R::TargetSizeOf(R::SourceTargetDesc{}, sizes).has_value(), "[rt-size-unknown] none before the main size is known");
    sizes.Set(C::kMainTarget, C::TargetSize{{1920, 1080}, 2.0F});
    sizes.Set(b, C::TargetSize{{64, 32}, 1.0F});
    const auto follows = R::TargetSizeOf(R::SourceTargetDesc{}, sizes);
    REQUIRE_MESSAGE(follows.has_value(), "[rt-size-follows] a following target has the main target's size");
    CHECK_MESSAGE((follows->physicalSize == GYO::Engine::Math::Vec2i{1920, 1080} && follows->scaleFactor == 2.0F), "[rt-size-follows] and scale");
    const auto followsB = R::TargetSizeOf(R::SourceTargetDesc{b}, sizes);
    REQUIRE_MESSAGE(followsB.has_value(), "[rt-size-follows-other] a target may follow another target");
    CHECK_MESSAGE(followsB->physicalSize == (GYO::Engine::Math::Vec2i{64, 32}), "[rt-size-follows-other] its size, not the main one");
    const auto fixedNow = R::TargetSizeOf(R::SourceTargetDesc{R::TargetSize{8, 4}}, sizes);
    CHECK_MESSAGE((fixedNow.has_value() && fixedNow->scaleFactor == 1.0F), "[rt-size-fixed] scale factor 1 with a window of scale 2 too");
}

TEST_CASE("render: the render World keeps one target of its own per source target, in step; another table starts again") {
    namespace C = GYO::Engine::Camera;
    R::RenderTargets source;
    R::RenderTextures textures;
    R::TargetTextures mapped;
    const R::TextureId picture = textures.Create(R::TextureDesc{1, 1}, Pixels(1, 1));  // an image's texture, id 1
    const C::CameraTarget minimap = source.Create(R::SourceTargetDesc{R::TargetSize{8, 8}});
    const C::CameraTarget glow = source.Create(R::SourceTargetDesc{minimap});
    R::SyncTargetTextures(&source, textures, mapped);
    REQUIRE(mapped.Find(minimap) != nullptr);
    REQUIRE(mapped.Find(glow) != nullptr);
    const R::TextureId made = *mapped.Find(minimap);
    CHECK_MESSAGE((made.value != picture.value && textures.FindTarget(made) != nullptr), "[rt-sync-made] a render target of RenderTextures, its own id");
    CHECK_MESSAGE((std::get<R::FollowTarget>(textures.FindTarget(*mapped.Find(glow))->size).target == made),
                  "[rt-sync-follows] a follower follows the render target made for the source target it follows");
    CHECK_MESSAGE(mapped.Find(C::kMainTarget) == nullptr, "[rt-sync-main] nothing is made for the main target");

    // The frame applied its asks (as render_world's Cleanup does), then they are cleared.
    textures.Apply(R::AssumedReport(R::RenderCommands{}, textures.Pending()));
    textures.ClearPending();
    R::SyncTargetTextures(&source, textures, mapped);
    CHECK_MESSAGE((textures.Pending().empty() && *mapped.Find(minimap) == made), "[rt-sync-kept] the same target is kept, nothing asked");

    // The minimap gets another size: made again, and its follower too (it follows the new one); the old ones let go.
    source.Release(glow);
    source.Release(minimap);
    R::RenderTargets& same = source;
    const C::CameraTarget resized = same.Create(R::SourceTargetDesc{R::TargetSize{4, 4}});
    R::SyncTargetTextures(&same, textures, mapped);
    CHECK_MESSAGE((mapped.Find(minimap) == nullptr && mapped.Find(glow) == nullptr && textures.FindTarget(made) == nullptr),
                  "[rt-sync-gone] a target the source no longer has is let go");
    REQUIRE(mapped.Find(resized) != nullptr);

    // A replaced source World: another table gives id 1 again, with the same description. The old target is let go and
    // a new one made: the new World never sees the old one's picture.
    R::RenderTargets replaced;
    for (std::uint32_t i = 0; i < resized.id; ++i) static_cast<void>(replaced.Create(R::SourceTargetDesc{R::TargetSize{4, 4}}));
    const R::TextureId before = *mapped.Find(resized);
    R::SyncTargetTextures(&replaced, textures, mapped);
    REQUIRE(mapped.Find(resized) != nullptr);
    CHECK_MESSAGE((*mapped.Find(resized) != before && textures.FindTarget(before) == nullptr),
                  "[rt-sync-other-table] another table: what was made for the old one is let go, the same id made again");

    // The frame applied its asks (as render_world's Cleanup does), then they are cleared.
    textures.Apply(R::AssumedReport(R::RenderCommands{}, textures.Pending()));
    textures.ClearPending();
    const R::TextureId last = *mapped.Find(resized);
    R::SyncTargetTextures(nullptr, textures, mapped);
    CHECK_MESSAGE((mapped.entries.empty() && textures.FindTarget(last) == nullptr), "[rt-sync-none] no source: everything is let go");
}

TEST_CASE("render: the shader ABI's vertex layout is Vertex2D's") {
    CHECK_MESSAGE(R::ShaderAbi::kPass2DVertexStride == 20, "[rd-abi-stride] 20 bytes a vertex");
    const R::ShaderAbi::Pass2DViewUniform u = R::ShaderAbi::ToUniform(R::ViewTransform{{1.0F, 2.0F}, {3.0F, 4.0F}});
    CHECK_MESSAGE((u.lanes[0] == 1.0F && u.lanes[1] == 2.0F && u.lanes[2] == 3.0F && u.lanes[3] == 4.0F),
                  "[rd-abi-uniform] scale in xy, offset in zw");
}
