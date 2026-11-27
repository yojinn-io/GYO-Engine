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
    } else if (const auto* held = std::get_if<std::shared_ptr<const Model::ModelData>>(&model)) {
        out.current = *held;
        out.holder = out.current;
        out.data = out.current.get();
        out.key.held = out.data;
    } else if (const auto& handed = std::get<std::shared_ptr<HandedModel>>(model)) {
        out.current = handed->Model();
        out.holder = handed;
        out.handed = handed;
        out.data = out.current.get();
        out.key.held = handed.get();
    }
    return out;
}

void HandedModel::TakeVertices() {
    const std::lock_guard lock(mutex_);
    if (taken_ || model_ == nullptr) return;
    auto left = std::make_shared<Model::ModelData>();
    left->nodes = model_->nodes;
    left->skins = model_->skins;
    left->clips = model_->clips;
    left->materials = model_->materials;
    left->meshes.reserve(model_->meshes.size());
    for (const Model::Mesh& mesh : model_->meshes) {
        if (mesh.skin != Model::kNoSkin) {
            left->meshes.push_back(mesh);
            continue;
        }
        Model::Mesh stays;
        stays.name = mesh.name;
        stays.node = mesh.node;
        stays.material = mesh.material;
        stays.skin = mesh.skin;
        left->meshes.push_back(std::move(stays));
    }
    model_ = std::move(left);
    taken_ = true;
}

bool HasGeometry(const Model::Mesh& mesh) noexcept {
    return !mesh.positions.empty() && !mesh.indices.empty() && mesh.indices.size() % 3 == 0;
}

std::optional<MeshId> RenderModelMeshes::Use(const ModelMeshKey& key, const Model::Mesh& mesh, std::shared_ptr<const void> holder,
                                             RenderMeshes& meshes, std::uint32_t& indexCount, const bool whileHeld) {
    auto found = entries_.find(key);
    if (found == entries_.end()) {
        if (!HasGeometry(mesh)) return std::nullopt;
        // This frame's bytes are spent: a later frame makes it.
        if (bytesPerFrame_ && bytesThisFrame_ >= *bytesPerFrame_) {
            ++waitingThisFrame_;
            return std::nullopt;
        }
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
        bytesThisFrame_ += mesh.positions.size() * sizeof(Vertex3D) + mesh.indices.size() * sizeof(std::uint32_t);
        found = entries_.emplace(key, Entry{id, static_cast<std::uint32_t>(mesh.indices.size()), std::move(holder), false, whileHeld}).first;
    }
    found->second.used = true;
    indexCount = found->second.indexCount;
    return found->second.mesh;
}

void RenderModelMeshes::EndFrame(RenderMeshes& meshes) {
    bytesThisFrame_ = 0;
    waiting_ = waitingThisFrame_;
    waitingThisFrame_ = 0;
    // How many of this cache's entries hold each handed holder: the holder is held elsewhere when it has more owners.
    std::map<const void*, long> ours;
    for (const auto& [key, entry] : entries_) {
        if (entry.whileHeld) ++ours[entry.holder.get()];
    }
    for (auto it = entries_.begin(); it != entries_.end();) {
        const bool gone = it->second.whileHeld ? it->second.holder.use_count() <= ours[it->second.holder.get()]
                                               : it->first.held != nullptr && !it->second.used;
        if (gone) {
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
    if (!camera.is3d || !camera.computed.valid || !camera.view.valid) return std::nullopt;
    // The camera's view (camera's CM-7): world to clip space for the pass, view space for the distances.
    const Math::Matrix4& viewFromWorld = camera.view.viewFromWorld;
    Pass3D pass;
    pass.target = camera.target;
    const Camera::Viewport& vp = camera.computed.viewport;
    pass.viewport = PixelRect{static_cast<std::uint32_t>(vp.position.x), static_cast<std::uint32_t>(vp.position.y),
                              static_cast<std::uint32_t>(vp.size.x), static_cast<std::uint32_t>(vp.size.y)};
    pass.clipFromWorld = camera.view.clipFromWorld;
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
        order.push_back(Entry{i, group, Math::TransformPoint(viewFromWorld, origin).z});
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
    // bins are drawn by their nearest instance and a pass where nothing shares a key is drawn as before. The blended and
    // the not depth-tested are sorted phases: only neighbours with the same key join (Bevy's batching/mod.rs:236-243),
    // so the far-to-near order holds. Items with geometry of their own (skinned on the CPU) never join.
    // batchOf[k]: the batch of order[k]; firstOf[b]: the item that opened batch b (its draw's state).
    std::vector<std::size_t> batchOf(order.size());
    std::vector<std::size_t> firstOf;
    std::map<BatchKey, std::size_t> bins;
    for (std::size_t k = 0; k < order.size(); ++k) {
        const Entry& e = order[k];
        const Extracted3dItem& item = items[e.index];
        if (item.mesh == MeshId{}) {
            batchOf[k] = firstOf.size();
            firstOf.push_back(e.index);
            continue;
        }
        const BatchKey key = KeyOf(item);
        if (e.group == 0) {
            const auto [bin, made] = bins.try_emplace(key, firstOf.size());
            if (made) firstOf.push_back(e.index);
            batchOf[k] = bin->second;
            continue;
        }
        // An item with geometry of its own has mesh 0 in its key, so it never matches an item with a mesh.
        if (!firstOf.empty() && KeyOf(items[firstOf.back()]) == key) {
            batchOf[k] = firstOf.size() - 1;
        } else {
            batchOf[k] = firstOf.size();
            firstOf.push_back(e.index);
        }
    }

    // Each batch's instances in one range, in the order of its items (near to far in a bin): a count, then a fill.
    std::vector<std::uint32_t> offsetOf(firstOf.size() + 1, 0);
    for (const std::size_t b : batchOf) ++offsetOf[b + 1];
    for (std::size_t b = 0; b < firstOf.size(); ++b) offsetOf[b + 1] += offsetOf[b];
    std::vector<std::uint32_t> cursor(offsetOf.begin(), offsetOf.end() - 1);
    pass.instances.resize(order.size());
    for (std::size_t k = 0; k < order.size(); ++k) {
        const Extracted3dItem& item = items[order[k].index];
        pass.instances[cursor[batchOf[k]]++] = Instance3D{item.world, item.color};
    }

    pass.draws.reserve(firstOf.size());
    for (std::size_t b = 0; b < firstOf.size(); ++b) {
        const Extracted3dItem& item = items[firstOf[b]];
        Draw3D draw;
        draw.instanceOffset = offsetOf[b];
        draw.instanceCount = offsetOf[b + 1] - offsetOf[b];
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
    for (RenderPass& pass : passes.nonCameraViews) out.passes.push_back(std::move(pass));
    passes.nonCameraViews.clear();
    std::vector<std::optional<TextureId>> drawn;
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
            for (std::vector<RenderPass>& phase : passes.byCamera[i].lists) {
                for (RenderPass& pass : phase) out.passes.push_back(std::move(pass));
                phase.clear();
            }
        }
    }
}

} // namespace GYO::Engine::Render
