// The 3D pipeline's functions and the composing of cameras (docs/architecture/target/v2/render.zh-Hant.md section 8,
// RD-7 to RD-9): one camera's Pass3D (order of the groups, distance, layers, retained and per-frame geometry), CPU
// skinning, the retained model meshes, ComposePasses (camera order, the clears). No World, no backend. Tags "[p3-...]"
// and "[cp-...]" are the mutants' expectations.
#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <optional>
#include <ostream>
#include <variant>
#include <vector>

#include "engine/camera/Camera.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/scalar/Constants.hpp"
#include "engine/model/ModelData.hpp"
#include "engine/render/Compose.hpp"
#include "engine/render/Pipeline3d.hpp"
#include "engine/render/RenderMeshes.hpp"
#include "engine/transform/Transform.hpp"

namespace C = GYO::Engine::Camera;
namespace Md = GYO::Engine::Model;
namespace M = GYO::Engine::Math;
namespace R = GYO::Engine::Render;
namespace T = GYO::Engine::Transform;

namespace {

M::Matrix4 At(const float x, const float y, const float z) {
    T::Transform t;
    t.translation = {x, y, z};
    return T::ToMatrix(t);
}

bool Near(const float a, const float b) { return std::fabs(a - b) <= 1e-4F; }

// A 3D camera at (1, 0, -2) looking along +Z on a 100 x 100 target.
R::ExtractedCamera Camera3d(const std::optional<R::TextureId> target = std::nullopt, const std::int32_t order = 0) {
    const C::TargetSize size{{100, 100}, 1.0F};
    R::ExtractedCamera camera;
    camera.computed = C::Compute(C::Camera{}, C::Projection{C::PerspectiveProjection{}}, &size);
    // The view as UpdateCameraViews makes it (camera's ViewOf): Queue3d takes its clipFromWorld and viewFromWorld.
    camera.view = C::ViewOf(camera.computed, T::GlobalTransform{At(1, 0, -2)});
    camera.is3d = true;
    camera.order = order;
    camera.target = target;
    return camera;
}

R::Extracted3dItem Item(const std::uint64_t entity, const float z, const R::BlendMode blend = R::BlendMode::Opaque, const bool depthTest = true) {
    R::Extracted3dItem item;
    item.entity = entity;
    item.world = At(0, 0, z);
    item.blend = blend;
    item.depthTest = depthTest;
    item.mesh = R::MeshId{static_cast<std::uint32_t>(entity)};
    item.indexCount = 3;
    return item;
}

} // namespace

TEST_CASE("pipeline3d: a 3D camera's pass: opaque near to far, then blended far to near, then overlays; layers pick") {
    std::vector<R::Extracted3dItem> items{Item(1, 10.0F), Item(2, 5.0F), Item(3, 8.0F, R::BlendMode::Alpha), Item(4, 20.0F, R::BlendMode::Alpha),
                                          Item(5, 1.0F, R::BlendMode::Opaque, false), Item(6, 30.0F, R::BlendMode::Alpha, false), Item(7, 5.0F)};
    items.push_back(Item(8, 2.0F));
    items.back().layers = 2;
    const std::optional<R::Pass3D> pass = R::Queue3d(Camera3d(), items);
    REQUIRE(pass.has_value());
    std::vector<std::uint32_t> order;
    for (const R::Draw3D& d : pass->draws) order.push_back(d.mesh.value);
    CHECK_MESSAGE((order.size() == 7), "[p3-layers] the item on another layer is left out");
    CHECK_MESSAGE((order.size() >= 3 && order[0] == 2 && order[1] == 7 && order[2] == 1), "[p3-opaque-near-first] opaque near to far, ties by entity");
    CHECK_MESSAGE((order.size() >= 5 && order[3] == 4 && order[4] == 3), "[p3-blend-far-first] blended far to near, after the opaque");
    CHECK_MESSAGE((order.size() == 7 && order[5] == 6 && order[6] == 5), "[p3-overlay-last] not depth-tested last, far to near");
    CHECK_MESSAGE(pass->draws[0].indexCount == 3, "[p3-retained] a retained mesh's index count");
    const R::ExtractedCamera camera = Camera3d();
    const std::optional<M::Matrix4> viewFromWorld = M::Inverse(At(1, 0, -2));
    REQUIRE(viewFromWorld.has_value());
    const M::Matrix4 expected = M::Multiply(camera.computed.clipFromView, *viewFromWorld);
    bool same = true;
    for (std::size_t i = 0; i < 16; ++i) same = same && pass->clipFromWorld.values[i] == expected.values[i];
    CHECK_MESSAGE(same, "[p3-clip-from-world] clip from view times view from world");
    CHECK_MESSAGE((pass->viewport == R::PixelRect{0, 0, 100, 100}), "[p3-viewport] the camera's viewport");
    CHECK_MESSAGE(pass->clearDepth, "[p3-clear-depth] the camera's depth setting (cleared by default)");
    R::ExtractedCamera keeps = Camera3d();
    keeps.clearDepth = false;
    const std::optional<R::Pass3D> kept = R::Queue3d(keeps, items);
    REQUIRE(kept.has_value());
    CHECK_MESSAGE(!kept->clearDepth, "[p3-clear-depth] a camera that keeps the depth");
}

