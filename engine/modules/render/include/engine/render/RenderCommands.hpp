#pragma once

// What a renderer is asked to draw this frame, as data (docs/architecture/target/v2/render.zh-Hant.md, section 2;
// SDL declaration section 3.3, P27; Design Invariant RD-1). No SDL, no GPU object, no backend type: a backend (today
// sdl_gpu's GPUSubmit) reads it and draws it; a test reads it and compares it. Batch R2a carries one kind of pass,
// the 2D pass (textures, blending, clipping); the 3D scene, cameras and custom shaders come with batch R2b.
//
// Coordinates of a 2D pass are view units with y down: the pass's view rectangle is stretched over the whole
// target, its top-left corner on the target's top-left pixel. Texture coordinate (0, 0) is a texture's top-left
// corner and (1, 1) its bottom-right one, the layout image's ImageData has (row 0 on top; nothing is flipped). The
// one place where up and down turn round is ViewToClip (y-down view units to y-up clip space).
//
// Colours are bytes as they are stored (sRGB-encoded for colour pictures), alpha straight: the 2D pass blends the
// bytes as they are, like Dear ImGui and SDL_Renderer do (render.zh-Hant.md section 2, "colour").

#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "engine/kernel/Hash.hpp"
#include "engine/math/geometry/Rect.hpp"
#include "engine/math/linear/Matrix4.hpp"
#include "engine/math/linear/Vec2.hpp"
#include "engine/math/linear/Vec3.hpp"
#include "engine/render/RenderTextures.hpp"

namespace GYO::Engine::Render {

// A colour as stored: red, green, blue, alpha, 8 bits each, alpha straight.
struct Color8 final {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend bool operator==(const Color8&, const Color8&) = default;
};

// One vertex of a 2D pass. Its byte layout is the shader's vertex input (ShaderAbi.hpp): position at 0, uv at 8,
// colour at 16, 20 bytes in all.
struct Vertex2D final {
    Math::Vec2 position{};
    Math::Vec2 uv{};
    Color8 color{};
};
static_assert(sizeof(Vertex2D) == 20 && alignof(Vertex2D) == 4);
static_assert(std::is_trivially_copyable_v<Vertex2D> && std::is_standard_layout_v<Vertex2D>);

// How a draw's colour is put on the target.
enum class BlendMode : std::uint8_t {
    // Straight alpha: colour = src * src.a + dst * (1 - src.a); alpha = src.a + dst.a * (1 - src.a).
    Alpha = 1,
    // The source replaces the target, alpha included (Bevy's AlphaMode2D::Opaque).
    Opaque,
};

[[nodiscard]] constexpr const char* ToString(const BlendMode mode) noexcept {
    switch (mode) {
    case BlendMode::Alpha: return "Alpha";
    case BlendMode::Opaque: return "Opaque";
    }
    return "Unknown";
}

// One draw of a 2D pass: indexCount indices from indexOffset of the pass's indices, each index plus vertexOffset
// naming a vertex of the pass, as triangles. Draws are drawn in their order; a later draw lies over an earlier one.
struct Draw2D final {
    // TextureId{} is the white texture: the vertex colours alone (shapes without a picture).
    TextureId texture{};
    BlendMode blend = BlendMode::Alpha;
    // Only what lies inside is drawn, in view units; nullopt: the whole target.
    std::optional<Math::Rect> clip;
    std::uint32_t indexOffset = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t vertexOffset = 0;
};

// A target's size in pixels.
struct TargetSize final {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const TargetSize&, const TargetSize&) = default;
};

// A rectangle of whole pixels on a target.
struct PixelRect final {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    friend bool operator==(const PixelRect&, const PixelRect&) = default;
};

// A 2D pass on one target. The vertices and indices of all its draws are in one list each, so a producer such as
// Dear ImGui hands its draw list over as it is.
struct Pass2D final {
    // The target, in camera's id space (CameraTarget::id): 0 is the main target, the main window when there is one.
    // What another id names is the backend's to map.
    std::uint32_t target = 0;
    // The whole target (not only the viewport, as a GPU clear does) is cleared to this colour before the draws (the
    // 2D pipeline gives it to a target's first camera only; Pipeline2d.hpp, Queue2d). A target's first pass of the frame with no clear starts from transparent black: a window's
    // picture is undefined when a frame begins.
    std::optional<Color8> clear;
    // The pixels of the target the pass draws on (a camera's viewport; the rest is left as it is, so a letterboxed
    // picture's bars are what the clear left); nullopt: the whole target. Cut to the target.
    std::optional<PixelRect> viewport;
    // The rectangle of view units stretched over the viewport; nullopt: the viewport's own pixels (view unit = pixel,
    // origin at the viewport's top-left pixel).
    std::optional<Math::Rect> view;
    std::vector<Vertex2D> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Draw2D> draws;
};

// One vertex of a 3D mesh: position, normal and uv, 32 bytes (ShaderAbi.hpp: the 3D pass binds position and uv; the
// normal is kept for lit shading to come). Model space follows math.md (left-handed, +Y up, +Z forward).
struct Vertex3D final {
    Math::Vec3 position{};
    Math::Vec3 normal{};
    Math::Vec2 uv{};
};
static_assert(sizeof(Vertex3D) == 32 && alignof(Vertex3D) == 4);
static_assert(std::is_trivially_copyable_v<Vertex3D> && std::is_standard_layout_v<Vertex3D>);

// A mesh kept from frame to frame (RenderMeshes). MeshId{} (0): none, the draw takes the pass's own vertices.
struct MeshId final {
    std::uint32_t value = 0;

