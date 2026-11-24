#include "engine/model/ModelData.hpp"

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace GYO::Engine::Model {
namespace {

using Failure = ModelDataFailure;

// How far the weights of a vertex may be from summing to 1 (float rounding
// of four normalized weights stays far inside it).
constexpr float kWeightSumTolerance = 1.0e-4F;

bool Finite(const float v) noexcept { return std::isfinite(v); }
bool Finite(const Math::Vec2 v) noexcept { return std::isfinite(v.x) && std::isfinite(v.y); }
bool Finite(const Math::Vec3 v) noexcept { return Math::IsFinite(v); }
bool Finite(const Math::Quaternion q) noexcept { return Math::IsFinite(q); }
bool Finite(const Math::Matrix4& m) noexcept { return Math::IsFinite(m); }
bool Finite(const LocalTransform& t) noexcept {
    return Finite(t.translation) && Finite(t.rotation) && Finite(t.scale);
}

template <class T>
bool AllFinite(const std::vector<T>& values) noexcept {
    for (const T& v : values) {
        if (!Finite(v)) return false;
    }
    return true;
}

Base::Err<Failure> Fail(const ModelDataCode code, const char* message, std::string detail) {
    return Base::Err(Failure::Make(code, message, std::move(detail)));
}

template <class T>
Base::Result<void, Failure> CheckKeys(const Keys<T>& keys, const float duration, const std::string& where) {
    if (keys.times.size() != keys.values.size()) {
        return Fail(ModelDataCode::BadClip, "a channel has one value per key time", where);
    }
    for (std::size_t i = 0; i < keys.times.size(); ++i) {
        const float t = keys.times[i];
        if (!Finite(t) || t < 0.0F || t > duration) {
            return Fail(ModelDataCode::BadClip, "a key time is within [0, duration]",
                        where + " key " + std::to_string(i));
        }
        if (i > 0 && !(keys.times[i - 1] < t)) {
            return Fail(ModelDataCode::BadClip, "key times are strictly increasing",
                        where + " key " + std::to_string(i));
        }
        if (!Finite(keys.values[i])) {
            return Fail(ModelDataCode::BadClip, "a key value is finite", where + " key " + std::to_string(i));
        }
    }
    return {};
}

Base::Result<void, Failure> CheckSkin(const Skin& skin, const std::size_t nodeCount, const std::string& where) {
    for (const std::uint32_t joint : skin.joints) {
        if (joint >= nodeCount) {
            return Fail(ModelDataCode::BadSkin, "a skin joint is a node", where + " joint node " + std::to_string(joint));
        }
    }
    if (skin.inverseBindMatrices.size() != skin.joints.size()) {
        return Fail(ModelDataCode::BadSkin, "a skin has one inverse bind matrix per joint", where);
    }
    if (!AllFinite(skin.inverseBindMatrices)) {
        return Fail(ModelDataCode::BadSkin, "inverse bind matrices are finite", where);
    }
    return {};
}

// The per-vertex skinning of a mesh whose own rules hold.
Base::Result<void, Failure> CheckMeshSkin(const Mesh& mesh, const std::vector<Skin>& skins, const std::string& where) {
    const std::size_t vertexCount = mesh.positions.size();
    if (mesh.skin == kNoSkin) {
        if (!mesh.vertexJoints.empty() || !mesh.vertexWeights.empty()) {
            return Fail(ModelDataCode::BadSkin, "a mesh without a skin has no joints or weights", where);
        }
        return {};
    }
    if (mesh.skin >= skins.size()) return Fail(ModelDataCode::BadSkin, "a mesh's skin is a skin of the model", where);
    const std::size_t jointCount = skins[mesh.skin].joints.size();
    if (mesh.vertexJoints.size() != vertexCount || mesh.vertexWeights.size() != vertexCount) {
        return Fail(ModelDataCode::BadSkin, "a skinned mesh has joints and weights for every vertex", where);
    }
    for (std::size_t v = 0; v < vertexCount; ++v) {
        float sum = 0.0F;
        bool allZero = true;
        for (std::size_t k = 0; k < 4; ++k) {
            const float w = mesh.vertexWeights[v][k];
            if (!Finite(w) || w < 0.0F) {
                return Fail(ModelDataCode::BadSkin, "a weight is finite and not negative",
                            where + " vertex " + std::to_string(v));
            }
            if (mesh.vertexJoints[v][k] >= jointCount) {
                return Fail(ModelDataCode::BadSkin, "a vertex names joints of its skin",
                            where + " vertex " + std::to_string(v));
            }
            sum += w;
            allZero = allZero && w == 0.0F;
        }
        if (!allZero && std::fabs(sum - 1.0F) > kWeightSumTolerance) {
            return Fail(ModelDataCode::BadSkin, "the weights of a vertex sum to 1 or are all 0",
                        where + " vertex " + std::to_string(v) + " sum " + std::to_string(sum));
        }
    }
    return {};
}

} // namespace

std::optional<std::uint32_t> ModelData::FindNode(const std::string_view name) const noexcept {
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].name == name) return static_cast<std::uint32_t>(i);
    }
    return std::nullopt;
}