TEST_CASE("pipeline3d: a skinned item's vertices go into the pass, each draw naming its own range") {
    R::Extracted3dItem a = Item(1, 5.0F);
    a.mesh = R::MeshId{};
    a.vertices.resize(3);
    a.indices = {0, 1, 2};
    R::Extracted3dItem b = a;
    b.entity = 2;
    b.world = At(0, 0, 6.0F);
    b.vertices.resize(4);
    b.indices = {0, 1, 2, 0, 2, 3};
    const std::optional<R::Pass3D> pass = R::Queue3d(Camera3d(), std::vector<R::Extracted3dItem>{a, b});
    REQUIRE(pass.has_value());
    REQUIRE(pass->draws.size() == 2);
    CHECK_MESSAGE((pass->vertices.size() == 7 && pass->indices.size() == 9), "[p3-own-geometry] both items' vertices and indices");
    CHECK_MESSAGE((pass->draws[1].vertexOffset == 3 && pass->draws[1].indexOffset == 3 && pass->draws[1].indexCount == 6 &&
                   pass->draws[1].mesh == R::MeshId{}),
                  "[p3-own-offsets] the second draw names its range");
}

namespace {

// An opaque item of mesh 10 (no texture), its colour's red the entity.
R::Extracted3dItem Copy(const std::uint64_t entity, const float z) {
    R::Extracted3dItem item = Item(entity, z);
    item.mesh = R::MeshId{10};
    item.color = R::Color8{static_cast<std::uint8_t>(entity), 0, 0, 255};
    return item;
}

// The red of each instance of a draw (Copy's entity), in the draw's order.
std::vector<std::uint32_t> Reds(const R::Pass3D& pass, const R::Draw3D& draw) {
    std::vector<std::uint32_t> out;
    for (std::uint32_t i = 0; i < draw.instanceCount; ++i) out.push_back(pass.instances[draw.instanceOffset + i].color.r);
    return out;
}

} // namespace

TEST_CASE("pipeline3d: opaque items of one mesh, texture and pipeline are instances of one draw, where the nearest stands") {
    // big-map declaration section 16, I1, I2, I4.
    std::vector<R::Extracted3dItem> items{Copy(1, 9.0F), Copy(2, 3.0F), Copy(3, 6.0F), Copy(4, 12.0F)};
    items.push_back(Copy(5, 5.0F));
    items.back().mesh = R::MeshId{11};  // another mesh
    items.push_back(Copy(6, 4.0F));
    items.back().texture = R::TextureId{7};  // another texture
    items.push_back(Copy(7, 2.0F));
    items.back().cull = R::CullMode::Back;  // another cull
    items.push_back(Copy(8, 1.0F));
    items.back().depthWrite = false;  // another depth write
    const std::optional<R::Pass3D> pass = R::Queue3d(Camera3d(), items);
    REQUIRE(pass.has_value());
    CHECK_MESSAGE(pass->instances.size() == 8, "[in-every-item] every item is an instance");
    CHECK_MESSAGE(pass->draws.size() == 5, "[in-one-draw] the four copies are one draw, the four others one each");
    // The instances (by entity) of the draw that matches, or none when no draw does.
    const auto of = [&](const auto& match) {
        for (const R::Draw3D& d : pass->draws) {
            if (match(d)) return Reds(*pass, d);
        }
        return std::vector<std::uint32_t>{};
    };
    CHECK_MESSAGE(of([](const R::Draw3D& d) { return !d.depthWrite; }) == (std::vector<std::uint32_t>{8}),
                  "[in-depth-write-apart] another depth write is a draw of its own");
    CHECK_MESSAGE(of([](const R::Draw3D& d) { return d.cull == R::CullMode::Back; }) == (std::vector<std::uint32_t>{7}),
                  "[in-cull-apart] another cull is a draw of its own");
    CHECK_MESSAGE(of([](const R::Draw3D& d) { return d.texture == R::TextureId{7}; }) == (std::vector<std::uint32_t>{6}),
                  "[in-texture-apart] another texture is a draw of its own");
    CHECK_MESSAGE(of([](const R::Draw3D& d) { return d.mesh == R::MeshId{11}; }) == (std::vector<std::uint32_t>{5}),
                  "[in-mesh-apart] another mesh is a draw of its own");
    const auto isCopies = [](const R::Draw3D& d) {
        return d.mesh == R::MeshId{10} && d.texture == R::TextureId{} && d.cull == R::CullMode::None && d.depthWrite;
    };
    CHECK_MESSAGE(of(isCopies) == (std::vector<std::uint32_t>{2, 3, 1, 4}), "[in-bin-near-first] the copies, one draw, near to far");
    // Bins where their nearest item stands: 8 (z 1), 7 (z 2), the copies (from z 3), 6 (z 4), 5 (z 5).
    std::vector<std::uint32_t> firsts;
    for (const R::Draw3D& d : pass->draws) firsts.push_back(pass->instances[d.instanceOffset].color.r);
    CHECK_MESSAGE(firsts == (std::vector<std::uint32_t>{8, 7, 2, 6, 5}), "[in-bins-by-nearest] each draw where its nearest instance stands");
    for (const R::Draw3D& d : pass->draws) {
        if (!isCopies(d)) continue;
        CHECK_MESSAGE(d.indexCount == 3, "[in-draw-mesh] the draw names the shared mesh's indices");
        CHECK_MESSAGE((Near(pass->instances[d.instanceOffset].world.values[14], 3.0F) &&
                       Near(pass->instances[d.instanceOffset + 3].world.values[14], 12.0F)),
                      "[in-instance-world] each instance carries its item's world");
    }
}

