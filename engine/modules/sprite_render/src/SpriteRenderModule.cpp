#include "engine/sprite_render/SpriteRenderModule.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "engine/base/Result.hpp"
#include "engine/kernel/Params.hpp"
#include "engine/kernel/Registry.hpp"
#include "engine/kernel/World.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderTextures.hpp"
#include "engine/render_world/RenderWorldModule.hpp"
#include "engine/transform/Transform.hpp"
#include "engine/world_asset/AssetView.hpp"

namespace GYO::Engine::SpriteRender {

namespace {

using SystemResult = Base::Result<void, Kernel::KernelError>;

const SpriteRenderOptions& OptionsOf(const SpriteRenderOptions* given) {
    static const SpriteRenderOptions defaults{};
    return given != nullptr ? *given : defaults;
}

std::uint32_t LayersOf(const Kernel::Extract<Kernel::Read<Render::RenderLayers>>& layers, const Kernel::EntityId id) {
    const Render::RenderLayers* l = layers.Get(id);
    return l != nullptr ? l->mask : Render::RenderLayers{}.mask;
}

SystemResult ExtractSprites2d(Kernel::Extract<Kernel::Read<Render::Sprite>> sprites,
                              Kernel::Extract<Kernel::Read<Render::Shape2d>> shapes,
                              Kernel::Extract<Kernel::Read<Transform::GlobalTransform>> globals,
                              Kernel::Extract<Kernel::Read<Render::RenderLayers>> layers,
                              Kernel::ExtractRes<WorldAsset::AssetView> assets, Kernel::ResMut<Render::Extracted2dItems> out,
                              Kernel::ResMut<Render::RenderImages> images, Kernel::ResMut<Render::RenderTextures> textures) {
    const WorldAsset::AssetView* view = assets.Present() ? &*assets : nullptr;
    std::vector<Render::Extracted2dItem> items;
    Render::RenderImages& cache = images.Get();
    Render::RenderTextures& texs = textures.Get();
    const auto textureOf = [&](const Render::ImageRef& image, const Render::TextureFilter filter, Math::Vec2& size) {
        Render::ResolvedImage resolved = Render::ResolveImage(image, filter, view);
        return cache.Use(resolved.key, resolved.data, std::move(resolved.holder), texs, size);
    };
    sprites.ForEach([&](const Kernel::EntityId id, const Render::Sprite& sprite) {
        const Transform::GlobalTransform* g = globals.Get(id);
        if (g == nullptr) return;
        Math::Vec2 size{};
        const std::optional<Render::TextureId> texture = textureOf(sprite.image, sprite.filter, size);
        if (!texture) return;
        Render::Extracted2dItem item;
        item.entity = id;
        item.world = g->matrix;
        item.layers = LayersOf(layers, id);
        item.texture = *texture;
        item.blend = sprite.blend;
        Render::SpriteGeometry(sprite, size, item.vertices, item.indices);
        items.push_back(std::move(item));
    });
    shapes.ForEach([&](const Kernel::EntityId id, const Render::Shape2d& shape) {
        const Transform::GlobalTransform* g = globals.Get(id);
        if (g == nullptr || shape.vertices.empty()) return;
        Math::Vec2 size{};
        const std::optional<Render::TextureId> texture = textureOf(shape.image, shape.filter, size);
        if (!texture) return;
        Render::Extracted2dItem item;
        item.entity = id;
        item.world = g->matrix;
        item.layers = LayersOf(layers, id);
        item.texture = *texture;
        item.blend = shape.blend;
        item.vertices = shape.vertices;
        item.indices = Render::TriangleListOf(shape);
        items.push_back(std::move(item));
    });
    cache.EndFrame(texs);
    out.Get().items = std::move(items);
    return {};
}

SystemResult Queue2dPasses(Kernel::Res<Render::ExtractedCameras> cameras, Kernel::Res<Render::Extracted2dItems> items,
                           Kernel::ResMut<Render::RenderCommands> commands) {
    // A target's first pass of the frame clears the whole target; later cameras on it clear their viewport only.
    std::vector<std::uint32_t> drawn;
    for (const Render::Pass2D& pass : commands.Read().passes) drawn.push_back(pass.target);
    for (const Render::ExtractedCamera& camera : cameras->cameras) {
        const bool first = std::find(drawn.begin(), drawn.end(), camera.target) == drawn.end();
        std::optional<Render::Pass2D> pass = Render::Queue2d(camera, items->items, first);
        if (!pass) continue;
        drawn.push_back(camera.target);
        commands.Get().passes.push_back(std::move(*pass));
    }
    return {};
}

template <class T>
void PutIfAbsent(Kernel::World& world) {
    if (world.Resource<T>() == nullptr) world.SetResource(T{});
}

} // namespace

void SpriteRenderModule::Build(Orchestration::WorldSetup& setup) {
    const SpriteRenderOptions& options = OptionsOf(setup.Options<SpriteRenderOptions>());
    Kernel::Registrar& r = setup.Registrar();
    r.Resource<Render::Extracted2dItems>({.name = "Extracted2dItems"});
    r.Resource<Render::RenderImages>({.name = "RenderImages"});

    Kernel::SystemOptions extract;
    extract.name = "ExtractSprites2d";
    Kernel::Mount extractMount;
    extractMount.schedule = std::string(RenderWorld::kExtractSchedule);
    extractMount.stage = std::string(RenderWorld::kExtractSchedule);
    extract.mounts.push_back(std::move(extractMount));
    r.AddSystem<&ExtractSprites2d>(std::move(extract));

    Kernel::SystemOptions queue;
    queue.name = "Queue2dPasses";
    Kernel::Mount queueMount;
    queueMount.schedule = options.schedule;
    queueMount.stage = options.stage;
    queue.mounts.push_back(std::move(queueMount));
    r.AddSystem<&Queue2dPasses>(std::move(queue));

    setup.Prepare([](Kernel::World& world) -> Base::Result<void, Orchestration::OrchestrationError> {
        PutIfAbsent<Render::Extracted2dItems>(world);
        PutIfAbsent<Render::RenderImages>(world);
        return {};
    });
}

} // namespace GYO::Engine::SpriteRender
