// The compile-failure sample of the spatial queries (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md,
// section 4.5): the author's Query must carry Without<Kernel::ChildOf>. Built at configure time
// (tests/common/physics_3d/CMakeLists.txt): GYO_PHYSICS3D_SAMPLE_ACCEPTED is the control, every other macro a
// variant that must fail with its own static_assert.
#include "engine/physics_3d/SpatialQuery.hpp"

namespace P3 = GYO::Engine::Physics3d;
namespace K = GYO::Kernel;
namespace T = GYO::Engine::Transform;

#if defined(GYO_PHYSICS3D_SAMPLE_ACCEPTED)
using Colliders = K::Query<K::Read<P3::Collider>, K::Read<T::Transform>, K::Without<K::ChildOf>>;
#elif defined(GYO_PHYSICS3D_SAMPLE_NO_WITHOUT_CHILD_OF)
using Colliders = K::Query<K::Read<P3::Collider>, K::Read<T::Transform>>;
#endif

bool Sample(const Colliders& colliders) {
    auto hit = P3::CastRay(colliders, GYO::Engine::Math::Ray{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}}, 10.0F);
    return hit.has_value();
}
