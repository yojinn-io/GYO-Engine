#include "engine/render/Pipeline3d.hpp"

#include <algorithm>
#include <cmath>
#include <compare>
#include <map>
#include <numeric>
#include <memory>
#include <utility>
#include <variant>

#include "engine/math/linear/Vec4.hpp"
#include "engine/render/Compose.hpp"
#include "engine/world_asset/AssetView.hpp"

namespace GYO::Engine::Render {

namespace {

// What two items must share to be instances of one draw: the retained mesh (Bevy's BatchCompareData, the mesh asset,
// bevy_pbr/src/render/mesh.rs:2910), the texture (its material bind group, :2892-2899) and the pipeline (blend, depth,
// cull; Bevy's pipeline id, bevy_render/src/batching/mod.rs:41-52). The world and the colour go with each instance.
struct BatchKey final {
    std::uint32_t mesh = 0;
    std::uint32_t texture = 0;
    BlendMode blend = BlendMode::Opaque;
    bool depthTest = true;
    bool depthWrite = true;
    CullMode cull = CullMode::None;

    friend auto operator<=>(const BatchKey&, const BatchKey&) = default;
};

BatchKey KeyOf(const Extracted3dItem& item) {
    BatchKey key;
    key.mesh = item.mesh.value;
    key.texture = item.texture.value;
    key.blend = item.blend;
    key.depthTest = item.depthTest;
    key.depthWrite = item.depthWrite;
    key.cull = item.cull;
    return key;
}

} // namespace

std::vector<Vertex3D> MeshVertices(const Model::Mesh& mesh) {
    std::vector<Vertex3D> out(mesh.positions.size());
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i].position = mesh.positions[i];
        if (i < mesh.normals.size()) out[i].normal = mesh.normals[i];
        if (i < mesh.uvs.size()) out[i].uv = mesh.uvs[i];
    }
    return out;
}

ResolvedModel ResolveModel(const ModelRef& model, const WorldAsset::AssetView* view) {
    ResolvedModel out;
    if (const auto* handle = std::get_if<Asset::Handle<Model::ModelData>>(&model)) {
        out.key.asset = handle->Id().Text();
        out.key.life = Asset::Detail::HandleAccess::Life(*handle);
        if (view != nullptr && !out.key.asset.empty()) out.data = view->Get(*handle);
    } else {
        out.holder = std::get<std::shared_ptr<const Model::ModelData>>(model);
        out.data = out.holder.get();
        out.key.held = out.data;
    }
    return out;
}

std::optional<MeshId> RenderModelMeshes::Use(const ModelMeshKey& key, const Model::Mesh& mesh,
                                             std::shared_ptr<const Model::ModelData> holder, RenderMeshes& meshes,
                                             std::uint32_t& indexCount) {
    auto found = entries_.find(key);
    if (found == entries_.end()) {
        if (mesh.positions.empty() || mesh.indices.empty() || mesh.indices.size() % 3 != 0) return std::nullopt;
        // A new life of the asset: the old life's meshes can never be drawn again (as RenderImages).
        for (auto it = entries_.begin(); key.held == nullptr && it != entries_.end();) {
            if (it->first.held == nullptr && it->first.asset == key.asset && it->first.life != key.life) {
                meshes.Release(it->second.mesh);
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
        const MeshId id = meshes.Create(MeshVertices(mesh), mesh.indices);
        found = entries_.emplace(key, Entry{id, static_cast<std::uint32_t>(mesh.indices.size()), std::move(holder), false}).first;
    }
    found->second.used = true;
    indexCount = found->second.indexCount;
    return found->second.mesh;
}

void RenderModelMeshes::EndFrame(RenderMeshes& meshes) {
    for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->first.held != nullptr && !it->second.used) {
            meshes.Release(it->second.mesh);
            it = entries_.erase(it);
            continue;
        }
        it->second.used = false;
        ++it;
    }
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

    // Automatic batching (big-map declaration section 16; Bevy's batch_and_prepare_*_render_phase,
    // bevy_render/src/batching/): each batch is one draw, its items the draw's instances. The opaque group is binned by
    // key (Bevy's Opaque3d bins, no_gpu_preprocessing.rs:122-154): a bin is made where its nearest item stands, so the
    // bins keep the near-to-far order and a pass where nothing shares a key is drawn as before. The blended and the
    // not depth-tested are sorted phases: only neighbours with the same key join (Bevy's batching/mod.rs:236-243), so the
    // far-to-near order holds. Items with geometry of their own (skinned on the CPU) never join.
    std::vector<std::vector<std::size_t>> batches;
    std::map<BatchKey, std::size_t> bins;
    for (const Entry& e : order) {
        const Extracted3dItem& item = items[e.index];
        if (item.mesh == MeshId{}) {
            batches.push_back({e.index});
            continue;
        }
        const BatchKey key = KeyOf(item);
        if (e.group == 0) {
            const auto [bin, made] = bins.try_emplace(key, batches.size());
            if (made) {
                batches.push_back({e.index});
            } else {
                batches[bin->second].push_back(e.index);
            }
            continue;
        }
        const Extracted3dItem* last = batches.empty() ? nullptr : &items[batches.back().front()];
        if (last != nullptr && last->mesh != MeshId{} && KeyOf(*last) == key) {
            batches.back().push_back(e.index);
        } else {
            batches.push_back({e.index});
        }
    }

    for (const std::vector<std::size_t>& batch : batches) {
        const Extracted3dItem& item = items[batch.front()];
        Draw3D draw;
        draw.instanceOffset = static_cast<std::uint32_t>(pass.instances.size());
        draw.instanceCount = static_cast<std::uint32_t>(batch.size());
        for (const std::size_t i : batch) pass.instances.push_back(Instance3D{items[i].world, items[i].color});
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
