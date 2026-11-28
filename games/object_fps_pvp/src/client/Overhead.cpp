#include "object_fps_pvp/client/Overhead.hpp"

#include "object_fps_pvp/world/PvpWorld.hpp"

namespace object_fps_pvp::client {

namespace C = GYO::Engine::Camera;
namespace Math = GYO::Engine::Math;
namespace T = GYO::Engine::Transform;

void HashValue(GYO::Kernel::Hasher&, const Overhead&) {}
void HashValue(GYO::Kernel::Hasher&, const OverheadPart&) {}

OverheadPlacement PlaceOverhead(const C::ComputedCamera& world, const T::GlobalTransform& worldView, const C::ComputedCamera& hud,
                                const T::GlobalTransform& hudView, const Math::Vec3 anchor,
                                const GYO::Engine::Physics3d::BoxSet* walls) {
    const auto onPicture = C::WorldToViewport(world, worldView, anchor);
    if (!onPicture) {
        const C::ViewportConversionFailureCode code = onPicture.error().code;
        return code == C::ViewportConversionFailureCode::PastNearPlane || code == C::ViewportConversionFailureCode::PastFarPlane
                   ? OverheadHidden::Behind
                   : OverheadHidden::NoView;
    }
    // The viewport in logical pixels, as WorldToViewport gives its point.
    const float scale = world.scaleFactor;
    const Math::Vec2 minimum{static_cast<float>(world.viewport.position.x) / scale, static_cast<float>(world.viewport.position.y) / scale};
    const Math::Vec2 size{static_cast<float>(world.viewport.size.x) / scale, static_cast<float>(world.viewport.size.y) / scale};
    const Math::Vec2 point = *onPicture;
    if (point.x < minimum.x || point.y < minimum.y || point.x > minimum.x + size.x || point.y > minimum.y + size.y) {
        return OverheadHidden::OffScreen;
    }
    const auto onHud = C::ViewportToWorld2d(hud, hudView, point);
    if (!onHud) return OverheadHidden::NoView;
    if (walls != nullptr) {
        const Math::Vec3 eye{worldView.matrix.values[12], worldView.matrix.values[13], worldView.matrix.values[14]};
        if (world::Blocked(*walls, eye, anchor)) return OverheadHidden::Occluded;
    }
    return *onHud;
}

} // namespace object_fps_pvp::client
