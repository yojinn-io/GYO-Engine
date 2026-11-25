#include "engine/render/Pipeline3d.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "engine/math/linear/Vec4.hpp"
#include "engine/render/Compose.hpp"

namespace GYO::Engine::Render {

std::vector<Vertex3D> MeshVertices(const Model::Mesh& mesh) {
    std::vector<Vertex3D> out(mesh.positions.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i].position = mesh.positions[i];
        if (i < mesh.normals.size()) out[i].normal = mesh.normals[i];
        if (i < mesh.uvs.size()) out[i].uv = mesh.uvs[i];
    }
    return out;
}

std::optional<MeshId> RenderModelMeshes::Use(const ModelMeshKey& key, const Model::Mesh& mesh, RenderMeshes& meshes,
                                             std::uint32_t& indexCount) {
    auto found = entries_.find(key);
    if (found == entries_.end()) {
        if (mesh.positions.empty() || mesh.indices.empty() || mesh.indices.size() % 3 != 0) return std::nullopt;
        // A new life of the asset: the old life's meshes can never be drawn again (as RenderImages).
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->first.asset == key.asset && it->first.life != key.life) {
                meshes.Release(it->second.mesh);
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
        const MeshId id = meshes.Create(MeshVertices(mesh), mesh.indices);
        found = entries_.emplace(key, Entry{id, static_cast<std::uint32_t>(mesh.indices.size())}).first;
    }
    indexCount = found->second.indexCount;
    return found->second.mesh;
}

std::vector<Vertex3D> SkinVertices(const Model::ModelData& model, const Model::Mesh& mesh, const std::span<const Math::Matrix4> nodes) {
    const Model::Skin& skin = model.skins[mesh.skin];
    // The joint palette: nodes[joint] x inverseBind(joint), once per joint for this mesh (a model whose skin is shared
    // by several meshes computes it once per mesh; RC-RD 6 covers the cost).
    std::vector<Math::Matrix4> palette(skin.joints.size());
    for (std::size_t j = 0; j < palette.size(); ++j) palette[j] = Math::Multiply(nodes[skin.joints[j]], skin.inverseBindMatrices[j]);
    std::vector<Vertex3D> out = MeshVertices(mesh);
    for (std::size_t v = 0; v < out.size() && v < mesh.vertexJoints.size() && v < mesh.vertexWeights.size(); ++v) {
        const auto& joints = mesh.vertexJoints[v];
        const auto& weights = mesh.vertexWeights[v];
        Math::Vec3 position{};
        Math::Vec3 normal{};
        float total = 0.0F;
        for (std::size_t k = 0; k < 4; ++k) {
            const float w = weights[k];
            if (w == 0.0F) continue;
            const Math::Matrix4& m = palette[joints[k]];
            position = position + Math::TransformPoint(m, out[v].position) * w;
            normal = normal + Math::TransformVector(m, out[v].normal) * w;
            total += w;
        }
        if (total == 0.0F) {
            // No influence: the vertex sits at the mesh's node as a mesh without a skin does (model.zh-Hant.md 2.1),
            // brought into model space here because a skinned mesh's world is the entity's alone (PlaceMesh).
            if (mesh.node < nodes.size()) {
                out[v].position = Math::TransformPoint(nodes[mesh.node], out[v].position);
                out[v].normal = Math::TransformVector(nodes[mesh.node], out[v].normal);
            }
            continue;
        }
        out[v].position = position;
        const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        out[v].normal = length > 0.0F ? normal * (1.0F / length) : normal;
    }
    return out;
}

Math::Matrix4 PlaceMesh(const Math::Matrix4& global, const Model::Mesh& mesh, const std::span<const Math::Matrix4> nodes) {
    if (mesh.skin != Model::kNoSkin) return global;
    return mesh.node < nodes.size() ? Math::Multiply(global, nodes[mesh.node]) : global;
}