TEST_CASE("pipeline3d: blended and overlay items join only their neighbours of the same key, far to near") {
    // big-map declaration section 16, I2, I4: the sorted groups keep their order.
    const auto blended = [](const std::uint64_t entity, const float z, const std::uint32_t mesh) {
        R::Extracted3dItem item = Copy(entity, z);
        item.mesh = R::MeshId{mesh};
        item.blend = R::BlendMode::Alpha;
        return item;
    };
    std::vector<R::Extracted3dItem> items{blended(1, 10.0F, 20), blended(2, 8.0F, 20), blended(3, 9.0F, 21), blended(4, 7.0F, 20),
                                          blended(5, 6.0F, 20)};
    // Overlays: the farthest is mesh 20 blended like 5, but not depth-tested; then two of mesh 30 that differ in blend.
    items.push_back(blended(6, 100.0F, 20));
    items.back().depthTest = false;
    items.push_back(blended(7, 5.0F, 30));
    items.back().depthTest = false;
    items.push_back(Copy(8, 4.0F));
    items.back().mesh = R::MeshId{30};
    items.back().depthTest = false;
    const std::optional<R::Pass3D> pass = R::Queue3d(Camera3d(), items);
    REQUIRE(pass.has_value());
    std::vector<std::vector<std::uint32_t>> draws;
    for (const R::Draw3D& d : pass->draws) draws.push_back(Reds(*pass, d));
    REQUIRE(draws.size() >= 3);
    CHECK_MESSAGE((draws[0] == std::vector<std::uint32_t>{1} && draws[1] == std::vector<std::uint32_t>{3}),
                  "[in-blend-neighbours-only] the same mesh with another between is not joined");
    CHECK_MESSAGE(draws[2] == (std::vector<std::uint32_t>{2, 4, 5}), "[in-blend-neighbours-join] neighbours of one key join, far to near");
    CHECK_MESSAGE((draws.size() == 6 && draws[3] == std::vector<std::uint32_t>{6}),
                  "[in-depth-test-apart] a neighbour that is not depth-tested is a draw of its own");
    CHECK_MESSAGE((draws.size() == 6 && draws[4] == std::vector<std::uint32_t>{7} && draws[5] == std::vector<std::uint32_t>{8}),
                  "[in-blend-apart] neighbours of another blend are draws of their own");
}

TEST_CASE("pipeline3d: culled items and items of other layers are not instances; items of their own geometry never join") {
    // big-map declaration section 16, I3.
    std::vector<R::Extracted3dItem> items{Copy(1, 5.0F), Copy(2, 6.0F), Copy(3, 7.0F), Copy(4, 8.0F)};
    items[2].layers = 2;
    R::ExtractedCamera camera = Camera3d();
    camera.culled = {2};
    R::Extracted3dItem skinned = Copy(5, 9.0F);
    skinned.mesh = R::MeshId{};
    skinned.vertices.resize(3);
    skinned.indices = {0, 1, 2};
    items.push_back(skinned);
    skinned.entity = 6;
    skinned.world = At(0, 0, 10.0F);
    items.push_back(skinned);
    const std::optional<R::Pass3D> pass = R::Queue3d(camera, items);
    REQUIRE(pass.has_value());
    REQUIRE(!pass->draws.empty());
    CHECK_MESSAGE(Reds(*pass, pass->draws[0]) == (std::vector<std::uint32_t>{1, 4}), "[in-culled-out] the culled and the other layer's are no instances");
    CHECK_MESSAGE((pass->draws.size() == 3 && pass->draws[1].instanceCount == 1 && pass->draws[2].instanceCount == 1 &&
                   pass->draws[2].vertexOffset == 3),
                  "[in-own-apart] items with vertices of their own are a draw each");
}

TEST_CASE("pipeline3d: a 2D camera or a camera without a size draws no 3D pass") {
    R::ExtractedCamera camera = Camera3d();
    camera.is3d = false;
    camera.is2d = true;
    CHECK_MESSAGE(!R::Queue3d(camera, {}).has_value(), "[p3-2d-camera] a 2D camera");
    R::ExtractedCamera invalid = Camera3d();
    invalid.computed.valid = false;
    CHECK_MESSAGE(!R::Queue3d(invalid, {}).has_value(), "[p3-invalid-camera]");
}