    friend bool operator==(const MeshId&, const MeshId&) = default;
};

// Which triangles a 3D draw leaves out.
enum class CullMode : std::uint8_t {
    // Both faces are drawn.
    None = 1,
    // Triangles whose corners go clockwise on the screen are left out (counter-clockwise is the front, as Bevy's
    // FrontFace::Ccw).
    Back,
};

// One instance of a 3D draw: where it is and its colour (big-map declaration section 16; Bevy's MeshUniform,
// bevy_pbr/src/render/mesh.rs:532, whose world_from_local and bindless material slot are these two). Its byte layout
// is the 3D pass's instance input (ShaderAbi.hpp): world at 0 (Matrix4's 16 floats in order), colour at 64; 68 bytes.
struct Instance3D final {
    // Local to world.
    Math::Matrix4 world{};
    // Multiplies the texture's colours (bytes as stored, alpha straight, as the 2D pass).
    Color8 color{255, 255, 255, 255};
};
static_assert(sizeof(Instance3D) == 68 && alignof(Instance3D) == 4);
static_assert(std::is_trivially_copyable_v<Instance3D> && std::is_standard_layout_v<Instance3D>);

// One draw of a 3D pass: instanceCount instances from instanceOffset of the pass's instances, each drawing indexCount
// indices from indexOffset of its mesh's indices (or of the pass's own, mesh {}), each index plus vertexOffset naming a
// vertex there, as triangles, placed by the instance's world. Instances are drawn in their order (as draws are), so a
// blended draw of several instances blends them in that order. A draw of one thing is a draw of one instance.
struct Draw3D final {
    MeshId mesh{};
    std::uint32_t indexOffset = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t vertexOffset = 0;
    std::uint32_t instanceOffset = 0;
    std::uint32_t instanceCount = 0;
    TextureId texture{};
    BlendMode blend = BlendMode::Opaque;
    // false: drawn over whatever is nearer (an overlay in the world).
    bool depthTest = true;
    bool depthWrite = true;
    CullMode cull = CullMode::None;
};

// A 3D pass on one target (one camera's 3D scene). Its depth starts cleared unless clearDepth is false (each camera
// has a depth of its own by default, as in Bevy: a viewmodel camera after the world camera draws over the world); its
// colour starts as the target is (the camera's clear is a pass of its own before it). Draws are drawn in their order.
struct Pass3D final {
    std::uint32_t target = 0;
    // As Pass2D's: the pixels of the target the pass draws on; nullopt: the whole target.
    std::optional<PixelRect> viewport;
    // World to clip space (math.md: depth 0 at near, 1 at far; y up).
    Math::Matrix4 clipFromWorld{};
    // false: the depth the passes before it left (Bevy's Camera3dDepthLoadOp::Load); a depth no pass wrote this frame
    // is undefined, as in Bevy.
    bool clearDepth = true;
    // Geometry made for this frame only (a mesh skinned on the CPU, a shape that changes), named by draws with mesh {}.
    std::vector<Vertex3D> vertices;
    std::vector<std::uint32_t> indices;
    // The draws' instances, each draw naming its range.
    std::vector<Instance3D> instances;
    std::vector<Draw3D> draws;
};

using RenderPass = std::variant<Pass2D, Pass3D>;

// The target a pass draws on.
[[nodiscard]] inline std::uint32_t TargetOf(const RenderPass& pass) noexcept {
    return std::visit([](const auto& p) { return p.target; }, pass);
}

// This frame's passes, drawn in their order (render_world's Resource; empty at the start of each frame).
struct RenderCommands final {
    std::vector<RenderPass> passes;
};

// The render World is not decided state: nothing of it goes into a consistency fingerprint (as sdl_window's handle).
inline void HashValue(Kernel::Hasher&, const RenderCommands&) {}

// Clip space of a view: clip = position * scale + offset (x right and y up, -1 to 1 over the target).
struct ViewTransform final {
    Math::Vec2 scale{};
    Math::Vec2 offset{};
};

// The view rectangle's top-left corner goes to clip (-1, 1), its bottom-right corner to (1, -1). A view with zero
// width or height is a Programmer Error.
[[nodiscard]] ViewTransform ViewToClip(Math::Rect view);

// The pixels a pass draws on: its viewport cut to the target, or the whole target. Empty (zero width or height)
// when the viewport lies outside the target.
[[nodiscard]] PixelRect ViewportOf(const Pass2D& pass, TargetSize target);
// The same for a 3D pass.
[[nodiscard]] PixelRect ViewportOf(const Pass3D& pass, TargetSize target);

// The view of a pass on a target of this size: its own view, or the pixels of its viewport (ViewportOf).
[[nodiscard]] Math::Rect ViewOf(const Pass2D& pass, TargetSize target);

// The pixels a draw's clip lets through, the view stretched over viewport: those whose centre lies inside the clip,
// left and top edges included, right and bottom edges not (the rasterizer's own rule, so a clip on whole pixels is
// exact), cut to the viewport; nullopt clip: the whole viewport. Empty (zero width or height) when nothing is let
// through.
[[nodiscard]] PixelRect ClipToPixels(const std::optional<Math::Rect>& clip, Math::Rect view, PixelRect viewport);
// The same with the view stretched over the whole target.
[[nodiscard]] PixelRect ClipToPixels(const std::optional<Math::Rect>& clip, Math::Rect view, TargetSize target);

} // namespace GYO::Engine::Render
