#include "engine/mesh_render/MeshRenderModule.hpp"

#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/model/Animation.hpp"
#include "engine/render/Compose.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/Pipeline3d.hpp"
#include "engine/render/RenderMeshes.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/world_asset/AssetView.hpp"

namespace GYO::Engine::MeshRender {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const MeshRenderOptions& OptionsOf(const MeshRenderOptions* given) {
    static const MeshRenderOptions defaults{};
    return given != nullptr ? *given : defaults;
}

// The model-space matrix of every node: the entity's SkinPose when it fits the model, else the rest pose.
std::vector<Math::Matrix4> NodesOf(const Model::ModelData& model, const Render::SkinPose* pose) {
    if (pose != nullptr && pose->nodes.size() == model.nodes.size()) return pose->nodes;
    std::vector<Model::LocalTransform> local(model.nodes.size());
    Model::RestPose(model.nodes, local);
    std::vector<Math::Matrix4> nodes(model.nodes.size());
    Model::ComputeModelSpace(model.nodes, local, nodes);
    return nodes;
}

SystemResult ExtractMeshes3d(Kernel::Extract<Kernel::Read<Render::Mesh3d>> meshes,
                             Kernel::Extract<Kernel::Read<Transform::GlobalTransform>> globals,
                             Kernel::Extract<Kernel::Read<Render::SkinPose>> poses,
                             Kernel::Extract<Kernel::Read<Render::RenderLayers>> layers,
                             Kernel::ExtractRes<WorldAsset::AssetView> assets, Kernel::ResMut<Render::Extracted3dItems> out,
                             Kernel::ResMut<Render::RenderModelMeshes> modelMeshes, Kernel::ResMut<Render::RenderMeshes> renderMeshes,
                             Kernel::ResMut<Render::RenderImages> images, Kernel::ResMut<Render::RenderTextures> textures) {
    const WorldAsset::AssetView* view = assets.Present() ? &*assets : nullptr;
    std::vector<Render::Extracted3dItem> items;
    meshes.ForEach([&](const Kernel::EntityId id, const Render::Mesh3d& m) {
        const Transform::GlobalTransform* g = globals.Get(id);
        if (g == nullptr) return;
        // The model: an asset loaded in the source's view, or one the author holds (ModelRef).
        const Model::ModelData* model = nullptr;
        Render::ModelMeshKey key;
        std::shared_ptr<const Model::ModelData> holder;
        if (const auto* handle = std::get_if<Asset::Handle<Model::ModelData>>(&m.model)) {
            if (view == nullptr) return;
            model = view->Get(*handle);
            key.asset = handle->Id().Text();
            key.life = Asset::Detail::HandleAccess::Life(*handle);
        } else {
            holder = std::get<std::shared_ptr<const Model::ModelData>>(m.model);
            model = holder.get();
            key.held = model;
        }
        if (model == nullptr || m.mesh >= model->meshes.size()) return;
        key.mesh = m.mesh;
        const Model::Mesh& mesh = model->meshes[m.mesh];
        Math::Vec2 size{};
        Render::ResolvedImage image = Render::ResolveImage(m.image, m.filter, view);
        const std::optional<Render::TextureId> texture =
            images.Get().Use(image.key, image.data, std::move(image.holder), textures.Get(), size);
        if (!texture) return;
        Render::Extracted3dItem item;
        item.entity = id;
        const Render::RenderLayers* l = layers.Get(id);
        item.layers = l != nullptr ? l->mask : Render::RenderLayers{}.mask;
        item.texture = *texture;
        item.color = m.color;
        item.blend = m.blend;
        item.depthTest = m.depthTest;
        item.depthWrite = m.depthWrite.value_or(m.blend == Render::BlendMode::Opaque);
        item.cull = m.cull;
        const std::vector<Math::Matrix4> nodes = NodesOf(*model, poses.Get(id));
        item.world = Render::PlaceMesh(g->matrix, mesh, nodes);
        if (mesh.skin != Model::kNoSkin && mesh.skin < model->skins.size()) {
            item.vertices = Render::SkinVertices(*model, mesh, nodes);
            item.indices = mesh.indices;
            item.indices.resize(item.indices.size() - item.indices.size() % 3);
            if (item.vertices.empty() || item.indices.empty()) return;
        } else {
            const std::optional<Render::MeshId> retained =
                modelMeshes.Get().Use(key, mesh, std::move(holder), renderMeshes.Get(), item.indexCount);
            if (!retained) return;
            item.mesh = *retained;
        }
        items.push_back(std::move(item));
    });
    out.Get().items = std::move(items);
    return {};
}

SystemResult Queue3dPasses(Kernel::Res<Render::ExtractedCameras> cameras, Kernel::Res<Render::Extracted3dItems> items,
                           Kernel::ResMut<Render::CameraPasses> passes) {
    Render::CameraPasses& slots = passes.Get();
    for (std::size_t i = 0; i < cameras->cameras.size() && i < slots.byCamera.size(); ++i) {
        std::optional<Render::Pass3D> pass = Render::Queue3d(cameras->cameras[i], items->items);
        if (pass) slots.byCamera[i].push_back(std::move(*pass));
    }
    return {};
}

// After ClearRenderFrame cleared this frame's mesh asks: the meshes of held models no Mesh3d drew this frame go with
// the next frame's asks (as RenderImages::EndFrame for held images).
SystemResult EndModelMeshesFrame(Kernel::ResMut<Render::RenderModelMeshes> modelMeshes, Kernel::ResMut<Render::RenderMeshes> renderMeshes) {
    modelMeshes.Get().EndFrame(renderMeshes.Get());
    return {};
}

template <class T>
void PutIfAbsent(Kernel::World& world) {
    if (world.Resource<T>() == nullptr) world.SetResource(T{});
}

} // namespace

