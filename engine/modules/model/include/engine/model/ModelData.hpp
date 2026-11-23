#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/asset/AssetTypeName.hpp"
#include "engine/base/Error.hpp"
#include "engine/base/Result.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"

// A model as plain CPU data (docs/architecture/target/v2/model.zh-Hant.md,
// section 2; Design Invariant MD-2): the node hierarchy with its rest pose,
// materials as data, triangle meshes with their skins, and animation clips.
// No renderer handle, no decoded texture, no importer pointer, no file path
// that is opened: a Match, a test or a tool uses it with no device.
//
// Model space follows the Math conventions: metres, +Y up, +Z forward, a
// left-handed basis, column vectors (Math::Multiply(parent, local)).
//
// Immutable once loaded; read it from any thread and destroy it on any thread
// (asset AS-10 (c)). Its type name for the catalog is "model".

namespace GYO::Engine::Model {

// A pose relative to the parent node: translation, rotation, scale (TRS).
struct LocalTransform final {
    Math::Vec3 translation{};
    Math::Quaternion rotation{};
    Math::Vec3 scale{1.0F, 1.0F, 1.0F};
};

// Translation * Rotation * Scale.
[[nodiscard]] inline Math::Matrix4 ToMatrix(const LocalTransform& transform) noexcept {
    return Math::ComposeTRS(transform.translation, transform.rotation, transform.scale);
}

inline constexpr std::uint32_t kNoParent = std::numeric_limits<std::uint32_t>::max();
inline constexpr std::uint32_t kNoMaterial = std::numeric_limits<std::uint32_t>::max();

// One node of the hierarchy. A skeleton is the node list itself: every node
// may be a joint, and a clip or a skin names nodes by index.
struct Node final {
    std::string name;
    // An earlier node (parents precede their children), or kNoParent for a
    // root.
    std::uint32_t parent = kNoParent;
    // The pose the file gives the node when nothing animates it.
    LocalTransform rest{};
};

// A material is data a renderer reads; textures are named, never decoded.
struct Material final {
    std::string name;
    // Linear RGB with straight alpha; white when the file gives none.
    std::array<float, 4> baseColor{1.0F, 1.0F, 1.0F, 1.0F};
    // The base colour texture's file name as written in the file (relative
    // when the file has a relative one); empty when there is none. A product
    // maps it, or the material's name, to a catalog id.
    std::string baseColorTexture;

    [[nodiscard]] friend bool operator==(const Material&, const Material&) = default;
};

// Linear blend skinning data of one mesh (as Bevy's SkinnedMesh with its
// inverse bindposes and the joint index and weight attributes).
struct Skin final {
    // The nodes that move the mesh.
    std::vector<std::uint32_t> joints;
    // One per joint: from the mesh's space to the joint's space at bind time.
    // A skinned position in model space is the sum over the vertex's four
    // influences of weight * global(joint) * inverseBind(joint) * position.
    std::vector<Math::Matrix4> inverseBindMatrices;
    // One per vertex: indices into joints, then the weights, largest first.
    // The weights of a vertex sum to 1, or are all 0 (no influence).
    std::vector<std::array<std::uint16_t, 4>> vertexJoints;
    std::vector<std::array<float, 4>> vertexWeights;

    [[nodiscard]] bool Empty() const noexcept { return joints.empty(); }
};

// A triangle list with one material. A mesh without a skin sits at its node;
// a skinned mesh's vertices land in model space through the skin.
struct Mesh final {
    std::string name;
    std::uint32_t node = 0;
    std::uint32_t material = kNoMaterial;
    std::vector<Math::Vec3> positions;
    std::vector<Math::Vec3> normals;
    // Empty, or one per vertex. (0, 0) is the top-left corner of the texture,
    // the corner the first byte of an image's pixels is (image IM-2).
    std::vector<Math::Vec2> uvs;
    // Three per triangle.
    std::vector<std::uint32_t> indices;
    Skin skin;
};

// Keys of one channel. Times in seconds, strictly increasing, within the
// clip's duration; values interpolate linearly (rotations by slerp).
template <class T>
struct Keys final {
    std::vector<float> times;
    std::vector<T> values;

    [[nodiscard]] bool Empty() const noexcept { return times.empty(); }
};

// The animated channels of one node. An empty channel leaves the node's rest
// value.
struct Track final {
    std::uint32_t node = 0;
    Keys<Math::Vec3> translation;
    Keys<Math::Quaternion> rotation;
    Keys<Math::Vec3> scale;
};

// An animation clip of one node list. Nodes without a track keep their rest
// pose.
struct AnimationClip final {
    std::string name;
    float duration = 0.0F;
    // The size of the node list the clip was made for; sampling it against
    // another list is a Programmer Error.
    std::uint32_t nodeCount = 0;
    // At most one per node.
    std::vector<Track> tracks;
};

struct ModelData final {
    std::vector<Node> nodes;
    std::vector<Material> materials;
    std::vector<Mesh> meshes;
    std::vector<AnimationClip> clips;

    // The first node or clip with the name.
    [[nodiscard]] std::optional<std::uint32_t> FindNode(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> FindClip(std::string_view name) const noexcept;
};

// The code of the first rule a ModelData breaks (model.zh-Hant.md section 2.2),
// in the order Validate checks them.
enum class ModelDataCode : std::uint8_t {
    // A node's parent is not an earlier node, or a rest value is not finite.
    BadNode = 1,
    // A mesh's node or material is out of range, its vertex arrays differ in
    // length, an index is out of range, the index count is not a multiple of
    // 3, or a value is not finite.
    BadMesh = 2,
    // A skin's joint is out of range, the joints and the inverse bind
    // matrices differ in count, the per-vertex arrays are not one per vertex,
    // a vertex names a joint the skin does not have, or a weight is negative,
    // not finite, or the weights neither sum to 1 nor are all 0.
    BadSkin = 3,
    // A clip's node count is not the node list's, a track names a node out of
    // range or the same node twice, a channel's times and values differ in
    // count, the times are not strictly increasing or leave [0, duration], or
    // a value or the duration is not finite.
    BadClip = 4,
};

[[nodiscard]] constexpr const char* ToString(const ModelDataCode code) noexcept {
    switch (code) {
    case ModelDataCode::BadNode: return "BadNode";
    case ModelDataCode::BadMesh: return "BadMesh";
    case ModelDataCode::BadSkin: return "BadSkin";
    case ModelDataCode::BadClip: return "BadClip";
    }
    return "Unknown";
}

using ModelDataFailure = Base::Error<ModelDataCode>;

// The rules every ModelData the loader returns keeps. A tool or a test that
// builds one by hand checks it here before the functions of Animation.hpp
// read it.
[[nodiscard]] Base::Result<void, ModelDataFailure> Validate(const ModelData& model);

// The clip rules alone, against a node list (a clip made by Retarget, for
// example).
[[nodiscard]] Base::Result<void, ModelDataFailure> ValidateClip(std::span<const Node> nodes,
                                                                const AnimationClip& clip);

} // namespace GYO::Engine::Model

template <>
struct GYO::Engine::Asset::AssetTypeName<GYO::Engine::Model::ModelData> {
    static constexpr std::string_view value = "model";
};
