#include "object_fps_pvp/character/CharacterRig.hpp"

#include "data/JsonFields.hpp"

#include "engine/asset/LoadContext.hpp"
#include "engine/io/fs/NativeFileSystem.hpp"
#include "engine/io/helpers/ReadAllBytes.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/model/Animation.hpp"
#include "engine/model/FbxLoader.hpp"
#include "engine/model/Retarget.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <string_view>

namespace object_fps_pvp::character {

namespace A = GYO::Engine::Asset;
namespace Base = GYO::Engine::Base;
namespace Json = GYO::Engine::Json;
namespace IO = GYO::Engine::IO;

namespace {

constexpr std::array<std::string_view, kClipCount> kClipNames{
    "idle", "walk", "jog", "shoot", "reload", "jump_start", "jump_loop", "jump_land", "death",
    "aim_up", "aim_neutral", "aim_down", "hit", "hit_head", "crouch_idle", "crouch_move"};

std::vector<std::byte> ReadBytes(IO::FS::NativeFileSystem& fs, const std::string& path) {
    auto uri = IO::FS::NativeFileUri(path);
    if (!uri) throw std::runtime_error(path + ": " + Base::Describe(uri.error()));
    auto bytes = IO::Helpers::ReadAllBytes(fs, *uri);
    if (!bytes) throw std::runtime_error(path + ": " + Base::Describe(bytes.error()));
    return std::move(*bytes);
}

std::string ReadText(IO::FS::NativeFileSystem& fs, const std::string& path) {
    const std::vector<std::byte> bytes = ReadBytes(fs, path);
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

// One catalog entry the character names: its path under the content root and its settings (asset_catalog.json, the
// game's Data Contract with the asset Module).
struct CatalogEntry final {
    std::string path;
    bool meshes = true;
};

std::map<std::string, CatalogEntry> ReadCatalog(IO::FS::NativeFileSystem& fs, const std::string& root) {
    const std::string text = ReadText(fs, root + "/asset_catalog.json");
    const auto document = data::ParseDocument(text);
    const auto catalog = data::AsObject(document.Root(), "asset_catalog.json");
    const auto assets = data::Member(catalog, "assets").AsArray();
    if (!assets) throw std::runtime_error("asset_catalog.json: assets is not an array");
    std::map<std::string, CatalogEntry> entries;
    for (const auto item : assets.value()) {
        const auto asset = data::AsObject(item, "a catalog entry");
        CatalogEntry entry;
        entry.path = data::String(data::Member(asset, "path"), "path");
        if (data::HasMember(asset, "settings")) {
            const auto settings = data::AsObject(data::Member(asset, "settings"), "settings");
            if (data::HasMember(settings, "meshes")) {
                const auto meshes = data::Member(settings, "meshes").AsBool();
                entry.meshes = meshes ? *meshes : true;
            }
        }
        entries[data::String(data::Member(asset, "id"), "id")] = entry;
    }
    return entries;
}

Model::ModelData LoadFbx(IO::FS::NativeFileSystem& fs, const std::string& root, const std::map<std::string, CatalogEntry>& catalog,
                         const std::string& id) {
    const auto found = catalog.find(id);
    if (found == catalog.end()) throw std::runtime_error("asset_catalog.json has no '" + id + "'");
    const std::string path = root + "/" + found->second.path;
    const std::vector<std::byte> bytes = ReadBytes(fs, path);
    Model::FbxLoader loader;
    Model::FbxSettings settings;
    settings.textures = false;  // no texture is needed here (the Client maps its own)
    settings.meshes = found->second.meshes;
    A::LoadContext context(A::AssetId(id), found->second.path, bytes.size());
    auto model = loader.Load(bytes, settings, context);
    if (!model) throw std::runtime_error(path + ": " + Base::Describe(model.error()));
    return std::move(*model);
}

std::uint32_t NodeOf(const Model::ModelData& model, const std::string& name, const std::string_view what) {
    const auto found = model.FindNode(name);
    if (!found) throw std::runtime_error(std::string{what} + ": the body has no node '" + name + "'");
    return *found;
}

Math::Vec3 Triple(const Json::Value& value, const std::string_view what) {
    const auto array = value.AsArray();
    if (!array || array.value().Size() != 3) throw std::runtime_error(std::string{what} + " is three numbers");
    float v[3]{};
    std::size_t i = 0;
    for (const auto item : array.value()) v[i++] = static_cast<float>(data::Number(item, what));
    return {v[0], v[1], v[2]};
}

// The model's height and lowest point in its rest pose, skinned as render skins it (model.zh-Hant.md 2.1).
void RestBounds(const Model::ModelData& model, float& minimum, float& maximum) {
    std::vector<Model::LocalTransform> rest(model.nodes.size());
    Model::RestPose(model.nodes, rest);
    std::vector<Math::Matrix4> nodes(model.nodes.size());
    Model::ComputeModelSpace(model.nodes, rest, nodes);
    minimum = std::numeric_limits<float>::max();
    maximum = -minimum;
    for (const Model::Mesh& mesh : model.meshes) {
        for (std::size_t v = 0; v < mesh.positions.size(); ++v) {
            Math::Vec3 p{};
            if (mesh.skin != Model::kNoSkin) {
                const Model::Skin& skin = model.skins[mesh.skin];
                for (std::size_t w = 0; w < 4; ++w) {
                    const float weight = mesh.vertexWeights[v][w];
                    if (weight == 0.0F) continue;
                    const std::uint16_t joint = mesh.vertexJoints[v][w];
                    const Math::Matrix4 m = Math::Multiply(nodes[skin.joints[joint]], skin.inverseBindMatrices[joint]);
                    p = p + Math::TransformPoint(m, mesh.positions[v]) * weight;
                }
            } else {
                p = Math::TransformPoint(nodes[mesh.node], mesh.positions[v]);
            }
            minimum = std::min(minimum, p.y);
            maximum = std::max(maximum, p.y);
        }
    }
}

} // namespace

std::optional<CharacterRig> LoadCharacterRig(const std::string& root, const float bodyHeight, std::string& error) {
    error.clear();
    try {
        IO::FS::NativeFileSystem fs;
        const std::string dir = root + "/characters/player/";
        CharacterRig rig;
        const std::map<std::string, CatalogEntry> catalog = ReadCatalog(fs, root);
        // character.json: the body model, and how the clips are carried over.
        const std::string characterText = ReadText(fs, dir + "character.json");
        const auto characterDocument = data::ParseDocument(characterText);
        const auto character = data::AsObject(characterDocument.Root(), "character.json");
        rig.bodyAssetId = data::String(data::Member(character, "model_asset_id"), "model_asset_id");
        auto body = std::make_shared<Model::ModelData>(LoadFbx(fs, root, catalog, rig.bodyAssetId));
        const auto binding = data::AsObject(data::Member(character, "animation_binding"), "animation_binding");
        Model::RetargetSettings settings;
        settings.translationScale = static_cast<float>(data::Number(data::Member(binding, "translation_scale"), "translation_scale"));
        std::vector<std::string> ignored;
        if (const auto list = data::Member(binding, "ignored_source_nodes").AsArray()) {
            for (const auto item : list.value()) ignored.push_back(data::String(item, "an ignored node"));
        }
        // animations.animset.json: the clip of each role.
        const std::string setText = ReadText(fs, dir + "animations.animset.json");
        const auto setDocument = data::ParseDocument(setText);
        const auto set = data::AsObject(setDocument.Root(), "animations.animset.json");
        const auto clips = data::AsObject(data::Member(set, "clips"), "clips");
        // The clips' model (the animation library), as the set names it; one library for every clip.
        std::string libraryId;
        for (std::size_t i = 0; i < kClipCount; ++i) {
            const auto entry = data::AsObject(data::Member(clips, kClipNames[i]), kClipNames[i]);
            const std::string id = data::String(data::Member(entry, "model_asset_id"), "model_asset_id");
            if (!libraryId.empty() && id != libraryId) throw std::runtime_error("the clips come from more than one model");
            libraryId = id;
        }
        const Model::ModelData library = LoadFbx(fs, root, catalog, libraryId);
        auto bound = Model::BindByName(library.nodes, body->nodes);
        if (!bound) throw std::runtime_error("binding the clips' nodes: " + Base::Describe(bound.error()));
        std::vector<Model::JointBinding> bindings;
        for (const Model::JointBinding& b : bound->bindings) {
            if (std::find(ignored.begin(), ignored.end(), library.nodes[b.source].name) == ignored.end()) bindings.push_back(b);
        }
        for (std::size_t i = 0; i < kClipCount; ++i) {
            const auto entry = data::AsObject(data::Member(clips, kClipNames[i]), kClipNames[i]);
            const std::string name = data::String(data::Member(entry, "clip"), "clip");
            const auto found = library.FindClip(name);
            if (!found) throw std::runtime_error("the animation library has no clip '" + name + "'");
            auto clip = Model::Retarget(library.nodes, body->nodes, library.clips[*found], bindings, settings);
            if (!clip) throw std::runtime_error("retargeting '" + name + "': " + Base::Describe(clip.error()));
            rig.clips[i] = std::move(*clip);
        }
        // presentation.json: the rules and the upper body.
        auto presentation = sim::ParsePresentation(ReadText(fs, dir + "presentation.json"), error);
        if (!presentation) throw std::runtime_error("presentation.json: " + error);
        rig.presentation = *presentation;
        rig.rules = sim::MakeAnimationRules(*presentation, rig.clips[static_cast<std::size_t>(Clip::Walk)].duration,
                                            rig.clips[static_cast<std::size_t>(Clip::Jog)].duration);
        const std::uint32_t upperRoot = NodeOf(*body, presentation->upperBodyRoot, "upper_body_root");
        rig.upperMask.assign(body->nodes.size(), 0);
        for (std::size_t i = 0; i < body->nodes.size(); ++i) {
            const std::uint32_t parent = body->nodes[i].parent;
            rig.upperMask[i] = i == upperRoot || (parent != Model::kNoParent && rig.upperMask[parent] != 0) ? 1 : 0;
        }
        rig.weaponNode = NodeOf(*body, presentation->weaponNode, "weapon node");
        const float* r = presentation->weaponRotation;
        rig.weaponMount = Math::ComposeTRS(presentation->weaponTranslation, Math::Quaternion{r[0], r[1], r[2], r[3]},
                                           Math::Vec3{presentation->weaponScale, presentation->weaponScale, presentation->weaponScale});
        // The body's height: the arena's body over the model's.
        float minimum = 0, maximum = 0;
        RestBounds(*body, minimum, maximum);
        if (!(maximum - minimum > 1e-3F)) throw std::runtime_error("the body model has no usable height");
        rig.scale = bodyHeight / (maximum - minimum);
        rig.anchor = {0.0F, minimum, 0.0F};
        // hitbox.json: the shapes on the bones.
        const std::string hitText = ReadText(fs, dir + "hitbox.json");
        const auto hitDocument = data::ParseDocument(hitText);
        const auto hits = data::AsObject(hitDocument.Root(), "hitbox.json");
        const auto shapes = data::Member(hits, "shapes").AsArray();
        if (!shapes) throw std::runtime_error("hitbox.json: shapes is not an array");
        for (const auto item : shapes.value()) {
            const auto shape = data::AsObject(item, "a hit shape");
            HitShapeDefinition definition;
            const std::string part = data::String(data::Member(shape, "part"), "part");
            definition.part = part == "head" ? sim::HitPart::Head : part == "leg" ? sim::HitPart::Leg : sim::HitPart::Body;
            if (part != "head" && part != "body" && part != "leg") throw std::runtime_error("hitbox.json: unknown part '" + part + "'");
            definition.radius = static_cast<float>(data::Number(data::Member(shape, "radius"), "radius"));
            if (!(definition.radius > 0)) throw std::runtime_error("hitbox.json: a radius is not positive");
            const std::string kind = data::String(data::Member(shape, "kind"), "kind");
            if (kind == "sphere") {
                definition.ball = true;
                definition.node = NodeOf(*body, data::String(data::Member(shape, "bone"), "bone"), "hitbox.json");
                definition.offset = Triple(data::Member(shape, "offset"), "offset");
            } else if (kind == "capsule") {
                definition.node = NodeOf(*body, data::String(data::Member(shape, "from"), "from"), "hitbox.json");
                definition.nodeTo = NodeOf(*body, data::String(data::Member(shape, "to"), "to"), "hitbox.json");
            } else {
                throw std::runtime_error("hitbox.json: unknown kind '" + kind + "'");
            }
            rig.hitShapes.push_back(definition);
        }
        rig.body = std::move(body);
        return rig;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

void ComposeBody(const CharacterRig& rig, const sim::Locomotion& locomotion, const sim::ActionPose& actions, const bool crouched,
                 const double idleSeconds, std::vector<Model::LocalTransform>& out) {
    const auto& nodes = rig.body->nodes;
    const std::size_t count = nodes.size();
    out.resize(count);
    const auto clip = [&rig](const Clip c) -> const Model::AnimationClip& { return rig.clips[static_cast<std::size_t>(c)]; };
    const auto sample = [&](const Clip c, const double seconds, const Model::Wrap wrap, std::vector<Model::LocalTransform>& pose) {
        pose.resize(count);
        Model::SampleClip(nodes, clip(c), static_cast<float>(seconds), wrap, pose);
    };
    const auto progress = [&](const Clip c, const double p) { return std::clamp(p, 0.0, 1.0) * clip(c).duration; };
    std::vector<Model::LocalTransform> lower(count), upper(count), scratch(count), other(count);
    if (actions.lower == sim::LowerClip::Death) {
        // The death owns the whole body.
        sample(Clip::Death, actions.lowerSeconds, Model::Wrap::Clamp, out);
        return;
    }
    // The lower body: the gait (its phase is the cycles walked), eased in from standing; or a jump phase.
    const double phase = locomotion.cycles - std::floor(locomotion.cycles);
    switch (actions.lower) {
    case sim::LowerClip::JumpStart: sample(Clip::JumpStart, progress(Clip::JumpStart, actions.lowerSeconds), Model::Wrap::Clamp, lower); break;
    case sim::LowerClip::JumpLoop: sample(Clip::JumpLoop, actions.lowerSeconds, Model::Wrap::Loop, lower); break;
    case sim::LowerClip::JumpLand: sample(Clip::JumpLand, progress(Clip::JumpLand, actions.lowerSeconds), Model::Wrap::Clamp, lower); break;
    default:
        if (crouched) {
            sample(Clip::CrouchIdle, idleSeconds, Model::Wrap::Loop, lower);
            sample(Clip::CrouchMove, phase * clip(Clip::CrouchMove).duration, Model::Wrap::Loop, other);
        } else {
            sample(Clip::Idle, idleSeconds, Model::Wrap::Loop, lower);
            sample(Clip::Walk, phase * clip(Clip::Walk).duration, Model::Wrap::Loop, scratch);
            sample(Clip::Jog, phase * clip(Clip::Jog).duration, Model::Wrap::Loop, other);
            Model::Blend(scratch, other, static_cast<float>(locomotion.jogWeight), other);
        }
        Model::Blend(lower, other, static_cast<float>(locomotion.moveWeight), lower);
        break;
    }
    // The upper body: the aim between neutral and up or down, plus the motion of the action (or of the idle) since its
    // first frame, in each node's own space (master's AddMotion, PlayerPose.cpp:44-52, 570-588: the aim stays while
    // the action plays, so the head keeps following the pitch during a shot).
    sample(Clip::AimNeutral, 0.0, Model::Wrap::Clamp, upper);
    if (actions.aim != 0.0F) {
        sample(actions.aim < 0 ? Clip::AimUp : Clip::AimDown, 0.0, Model::Wrap::Clamp, scratch);
        Model::Blend(upper, scratch, std::abs(actions.aim), upper);
    }
    Clip motionClip = Clip::Idle;
    double motionSeconds = idleSeconds;
    Model::Wrap motionWrap = Model::Wrap::Loop;
    switch (actions.upper) {
    case sim::UpperClip::Shoot: motionClip = Clip::Shoot; break;
    case sim::UpperClip::Reload: motionClip = Clip::Reload; break;
    case sim::UpperClip::Hit: motionClip = Clip::Hit; break;
    case sim::UpperClip::HitHead: motionClip = Clip::HitHead; break;
    case sim::UpperClip::Aim: break;
    }
    if (motionClip != Clip::Idle) {
        motionSeconds = progress(motionClip, actions.upperProgress);
        motionWrap = Model::Wrap::Clamp;
    }
    sample(motionClip, motionSeconds, motionWrap, other);
    sample(motionClip, 0.0, Model::Wrap::Clamp, scratch);
    for (std::size_t i = 0; i < count; ++i) {
        if (rig.upperMask[i] == 0) {
            out[i] = lower[i];
            continue;
        }
        const Model::LocalTransform& base = upper[i];
        const Math::Quaternion delta =
            Math::Multiply(Math::Inverse(Math::NormalizeOrIdentity(scratch[i].rotation)), Math::NormalizeOrIdentity(other[i].rotation));
        out[i].translation = base.translation + (other[i].translation - scratch[i].translation);
        out[i].rotation = Math::NormalizeOrIdentity(Math::Multiply(Math::NormalizeOrIdentity(base.rotation), delta));
        out[i].scale = base.scale;
    }
}

void ModelSpace(const CharacterRig& rig, const std::vector<Model::LocalTransform>& local, std::vector<Math::Matrix4>& out) {
    out.resize(rig.body->nodes.size());
    Model::ComputeModelSpace(rig.body->nodes, local, out);
}

Math::Matrix4 BodyToWorld(const CharacterRig& rig, const Math::Vec3& feet, const float yaw) {
    const Math::Matrix4 place = Math::Multiply(Math::MakeTranslation(feet), Math::MakeRotationY(yaw));
    return Math::Multiply(Math::Multiply(place, Math::MakeScale({rig.scale, rig.scale, rig.scale})),
                          Math::MakeTranslation(Math::Vec3{0.0F, 0.0F, 0.0F} - rig.anchor));
}

std::vector<HitShape> PlaceHitShapes(const CharacterRig& rig, const std::vector<Math::Matrix4>& modelSpace, const Math::Vec3& feet,
                                     const float yaw) {
    const Math::Matrix4 world = BodyToWorld(rig, feet, yaw);
    std::vector<HitShape> placed;
    placed.reserve(rig.hitShapes.size());
    for (const HitShapeDefinition& shape : rig.hitShapes) {
        HitShape out;
        out.part = shape.part;
        out.radius = shape.radius;
        if (shape.ball) {
            out.a = out.b = Math::TransformPoint(world, Math::TransformPoint(modelSpace[shape.node], shape.offset));
        } else {
            out.a = Math::TransformPoint(world, Math::TransformPoint(modelSpace[shape.node], {}));
            out.b = Math::TransformPoint(world, Math::TransformPoint(modelSpace[shape.nodeTo], {}));
        }
        placed.push_back(out);
    }
    return placed;
}

Math::Matrix4 WeaponToWorld(const CharacterRig& rig, const std::vector<Math::Matrix4>& modelSpace, const Math::Vec3& feet, const float yaw) {
    return Math::Multiply(Math::Multiply(BodyToWorld(rig, feet, yaw), modelSpace[rig.weaponNode]), rig.weaponMount);
}

} // namespace object_fps_pvp::character
