#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Quaternion.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"

namespace Engine::Model {

// Model space uses metres, +Y up, +Z forward, and a left-handed basis (the
// Engine::Math conventions). Vectors, quaternions and matrices are Math types;
// Math::Multiply(parent, local) and Math::TransformPoint compose and apply them.

// Asset node transform: translation, rotation, scale (TRS).
struct Transform final {
    Math::Vec3 translation{};
    Math::Quaternion rotation{};
    Math::Vec3 scale{1.0F, 1.0F, 1.0F};
};

// Translation * Rotation * Scale; a zero rotation quaternion applies no rotation.
[[nodiscard]] Math::Matrix4 ToMatrix(const Transform& transform) noexcept;

struct Node final {
    std::string name;
    // Parent indices precede their children; nullopt denotes a root.
    std::optional<std::size_t> parentIndex;
    Transform localTransform{};
};

struct Material final {
    // Stable source material name. Applications map this to catalog AssetIds;
    // model loaders never open paths embedded in authoring files.
    std::string name;
    // Linear RGB with straight alpha, independent of a renderer or texture.
    // Missing source color keeps the neutral white multiplier.
    std::array<float, 4> baseColorLinear{1.0F, 1.0F, 1.0F, 1.0F};
};

struct SkinJoint final {
    std::size_t nodeIndex{};
    Math::Matrix4 geometryToJoint{};
};

struct SkinVertex final {
    Math::Vec3 position{};
    Math::Vec3 normal{0, 1, 0};
    // Top-left texture origin. Tile coordinates outside [0,1] are preserved;
    // the presentation material chooses repeat or clamp sampling.
    Math::Vec2 uv{};
    // Indices into the owning MeshPart::joints, not the global node table.
    std::array<std::uint32_t, 4> joints{};
    std::array<float, 4> weights{};
};

struct MeshPart final {
    std::string name;
    std::size_t nodeIndex{};
    std::size_t materialIndex{};
    // Geometry transforms affect rigid/unweighted vertices only; a weighted
    // vertex uses geometryToJoint, which already includes this transform.
    Math::Matrix4 geometryToNode{};
    std::vector<SkinVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<SkinJoint> joints;
};

template<class T> struct Keyframe final {
    double timeSeconds{};
    T value{};
};

struct NodeTrack final {
    std::size_t nodeIndex{};
    std::vector<Keyframe<Math::Vec3>> translations;
    std::vector<Keyframe<Math::Quaternion>> rotations;
    std::vector<Keyframe<Math::Vec3>> scales;
};

struct AnimationClip final {
    std::string name;
    double durationSeconds{};
    std::vector<NodeTrack> tracks;
};

// Fully owning CPU data: no renderer handles, importer pointers or file paths.
struct ModelAsset final {
    std::vector<Node> nodes;
    std::vector<Material> materials;
    std::vector<MeshPart> meshes;
    std::vector<AnimationClip> clips;

    [[nodiscard]] std::optional<std::size_t> FindClip(std::string_view name) const noexcept;
    [[nodiscard]] std::optional<std::size_t> FindNode(std::string_view name) const noexcept;
};

} // namespace Engine::Model
