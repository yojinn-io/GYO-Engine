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
R::ExtractedCamera Camera3d(const std::uint32_t target = 0, const std::int32_t order = 0) {
    const C::TargetSize size{{100, 100}, 1.0F};
    R::ExtractedCamera camera;
    camera.computed = C::Compute(C::Camera{}, C::Projection{C::PerspectiveProjection{}}, &size);
    camera.view = At(1, 0, -2);
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
    const std::optional<M::Matrix4> viewFromWorld = M::Inverse(camera.view);
    REQUIRE(viewFromWorld.has_value());
    const M::Matrix4 expected = M::Multiply(camera.computed.clipFromView, *viewFromWorld);
    bool same = true;
    for (std::size_t i = 0; i < 16; ++i) same = same && pass->clipFromWorld.values[i] == expected.values[i];
    CHECK_MESSAGE(same, "[p3-clip-from-world] clip from view times view from world");
    CHECK_MESSAGE((pass->viewport == R::PixelRect{0, 0, 100, 100}), "[p3-viewport] the camera's viewport");
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
    const std::optional<R::MeshId> first = cache.Use(R::ModelMeshKey{"m.body", 1, 0}, mesh, meshes, count);
    REQUIRE(first.has_value());
    CHECK(count == 3);
    CHECK_MESSAGE((cache.Use(R::ModelMeshKey{"m.body", 1, 0}, mesh, meshes, count) == first && meshes.LiveCount() == 1),
                  "[p3-mesh-once] one retained mesh for one model mesh");
    const std::optional<R::MeshId> second = cache.Use(R::ModelMeshKey{"m.body", 2, 0}, mesh, meshes, count);
    REQUIRE(second.has_value());
    CHECK_MESSAGE((!meshes.Live(*first) && meshes.LiveCount() == 1), "[p3-mesh-new-life] the old life's mesh goes");
    Md::Mesh empty;
    CHECK_MESSAGE(!cache.Use(R::ModelMeshKey{"m.empty", 1, 0}, empty, meshes, count).has_value(), "[p3-mesh-empty] nothing to draw");
}

TEST_CASE("compose: cameras in order, each clear first: the first on a target clears it, a later one its viewport") {
    R::ExtractedCameras cameras;
    R::ExtractedCamera world = Camera3d(0, 0);
    world.clear = R::Color8{1, 2, 3, 255};
    R::ExtractedCamera hud = Camera3d(0, 1);
    hud.is3d = false;
    hud.is2d = true;
    hud.clear = std::nullopt;
    R::ExtractedCamera split = Camera3d(0, 2);
    split.clear = R::Color8{9, 9, 9, 255};
    split.computed.viewport = C::Viewport{{50, 0}, {50, 100}};
    R::ExtractedCamera other = Camera3d(5, 3);
    other.clear = R::Color8{7, 7, 7, 255};
    cameras.cameras = {world, hud, split, other};
    R::CameraPasses passes;
    passes.byCamera.resize(4);
    passes.byCamera[0].push_back(R::Pass3D{});
    passes.byCamera[1].push_back(R::Pass2D{});
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
    CHECK_MESSAGE((otherClear != nullptr && otherClear->clear == R::Color8{7, 7, 7, 255} && otherClear->target == 5),
                  "[cp-per-target] the first camera on another target clears that target");
    CHECK_MESSAGE((passes.byCamera[0].empty() && passes.byCamera[1].empty()), "[cp-emptied] the slots are emptied");
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