TEST_CASE("pipeline3d: CPU skinning follows the model's formula: weights x joint x inverse bind") {
    Md::ModelData model;
    model.nodes.resize(2);
    model.nodes[1].parent = 0;
    Md::Skin skin;
    skin.joints = {0, 1};
    skin.inverseBindMatrices = {At(0, 0, 0), At(0, -1, 0)};  // joint 1 sits at y 1 at bind time
    model.skins.push_back(skin);
    Md::Mesh mesh;
    mesh.skin = 0;
    mesh.positions = {{0, 0, 0}, {0, 1, 0}, {1, 1, 0}};
    mesh.normals = {{0, 0, -1}, {0, 0, -1}, {0, 0, -1}};
    mesh.indices = {0, 1, 2};
    mesh.vertexJoints = {{0, 0, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}};
    mesh.vertexWeights = {{1, 0, 0, 0}, {1, 0, 0, 0}, {0.5F, 0.5F, 0, 0}};
    // Node 1 moved to x 2, y 1 this frame; node 0 stays.
    const std::vector<M::Matrix4> nodes{At(0, 0, 0), At(2, 1, 0)};
    const std::vector<R::Vertex3D> v = R::SkinVertices(model, mesh, nodes);
    REQUIRE(v.size() == 3);
    CHECK_MESSAGE((Near(v[0].position.x, 0) && Near(v[0].position.y, 0)), "[p3-skin-root] a vertex on the still joint stays");
    CHECK_MESSAGE((Near(v[1].position.x, 2) && Near(v[1].position.y, 1)), "[p3-skin-joint] a vertex on the moved joint follows it");
    CHECK_MESSAGE((Near(v[2].position.x, 2) && Near(v[2].position.y, 1)), "[p3-skin-blend] half and half: (1,1) and (3,1) give (2,1)");
    CHECK_MESSAGE(Near(v[2].normal.z, -1.0F), "[p3-skin-normal] normals follow the rotation part, normalized");
}

TEST_CASE("pipeline3d: a model mesh is retained once; a new life lets the old go; vertices keep uv and normal") {
    Md::Mesh mesh;
    mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    mesh.uvs = {{0, 0}, {1, 0}, {0, 1}};
    mesh.indices = {0, 1, 2};
    const std::vector<R::Vertex3D> v = R::MeshVertices(mesh);
    CHECK_MESSAGE((v.size() == 3 && v[1].uv.x == 1.0F && v[1].normal.x == 0.0F), "[p3-mesh-vertices] uv kept, no normals: zero");
    R::RenderMeshes meshes;
    R::RenderModelMeshes cache;
    std::uint32_t count = 0;
    const std::optional<R::MeshId> first = cache.Use(R::ModelMeshKey{"m.body", 1, 0}, mesh, nullptr, meshes, count);
    REQUIRE(first.has_value());
    CHECK(count == 3);
    CHECK_MESSAGE((cache.Use(R::ModelMeshKey{"m.body", 1, 0}, mesh, nullptr, meshes, count) == first && meshes.LiveCount() == 1),
                  "[p3-mesh-once] one retained mesh for one model mesh");
    const std::optional<R::MeshId> second = cache.Use(R::ModelMeshKey{"m.body", 2, 0}, mesh, nullptr, meshes, count);
    REQUIRE(second.has_value());
    CHECK_MESSAGE((!meshes.Live(*first) && meshes.LiveCount() == 1), "[p3-mesh-new-life] the old life's mesh goes");
    Md::Mesh empty;
    CHECK_MESSAGE(!cache.Use(R::ModelMeshKey{"m.empty", 1, 0}, empty, nullptr, meshes, count).has_value(), "[p3-mesh-empty] nothing to draw");
}

TEST_CASE("pipeline3d: the bytes of new meshes in a frame are a soft limit; the next frame makes the rest") {
    Md::Mesh mesh;
    mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    mesh.indices = {0, 1, 2};
    const std::uint64_t bytes = 3 * sizeof(R::Vertex3D) + 3 * sizeof(std::uint32_t);
    R::RenderMeshes meshes;
    std::uint32_t count = 0;
    // A limit below one mesh: the first is still made (the one that passes it), the second waits.
    R::RenderModelMeshes limited(bytes - 1);
    CHECK_MESSAGE(limited.Use(R::ModelMeshKey{"m.a", 1, 0}, mesh, nullptr, meshes, count).has_value(), "[p3-bytes-soft] the one that passes the limit is made");
    CHECK(limited.BytesThisFrame() == bytes);
    CHECK_MESSAGE(!limited.Use(R::ModelMeshKey{"m.b", 1, 0}, mesh, nullptr, meshes, count).has_value(), "[p3-bytes-wait] past the limit, a new mesh waits");
    CHECK_MESSAGE(limited.Use(R::ModelMeshKey{"m.a", 1, 0}, mesh, nullptr, meshes, count).has_value(), "[p3-bytes-made-drawn] a made mesh is still drawn");
    limited.EndFrame(meshes);
    CHECK(limited.BytesThisFrame() == 0);
    CHECK_MESSAGE(limited.Use(R::ModelMeshKey{"m.b", 1, 0}, mesh, nullptr, meshes, count).has_value(), "[p3-bytes-next] the next frame makes it");
    // No limit: every mesh in one frame.
    R::RenderModelMeshes open;
    for (int i = 0; i < 4; ++i) {
        CHECK_MESSAGE(open.Use(R::ModelMeshKey{"m.c", 1, nullptr, static_cast<std::uint32_t>(i)}, mesh, nullptr, meshes, count).has_value(), "[p3-bytes-none]");
    }
}

