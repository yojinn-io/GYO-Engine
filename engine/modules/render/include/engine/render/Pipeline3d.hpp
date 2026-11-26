#pragma once

// The 3D pipeline as data and functions (docs/architecture/target/v2/render.zh-Hant.md, section 8; SDL declaration
// section 6.4, R2b; batch r2b-3d): what the render World extracts of the source World's Mesh3ds each frame and how
// one 3D camera's Pass3D is made. mesh_render's Systems call these; everything here runs without a World.
//
// Per kind (SDL declaration section 6.1 point 8, render.zh-Hant.md section 8.3): a mesh item is data for this frame
// (Extracted3dItems, rebuilt each extract; Bevy keeps RenderMeshInstances keyed by the source entity,
// bevy_pbr/src/render/mesh.rs:1214, because it keeps per-instance GPU data; here each frame's instances are written
// again, as Bevy's CPU path clears its instance buffer every frame, no_gpu_preprocessing.rs:64). What stays is keyed by
// the model mesh: its retained GPU mesh (RenderModelMeshes).

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/model/ModelData.hpp"
#include "engine/render/Drawables.hpp"
#include "engine/render/Pipeline2d.hpp"
#include "engine/render/RenderCommands.hpp"
#include "engine/render/RenderMeshes.hpp"

namespace GYO::Engine::Render {

// One mesh to draw this frame.
struct Extracted3dItem final {
    std::uint64_t entity = 0;
    // Local to world: the entity's GlobalTransform times the mesh's node (no skin), or the GlobalTransform alone (a
    // skinned mesh's vertices are in model space already).
    Math::Matrix4 world{};
    std::uint32_t layers = 1;
    TextureId texture{};
    Color8 color{255, 255, 255, 255};
    BlendMode blend = BlendMode::Opaque;
    bool depthTest = true;
    bool depthWrite = true;
    CullMode cull = CullMode::None;
    // A retained mesh (no skin) and its index count, or MeshId{} with this frame's vertices and indices (skinned).
    MeshId mesh{};
    std::uint32_t indexCount = 0;
    std::vector<Vertex3D> vertices;
    std::vector<std::uint32_t> indices;
};

// render's Resource in the render World (mesh_render): this frame's mesh items, in source entity order.
struct Extracted3dItems final {
    std::vector<Extracted3dItem> items;
};

// The key of a model mesh's retained GPU mesh: the asset (id and life) or the model the author holds (the address of
// its ModelData, or of its HandedModel: kept alive by RenderModelMeshes while it has a mesh), and the mesh's index.
struct ModelMeshKey final {
    std::string asset;
    std::uint64_t life = 0;
    const void* held = nullptr;
    std::uint32_t mesh = 0;

    friend auto operator<=>(const ModelMeshKey&, const ModelMeshKey&) = default;
};

// A model as this frame draws it: its key (the mesh index still 0), its data (null: an asset not loaded yet, or no
// source asset view), and for a model the author holds, the holder that keeps its key alive (as ResolveImage) and
// the data alive this frame. handed: the HandedModel, when it is one.
struct ResolvedModel final {
    ModelMeshKey key;
    const Model::ModelData* data = nullptr;
    std::shared_ptr<const void> holder;
    std::shared_ptr<const Model::ModelData> current;
    std::shared_ptr<HandedModel> handed;
};
// view: the source World's AssetView (read through ExtractRes), or null when the source has none (an asset then does
// not resolve; a held model does).
[[nodiscard]] ResolvedModel ResolveModel(const ModelRef& model, const WorldAsset::AssetView* view);

// render's Resource in the render World (mesh_render): the retained meshes made from model meshes, made the first
// time one is drawn (Bevy's RenderAssets<RenderMesh>). As RenderImages: a new life of an asset lets the old life's
// meshes go, and an asset's stay while the render World lives; a held model's go after a frame in which no Mesh3d drew
// it (EndFrame). bytesPerFrame: how many bytes of new meshes one frame makes (Bevy's RenderAssetBytesPerFrame,
// bevy_render/src/render_asset.rs:509-560; none: no limit), a soft limit as Bevy's: meshes are made until the frame's
// bytes reach it (the one that passes it is made), the next ones wait for a later frame.
class RenderModelMeshes final {
public:
    explicit RenderModelMeshes(std::optional<std::uint64_t> bytesPerFrame = std::nullopt) : bytesPerFrame_(bytesPerFrame) {}