std::optional<std::uint32_t> ModelData::FindClip(const std::string_view name) const noexcept {
    for (std::size_t i = 0; i < clips.size(); ++i) {
        if (clips[i].name == name) return static_cast<std::uint32_t>(i);
    }
    return std::nullopt;
}

Base::Result<void, ModelDataFailure> ValidateClip(const std::span<const Node> nodes, const AnimationClip& clip) {
    const std::string where = "clip \"" + clip.name + "\"";
    if (clip.nodeCount != nodes.size()) {
        return Fail(ModelDataCode::BadClip, "a clip's node count is the node list's",
                    where + " nodeCount " + std::to_string(clip.nodeCount) + " nodes " + std::to_string(nodes.size()));
    }
    if (!Finite(clip.duration) || clip.duration < 0.0F) {
        return Fail(ModelDataCode::BadClip, "a clip's duration is finite and not negative", where);
    }
    std::vector<bool> seen(nodes.size(), false);
    for (const Track& track : clip.tracks) {
        const std::string trackWhere = where + " node " + std::to_string(track.node);
        if (track.node >= nodes.size()) {
            return Fail(ModelDataCode::BadClip, "a track names a node of the list", trackWhere);
        }
        if (seen[track.node]) return Fail(ModelDataCode::BadClip, "a node has at most one track", trackWhere);
        seen[track.node] = true;
        if (auto r = CheckKeys(track.translation, clip.duration, trackWhere + " translation"); !r) return r;
        if (auto r = CheckKeys(track.rotation, clip.duration, trackWhere + " rotation"); !r) return r;
        if (auto r = CheckKeys(track.scale, clip.duration, trackWhere + " scale"); !r) return r;
    }
    return {};
}

Base::Result<void, ModelDataFailure> Validate(const ModelData& model) {
    const std::size_t nodeCount = model.nodes.size();
    // 1
    for (std::size_t i = 0; i < nodeCount; ++i) {
        const Node& node = model.nodes[i];
        if (node.parent != kNoParent && node.parent >= i) {
            return Fail(ModelDataCode::BadNode, "a node's parent precedes it",
                        "node " + std::to_string(i) + " \"" + node.name + "\" parent " + std::to_string(node.parent));
        }
        if (!Finite(node.rest)) {
            return Fail(ModelDataCode::BadNode, "a node's rest pose is finite",
                        "node " + std::to_string(i) + " \"" + node.name + "\"");
        }
    }
    // 3 for the skins
    for (std::size_t k = 0; k < model.skins.size(); ++k) {
        if (auto r = CheckSkin(model.skins[k], nodeCount, "skin " + std::to_string(k)); !r) return r;
    }
    // 2, then 3 for the same mesh
    for (std::size_t m = 0; m < model.meshes.size(); ++m) {
        const Mesh& mesh = model.meshes[m];
        const std::string where = "mesh " + std::to_string(m) + " \"" + mesh.name + "\"";
        const std::size_t vertexCount = mesh.positions.size();
        if (mesh.node >= nodeCount) return Fail(ModelDataCode::BadMesh, "a mesh sits at a node", where);
        if (mesh.material != kNoMaterial && mesh.material >= model.materials.size()) {
            return Fail(ModelDataCode::BadMesh, "a mesh's material is a material of the model or none", where);
        }
        if (mesh.normals.size() != vertexCount || (!mesh.uvs.empty() && mesh.uvs.size() != vertexCount)) {
            return Fail(ModelDataCode::BadMesh, "a mesh has a normal, and a uv or none, per position", where);
        }
        if (mesh.indices.size() % 3 != 0) {
            return Fail(ModelDataCode::BadMesh, "a mesh's indices are three per triangle", where);
        }
        for (const std::uint32_t index : mesh.indices) {
            if (index >= vertexCount) return Fail(ModelDataCode::BadMesh, "an index names a vertex", where);
        }
        if (!AllFinite(mesh.positions) || !AllFinite(mesh.normals) || !AllFinite(mesh.uvs)) {
            return Fail(ModelDataCode::BadMesh, "a mesh's vertex values are finite", where);
        }
        if (auto r = CheckMeshSkin(mesh, model.skins, where); !r) return r;
    }
    // 4
    for (const AnimationClip& clip : model.clips) {
        if (auto r = ValidateClip(model.nodes, clip); !r) return r;
    }
    return {};
}

} // namespace GYO::Engine::Model