TEST_CASE("pipeline3d: a handed model's vertices are taken from its meshes without a skin; a skinned mesh keeps them") {
    Md::ModelData model;
    model.nodes.resize(2);
    Md::Mesh plain;
    plain.name = "plain";
    plain.node = 1;
    plain.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    plain.indices = {0, 1, 2};
    Md::Mesh skinned = plain;
    skinned.name = "skinned";
    skinned.skin = 0;
    model.meshes = {plain, skinned};
    R::HandedModel handed(std::make_shared<const Md::ModelData>(std::move(model)));
    CHECK_FALSE(handed.Taken());
    CHECK(handed.Model()->meshes[0].positions.size() == 3);
    handed.TakeVertices();
    const auto left = handed.Model();
    CHECK(handed.Taken());
    CHECK_MESSAGE((left->meshes[0].positions.empty() && left->meshes[0].indices.empty() && left->meshes[0].node == 1 &&
                   left->meshes[0].name == "plain"),
                  "[p3-handed-plain] a mesh without a skin keeps its place, not its vertices");
    CHECK_MESSAGE((left->meshes[1].positions.size() == 3 && left->meshes[1].indices.size() == 3), "[p3-handed-skinned] a skinned mesh keeps its vertices");
    CHECK(left->nodes.size() == 2);
}

TEST_CASE("compose: cameras in order, each clear first: the first on a target clears it, a later one its viewport") {
    R::ExtractedCameras cameras;
    R::ExtractedCamera world = Camera3d(std::nullopt, 0);
    world.clear = R::Color8{1, 2, 3, 255};
    R::ExtractedCamera hud = Camera3d(std::nullopt, 1);
    hud.is3d = false;
    hud.is2d = true;
    hud.clear = std::nullopt;
    R::ExtractedCamera split = Camera3d(std::nullopt, 2);
    split.clear = R::Color8{9, 9, 9, 255};
    split.computed.viewport = C::Viewport{{50, 0}, {50, 100}};
    R::ExtractedCamera other = Camera3d(R::TextureId{5}, 3);
    other.clear = R::Color8{7, 7, 7, 255};
    cameras.cameras = {world, hud, split, other};
    R::CameraPasses passes;
    passes.byCamera.resize(4);
    passes.byCamera[0][R::CameraPhase::Opaque].push_back(R::Pass3D{});
    passes.byCamera[1][R::CameraPhase::Transparent].push_back(R::Pass2D{});
    R::RenderCommands out;
    R::ComposePasses(cameras, passes, out);
    REQUIRE(out.passes.size() == 5);
    const auto* clear0 = std::get_if<R::Pass2D>(&out.passes[0]);
    CHECK_MESSAGE((clear0 != nullptr && clear0->clear == R::Color8{1, 2, 3, 255} && clear0->draws.empty()), "[cp-first-clear] the first camera clears the target");
    CHECK_MESSAGE(std::holds_alternative<R::Pass3D>(out.passes[1]), "[cp-order] then its 3D pass");
    const auto* hudPass = std::get_if<R::Pass2D>(&out.passes[2]);
    CHECK_MESSAGE((hudPass != nullptr && !hudPass->clear.has_value()), "[cp-none] a camera without a clear adds no clear");
    const auto* splitClear = std::get_if<R::Pass2D>(&out.passes[3]);
    CHECK_MESSAGE((splitClear != nullptr && !splitClear->clear.has_value() && splitClear->draws.size() == 1 &&
                   splitClear->draws[0].blend == R::BlendMode::Opaque && splitClear->viewport == R::PixelRect{50, 0, 50, 100} &&
                   splitClear->vertices[2].position.x == 50.0F && splitClear->vertices[0].color == R::Color8{9, 9, 9, 255}),
                  "[cp-later-viewport] a later camera clears its viewport with an opaque rectangle");
    const auto* otherClear = std::get_if<R::Pass2D>(&out.passes[4]);
    CHECK_MESSAGE((otherClear != nullptr && otherClear->clear == R::Color8{7, 7, 7, 255} && otherClear->target == R::TextureId{5}),
                  "[cp-per-target] the first camera on another target clears that target");
    CHECK_MESSAGE((passes.byCamera[0][R::CameraPhase::Opaque].empty() && passes.byCamera[1][R::CameraPhase::Transparent].empty()),
                  "[cp-emptied] the slots are emptied");
}