void MeshRenderModule::Build(Orchestration::WorldSetup& setup) {
    const MeshRenderOptions& options = OptionsOf(setup.Options<MeshRenderOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<Render::Extracted3dItems>({.name = "Extracted3dItems"});
    r.Resource<Render::RenderModelMeshes>({.name = "RenderModelMeshes"});

    Kernel::SystemOptions extract;
    extract.name = "ExtractMeshes3d";
    Kernel::Mount extractMount;
    extractMount.schedule = std::string(RenderWorld::kExtractSchedule);
    extractMount.stage = std::string(RenderWorld::kExtractSchedule);
    extract.mounts.push_back(std::move(extractMount));
    r.AddSystem<&ExtractMeshes3d>(std::move(extract));

    Kernel::SystemOptions queue;
    queue.name = "Queue3dPasses";
    Kernel::Mount queueMount;
    queueMount.schedule = options.schedule;
    queueMount.stage = options.stage;
    queue.mounts.push_back(std::move(queueMount));
    r.AddSystem<&Queue3dPasses>(std::move(queue));

    Kernel::SystemOptions end;
    end.name = "EndModelMeshesFrame";
    Kernel::Mount endMount;
    // render_world's stage, in render_world's schedule (its options when given).
    const RenderWorld::RenderWorldOptions* renderWorld = setup.Options<RenderWorld::RenderWorldOptions>();
    endMount.schedule = renderWorld != nullptr ? renderWorld->schedule : RenderWorld::RenderWorldOptions{}.schedule;
    endMount.stage = std::string(RenderWorld::kCleanupStage);
    endMount.after.push_back("ClearRenderFrame");
    end.mounts.push_back(std::move(endMount));
    r.AddSystem<&EndModelMeshesFrame>(std::move(end));

    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent<Render::Extracted3dItems>(world);
        PutIfAbsent<Render::RenderModelMeshes>(world);
        return {};
    });
}

} // namespace GYO::Engine::MeshRender
