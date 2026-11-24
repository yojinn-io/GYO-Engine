#include "object_fps_pvp/client/VoxelModelLoader.hpp"

#include "object_fps_pvp/data/VoxelMeshFile.hpp"

#include <string>

namespace object_fps_pvp::client {

namespace A = GYO::Engine::Asset;
namespace Base = GYO::Engine::Base;
namespace Model = GYO::Engine::Model;

Base::Result<VoxelModelSettings, A::LoaderFailure<VoxelModelCode>> VoxelModelLoader::ParseSettings(
    const std::optional<std::string_view> settingsJson) const {
    if (settingsJson && *settingsJson != "{}") {
        return Base::Err(A::LoaderFailure<VoxelModelCode>::Make(VoxelModelCode::BadSettings, "a voxel model takes no settings"));
    }
    return VoxelModelSettings{};
}

Base::Result<Model::ModelData, A::LoaderFailure<VoxelModelCode>> VoxelModelLoader::Load(const std::span<const std::byte> bytes,
                                                                                       const VoxelModelSettings&,
                                                                                       A::LoadContext&) const {
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    std::string error;
    const auto file = data::ParseVoxelMeshFile(text, error);
    if (!file) return Base::Err(A::LoaderFailure<VoxelModelCode>::Make(VoxelModelCode::NotVoxelMesh, "not a gyo.voxel_mesh v1", error));
    const data::VoxelSurface surface = data::ExpandQuads(*file);
    Model::ModelData model;
    Model::Node root;
    root.name = "voxels";
    model.nodes.push_back(root);
    Model::Material material;
    material.name = "palette";
    model.materials.push_back(material);
    Model::Mesh mesh;
    mesh.name = "voxels";
    mesh.node = 0;
    mesh.material = 0;
    mesh.positions = surface.positions;
    mesh.normals = surface.normals;
    mesh.uvs.reserve(surface.uvs.size());
    for (const auto& uv : surface.uvs) mesh.uvs.push_back({uv[0], uv[1]});
    mesh.indices = surface.indices;
    model.meshes.push_back(std::move(mesh));
    return model;
}

} // namespace object_fps_pvp::client