TEST_CASE("compose: a camera on a target an earlier camera's pass drew on clears only its viewport") {
    // The world camera draws on the main target and post-processes into the target 5 a later camera shows: the later
    // camera is not the first on target 5.
    R::ExtractedCameras cameras;
    R::ExtractedCamera world = Camera3d(std::nullopt, 0);
    world.clear = R::Color8{1, 2, 3, 255};
    R::ExtractedCamera monitor = Camera3d(R::TextureId{5}, 1);
    monitor.clear = R::Color8{4, 5, 6, 255};
    cameras.cameras = {world, monitor};
    R::CameraPasses passes;
    passes.byCamera.resize(2);
    R::PassFullscreen post;
    post.target = R::TextureId{5};
    passes.byCamera[0][R::CameraPhase::PostProcess].push_back(post);
    R::RenderCommands out;
    R::ComposePasses(cameras, passes, out);
    REQUIRE_MESSAGE(out.passes.size() == 3, "[cp-written-all] the world's clear, its post-processing, the monitor's clear");
    const auto* clear = std::get_if<R::Pass2D>(&out.passes[2]);
    CHECK_MESSAGE((clear != nullptr && !clear->clear.has_value() && clear->draws.size() == 1),
                  "[cp-written-not-first] the later camera clears its viewport, not the picture the earlier pass drew");
}

TEST_CASE("compose: the views that are no camera's first; each camera's clear, then its phases in order") {
    R::ExtractedCameras cameras;
    R::ExtractedCamera world = Camera3d(std::nullopt, 0);
    world.clear = R::Color8{1, 2, 3, 255};
    R::ExtractedCamera hud = Camera3d(std::nullopt, 1);
    hud.clear = std::nullopt;
    cameras.cameras = {world, hud};
    R::CameraPasses passes;
    passes.byCamera.resize(2);
    // Put in the reverse of the order they are drawn in, as Systems in any order would: the order is the phases'.
    const auto tagged = [](const std::uint32_t id) {
        R::PassFullscreen pass;
        pass.inputs = {R::TextureId{id}};
        return pass;
    };
    passes.byCamera[1][R::CameraPhase::Overlay].push_back(tagged(9));
    passes.byCamera[0][R::CameraPhase::Overlay].push_back(tagged(6));
    passes.byCamera[0][R::CameraPhase::PostProcess].push_back(tagged(5));
    passes.byCamera[0][R::CameraPhase::Transparent].push_back(tagged(4));
    passes.byCamera[0][R::CameraPhase::Opaque].push_back(tagged(3));
    passes.byCamera[0][R::CameraPhase::Opaque].push_back(tagged(7));
    passes.byCamera[0][R::CameraPhase::Prepass].push_back(tagged(2));
    R::PassDepth shadow;
    shadow.target = R::TextureId{8};
    passes.beforeCameras.push_back(shadow);
    R::RenderCommands out;
    R::ComposePasses(cameras, passes, out);
    std::vector<std::uint32_t> order;
    for (const R::RenderPass& pass : out.passes) {
        if (const auto* f = std::get_if<R::PassFullscreen>(&pass)) order.push_back(f->inputs[0].value);
        if (const auto* d = std::get_if<R::PassDepth>(&pass)) order.push_back(100 + d->target.value);
        if (const auto* c = std::get_if<R::Pass2D>(&pass)) order.push_back(c->clear ? 1 : 0);
    }
    REQUIRE_MESSAGE(out.passes.size() == 9, "[cp-phase-all] every pass, the view of no camera's among them");
    CHECK_MESSAGE(order[0] == 108, "[cp-non-camera-first] a view that is no camera's comes before every camera");
    CHECK_MESSAGE(order[1] == 1, "[cp-non-camera-own-target] the shadow map is its own target: the first camera on the main one clears it all");
    CHECK_MESSAGE((order[2] == 2 && order[3] == 3 && order[4] == 7 && order[5] == 4 && order[6] == 5 && order[7] == 6),
                  "[cp-phase-order] Prepass, Opaque (in its order), Transparent, PostProcess, Overlay");
    CHECK_MESSAGE(order[8] == 9, "[cp-phase-camera-order] then the next camera's");
    CHECK_MESSAGE((passes.beforeCameras.empty() && passes.byCamera[0][R::CameraPhase::Opaque].empty() &&
                   passes.byCamera[0][R::CameraPhase::Overlay].empty()),
                  "[cp-phase-emptied] every list is emptied");
}

TEST_CASE("pipeline3d: a mesh without a skin sits at its node; a skinned one at the entity alone") {
    Md::Mesh mesh;
    mesh.node = 1;
    const std::vector<M::Matrix4> nodes{At(0, 0, 0), At(0, 3, 0)};
    const M::Matrix4 placed = R::PlaceMesh(At(10, 0, 0), mesh, nodes);
    CHECK_MESSAGE((placed.values[12] == 10.0F && placed.values[13] == 3.0F), "[p3-place-node] the entity times the node");
    mesh.skin = 0;
    const M::Matrix4 skinned = R::PlaceMesh(At(10, 0, 0), mesh, nodes);
    CHECK_MESSAGE((skinned.values[12] == 10.0F && skinned.values[13] == 0.0F), "[p3-place-skinned] the entity alone");
}

