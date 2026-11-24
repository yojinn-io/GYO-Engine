#include "object_connect/runtime/PlayfieldFit.hpp"

#include "object_connect/ui/MenuLayout.hpp"

#include "engine/camera/Camera.hpp"

#include <cstdint>

namespace object_connect::runtime {

std::optional<PlayfieldFit> FitPlayfield(const GYO::Engine::Math::Vec2i physicalSize, const float scaleFactor) {
    namespace C = GYO::Engine::Camera;
    C::OrthographicProjection projection = C::OrthographicProjection::Default2d();
    projection.scalingMode = C::ScalingFixed{ui::kUiWidth, ui::kUiHeight};
    const C::TargetSize size{physicalSize, scaleFactor};
    const C::Letterbox letterbox{ui::kUiWidth, ui::kUiHeight};
    const C::ComputedCamera computed = C::Compute(C::Camera{}, C::Projection{projection}, &size, &letterbox);
    if (!computed.valid || computed.viewport.size.x <= 0 || computed.viewport.size.y <= 0) {
        return std::nullopt;
    }
    const C::Viewport& viewport = computed.viewport;
    PlayfieldFit fit;
    fit.physical = {static_cast<std::uint32_t>(viewport.position.x), static_cast<std::uint32_t>(viewport.position.y),
                    static_cast<std::uint32_t>(viewport.size.x), static_cast<std::uint32_t>(viewport.size.y)};
    fit.logicalOrigin = {static_cast<float>(viewport.position.x) / computed.scaleFactor,
                         static_cast<float>(viewport.position.y) / computed.scaleFactor};
    fit.logicalSize = {static_cast<float>(viewport.size.x) / computed.scaleFactor,
                       static_cast<float>(viewport.size.y) / computed.scaleFactor};
    fit.windowLogicalSize = {static_cast<float>(physicalSize.x) / computed.scaleFactor,
                             static_cast<float>(physicalSize.y) / computed.scaleFactor};
    return fit;
}

} // namespace object_connect::runtime