    // The retained mesh of this model mesh, made on meshes when there is none; its index count in indexCount. holder
    // keeps a held model's key alive while its mesh is kept (null for an asset). A mesh without vertices or whole
    // triangles, or a new one past this frame's bytes: nullopt (nothing to draw this frame).
    std::optional<MeshId> Use(const ModelMeshKey& key, const Model::Mesh& mesh, std::shared_ptr<const void> holder,
                              RenderMeshes& meshes, std::uint32_t& indexCount);
    // Whether this model mesh has its retained mesh.
    [[nodiscard]] bool Has(const ModelMeshKey& key) const { return entries_.contains(key); }
    // At the end of the frame's extract (ExtractMeshes3d, the one user): the meshes of held models no Mesh3d drew this
    // frame are released (their release goes with this frame's asks).
    void EndFrame(RenderMeshes& meshes);
    [[nodiscard]] std::size_t Count() const noexcept { return entries_.size(); }
    // The bytes of the meshes made this frame (vertices and indices as they are handed to RenderMeshes).
    [[nodiscard]] std::uint64_t BytesThisFrame() const noexcept { return bytesThisFrame_; }

private:
    struct Entry {
        MeshId mesh;
        std::uint32_t indexCount = 0;
        std::shared_ptr<const void> holder;
        bool used = false;
    };
    std::map<ModelMeshKey, Entry> entries_;
    std::optional<std::uint64_t> bytesPerFrame_;
    std::uint64_t bytesThisFrame_ = 0;
};

inline void HashValue(Kernel::Hasher&, const Extracted3dItems&) {}
inline void HashValue(Kernel::Hasher&, const RenderModelMeshes&) {}

// A model mesh's vertices as Vertex3D: positions, normals (zero when the mesh has none) and uvs (zero when none).
[[nodiscard]] std::vector<Vertex3D> MeshVertices(const Model::Mesh& mesh);

// Layer 3 of skeletal animation on the CPU (model.zh-Hant.md section 2.1, the formula render follows): each vertex's
// model-space position is the sum over its four influences of weight x nodes[joint] x inverseBind(joint) x position;
// normals the same with the matrices' rotation part, normalized. A vertex whose weights are all 0 keeps its
// position. nodes: the model-space matrix of every node (SkinPose). The mesh must have a skin of model.
[[nodiscard]] std::vector<Vertex3D> SkinVertices(const Model::ModelData& model, const Model::Mesh& mesh,
                                                 std::span<const Math::Matrix4> nodes);

// Local to world of a model mesh: the entity's GlobalTransform times the mesh's node in model space (nodes) when the
// mesh has no skin; the GlobalTransform alone for a skinned mesh (its vertices are in model space once skinned).
[[nodiscard]] Math::Matrix4 PlaceMesh(const Math::Matrix4& global, const Model::Mesh& mesh, std::span<const Math::Matrix4> nodes);

// The 3D pass one camera draws: its viewport, its clip-from-world, and the items whose layers it draws: first the
// opaque, depth-tested ones near to far (fewer pixels shaded behind nearer ones; Bevy bins its Opaque3d by pipeline,
// material and mesh instead, bevy_core_pipeline/src/core_3d/mod.rs:172-188), then the blended, depth-tested ones far
// to near (Bevy's Transparent3d), then those not depth-tested far to near (overlays
// in the world); ties by source entity. Distance: the view-space z of the item's origin. Items that draw the same
// retained mesh with the same texture, blend, depth and cull are instances of one draw, each with its world and colour
// (automatic batching, big-map declaration section 16): the opaque ones wherever they stand (a draw where its nearest
// instance stands, its instances near to far), the others only when they are neighbours in that order. An item with
// geometry of its own (skinned) is a draw of its own. nullopt: the camera is not valid or not a 3D camera.
[[nodiscard]] std::optional<Pass3D> Queue3d(const ExtractedCamera& camera, std::span<const Extracted3dItem> items);

} // namespace GYO::Engine::Render
