#pragma once

// Meshes kept from frame to frame, as data (docs/architecture/target/v2/render.zh-Hant.md, section 8): the data side
// gives the ids, as RenderTextures does for textures. RenderMeshes hands out a MeshId when a mesh is asked for and
// keeps the asks of this frame (make, let go) in order; a backend reads them, makes its own GPU buffers and keys them
// by the id. 3D draws name a mesh by its id (Draw3D::mesh). Bevy's RenderAssets<RenderMesh> keyed by the mesh asset
// (bevy_render/src/render_asset.rs:220) is the same shape.

#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/render/RenderCommands.hpp"

namespace GYO::Engine::Render {

struct MeshCreate final {
    MeshId id;
    std::vector<Vertex3D> vertices;
    // Triangles: every three name vertices of this mesh.
    std::vector<std::uint32_t> indices;
};

struct MeshRelease final {
    MeshId id;
};

using MeshOp = std::variant<MeshCreate, MeshRelease>;

// render_world's Resource. The asks of a frame stay until render_world clears them after the frame was drawn (its
// Cleanup stage). Ids are never given twice.
class RenderMeshes final {
public:
    // A new mesh. No vertices, no indices, or a count of indices that is not a multiple of 3, is a Programmer Error.
    MeshId Create(std::vector<Vertex3D> vertices, std::vector<std::uint32_t> indices);
    // Lets the mesh go; draws must not name it any more. An id that is not live is a Programmer Error.
    void Release(MeshId id);

    [[nodiscard]] bool Live(MeshId id) const noexcept;
    [[nodiscard]] std::size_t LiveCount() const noexcept { return live_.size(); }
    [[nodiscard]] std::span<const MeshOp> Pending() const noexcept { return pending_; }
    void ClearPending() noexcept { pending_.clear(); }

private:
    std::uint32_t next_ = 1;
    std::vector<std::uint32_t> live_;
    std::vector<MeshOp> pending_;
};

inline void HashValue(Kernel::Hasher&, const RenderMeshes&) {}

} // namespace GYO::Engine::Render
