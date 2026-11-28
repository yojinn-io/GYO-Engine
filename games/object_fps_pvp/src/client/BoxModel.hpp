#pragma once

// A box's model for the Client's things that are boxes (a door's leaf; the overhead UI card's targets use it too,
// interact-door declaration section 8): one mesh, its six faces with their own normals, no material (Mesh3d's color
// paints it).

#include "engine/math/linear/Vec3.hpp"
#include "engine/model/ModelData.hpp"

#include <memory>

namespace object_fps_pvp::client {

// A box of `size`, its local origin at `origin` inside it as a share of each side (0.5, 0.5, 0.5: the centre;
// 0, 0, 0.5: a door's leaf at its hinge, on the floor, mid-thickness).
[[nodiscard]] std::shared_ptr<const GYO::Engine::Model::ModelData> BoxModel(GYO::Engine::Math::Vec3 size, GYO::Engine::Math::Vec3 origin);

} // namespace object_fps_pvp::client
