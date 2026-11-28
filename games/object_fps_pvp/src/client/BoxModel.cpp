#include "client/BoxModel.hpp"

#include <array>
#include <cstdint>

namespace object_fps_pvp::client {

namespace Math = GYO::Engine::Math;
namespace Model = GYO::Engine::Model;

std::shared_ptr<const Model::ModelData> BoxModel(const Math::Vec3 size, const Math::Vec3 origin) {
    const Math::Vec3 low{-origin.x * size.x, -origin.y * size.y, -origin.z * size.z};
    const Math::Vec3 high = low + size;
    Model::Mesh mesh;
    mesh.name = "box";
    // Each face: its outward normal and its four corners counter-clockwise seen from outside.
    struct Face {
        Math::Vec3 normal;
        std::array<Math::Vec3, 4> corners;
    };
    const std::array<Face, 6> faces{{
        {{1, 0, 0}, {{{high.x, low.y, high.z}, {high.x, low.y, low.z}, {high.x, high.y, low.z}, {high.x, high.y, high.z}}}},
        {{-1, 0, 0}, {{{low.x, low.y, low.z}, {low.x, low.y, high.z}, {low.x, high.y, high.z}, {low.x, high.y, low.z}}}},
        {{0, 1, 0}, {{{low.x, high.y, high.z}, {high.x, high.y, high.z}, {high.x, high.y, low.z}, {low.x, high.y, low.z}}}},
        {{0, -1, 0}, {{{low.x, low.y, low.z}, {high.x, low.y, low.z}, {high.x, low.y, high.z}, {low.x, low.y, high.z}}}},
        {{0, 0, 1}, {{{low.x, low.y, high.z}, {high.x, low.y, high.z}, {high.x, high.y, high.z}, {low.x, high.y, high.z}}}},
        {{0, 0, -1}, {{{high.x, low.y, low.z}, {low.x, low.y, low.z}, {low.x, high.y, low.z}, {high.x, high.y, low.z}}}},
    }};
    for (const Face& face : faces) {
        const auto first = static_cast<std::uint32_t>(mesh.positions.size());
        for (const Math::Vec3& corner : face.corners) {
            mesh.positions.push_back(corner);
            mesh.normals.push_back(face.normal);
            mesh.uvs.push_back(Math::Vec2{0.0F, 0.0F});
        }
        for (const std::uint32_t corner : {0U, 1U, 2U, 0U, 2U, 3U}) mesh.indices.push_back(first + corner);
    }
    auto model = std::make_shared<Model::ModelData>();
    model->nodes.push_back(Model::Node{.name = "box"});
    mesh.node = 0;
    model->meshes.push_back(std::move(mesh));
    return model;
}

} // namespace object_fps_pvp::client