std::optional<Pass3D> Queue3d(const ExtractedCamera& camera, const std::span<const Extracted3dItem> items) {
    if (!camera.is3d || !camera.computed.valid) return std::nullopt;
    const std::optional<Math::Matrix4> viewFromWorld = Math::Inverse(camera.view);
    if (!viewFromWorld) return std::nullopt;
    Pass3D pass;
    pass.target = camera.target;
    const Camera::Viewport& vp = camera.computed.viewport;
    pass.viewport = PixelRect{static_cast<std::uint32_t>(vp.position.x), static_cast<std::uint32_t>(vp.position.y),
                              static_cast<std::uint32_t>(vp.size.x), static_cast<std::uint32_t>(vp.size.y)};
    pass.clipFromWorld = Math::Multiply(camera.computed.clipFromView, *viewFromWorld);
    pass.clearDepth = camera.clearDepth;

    // 0: opaque and depth-tested, near to far; 1: blended and depth-tested, far to near; 2: not depth-tested, far to near.
    struct Entry {
        std::size_t index;
        int group;
        float distance;
    };
    std::vector<Entry> order;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const Extracted3dItem& item = items[i];
        if ((item.layers & camera.layers) == 0) continue;
        if (std::binary_search(camera.culled.begin(), camera.culled.end(), item.entity)) continue;
        if (item.mesh == MeshId{} && item.indices.empty()) continue;
        const int group = !item.depthTest ? 2 : (item.blend == BlendMode::Opaque ? 0 : 1);
        const Math::Vec3 origin{item.world.values[12], item.world.values[13], item.world.values[14]};
        order.push_back(Entry{i, group, Math::TransformPoint(*viewFromWorld, origin).z});
    }
    std::sort(order.begin(), order.end(), [&](const Entry& a, const Entry& b) {
        if (a.group != b.group) return a.group < b.group;
        if (a.distance != b.distance) return a.group == 0 ? a.distance < b.distance : a.distance > b.distance;
        if (items[a.index].entity != items[b.index].entity) return items[a.index].entity < items[b.index].entity;
        return a.index < b.index;
    });

    for (const Entry& e : order) {
        const Extracted3dItem& item = items[e.index];
        Draw3D draw;
        draw.world = item.world;
        draw.color = item.color;
        draw.texture = item.texture;
        draw.blend = item.blend;
        draw.depthTest = item.depthTest;
        draw.depthWrite = item.depthWrite;
        draw.cull = item.cull;
        if (item.mesh != MeshId{}) {
            draw.mesh = item.mesh;
            draw.indexCount = item.indexCount;
        } else {
            draw.vertexOffset = static_cast<std::uint32_t>(pass.vertices.size());
            draw.indexOffset = static_cast<std::uint32_t>(pass.indices.size());
            draw.indexCount = static_cast<std::uint32_t>(item.indices.size());
            pass.vertices.insert(pass.vertices.end(), item.vertices.begin(), item.vertices.end());
            pass.indices.insert(pass.indices.end(), item.indices.begin(), item.indices.end());
        }
        pass.draws.push_back(draw);
    }
    return pass;
}

void ComposePasses(const ExtractedCameras& cameras, CameraPasses& passes, RenderCommands& out) {
    std::vector<std::uint32_t> drawn;
    for (const RenderPass& pass : out.passes) drawn.push_back(TargetOf(pass));
    for (std::size_t i = 0; i < cameras.cameras.size(); ++i) {
        const ExtractedCamera& camera = cameras.cameras[i];
        const bool first = std::find(drawn.begin(), drawn.end(), camera.target) == drawn.end();
        drawn.push_back(camera.target);
        if (camera.clear) {
            Pass2D clear;
            clear.target = camera.target;
            if (first) {
                clear.clear = camera.clear;
            } else {
                const Camera::Viewport& vp = camera.computed.viewport;
                clear.viewport = PixelRect{static_cast<std::uint32_t>(vp.position.x), static_cast<std::uint32_t>(vp.position.y),
                                           static_cast<std::uint32_t>(vp.size.x), static_cast<std::uint32_t>(vp.size.y)};
                const auto w = static_cast<float>(vp.size.x);
                const auto h = static_cast<float>(vp.size.y);
                const Color8 c = *camera.clear;
                clear.vertices = {{{0.0F, 0.0F}, {}, c}, {{w, 0.0F}, {}, c}, {{w, h}, {}, c}, {{0.0F, h}, {}, c}};
                clear.indices = {0, 1, 2, 0, 2, 3};
                clear.draws.push_back(Draw2D{TextureId{}, BlendMode::Opaque, std::nullopt, 0, 6, 0});
            }
            out.passes.push_back(std::move(clear));
        }
        if (i < passes.byCamera.size()) {
            for (RenderPass& pass : passes.byCamera[i]) out.passes.push_back(std::move(pass));
            passes.byCamera[i].clear();
        }
    }
}

} // namespace GYO::Engine::Render