TEST_CASE("pipeline3d: skinning multiplies the joint by the inverse bind in that order; an unbound vertex sits at its node") {
    Md::ModelData model;
    model.nodes.resize(2);
    Md::Skin skin;
    skin.joints = {0};
    // The joint turned a quarter round z this frame; at bind time it sat at x 1.
    skin.inverseBindMatrices = {At(-1, 0, 0)};
    model.skins.push_back(skin);
    Md::Mesh mesh;
    mesh.node = 1;
    mesh.skin = 0;
    mesh.positions = {{2, 0, 0}, {5, 0, 0}};
    mesh.indices = {0, 1, 0};
    mesh.vertexJoints = {{0, 0, 0, 0}, {0, 0, 0, 0}};
    mesh.vertexWeights = {{1, 0, 0, 0}, {0, 0, 0, 0}};
    T::Transform turned;
    turned.rotation = M::MakeQuaternionFromAxisAngle({0, 0, 1}, M::Pi / 2.0F);
    const std::vector<M::Matrix4> nodes{T::ToMatrix(turned), At(0, 3, 0)};
    const std::vector<R::Vertex3D> v = R::SkinVertices(model, mesh, nodes);
    REQUIRE(v.size() == 2);
    // joint x inverseBind: (2,0) - (1,0) = (1,0), then turned: (0,1). The other order would give (0,2) - (1,0) = (-1,2).
    CHECK_MESSAGE((Near(v[0].position.x, 0) && Near(v[0].position.y, 1)), "[p3-skin-order] " << v[0].position.x << ", " << v[0].position.y);
    CHECK_MESSAGE((Near(v[1].position.x, 5) && Near(v[1].position.y, 3)), "[p3-skin-unbound] a vertex of no influence at its node: "
                                                                            << v[1].position.x << ", " << v[1].position.y);
}

// ---- Hdr cameras (render-pipeline declaration section 16, revision 16.8, v8-arch-gyo2-render-a4).

TEST_CASE("pipeline3d: a Hdr camera's 3D pass draws on its picture, testing its target's shared depth") {
    R::ExtractedCamera camera = Camera3d(R::TextureId{5});
    camera.hdr = R::TextureId{20};
    camera.depth = R::TextureId{21};
    const std::optional<R::Pass3D> pass = R::Queue3d(camera, {});
    REQUIRE(pass.has_value());
    CHECK_MESSAGE(pass->target == R::TextureId{20}, "[p3-hdr-scene-target] its picture, not its target");
    CHECK_MESSAGE(pass->depth == R::TextureId{21}, "[p3-hdr-depth] the depth of its target, shared by the 3D cameras there");
    camera.hdr = std::nullopt;
    CHECK_MESSAGE(R::Queue3d(camera, {})->target == R::TextureId{5}, "[p3-scene-target] without Hdr, its target");
    CHECK_MESSAGE((R::PhaseTarget(camera, R::CameraPhase::Overlay) == R::TextureId{5}), "[p3-phase-target-plain] every phase on its target");
    camera.hdr = R::TextureId{20};
    CHECK_MESSAGE((R::PhaseTarget(camera, R::CameraPhase::PostProcess) == R::TextureId{20} &&
                   R::PhaseTarget(camera, R::CameraPhase::Transparent) == R::TextureId{20} &&
                   R::PhaseTarget(camera, R::CameraPhase::Overlay) == R::TextureId{5}),
                  "[p3-phase-target-hdr] up to PostProcess on the picture, Overlay on the target");
}

