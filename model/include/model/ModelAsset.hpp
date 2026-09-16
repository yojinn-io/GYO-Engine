#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::Model {

// Model space uses metres, +Y up, +Z forward, and a left-handed basis.
struct Vec2 final { float x{}, y{}; };
struct Vec3 final { float x{}, y{}, z{}; };
struct Quaternion final { float x{}, y{}, z{}, w{1.0F}; };
struct Transform final {
    Vec3 translation{};
    Quaternion rotation{};
    Vec3 scale{1.0F, 1.0F, 1.0F};
};

// Column-major storage, column vectors: parent * local transforms a point.
struct Matrix4 final {
    std::array<float, 16> values{
        1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

[[nodiscard]] Matrix4 Multiply(const Matrix4& a, const Matrix4& b) noexcept;
[[nodiscard]] Matrix4 ToMatrix(const Transform& transform) noexcept;
[[nodiscard]] Vec3 TransformPoint(const Matrix4& matrix, Vec3 point) noexcept;

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
};

struct SkinJoint final {
    std::size_t nodeIndex{};
    Matrix4 geometryToJoint{};
};

struct SkinVertex final {
    Vec3 position{};
    Vec3 normal{0, 1, 0};
    // Top-left texture origin. Tile coordinates outside [0,1] are preserved;
    // the presentation material chooses repeat or clamp sampling.
    Vec2 uv{};
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
    Matrix4 geometryToNode{};
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
    std::vector<Keyframe<Vec3>> translations;
    std::vector<Keyframe<Quaternion>> rotations;
    std::vector<Keyframe<Vec3>> scales;
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
