#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "render/RenderHandle.hpp"

namespace Engine::Render {

struct Float2 final {
    float x{};
    float y{};
};

struct Float3 final {
    float x{};
    float y{};
    float z{};
};

struct Color final {
    // Render colors are linear RGB with straight (unassociated) alpha. Color
    // assets intended for display must be decoded from sRGB before they are
    // supplied as clear colors or submission tints.
    float red{1.0F};
    float green{1.0F};
    float blue{1.0F};
    float alpha{1.0F};
};

struct Rect final {
    float x{};
    float y{};
    float width{};
    float height{};
};

// GYO 3D presentation uses a left-handed coordinate system: +Y is up, +Z is
// forward, and rotations are expressed in radians. Scale, X/Y/Z rotation, and
// translation are applied in that order.
struct Transform3D final {
    Float3 translation{};
    Float3 rotationRadians{};
    Float3 scale{1.0F, 1.0F, 1.0F};
};

struct PerspectiveCamera3D final {
    Float3 position{};
    Float3 rotationRadians{};
    float verticalFieldOfViewRadians{1.0471975512F};
    float nearClip{0.05F};
    float farClip{100.0F};
};

struct Vertex3D final {
    Float3 position{};
    Float2 uv{};
};

struct MeshView final {
    std::span<const Vertex3D> vertices{};
    std::span<const std::uint32_t> indices{};
};

enum class TextureColorSpace {
    Linear,
    SRgb,
};

// ImageView contains decoded pixels only. File IO and image decoding belong
// to asset loaders, not to a render backend.
struct ImageView final {
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t rowPitch{}; // zero means tightly packed width * 4
    std::span<const std::byte> rgba8{};
    TextureColorSpace colorSpace{TextureColorSpace::SRgb};
};

enum class SurfaceMode {
    Opaque,
    AlphaMasked,
    Sky,
};

enum class SamplerMode {
    LinearClamp,
    LinearWrap,
};

// Scene sprites are composited with world meshes before the per-frame scene
// color transform. Overlay sprites are composited afterwards, so UI and other
// display-referred content are not affected by scene exposure or gamma.
enum class CompositeLayer {
    Scene,
    Overlay,
};

struct UvTransform final {
    Float2 scale{1.0F, 1.0F};
    Float2 offset{};
};

// ViewModel meshes use their own camera and a fresh depth buffer after the
// world and Scene sprites, before the scene color transform and Overlay UI.
enum class MeshLayer {
    World,
    ViewModel,
};

// Sprite source rectangles use normalized texture coordinates with (x, y) at
// the visual top-left. The shared XY quad reaches the top of screen space from
// its v=1 vertices, so its V coordinate must be inverted inside that rectangle.
// Keeping this conversion in the neutral render contract prevents individual
// backends (or callers such as text presentation) from inventing ad-hoc flips.
[[nodiscard]] constexpr UvTransform MakeSpriteUvTransform(
    Rect sourceUv) noexcept {
    return {
        {sourceUv.width, -sourceUv.height},
        {sourceUv.x, sourceUv.y + sourceUv.height},
    };
}

struct MaterialDesc final {
    std::string shader{"builtin/unlit"};
    TextureHandle texture{}; // invalid selects the renderer's white texture
    Color tint{};
    SamplerMode sampler{SamplerMode::LinearClamp};
};

struct MeshSubmission final {
    MeshHandle mesh{};
    MaterialDesc material{};
    Transform3D transform{};
    UvTransform uv{};
    SurfaceMode surface{SurfaceMode::Opaque};
    bool doubleSided{};
    MeshLayer layer{MeshLayer::World};
};

struct SpriteSubmission final {
    MaterialDesc material{};
    Rect destinationPixels{};
    // Normalized texture rectangle whose origin is the visual top-left.
    Rect sourceUv{0.0F, 0.0F, 1.0F, 1.0F};
    Float2 pivotNormalized{};
    float rotationRadians{};
    CompositeLayer layer{CompositeLayer::Overlay};
};

// A relative adjustment applied to the linear scene before its standard sRGB
// render-target conversion. gammaAdjustment=1 is the identity; values above
// one brighten midtones. Exposure is measured in stops (EV), so +1 doubles
// linear scene intensity. The portable accepted range is exposure in [-8, 8]
// and gamma adjustment in [0.25, 4].
struct SceneColorTransform final {
    float exposureEv{};
    float gammaAdjustment{1.0F};
};

struct FrameDescription final {
    Color clearColor{0.0F, 0.0F, 0.0F, 1.0F};
    SceneColorTransform sceneColorTransform{};
};

enum class PresentStatus {
    Presented,
    Skipped,
};

} // namespace Engine::Render
