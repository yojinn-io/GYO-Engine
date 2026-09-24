#include "RetroFPS/App/EnemyPresentation.hpp"
#include "RetroFPS/App/CharacterPresentationDefinition.hpp"
#include "AssetDefinitionHelpers.hpp"
#include "engine/asset/loaders/TextureAsset.hpp"
#include "model_renderer/ModelRenderer.hpp"
#include "render/PrimitiveMesh.hpp"
#include "render/IRenderDevice.hpp"
#include "render/RenderQueue.hpp"
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace fps {
struct EnemyPresentation::Impl final {
    using Resource = Engine::ModelRenderer::ModelResource;
    using Instance = Engine::ModelRenderer::ModelInstance;
    Engine::Render::IRenderDevice* device{};
    struct Accessory {
        CharacterAccessoryDefinition definition;
        std::shared_ptr<Resource> resource;
    };
    struct Definition {
        std::shared_ptr<const EnemyRig> rig;
        std::shared_ptr<Resource> resource;
        std::shared_ptr<Resource> weaponResource;
        std::vector<Accessory> accessories;
    };
    struct Actor {
        std::string definition;
        std::unique_ptr<Instance> instance;
        std::unique_ptr<Instance> weaponInstance;
        std::vector<std::unique_ptr<Instance>> accessories;
    };
    struct Wire {
        Engine::Render::MeshHandle mesh;
        std::vector<std::uint32_t> indices;
    };
    std::unordered_map<std::string, Definition> definitions;
    std::unordered_map<EnemyId, Actor> actors;
    std::vector<Wire> wires;
    std::optional<std::string> stage;
    ~Impl() { ClearInstances(); }
    void ClearInstances() noexcept {
        actors.clear();
        if (device)
            for (const auto& wire : wires)
                static_cast<void>(device->ReleaseMesh(wire.mesh));
        wires.clear();
        stage.reset();
    }
    void WireMesh(const Engine::Render::MeshData& data, Engine::Render::Color color,
                  std::size_t index, Engine::Render::RenderQueue& queue) {
        if (index >= wires.size())
            wires.push_back({});
        auto& wire = wires[index];
        if (!wire.mesh || wire.indices != data.indices) {
            if (wire.mesh)
                static_cast<void>(device->ReleaseMesh(wire.mesh));
            wire.mesh = {};
            auto made = device->CreateMesh(data.View());
            if (!made)
                throw std::runtime_error(made.error().message);
            wire.mesh = made.value();
            wire.indices = data.indices;
        } else {
            auto changed = device->UpdateMeshVertices(wire.mesh, data.vertices);
            if (!changed)
                throw std::runtime_error(changed.error().message);
        }
        Engine::Render::MeshSubmission draw;
        draw.mesh = wire.mesh;
        draw.material.tint = color;
        draw.layer = Engine::Render::MeshLayer::WorldOverlay;
        draw.doubleSided = true;
        auto submitted = queue.Submit(draw);
        if (!submitted)
            throw std::runtime_error(submitted.error().message);
    }
};
EnemyPresentation::EnemyPresentation() = default;
EnemyPresentation::~EnemyPresentation() = default;
void EnemyPresentation::Reset() noexcept { impl_.reset(); }
bool EnemyPresentation::Initialize(Engine::Render::IRenderDevice& device,
                                   Engine::Asset::AssetManager& assets, const EnemyCatalog& catalog,
                                   std::string& error) {
    Reset();
    error.clear();
    try {
        auto next = std::make_unique<Impl>();
        next->device = &device;
        const auto createResource = [&](const CharacterPresentationDefinition& character,
                                        const std::shared_ptr<const Engine::Model::ModelAsset>& model) {
            std::vector<Engine::ModelRenderer::ModelMaterial> materials;
            std::vector<std::shared_ptr<const Engine::Asset::Loaders::TextureAsset>> pixels;
            for (const auto& binding : character.materials) {
                const auto& c = binding.baseColorLinear;
                Engine::ModelRenderer::ModelMaterial material;
                material.tint = {c[0], c[1], c[2], c[3]};
                material.sampler = binding.sampler;
                if (binding.textureAssetId) {
                    auto texture =
                        asset_definition_detail::LoadShared<Engine::Asset::Loaders::TextureAsset>(
                            assets, *binding.textureAssetId, Engine::Asset::AssetType::Texture());
                    material.texture = Engine::Render::ImageView{
                        texture->width, texture->height, texture->width * 4,
                        std::as_bytes(std::span<const std::uint8_t>(texture->rgba)),
                        Engine::Render::TextureColorSpace::SRgb};
                    pixels.push_back(std::move(texture));
                }
                materials.push_back(material);
            }
            auto made = Impl::Resource::Create(device, model, materials);
            if (!made)
                throw std::runtime_error(made.error());
            return std::move(made.value());
        };
        for (const auto& enemy : catalog.GetDefinitions()) {
            if (!enemy.rig)
                throw std::runtime_error("enemy has no resolved CPU rig");
            const auto character = LoadCharacterPresentationDefinition(assets, enemy.rig->characterAssetId, error);
            if (!character) throw std::runtime_error(error);
            auto resource = createResource(*character, enemy.rig->model);
            std::shared_ptr<Impl::Resource> weaponResource;
            if (enemy.rig->weapon) {
                const auto weapon = LoadCharacterPresentationDefinition(assets,
                    enemy.rig->weapon->characterAssetId, error);
                if (!weapon) throw std::runtime_error(error);
                weaponResource = createResource(*weapon, enemy.rig->weapon->model);
            }
            std::vector<Impl::Accessory> accessories;
            for (const auto& accessory : character->accessories)
                accessories.push_back({accessory, createResource(*accessory.presentation, accessory.presentation->model)});
            next->definitions.emplace(enemy.id, Impl::Definition{
                enemy.rig, std::move(resource), std::move(weaponResource), std::move(accessories)});
        }
        impl_ = std::move(next);
        return true;
    } catch (const std::exception& e) {
        error = std::string("enemy renderer: ") + e.what();
        return false;
    }
}
bool EnemyPresentation::Submit(const GameSessionSnapshot& snapshot,
                               Engine::Render::RenderQueue& queue, bool debug, std::string& error) {
    error.clear();
    try {
        if (!impl_)
            throw std::runtime_error("enemy renderer is not initialized");
        auto& state = *impl_;
        if (!snapshot.activeStage || !snapshot.player) {
            state.ClearInstances();
            return true;
        }
        if (state.stage != snapshot.activeStage->levelId) {
            state.ClearInstances();
            state.stage = snapshot.activeStage->levelId;
        }
        std::unordered_set<EnemyId> visible;
        for (const auto& enemy : snapshot.enemies) {
            visible.insert(enemy.id);
            auto definition = state.definitions.find(enemy.definitionId);
            if (definition == state.definitions.end())
                throw std::runtime_error("unknown enemy definition " + enemy.definitionId);
            const auto& rig = *definition->second.rig;
            const auto weaponPose = rig.weapon
                ? std::optional{BuildEnemyWeaponPose(rig, enemy.pose)} : std::nullopt;
            std::vector<Engine::Model::Pose> accessoryPoses;
            for (const auto& accessory : definition->second.accessories)
                accessoryPoses.push_back(BuildCharacterAccessoryPose(accessory.definition, enemy.pose));
            auto actor = state.actors.find(enemy.id);
            if (actor != state.actors.end() && actor->second.definition != enemy.definitionId) {
                state.actors.erase(actor);
                actor = state.actors.end();
            }
            if (actor == state.actors.end()) {
                auto created =
                    Impl::Instance::Create(definition->second.resource, enemy.pose,
                                           {-rig.anchor.x, -rig.anchor.y, -rig.anchor.z});
                if (!created)
                    throw std::runtime_error(created.error());
                std::unique_ptr<Impl::Instance> weaponInstance;
                if (weaponPose) {
                    auto weapon = Impl::Instance::Create(definition->second.weaponResource,
                        *weaponPose, {-rig.anchor.x, -rig.anchor.y, -rig.anchor.z});
                    if (!weapon)
                        throw std::runtime_error(weapon.error());
                    weaponInstance = std::move(weapon.value());
                }
                std::vector<std::unique_ptr<Impl::Instance>> accessoryInstances;
                for (std::size_t i = 0; i < accessoryPoses.size(); ++i) {
                    auto accessory = Impl::Instance::Create(definition->second.accessories[i].resource,
                        accessoryPoses[i], {-rig.anchor.x, -rig.anchor.y, -rig.anchor.z});
                    if (!accessory) throw std::runtime_error(accessory.error());
                    accessoryInstances.push_back(std::move(accessory.value()));
                }
                actor = state.actors
                            .emplace(enemy.id,
                                     Impl::Actor{enemy.definitionId, std::move(created.value()),
                                                 std::move(weaponInstance), std::move(accessoryInstances)})
                            .first;
            } else {
                auto changed = actor->second.instance->UpdatePose(enemy.pose);
                if (!changed)
                    throw std::runtime_error(changed.error());
                if (weaponPose) {
                    auto weaponChanged = actor->second.weaponInstance->UpdatePose(*weaponPose);
                    if (!weaponChanged)
                        throw std::runtime_error(weaponChanged.error());
                }
                for (std::size_t i = 0; i < accessoryPoses.size(); ++i) {
                    const auto accessoryChanged = actor->second.accessories[i]->UpdatePose(accessoryPoses[i]);
                    if (!accessoryChanged) throw std::runtime_error(accessoryChanged.error());
                }
            }
            const float flash = enemy.hitFlashRemainingSeconds > 0 ? 1.5F : 1;
            const Engine::Render::Transform3D transform{
                {enemy.position.x, 0, enemy.position.z},
                {0, enemy.yawRadians, 0},
                {rig.scale, rig.scale, rig.scale}};
            auto submitted = actor->second.instance->Submit(
                queue, transform,
                Engine::Render::MeshLayer::World, {flash, flash, flash, 1});
            if (!submitted)
                throw std::runtime_error(submitted.error());
            if (actor->second.weaponInstance) {
                auto weaponSubmitted = actor->second.weaponInstance->Submit(
                    queue, transform, Engine::Render::MeshLayer::World, {flash, flash, flash, 1});
                if (!weaponSubmitted)
                    throw std::runtime_error(weaponSubmitted.error());
            }
            for (const auto& accessory : actor->second.accessories) {
                const auto accessorySubmitted = accessory->Submit(queue, transform,
                    Engine::Render::MeshLayer::World, {flash, flash, flash, 1});
                if (!accessorySubmitted) throw std::runtime_error(accessorySubmitted.error());
            }
        }
        std::erase_if(state.actors,
                      [&](const auto& pair) { return !visible.contains(pair.first); });
        if (!debug)
            return true;
        std::size_t count = 0;
        const auto capsule = [&](const Engine::Collision::Capsule& c, Engine::Render::Color color,
                                 float thickness = 0.012F) {
            auto mesh = Engine::Render::MakeWireCapsule(
                {c.segmentStart.x, c.segmentStart.y, c.segmentStart.z},
                {c.segmentEnd.x, c.segmentEnd.y, c.segmentEnd.z}, c.radius, thickness);
            if (!mesh)
                throw std::runtime_error(mesh.error().message);
            state.WireMesh(mesh.value(), color, count++, queue);
        };
        const auto& p = *snapshot.player;
        // The first-person camera is inside this capsule: a world-sized stroke
        // becomes a wide band near the camera. Only the line width differs.
        capsule(Engine::Collision::ToCapsule(
                    {{p.position.x, p.feetY, p.position.z}, p.bodyHeight, p.collisionRadius}),
                {0.1F, 1, 0.1F, 1}, 0.0006F);
        for (const auto& enemy : snapshot.enemies)
            if (enemy.state != EnemyState::Dead) {
                capsule(Engine::Collision::ToCapsule(enemy.body), {0.1F, 1, 0.1F, 1});
                for (const auto& region : enemy.hurtboxes)
                    capsule(region.shape, {1, 0.85F, 0.05F, 1});
                if (enemy.attackShape)
                    capsule(*enemy.attackShape, {1, 0.05F, 0.05F, 1});
            }
        for (const auto& box : snapshot.worldCollisionBoxes) {
            auto mesh = Engine::Render::MakeWireBox({box.minimum.x, box.minimum.y, box.minimum.z},
                                                    {box.maximum.x, box.maximum.y, box.maximum.z});
            if (!mesh)
                throw std::runtime_error(mesh.error().message);
            state.WireMesh(mesh.value(), {0.1F, 0.8F, 1, 1}, count++, queue);
        }
        while (state.wires.size() > count) {
            static_cast<void>(state.device->ReleaseMesh(state.wires.back().mesh));
            state.wires.pop_back();
        }
        return true;
    } catch (const std::exception& e) {
        error = std::string("enemy presentation: ") + e.what();
        return false;
    }
}
} // namespace fps