TEST_CASE("compose: a Hdr camera clears its picture, draws there, tonemaps after PostProcess, then its Overlay") {
    R::RenderTextures textures;
    const R::TextureId picture = textures.CreateTarget(R::RenderTargetDesc{R::FollowTarget{}, R::RenderTargetFormat::Rgba16Float});
    R::ExtractedCameras cameras;
    R::ExtractedCamera world = Camera3d(std::nullopt, 0);
    world.clear = R::Color8{1, 2, 3, 255};
    world.hdr = picture;
    world.tonemapping = R::Tonemapping::Method::AcesFitted;
    world.computed.viewport = C::Viewport{{10, 0}, {80, 100}};
    cameras.cameras = {world};
    R::CameraPasses passes;
    passes.byCamera.resize(1);
    passes.byCamera[0][R::CameraPhase::Opaque].push_back(*R::Queue3d(world, {}));
    R::PassFullscreen post;
    post.target = R::TextureId{30};
    passes.byCamera[0][R::CameraPhase::PostProcess].push_back(post);
    R::Pass2D overlay;
    passes.byCamera[0][R::CameraPhase::Overlay].push_back(overlay);
    R::RenderCommands out;
    R::ComposePasses(cameras, passes, out);
    REQUIRE_MESSAGE(out.passes.size() == 6, "[cp-hdr-all] the target's clear, the picture's clear, the scene, post, tonemapping, overlay");
    const auto* outputClear = std::get_if<R::Pass2D>(&out.passes[0]);
    CHECK_MESSAGE((outputClear != nullptr && !outputClear->target && outputClear->clear == R::Color8{1, 2, 3, 255}),
                  "[cp-hdr-output-cleared] the first camera on its target clears the target whole (the bars), as without Hdr");
    const auto* pictureClear = std::get_if<R::Pass2D>(&out.passes[1]);
    CHECK_MESSAGE((pictureClear != nullptr && pictureClear->target == picture && pictureClear->clear == R::Color8{1, 2, 3, 255} &&
                   pictureClear->draws.empty()),
                  "[cp-hdr-picture-cleared] its picture cleared whole to its colour");
    const auto* scene = std::get_if<R::Pass3D>(&out.passes[2]);
    CHECK_MESSAGE((scene != nullptr && scene->target == picture), "[cp-hdr-scene] its scene on the picture");
    const auto* posted = std::get_if<R::PassFullscreen>(&out.passes[3]);
    CHECK_MESSAGE((posted != nullptr && posted->target == R::TextureId{30}), "[cp-hdr-tonemap-after-post] its PostProcess passes before the tonemapping");
    const auto* tonemap = std::get_if<R::PassFullscreen>(&out.passes[4]);
    REQUIRE(tonemap != nullptr);
    const R::PassFullscreen expected =
        R::TonemappingPass(picture, std::nullopt, R::PixelRect{10, 0, 80, 100}, R::Tonemapping::Method::AcesFitted, R::BlendMode::Opaque);
    CHECK_MESSAGE((tonemap->target == expected.target && tonemap->viewport == expected.viewport && tonemap->shader == expected.shader &&
                   tonemap->inputs == expected.inputs && tonemap->parameters == expected.parameters),
                  "[cp-hdr-tonemap-pass] the picture through its curve onto its target, over its viewport");
    CHECK_MESSAGE(tonemap->blend == R::BlendMode::Opaque, "[cp-hdr-first-replaces] the first camera on its target replaces what is there");
    CHECK_MESSAGE(std::holds_alternative<R::Pass2D>(out.passes[5]), "[cp-hdr-tonemap-before-overlay] its Overlay after the tonemapping");
    CHECK_MESSAGE(!R::FirstUnwrittenRead(out, textures).has_value(), "[cp-hdr-written-before-read] the picture is drawn before it is sampled (RP-3)");
}

TEST_CASE("compose: a later Hdr camera clears only its own picture and blends its tonemapping over what earlier cameras drew") {
    R::ExtractedCameras cameras;
    // A 2D background camera, then a Hdr camera on the left half with a colour, then one over the whole target with none.
    R::ExtractedCamera background = Camera3d(std::nullopt, 0);
    background.is3d = false;
    background.is2d = true;
    background.clear = R::Color8{7, 7, 7, 255};
    R::ExtractedCamera left = Camera3d(std::nullopt, 1);
    left.clear = R::Color8{9, 9, 9, 255};
    left.hdr = R::TextureId{20};
    left.computed.viewport = C::Viewport{{0, 0}, {50, 100}};
    R::ExtractedCamera over = Camera3d(std::nullopt, 2);
    over.clear = std::nullopt;
    over.hdr = R::TextureId{21};
    cameras.cameras = {background, left, over};
    R::CameraPasses passes;
    passes.byCamera.resize(3);
    R::RenderCommands out;
    R::ComposePasses(cameras, passes, out);
    REQUIRE_MESSAGE(out.passes.size() == 5, "[cp-hdr-later-all] the background's clear; each Hdr camera's picture clear and tonemapping");
    const auto* leftClear = std::get_if<R::Pass2D>(&out.passes[1]);
    CHECK_MESSAGE((leftClear != nullptr && leftClear->target == R::TextureId{20} && leftClear->clear == R::Color8{9, 9, 9, 255}),
                  "[cp-hdr-own-picture] a later Hdr camera with a colour clears its own picture, nothing of the target");
    const auto* leftTonemap = std::get_if<R::PassFullscreen>(&out.passes[2]);
    CHECK_MESSAGE((leftTonemap != nullptr && leftTonemap->blend == R::BlendMode::Alpha && leftTonemap->viewport == R::PixelRect{0, 0, 50, 100}),
                  "[cp-hdr-later-blends] a camera not the first on its target blends its tonemapping over its viewport");
    const auto* overClear = std::get_if<R::Pass2D>(&out.passes[3]);
    CHECK_MESSAGE((overClear != nullptr && overClear->target == R::TextureId{21} && overClear->clear == R::Color8{0, 0, 0, 0}),
                  "[cp-hdr-none-transparent] a camera without a colour clears its picture to transparent black: what is under it stays");
    const auto* overTonemap = std::get_if<R::PassFullscreen>(&out.passes[4]);
    CHECK_MESSAGE((overTonemap != nullptr && overTonemap->inputs[0] == R::TextureId{21} && overTonemap->blend == R::BlendMode::Alpha),
                  "[cp-hdr-each-tonemaps] each Hdr camera tonemaps its own picture");
}
